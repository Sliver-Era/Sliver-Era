#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//按住键位状态更新
	void Input_Manager::held_update(const json& raw)
	{
		//帧号
		const uint64_t frame = raw.value("frame", uint64_t{0});
		//权威按住快照存在性
		const bool has_held = raw.contains(input_event::field_held) &&
			raw[input_event::field_held].is_array();

		//若本帧携带指针载荷
		if (raw.contains(input_event::field_pointer) && raw[input_event::field_pointer].is_object())
		{
			//缓存指针状态（位置缺省保持，增量与滚轮逐帧归零重取）
			const json& pointer = raw[input_event::field_pointer];
			cursor.x = pointer.value("x", cursor.x);
			cursor.y = pointer.value("y", cursor.y);
			cursor.dx = pointer.value("dx", 0.0);
			cursor.dy = pointer.value("dy", 0.0);
			cursor.wheel_x = pointer.value("wheel_x", 0.0);
			cursor.wheel_y = pointer.value("wheel_y", 0.0);
		}
		//若本帧未携带指针载荷
		else
		{
			//位置保持，增量与滚轮归零
			cursor.dx = 0.0;
			cursor.dy = 0.0;
			cursor.wheel_x = 0.0;
			cursor.wheel_y = 0.0;
		}

		//若缺少权威快照且尚未告警
		if (!has_held && !held_warned)
		{
			//是否存在组合绑定
			const bool has_modifier_binding = std::any_of(objects.begin(), objects.end(),
				[](const Input_Object& object) -> bool
				{
					//检查自身映射表
					for (const auto& entry : object.mapping)
						for (const auto& binding : entry.second)
							if (binding.modifier_mask != 0)
								return true;
					//检查注入生效表
					for (const auto& entry : object.injected_active)
						for (const auto& binding : entry.second)
							if (binding.modifier_mask != 0)
								return true;
					return false;
				});
			//若存在组合绑定则提示降级风险
			if (has_modifier_binding)
			{
				logger.warn("Input_Manager::原始帧缺少 held 快照\n"
					"组合键退化为边沿与帧连续性推断，修饰键漏报会让组合静默失效\n"
					"建议每帧投递 held");
				held_warned = true;
			}
		}

		//若存在权威快照
		if (has_held)
		{
			//整体替换为快照内容
			held_keys.clear();
			for (const auto& item : raw[input_event::field_held])
			{
				Input_Key key = 0;
				//若键位可识别
				if (input_mapping::key_parse(item, key))
					held_keys.insert(key);
			}
		}
		//若帧号不连续则清空旧状态（保守重同步）
		else if (frame_seen && frame != 0 && frame > last_frame + 1)
			held_keys.clear();

		//记录帧号
		if (frame != 0)
		{
			last_frame = frame;
			frame_seen = true;
		}

		//若无键位边沿字段
		if (!raw.contains(input_event::field_keys) || !raw[input_event::field_keys].is_array())
			return;

		//按边沿增量应用本帧变化
		for (const auto& edge : raw[input_event::field_keys])
		{
			//若边沿项无效
			if (!edge.is_object() || !edge.contains(input_event::field_key))
				continue;
			Input_Key key = 0;
			//若键位无法识别
			if (!input_mapping::key_parse(edge[input_event::field_key], key))
				continue;
			//按下沿登记、抬起沿移除
			if (edge.value(input_event::field_pressed, true))
				held_keys.insert(key);
			else
				held_keys.erase(key);
		}
	}

	//当前按住修饰键掩码
	uint8_t Input_Manager::held_modifier_mask(void) const
	{
		//掩码缓冲
		uint8_t mask = 0;
		//逐键合并修饰位
		for (const auto& key : held_keys)
			mask |= input_mapping::modifier_mask_of(key);
		return mask;
	}

	//原始输入转发
	void Input_Manager::raw_forward(shared_ptr<Event> evt)
	{
		//若目标对象字段无效
		if (!detail::field_check<string>(evt->config, input_event::field_object))
		{
			logger.warn("Input_Manager::原始输入未指定目标对象\n已丢弃");
			return;
		}
		//获取目标对象名
		const string target = evt->config[input_event::field_object];
		//获取目标对象
		Input_Object* object = object_find(target);
		//若目标对象不存在
		if (object == nullptr)
		{
			logger.warn("Input_Manager::原始输入目标对象不存在：{}", target);
			return;
		}

		//复制原始帧载荷并剥离目标字段
		json raw = evt->config;
		raw.erase(input_event::field_object);
		//构造原始输入事件
		shared_ptr<Event> raw_evt = object->event_terminal.build("Focus_System", target,
			input_event::category, input_event::raw_frame);
		//若事件构造失败
		if (!raw_evt)
		{
			logger.error("Input_Manager::原始输入事件构造失败");
			return;
		}
		//写入原始帧载荷
		raw_evt->config = std::move(raw);
		//投递给对象终端（对象接收入口再回到本系统分发）
		object->event_terminal.receive(raw_evt);
	}

	//文本会话转发
	void Input_Manager::text_forward(shared_ptr<Event> evt)
	{
		//若目标对象字段无效
		if (!detail::field_check<string>(evt->config, input_event::field_object))
		{
			logger.warn("Input_Manager::文本会话未指定目标对象");
			return;
		}
		//获取目标对象名
		const string target = evt->config[input_event::field_object];
		//获取目标对象
		Input_Object* object = object_find(target);
		//若目标对象不存在
		if (object == nullptr)
		{
			logger.warn("Input_Manager::文本会话目标对象不存在：{}", target);
			return;
		}

		//复制会话载荷并剥离目标字段
		json payload = evt->config;
		payload.erase(input_event::field_object);
		//构造文本会话事件
		auto session_evt = object->event_terminal.build("Focus_System", target,
			input_event::category, input_event::text_session);
		//若事件构造失败
		if (!session_evt)
		{
			logger.error("Input_Manager::文本会话事件构造失败");
			return;
		}
		//写入会话载荷
		session_evt->config = std::move(payload);
		//投递给对象终端
		object->event_terminal.receive(session_evt);
	}

	//原始帧映射
	void Input_Manager::object_map(const uint64_t& ID, const json& raw)
	{
		//获取目标对象
		Input_Object* object = object_find(ID);
		//若目标对象不存在
		if (object == nullptr)
			return;

		//帧号与时间步长
		const uint64_t frame = raw.value("frame", uint64_t{0});
		const double dt = raw.value("dt", 1.0 / 60.0);
		//发件对象名（后续可能发生对象重建，故先取出字符串）
		const string source_name = object->object_name;
		//文本模式标记
		const bool text_mode = raw.value("text_mode", false);

		//更新按住键位状态（文本模式下仍需跟踪物理按键）
		held_update(raw);

		//文本模式下挂起键位映射
		if (!text_mode)
			object_keys_process(ID, raw);

		//轴处理不受文本模式影响
		object_axes_process(ID, raw, source_name, frame, dt > 0.0 ? dt : 1.0 / 60.0);
	}

	//键位处理
	void Input_Manager::object_keys_process(const uint64_t& ID, const json& raw)
	{
		//获取目标对象
		Input_Object* object = object_find(ID);
		//若目标对象不存在
		if (object == nullptr)
			return;
		//若键位边沿字段无效
		if (!raw.contains("keys") || !raw["keys"].is_array())
			return;

		//帧号
		const uint64_t frame = raw.value("frame", uint64_t{0});
		//发件对象名（后续可能发生对象重建，故先取出字符串）
		const string source_name = object->object_name;

		//按下沿记录
		struct Press_Record
		{
			Input_Key key;
		};
		//监听拷贝记录
		struct Copy_Record
		{
			string child;
			Input_Key key;
			json edge;
		};
		//命令记录
		struct Command_Record
		{
			Input_Binding binding;
			Input_Key key;
			//是否为长按自动重复产出
			bool repeat = false;
		};

		//按下沿、重复沿、拷贝任务与命令缓冲
		vector<Press_Record> presses{};
		vector<Press_Record> repeats{};
		vector<Copy_Record> copies{};
		vector<Command_Record> commands{};
		//本帧已消费键位（组合命中的成员）
		std::unordered_set<Input_Key> consumed{};

		//查找宿主子对象集合
		auto child_it = children.find(source_name);

		//逐条解析键位边沿
		for (const auto& edge : raw["keys"])
		{
			//若边沿项非对象
			if (!edge.is_object() || !edge.contains("key"))
			{
				logger.warn("Input_Manager::原始输入边沿格式无效，已丢弃");
				continue;
			}
			//解析键位
			Input_Key key = 0;
			if (!input_mapping::key_parse(edge["key"], key))
			{
				logger.warn("Input_Manager::原始输入键位无法识别，已丢弃");
				continue;
			}

			//收集监听拷贝任务
			if (child_it != children.end())
			{
				for (const auto& child_name : child_it->second)
				{
					//获取子对象
					const Input_Object* child = object_find(child_name);
					//若子对象监听该键位
					if (child != nullptr && child->listening.count(key))
						copies.push_back({ child_name, key, edge });
				}
			}

			//抬起沿不产生命令
			if (!edge.value("pressed", true))
				continue;
			//长按重复沿单独收集（不参与组合的两轮求值与抑制）
			if (edge.value(input_event::field_repeat, false))
				repeats.push_back({ key });
			else
				presses.push_back({ key });
		}

		//先派发监听拷贝（拷贝可能引发对象重建，故每次按名重查）
		for (const auto& copy_record : copies)
			object_copy_to_children(ID, source_name, copy_record.key, copy_record.edge, frame);

		//拷贝期间对象可能已被销毁，故按编号重查
		object = object_find(ID);
		//若发件对象已不存在
		if (object == nullptr)
			return;

		//当前按住修饰键掩码（帧末状态）
		const uint8_t held_mask = held_modifier_mask();

		//第一轮：组合绑定优先
		for (const auto& press : presses)
		{
			//已被组合消费的键位不再参与
			if (consumed.count(press.key))
				continue;
			//查找最佳绑定
			Binding_Match match{};
			if (!binding_find(*object, press.key, held_mask, match))
				continue;
			//单键绑定留待第二轮
			if (match.binding->modifier_mask == 0)
				continue;
			//登记组合命令
			commands.push_back({ *match.binding, press.key });
			consumed.insert(press.key);
			//抑制该组合所需的物理修饰键
			for (const auto& held_key : held_keys)
			{
				//若该物理键属于组合所需修饰
				if ((input_mapping::modifier_mask_of(held_key) & match.binding->modifier_mask) != 0)
					consumed.insert(held_key);
			}
		}

		//第二轮：单键绑定兜底
		for (const auto& press : presses)
		{
			//已被消费的键位不再产出
			if (consumed.count(press.key))
				continue;
			//查找最佳绑定
			Binding_Match match{};
			if (!binding_find(*object, press.key, held_mask, match))
				continue;
			//组合绑定已在第一轮处理
			if (match.binding->modifier_mask != 0)
				continue;
			//登记单键命令
			commands.push_back({ *match.binding, press.key });
			consumed.insert(press.key);
		}

		//第三轮：长按重复沿（仅响应显式开启 repeat 的绑定）
		for (const auto& repeat : repeats)
		{
			//查找最佳绑定
			Binding_Match match{};
			if (!binding_find(*object, repeat.key, held_mask, match))
				continue;
			//未开启重复响应的绑定不产出
			if (!match.binding->allow_repeat)
				continue;
			//登记重复命令
			commands.push_back({ *match.binding, repeat.key, true });
		}

		//再派发命令（派发期间对象可能被销毁，故按编号重查）
		Input_Object* source = object_find(ID);
		//若发件对象已不存在
		if (source == nullptr)
			return;
		//逐条发送语义命令
		for (const auto& command_record : commands)
		{
			//构造命令事件
			auto command_evt = source->event_terminal.build(source_name, "", input_event::category, input_event::command);
			//若事件构造失败
			if (!command_evt)
			{
				logger.error("Input_Manager::语义命令事件构造失败");
				continue;
			}
			//写入语义与来源信息
			command_evt->config["command"] = command_record.binding.command;
			if (!command_record.binding.target.empty())
				command_evt->config["target"] = command_record.binding.target;
			command_evt->config["key"] = command_record.key;
			command_evt->config["frame"] = frame;
			//标记长按重复产出
			if (command_record.repeat)
				command_evt->config[input_event::field_repeat] = true;
			//发送命令
			source->event_terminal.send(command_evt, source->acl_key);

			//订阅方响应可能销毁发件对象，重新按编号查找
			source = object_find(ID);
			//若发件对象已不存在则终止
			if (source == nullptr)
				return;
		}
	}

	//监听拷贝
	void Input_Manager::object_copy_to_children(const uint64_t& ID, const string& source_name,
		const Input_Key& key, const json& edge, const uint64_t& frame)
	{
		//获取发件对象
		Input_Object* source = object_find(ID);
		//若发件对象不存在
		if (source == nullptr)
			return;
		//查找子对象集合
		auto it = children.find(source_name);
		//若未挂载子对象
		if (it == children.end())
			return;

		//复制子对象名集合，避免回调期间容器变化
		vector<string> child_names = it->second;
		//逐个派发拷贝
		for (const auto& child_name : child_names)
		{
			//获取子对象
			Input_Object* child = object_find(child_name);
			//若子对象不存在
			if (child == nullptr)
				continue;
			//非监听键位不拷贝
			if (!child->listening.count(key))
				continue;

			//构造缩小的原始帧事件
			auto copy_evt = child->event_terminal.build(source_name, child_name, input_event::category, input_event::raw_frame);
			//若事件构造失败
			if (!copy_evt)
			{
				logger.error("Input_Manager::监听拷贝事件构造失败");
				continue;
			}
			//写入帧号与单条边沿
			copy_evt->config["frame"] = frame;
			json keys = json::array();
			keys.push_back(edge);
			copy_evt->config["keys"] = std::move(keys);
			//投递给子对象
			child->event_terminal.receive(copy_evt);
		}
	}

	//轴处理
	void Input_Manager::object_axes_process(const uint64_t& ID, const json& raw,
		const string& source_name, const uint64_t& frame, const double& dt)
	{
		//获取目标对象
		Input_Object* object = object_find(ID);
		//若目标对象不存在
		if (object == nullptr)
			return;
		//若无轴绑定
		if (object->axis_binding.empty())
			return;

		//轴值与增量缓冲
		json values = json::object();
		json deltas = json::object();
		//指针载荷存在性
		const bool has_pointer = raw.contains(input_event::field_pointer) &&
			raw[input_event::field_pointer].is_object();
		//轴载荷存在性
		const bool has_axes = raw.contains(input_event::field_axes) &&
			raw[input_event::field_axes].is_object();

		//逐轴计算
		for (const auto& entry : object->axis_binding)
		{
			//轴名与绑定
			const string& axis_name = entry.first;
			const Axis_Binding& binding = entry.second;
			//原始值
			double raw_value = 0.0;
			//指针来源
			if (binding.source == "pointer.dx" && has_pointer)
				raw_value = raw[input_event::field_pointer].value("dx", 0.0);
			else if (binding.source == "pointer.dy" && has_pointer)
				raw_value = raw[input_event::field_pointer].value("dy", 0.0);
			else if (binding.source == "pointer.x" && has_pointer)
				raw_value = raw[input_event::field_pointer].value("x", 0.0);
			else if (binding.source == "pointer.y" && has_pointer)
				raw_value = raw[input_event::field_pointer].value("y", 0.0);
			//原始轴来源
			else if (has_axes && raw[input_event::field_axes].contains(binding.source) &&
				raw[input_event::field_axes][binding.source].is_number())
				raw_value = raw[input_event::field_axes][binding.source].get<double>();

			//来源是否为保留指针名
			const bool pointer_source = binding.source == "pointer.x" || binding.source == "pointer.y" ||
				binding.source == "pointer.dx" || binding.source == "pointer.dy";
			//若为非指针来源且本帧未提供该轴值
			if (!pointer_source &&
				(!has_axes || !raw[input_event::field_axes].contains(binding.source)) &&
				!object->axis_unfed_warned.count(axis_name))
			{
				logger.warn("Input_Manager::轴 {} 来源 {} 本帧未提供\n"
					"该轴恒为 0.0，需上层投递 Raw 的 axes 字段", axis_name, binding.source);
				object->axis_unfed_warned.insert(axis_name);
			}

			//统一做死区、缩放、反相、钳制与增量换算
			const input_mapping::Axis_Result result =
				input_mapping::axis_transform(binding, raw_value, dt);
			//写入跨帧轴状态
			object->axis_state[axis_name] = result.value;
			//记录本帧输出
			values[axis_name] = result.value;
			deltas[axis_name] = result.delta;
		}

		//若没有任何轴值则返回
		if (values.empty())
			return;

		//重查发件对象，避免计算期间对象被重建
		Input_Object* source = object_find(ID);
		//若发件对象已不存在
		if (source == nullptr)
			return;

		//构造轴状态事件
		auto axis_evt = source->event_terminal.build(source_name, "",
			input_event::category, input_event::axis_state);
		//若事件构造失败
		if (!axis_evt)
		{
			logger.error("Input_Manager::轴状态事件构造失败");
			return;
		}
		//写入轴值与增量
		axis_evt->config[input_event::field_frame] = frame;
		axis_evt->config["values"] = values;
		axis_evt->config["deltas"] = deltas;
		//发送轴状态
		source->event_terminal.send(axis_evt, source->acl_key);
	}

}
