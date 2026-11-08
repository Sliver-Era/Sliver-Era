#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取对象基类
#include "Engine/EngineFoundation/src/object/Object/对象.h"
//获取事件终端
#include "Engine/EngineFoundation/src/event/Event_Terminal/事件终端.h"
//获取输入类型
#include "../../common/输入类型.h"

//游戏引擎命名空间
namespace engine
{
	/*
	输入映射对象
	每个对象持有一个事件终端：接收入口接收原始输入，发送入口输出映射语义。
	自身映射表由创建/重载事件写入，注入映射（added_keys）单独存储，
	监听表只允许引用宿主自身映射表的键位。
	*/
	class Input_Object : public Object
	{
		//输入管理器友元
		friend class Input_Manager;
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//权限密钥
		int64_t acl_key = 0;

		//对象名
		std::string object_name{};
		//宿主名（空 = 主对象）
		std::string host_name{};
		//可聚焦标记
		bool focusable = true;
		//可接收输入标记
		bool enabled = true;
		//文本接收者标记
		bool text_sink = false;

		//自身映射表（同一主键可挂多条绑定，按修饰掩码区分）
		std::unordered_map<Input_Key, std::vector<Input_Binding>> mapping{};
		//轴绑定表
		std::unordered_map<std::string, Axis_Binding> axis_binding{};
		//轴状态（跨帧保留）
		std::unordered_map<std::string, double> axis_state{};
		//注入声明（本对象向宿主声明的新增映射）
		std::unordered_map<Input_Key, std::vector<Input_Binding>> injected_declared{};
		//注入生效（按宿主优先、先注册优先解析后的结果）
		std::unordered_map<Input_Key, std::vector<Input_Binding>> injected_active{};
		//监听表（只监听宿主自身映射表的键位）
		std::unordered_set<Input_Key> listening{};
		//已告警过的「无生产者」轴名（每对象每轴仅告警一次）
		std::unordered_set<std::string> axis_unfed_warned{};

	public:
		//默认构造
		Input_Object(void) = default;
		//析构函数
		~Input_Object(void) = default;
		//禁止拷贝
		Input_Object(const Input_Object&) = delete;
		Input_Object& operator=(const Input_Object&) = delete;
		//允许移动（对象池原地重建需要）
		Input_Object(Input_Object&&) = default;
		Input_Object& operator=(Input_Object&&) = default;

		//对象名获取
		const std::string& name_get(void) const
		{
			return object_name;
		}
		//宿主名获取
		const std::string& host_get(void) const
		{
			return host_name;
		}
		//自身映射键存在性
		bool mapping_has(const Input_Key& key) const
		{
			return mapping.count(key) > 0;
		}
		//注入生效数量
		std::size_t injected_active_size(void) const
		{
			//统计各主键下的生效绑定数
			std::size_t count = 0;
			for (const auto& entry : injected_active)
				count += entry.second.size();
			return count;
		}
		//监听表数量
		std::size_t listening_size(void) const
		{
			return listening.size();
		}
		//可聚焦标记获取
		bool focusable_get(void) const
		{
			return focusable;
		}
		//可接收输入标记获取
		bool enabled_get(void) const
		{
			return enabled;
		}
		//文本接收者标记获取
		bool text_sink_get(void) const
		{
			return text_sink;
		}
		//轴状态获取
		double axis_get(const std::string& name) const
		{
			//查找轴状态
			auto it = axis_state.find(name);
			//若轴未记录
			if (it == axis_state.end())
				return 0.0;
			return it->second;
		}
	};

}
