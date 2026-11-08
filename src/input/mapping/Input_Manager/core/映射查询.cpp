#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//匿名命名空间（仅本编译单元可见）
namespace
{
	//「主键 + 修饰掩码」组合标识
	uint64_t binding_token(const engine::Input_Key& key, const uint8_t& modifier_mask)
	{
		return (static_cast<uint64_t>(key) << 8) | modifier_mask;
	}
}

//引擎命名空间
namespace engine
{
	//对象存在性
	bool Input_Manager::object_exist(const string& name) const
	{
		return name_index.count(name) > 0;
	}

	//主对象判定
	bool Input_Manager::object_is_main(const string& name) const
	{
		//查找目标对象
		auto it = name_index.find(name);
		//若目标对象不存在
		if (it == name_index.end())
			return false;
		//获取目标对象
		const Input_Object* object = object_find(it->second);
		//宿主名为空即主对象
		if (object == nullptr)
			return false;
		return object->host_name.empty();
	}

	//子对象归属判定
	bool Input_Manager::object_is_child_of(const string& child, const string& host) const
	{
		//查找宿主子对象集合
		auto it = children.find(host);
		//若宿主未挂载任何子对象
		if (it == children.end())
			return false;
		//匹配目标子对象
		return std::find(it->second.begin(), it->second.end(), child) != it->second.end();
	}

	//注入生效数量查询
	std::size_t Input_Manager::object_injected_count(const string& name) const
	{
		//获取目标对象
		const Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return 0;
		return object->injected_active_size();
	}

	//监听表数量查询
	std::size_t Input_Manager::object_listening_count(const string& name) const
	{
		//获取目标对象
		const Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return 0;
		return object->listening_size();
	}

	//对象可聚焦查询
	bool Input_Manager::object_focusable_get(const string& name) const
	{
		//获取目标对象
		const Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return false;
		return object->focusable_get();
	}

	//对象可接收输入查询
	bool Input_Manager::object_enabled_get(const string& name) const
	{
		//获取目标对象
		const Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return false;
		return object->enabled_get();
	}

	//对象文本接收者查询
	bool Input_Manager::object_text_sink_get(const string& name) const
	{
		//获取目标对象
		const Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return false;
		return object->text_sink_get();
	}

	//对象轴状态查询
	double Input_Manager::object_axis_value_get(const string& name, const string& axis) const
	{
		//获取目标对象
		const Input_Object* object = object_find(name);
		//若目标对象不存在
		if (object == nullptr)
			return 0.0;
		return object->axis_get(axis);
	}

	//全局物理按住查询
	bool Input_Manager::key_held_get(const Input_Key& key) const
	{
		return held_keys.count(key) > 0;
	}

	//当前修饰键掩码查询
	uint8_t Input_Manager::modifier_mask_get(void) const
	{
		return held_modifier_mask();
	}

	//最近一帧指针状态查询
	Input_Cursor_Snapshot Input_Manager::cursor_get(void) const
	{
		return cursor;
	}

	//按名查找对象 —— 非常量重载
	Input_Object* Input_Manager::object_find(const string& name)
	{
		//查找对象编号
		auto it = name_index.find(name);
		//若目标对象不存在
		if (it == name_index.end())
			return nullptr;
		return object_find(it->second);
	}

	//按名查找对象 —— 常量重载
	const Input_Object* Input_Manager::object_find(const string& name) const
	{
		//查找对象编号
		auto it = name_index.find(name);
		//若目标对象不存在
		if (it == name_index.end())
			return nullptr;
		return object_find(it->second);
	}

	//按编号查找对象 —— 非常量重载
	Input_Object* Input_Manager::object_find(const uint64_t& ID)
	{
		//查找对象迭代器
		auto it = objects.find(ID);
		//若目标对象不存在
		if (it == objects.end())
			return nullptr;
		return &(*it);
	}

	//按编号查找对象 —— 常量重载
	const Input_Object* Input_Manager::object_find(const uint64_t& ID) const
	{
		//查找对象迭代器
		auto it = objects.find(ID);
		//若目标对象不存在
		if (it == objects.end())
			return nullptr;
		return &(*it);
	}

	//绑定查找（组合优先，其次宿主自身，再按子对象注册顺序）
	bool Input_Manager::binding_find(const Input_Object& object, const Input_Key& key,
		const uint8_t& held_mask, Binding_Match& match_out) const
	{
		//最佳命中绑定
		const Input_Binding* best = nullptr;
		//最佳命中来源
		string best_source{};
		//最佳命中是否来自宿主
		bool best_from_host = false;
		//最佳命中修饰键数量
		int best_count = -1;

		//候选登记（修饰键多者优先，同数量保持先到者）
		auto consider = [&](const Input_Binding& binding, const string& source,
			const bool& from_host) -> void
			{
				//若所需修饰键未全部按住
				if ((binding.modifier_mask & held_mask) != binding.modifier_mask)
					return;
				//组合修饰键数量
				const int count = std::popcount(binding.modifier_mask);
				//不优于当前最佳则跳过
				if (count <= best_count)
					return;
				best = &binding;
				best_source = source;
				best_from_host = from_host;
				best_count = count;
			};

		//优先登记宿主自身绑定
		auto own = object.mapping.find(key);
		//若宿主自身存在该主键
		if (own != object.mapping.end())
			for (const auto& binding : own->second)
				consider(binding, object.object_name, true);

		//其次按注册顺序登记子对象注入绑定
		auto it = children.find(object.object_name);
		//若宿主挂载了子对象
		if (it != children.end())
		{
			//遍历子对象
			for (const auto& child_name : it->second)
			{
				//获取子对象
				const Input_Object* child = object_find(child_name);
				//若子对象不存在
				if (child == nullptr)
					continue;
				//查找注入生效表
				auto injected = child->injected_active.find(key);
				//若子对象未注入该主键
				if (injected == child->injected_active.end())
					continue;
				//逐条登记注入绑定
				for (const auto& binding : injected->second)
					consider(binding, child_name, false);
			}
		}

		//若未命中任何候选
		if (best == nullptr)
			return false;
		match_out.binding = best;
		match_out.source = std::move(best_source);
		match_out.from_host = best_from_host;
		return true;
	}

	//注入生效重建
	void Input_Manager::injected_rebuild(const string& host_name)
	{
		//获取宿主对象
		const Input_Object* host = object_find(host_name);
		//若宿主不存在
		if (host == nullptr)
			return;

		//已占用的「主键 + 修饰掩码」组合
		std::unordered_set<uint64_t> taken{};
		//登记宿主自身绑定
		for (const auto& entry : host->mapping)
			for (const auto& binding : entry.second)
				taken.insert(binding_token(entry.first, binding.modifier_mask));

		//查找宿主子对象集合
		auto it = children.find(host_name);
		//若宿主未挂载任何子对象
		if (it == children.end())
			return;

		//按注册顺序解析子对象注入键
		for (const auto& child_name : it->second)
		{
			//获取子对象
			Input_Object* child = object_find(child_name);
			//若子对象不存在
			if (child == nullptr)
				continue;

			//清空上一次解析结果
			child->injected_active.clear();
			//逐主键解析注入声明
			for (const auto& entry : child->injected_declared)
			{
				//逐条组合解析
				for (const auto& binding : entry.second)
				{
					//组合标识
					const uint64_t token = binding_token(entry.first, binding.modifier_mask);
					//若组合已被占用
					if (taken.count(token))
					{
						logger.warn("Input_Manager::注入键位 {} 与已有映射冲突，已丢弃（子对象 {}）",
							entry.first, child_name);
						continue;
					}
					//登记生效注入
					child->injected_active[entry.first].push_back(binding);
					//标记组合已占用
					taken.insert(token);
				}
			}
		}
	}

	//单个子对象注入生效解析（按注册顺序只解析该子对象）
	void Input_Manager::injected_resolve(const string& host_name, const string& child_name)
	{
		//获取宿主与目标子对象
		const Input_Object* host = object_find(host_name);
		Input_Object* child = object_find(child_name);
		//若宿主或子对象不存在
		if (host == nullptr || child == nullptr)
			return;

		//已占用的「主键 + 修饰掩码」组合
		std::unordered_set<uint64_t> taken{};
		//登记宿主自身绑定
		for (const auto& entry : host->mapping)
			for (const auto& binding : entry.second)
				taken.insert(binding_token(entry.first, binding.modifier_mask));

		//查找宿主子对象集合
		auto it = children.find(host_name);
		//若宿主未挂载任何子对象
		if (it == children.end())
			return;

		//登记更早注册子对象的生效注入（先注册者优先）
		for (const auto& name : it->second)
		{
			//只统计目标子对象之前注册的兄弟
			if (name == child_name)
				break;
			//获取兄弟对象
			const Input_Object* sibling = object_find(name);
			//若兄弟不存在
			if (sibling == nullptr)
				continue;
			//登记兄弟生效注入组合
			for (const auto& entry : sibling->injected_active)
				for (const auto& binding : entry.second)
					taken.insert(binding_token(entry.first, binding.modifier_mask));
		}

		//清空上一次解析结果
		child->injected_active.clear();
		//逐主键解析注入声明
		for (const auto& entry : child->injected_declared)
		{
			//逐条组合解析
			for (const auto& binding : entry.second)
			{
				//组合标识
				const uint64_t token = binding_token(entry.first, binding.modifier_mask);
				//若组合已被占用
				if (taken.count(token))
				{
					logger.warn("Input_Manager::注入键位 {} 与已有映射冲突，已丢弃（子对象 {}）",
						entry.first, child_name);
					continue;
				}
				//登记生效注入
				child->injected_active[entry.first].push_back(binding);
				//标记组合已占用
				taken.insert(token);
			}
		}
	}

	//监听表重建
	void Input_Manager::listening_rebuild(const string& host_name)
	{
		//获取宿主对象
		const Input_Object* host = object_find(host_name);
		//若宿主不存在
		if (host == nullptr)
			return;

		//宿主自身允许监听的键位集合
		std::unordered_set<Input_Key> allowed{};
		//登记宿主自身映射键位（监听按主键判定，与修饰掩码无关）
		for (const auto& entry : host->mapping)
			allowed.insert(entry.first);

		//查找宿主子对象集合
		auto it = children.find(host_name);
		//若宿主未挂载任何子对象
		if (it == children.end())
			return;

		//逐个重建子对象监听表
		for (const auto& child_name : it->second)
		{
			//获取子对象
			Input_Object* child = object_find(child_name);
			//若子对象不存在
			if (child == nullptr)
				continue;

			//逐项校验监听键位
			for (auto listen_it = child->listening.begin(); listen_it != child->listening.end();)
			{
				//若键位已不在宿主映射表
				if (!allowed.count(*listen_it))
				{
					logger.warn("Input_Manager::监听键位 {} 已不在宿主映射表，已移除（子对象 {}）",
						*listen_it, child_name);
					listen_it = child->listening.erase(listen_it);
				}
				else
					++listen_it;
			}
		}
	}
}
