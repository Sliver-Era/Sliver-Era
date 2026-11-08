#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//对象创建
	void Input_Manager::object_create(const json& config)
	{
		//若对象名字段无效
		if (!detail::field_check<string>(config, "name"))
		{
			logger.warn("Input_Manager::对象创建缺少有效 name 字段\n已驳回");
			return;
		}
		//获取对象名
		const string name = config["name"];
		//若对象名已存在
		if (name_index.count(name))
		{
			logger.warn("Input_Manager::对象 {} 已存在\n已驳回重复创建", name);
			return;
		}

		//宿主名（可空）
		string host{};
		if (config.contains("host") && config["host"].is_string())
			host = config["host"].get<string>();
		//若指定宿主
		if (!host.empty())
		{
			//宿主必须已经存在
			if (!name_index.count(host))
			{
				logger.warn("Input_Manager::宿主 {} 不存在\n对象 {} 创建已驳回", host, name);
				return;
			}
			//宿主必须是主对象
			if (!object_is_main(host))
			{
				logger.warn("Input_Manager::宿主 {} 不是主对象\n对象 {} 创建已驳回", host, name);
				return;
			}
		}

		//预先解析自身映射表（失败则不创建）
		std::unordered_map<Input_Key, std::vector<Input_Binding>> mapping{};
		if (config.contains("mapping") && !input_mapping::mapping_parse(config["mapping"], mapping))
		{
			logger.warn("Input_Manager::对象 {} 映射表解析失败\n创建已驳回", name);
			return;
		}

		//预先解析注入声明
		std::unordered_map<Input_Key, std::vector<Input_Binding>> injected_declared{};
		if (config.contains("added_keys") && !input_mapping::mapping_parse(config["added_keys"], injected_declared))
		{
			logger.warn("Input_Manager::对象 {} 新增映射解析失败\n创建已驳回", name);
			return;
		}

		//预先解析轴绑定表
		std::unordered_map<string, Axis_Binding> axis_binding{};
		if (config.contains("axes") && !input_mapping::axis_parse(config["axes"], axis_binding))
		{
			logger.warn("Input_Manager::对象 {} 轴绑定表解析失败\n创建已驳回", name);
			return;
		}

		//预先解析监听表（依赖宿主自身映射表）
		std::unordered_set<Input_Key> listening{};
		if (config.contains("listen"))
		{
			//若未挂载宿主
			if (host.empty())
				logger.warn("Input_Manager::对象 {} 未挂载宿主，监听表已忽略", name);
			else
			{
				//获取宿主对象
				const Input_Object* host_object = object_find(host);
				//若宿主存在则解析监听表
				if (host_object != nullptr)
					input_mapping::listening_parse(config["listen"], *host_object, listening);
			}
		}

		//从对象池分配新对象
		const uint64_t ID = objects.build(Input_Object{});
		//获取新对象
		Input_Object* object = object_find(ID);
		//若对象创建失败
		if (object == nullptr)
		{
			logger.error("Input_Manager::对象池分配失败");
			return;
		}

		//写入对象信息
		object->object_name = name;
		object->host_name = host;
		object->focusable = config.value("focusable", true);
		object->enabled = config.value("enabled", true);
		object->text_sink = config.value("text_sink", false);
		object->acl_key = object->event_terminal.key().value_or(0);
		object->mapping = std::move(mapping);
		object->axis_binding = std::move(axis_binding);
		object->injected_declared = std::move(injected_declared);
		object->listening = std::move(listening);

		//绑定接收入口（按编号回到系统分发，避免捕获会移动的对象地址）
		object->event_terminal->bind(Port_ID::Evt_Receiver,
			[this, ID](shared_ptr<Event> evt) -> void
			{
				this->object_dispatch(ID, evt);
			});
		//绑定发送入口（语义命令进入本系统中转）
		object->event_terminal->bind(Port_ID::Evt_Sender,
			[this](shared_ptr<Event> evt) -> void
			{
				this->command_relay(evt);
			});

		//登记对象名索引
		name_index.emplace(name, ID);
		//若挂载到宿主
		if (!host.empty())
		{
			//登记子对象
			children[host].push_back(name);
			//按注册顺序解析该子对象的注入键
			injected_resolve(host, name);
		}

		//构造对象接入通知（含能力位）
		object_state_notify(ID);

		logger.info("Input_Manager::对象 {} 已创建", name);
	}

	//对象销毁（含级联）
	void Input_Manager::object_destroy(const string& name)
	{
		//若目标对象不存在
		if (!name_index.count(name))
		{
			logger.warn("Input_Manager::待销毁对象 {} 不存在", name);
			return;
		}

		//后代集合（先子后父）
		vector<string> cascade{};
		object_cascade_collect(name, cascade);
		//先卸载全部后代
		for (const auto& child_name : cascade)
			object_unload_single(child_name);
		//再卸载目标对象
		object_unload_single(name);
	}

	//对象重载（映射表 / 轴表 / 能力位）
	void Input_Manager::object_reload(const json& config)
	{
		//若目标对象字段无效
		if (!detail::field_check<string>(config, input_event::field_object))
		{
			logger.warn("Input_Manager::重载缺少有效 object 字段");
			return;
		}
		//获取目标对象名
		const string name = config[input_event::field_object];
		//获取目标对象
		Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
		{
			logger.warn("Input_Manager::待重载对象 {} 不存在", name);
			return;
		}

		//若提供映射表则整体替换
		if (config.contains("mapping"))
		{
			std::unordered_map<Input_Key, std::vector<Input_Binding>> mapping{};
			if (!input_mapping::mapping_parse(config["mapping"], mapping))
			{
				logger.warn("Input_Manager::对象 {} 映射表重载失败\n保留旧映射表", name);
				return;
			}
			object->mapping = std::move(mapping);
		}
		//若提供轴表则整体替换并清空轴状态
		if (config.contains("axes"))
		{
			std::unordered_map<string, Axis_Binding> axes{};
			if (!input_mapping::axis_parse(config["axes"], axes))
			{
				logger.warn("Input_Manager::对象 {} 轴表重载失败\n保留旧轴表", name);
				return;
			}
			object->axis_binding = std::move(axes);
			object->axis_state.clear();
		}
		//若提供能力位则逐项更新
		if (config.contains("focusable") && config["focusable"].is_boolean())
			object->focusable = config["focusable"].get<bool>();
		if (config.contains("enabled") && config["enabled"].is_boolean())
			object->enabled = config["enabled"].get<bool>();
		if (config.contains("text_sink") && config["text_sink"].is_boolean())
			object->text_sink = config["text_sink"].get<bool>();

		//先重解析注入键，再重建监听表
		injected_rebuild(name);
		listening_rebuild(name);
		//同步对象能力给焦点系统
		object_state_notify(object->ID());

		logger.info("Input_Manager::对象 {} 已重载", name);
	}
	//已存在对象的配置更新
	void Input_Manager::object_update(const json& item)
	{
		//获取对象名与目标对象
		const string name = item.value("name", string{});
		Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return;

		//若提供映射表则整体替换
		if (item.contains("mapping"))
		{
			std::unordered_map<Input_Key, std::vector<Input_Binding>> mapping{};
			if (!input_mapping::mapping_parse(item["mapping"], mapping))
				logger.warn("Input_Manager::对象 {} 配置更新映射表解析失败\n已保留旧表", name);
			else
				object->mapping = std::move(mapping);
		}
		//若提供轴绑定表则整体替换并清空轴状态
		if (item.contains("axes"))
		{
			std::unordered_map<string, Axis_Binding> axes{};
			if (!input_mapping::axis_parse(item["axes"], axes))
				logger.warn("Input_Manager::对象 {} 配置更新轴表解析失败\n已保留旧表", name);
			else
			{
				object->axis_binding = std::move(axes);
				object->axis_state.clear();
			}
		}
		//若提供能力位则逐项更新
		if (item.contains("focusable") && item["focusable"].is_boolean())
			object->focusable = item["focusable"].get<bool>();
		if (item.contains("enabled") && item["enabled"].is_boolean())
			object->enabled = item["enabled"].get<bool>();
		if (item.contains("text_sink") && item["text_sink"].is_boolean())
			object->text_sink = item["text_sink"].get<bool>();

		//重解析注入键与监听表
		injected_rebuild(name);
		listening_rebuild(name);
		//同步对象能力给焦点系统
		object_state_notify(object->ID());
	}

	//后代收集（先子后父）
	void Input_Manager::object_cascade_collect(const string& name, vector<string>& list) const
	{
		//查找子对象集合
		auto it = children.find(name);
		//若未挂载子对象
		if (it == children.end())
			return;
		//深度优先收集
		for (const auto& child_name : it->second)
		{
			object_cascade_collect(child_name, list);
			list.push_back(child_name);
		}
	}

	//单对象卸载
	void Input_Manager::object_unload_single(const string& name)
	{
		//查找对象编号
		auto it = name_index.find(name);
		//若目标对象不存在
		if (it == name_index.end())
			return;
		//记录对象编号
		const uint64_t ID = it->second;
		//获取目标对象
		Input_Object* object = object_find(ID);
		//记录宿主名
		const string host = object != nullptr ? object->host_name : string{};

		//构造对象卸载通知
		json destroyed = json::object();
		destroyed[input_event::field_object] = name;
		destroyed["host"] = host;
		//通知焦点系统
		focus_notify(input_event::object_destroyed, destroyed);

		//从宿主子对象集合移除
		if (!host.empty())
		{
			//查找宿主子对象集合
			auto host_it = children.find(host);
			//若宿主仍挂载子对象
			if (host_it != children.end())
			{
				//移除目标对象
				auto& list = host_it->second;
				list.erase(std::remove(list.begin(), list.end(), name), list.end());
				//若宿主已无子对象
				if (list.empty())
					children.erase(host_it);
			}
		}
		//清除目标对象的子对象集合
		children.erase(name);
		//移除对象名索引
		name_index.erase(it);
		//从对象池卸载
		objects.unload(ID);

		//子对象移除可能让此前被压制的注入键重新生效
		if (!host.empty())
			injected_rebuild(host);
	}
}