#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取对象池
#include "Engine/EngineFoundation/src/object/Object_Pool/对象池.h"
//获取数值池
#include "Engine/EngineFoundation/src/detail/class/Number_Pool/数值池.h"
//获取事件终端
#include "Engine/EngineFoundation/src/event/Event_Terminal/事件终端.h"
//获取事件类型
#include "Engine/EngineFoundation/src/event/Event/事件.h"
//获取输入映射系统运行包
#include "../输入映射系统运行包.h"

//游戏引擎命名空间
namespace engine
{
	/*
	输入管理器
	维护活跃映射对象集合与对象间的宿主/子对象关系，
	接收原始输入并按映射表产出语义命令；语义命令经事件终端广播，
	Enter 的目标若是自家子对象则改为向焦点系统发出子对象焦点请求。

	分工
	    Input_Object   单个映射对象（数据与终端）
	    Input_Mapping  配置解析与轴值变换（无状态）
	    Input_Manager  对象池、事件接入、生命周期、原始输入分发
	*/
	class Input_Manager
	{
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//权限密钥
		int64_t acl_key = 0;
		//接入标记
		bool attached = false;

		//对象编号分配器
		Number_Pool<uint64_t> id_pool;
		//映射对象池（以对象编号为定位键）
		Object_Pool<Input_Object> objects
		{
			[](Input_Object& object) -> uint64_t& { return object.id; }
		};
		//对象名 → 对象编号
		std::unordered_map<std::string, uint64_t> name_index{};
		//宿主名 → 子对象名集合（按注册顺序）
		std::unordered_map<std::string, std::vector<std::string>> children{};

		//全局物理按住键位（跨帧）
		std::unordered_set<Input_Key> held_keys{};
		//上一原始帧号
		uint64_t last_frame = 0;
		//是否已接收过原始帧
		bool frame_seen = false;
		//是否已就「缺少 held 快照」告警过（实例级，避免多实例互相遮蔽）
		bool held_warned = false;
		//最近一帧原始指针状态
		Input_Cursor_Snapshot cursor{};

		//绑定命中结果
		struct Binding_Match
		{
			//命中绑定
			const Input_Binding* binding = nullptr;
			//绑定来源对象名（宿主或注入子对象）
			std::string source{};
			//是否来自宿主自身
			bool from_host = true;
		};

	public:
		//构造函数
		Input_Manager(void);
		//析构函数
		~Input_Manager(void) = default;
		//禁止拷贝
		Input_Manager(const Input_Manager&) = delete;
		Input_Manager& operator=(const Input_Manager&) = delete;

		//事件中转站接入
		bool attach(void);

		//对象存在性
		bool object_exist(const std::string& name) const;
		//主对象判定
		bool object_is_main(const std::string& name) const;
		//子对象归属判定
		bool object_is_child_of(const std::string& child, const std::string& host) const;
		//注入生效数量查询
		std::size_t object_injected_count(const std::string& name) const;
		//监听表数量查询
		std::size_t object_listening_count(const std::string& name) const;
		//对象可聚焦查询
		bool object_focusable_get(const std::string& name) const;
		//对象可接收输入查询
		bool object_enabled_get(const std::string& name) const;
		//对象文本接收者查询
		bool object_text_sink_get(const std::string& name) const;
		//对象轴状态查询
		double object_axis_value_get(const std::string& name, const std::string& axis) const;
		//全局物理按住查询
		bool key_held_get(const Input_Key& key) const;
		//当前修饰键掩码查询
		uint8_t modifier_mask_get(void) const;
		//最近一帧指针状态查询
		Input_Cursor_Snapshot cursor_get(void) const;

	private:
		// ———— 事件处理 ————

		//事件处理
		void event_process(std::shared_ptr<Event> evt);
		//对象创建
		void object_create(const nlohmann::json& config);
		//对象销毁（含级联）
		void object_destroy(const std::string& name);
		//对象重载（映射表 / 轴表 / 能力位）
		void object_reload(const nlohmann::json& config);
		//原始输入转发
		void raw_forward(std::shared_ptr<Event> evt);
		//文本会话转发
		void text_forward(std::shared_ptr<Event> evt);
		//配置加载处理
		void config_apply(const nlohmann::json& config);
		//已存在对象的配置更新
		void object_update(const nlohmann::json& item);
		//对象状态通知（能力变化同步给焦点系统）
		void object_state_notify(const uint64_t& ID);
		//语义命令中转（Enter 目标为自家子对象时改为焦点请求）
		void command_relay(std::shared_ptr<Event> evt);

		// ———— 对象操作 ————

		//按名查找对象 —— 非常量重载
		Input_Object* object_find(const std::string& name);
		//按名查找对象 —— 常量重载
		const Input_Object* object_find(const std::string& name) const;
		//按编号查找对象 —— 非常量重载
		Input_Object* object_find(const uint64_t& ID);
		//按编号查找对象 —— 常量重载
		const Input_Object* object_find(const uint64_t& ID) const;
		//对象接收入口分发
		void object_dispatch(const uint64_t& ID, std::shared_ptr<Event> evt);
		//原始帧映射
		void object_map(const uint64_t& ID, const nlohmann::json& raw);
		//键位处理
		void object_keys_process(const uint64_t& ID, const nlohmann::json& raw);
		//轴处理
		void object_axes_process(const uint64_t& ID, const nlohmann::json& raw,
			const std::string& source_name, const uint64_t& frame, const double& dt);
		//监听拷贝
		void object_copy_to_children(const uint64_t& ID, const std::string& source_name,
			const Input_Key& key, const nlohmann::json& edge, const uint64_t& frame);
		//后代收集（先子后父）
		void object_cascade_collect(const std::string& name, std::vector<std::string>& list) const;
		//单对象卸载
		void object_unload_single(const std::string& name);

		// ———— 按住状态 ————

		//按住键位状态更新
		void held_update(const nlohmann::json& raw);
		//当前按住修饰键掩码
		uint8_t held_modifier_mask(void) const;

		// ———— 映射查询与关系重建 ————

		//绑定查找（组合优先，其次宿主自身，再按子对象注册顺序）
		bool binding_find(const Input_Object& object, const Input_Key& key,
			const uint8_t& held_mask, Binding_Match& match_out) const;
		//注入生效重建
		void injected_rebuild(const std::string& host_name);
		//单个子对象注入生效解析（按注册顺序只解析该子对象）
		void injected_resolve(const std::string& host_name, const std::string& child_name);
		//监听表重建
		void listening_rebuild(const std::string& host_name);

		// ———— 焦点通知 ————

		//焦点系统定向通知
		void focus_notify(const std::string& tag, const nlohmann::json& config);
	};

}
