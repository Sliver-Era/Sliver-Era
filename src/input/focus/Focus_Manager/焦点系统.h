#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取事件终端
#include "Engine/EngineFoundation/src/event/Event_Terminal/事件终端.h"
//获取事件类型
#include "Engine/EngineFoundation/src/event/Event/事件.h"
//获取 JSON 解析外部库
#include <nlohmann/json.hpp>

//游戏引擎命名空间
namespace engine
{
	/*
	焦点管理系统

	只维护主对象焦点栈（栈顶 = 当前主对象），并保存子对象与宿主的归属信息：
	    原始输入 → 当前活跃对象（有子对象接管时直接给子对象）
	    映射语义 → 收到 Enter 按目标转移（自家子对象内部转移，否则主对象转移）
	              → 收到 Escape 弹主对象栈
	             → 鼠标命中由 UI 上报，命中子对象时绕过宿主直投子对象

	所有转移先写入单值待生效槽，由 frame_begin() 在下一帧开头统一应用；
	同帧多次请求后写覆盖前写；帧内输入处理完即销毁，不跨帧存储。

	事件词汇（category = "Input"）
	    Raw            渲染层 → 本系统：frame / keys / pointer / axes
	    Command        映射对象 → 中转站广播：command / target / key / frame
	    MouseFocus     UI 系统 → 本系统：object
	    ChildFocus     输入系统 → 本系统：host / child
	    ObjectLinked   输入系统 → 本系统：object / host
	    ObjectDestroyed 输入系统 → 本系统：object / host
	    Forward        本系统 → 输入系统：object + 原始帧载荷
	    FocusChanged   本系统 → 中转站广播：main / child
	*/
	class Focus_System
	{
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//模态栈条目（记录压栈者，供弹栈归属校验）
		struct Modal_Entry
		{
			//模态根对象名
			std::string object{};
			//压栈者（事件 sender）
			std::string owner{};
		};

		//权限密钥
		int64_t acl_key = 0;
		//接入标记
		bool attached = false;

		//主对象焦点栈（栈顶 = 当前主对象）
		std::vector<std::string> main_stack{};
		//当前主对象名
		std::string current_main{};
		//当前子对象名（空 = 焦点在主对象自身）
		std::string current_child{};

		//待生效主对象
		std::string pending_main{};
		//待生效子对象
		std::string pending_child{};
		//待生效子焦点清空
		bool pending_child_clear = false;
		//待生效弹栈
		bool pending_pop = false;
		//待生效标记
		bool pending_valid = false;

		//模态栈（栈顶生效；空 = 无模态；记录压栈者供弹栈归属校验）
		std::vector<Modal_Entry> modal_stack{};
		//当前是否文本输入模式
		bool text_mode = false;

		//对象名 → 宿主名（空 = 主对象）
		std::unordered_map<std::string, std::string> host_of{};
		//宿主名 → 子对象名集合
		std::unordered_map<std::string, std::unordered_set<std::string>> children_of{};
		//对象名 → 可聚焦
		std::unordered_map<std::string, bool> focusable_of{};
		//对象名 → 可接收输入
		std::unordered_map<std::string, bool> enabled_of{};
		//对象名 → 文本接收者
		std::unordered_map<std::string, bool> text_sink_of{};

	public:
		//构造函数
		Focus_System(void);
		//析构函数
		~Focus_System(void) = default;
		//禁止拷贝
		Focus_System(const Focus_System&) = delete;
		Focus_System& operator=(const Focus_System&) = delete;

		//事件中转站接入
		bool attach(void);
		//帧首应用待生效转移
		void frame_begin(void);

		//当前主对象获取
		const std::string& current_main_get(void) const;
		//当前子对象获取
		const std::string& current_child_get(void) const;
		//主对象栈深度获取
		std::size_t stack_size(void) const;
		//文本模式状态获取
		bool text_mode_get(void) const;
		//模态栈深度获取
		std::size_t modal_size(void) const;

	private:
		// ———— 事件处理 ————

		//事件处理
		void event_process(std::shared_ptr<Event> evt);
		//原始输入处理
		void raw_process(std::shared_ptr<Event> evt);
		//语义命令处理
		void command_process(std::shared_ptr<Event> evt);
		//鼠标焦点处理
		void mouse_focus_process(std::shared_ptr<Event> evt);
		//子对象焦点请求处理
		void child_focus_process(std::shared_ptr<Event> evt);
		//对象接入处理
		void object_linked_process(std::shared_ptr<Event> evt);
		//对象卸载处理
		void object_destroyed_process(std::shared_ptr<Event> evt);
		//模态压栈处理
		void modal_push_process(std::shared_ptr<Event> evt);
		//模态弹栈处理
		void modal_pop_process(std::shared_ptr<Event> evt);
		//文本会话处理
		void text_session_process(std::shared_ptr<Event> evt);

		// ———— 焦点转移 ————

		//按目标对象请求转移（主对象压栈；子对象内部转移）
		void target_request(const std::string& target);
		//主对象压栈请求
		void main_push_request(const std::string& name);
		//主对象弹栈请求
		void main_pop_request(void);
		//子对象接管请求
		void child_request(const std::string& name);
		//帧首统一应用
		void transition_apply(void);

		// ———— 事件输出 ————

		//焦点变更广播
		void focus_changed_publish(void);
		//文本模式变更广播
		void text_mode_publish(void);
		//文本模式重算
		void text_mode_refresh(void);
		//事件发送
		void focus_send(const std::string& tag, const nlohmann::json& config,
			const std::string& receiver);
		//目标是否在模态闸门内（无模态时恒为真）
		bool object_within_modal(const std::string& name) const;
	};

}
