#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
	/*
	输入模块事件词汇
	所有输入相关事件均为 category = "Input"，标签与载荷字段名集中于此，
	避免映射侧与焦点侧各写一份字符串导致漂移。
	*/
	namespace input_event
	{
		//事件类别
		inline constexpr const char* category = "Input";

		// ———— 第三方 / 渲染层 → 本模块 ————

		//对象创建
		inline constexpr const char* create = "Create";
		//对象销毁
		inline constexpr const char* destroy = "Destroy";
		//对象重载
		inline constexpr const char* reload = "Reload";
		//原始输入帧（渲染层 → 焦点系统）
		inline constexpr const char* raw = "Raw";
		//鼠标焦点（UI 系统 → 焦点系统）
		inline constexpr const char* mouse_focus = "MouseFocus";
		//文本会话（渲染层 → 焦点系统）
		inline constexpr const char* text_session = "TextSession";
		//模态压栈 / 弹栈
		inline constexpr const char* modal_push = "ModalPush";
		inline constexpr const char* modal_pop = "ModalPop";

		// ———— 本模块内部 ————

		//原始输入转发（焦点系统 → 输入管理器）
		inline constexpr const char* forward = "Forward";
		//文本会话转发（焦点系统 → 输入管理器）
		inline constexpr const char* text_route = "TextRoute";
		//映射对象接收的原始帧
		inline constexpr const char* raw_frame = "RawFrame";
		//语义命令（映射对象 → 中转站广播）
		inline constexpr const char* command = "Command";
		//轴状态（映射对象 → 中转站广播）
		inline constexpr const char* axis_state = "AxisState";
		//文本语义（映射对象 → 中转站广播）
		inline constexpr const char* text = "Text";

		// ———— 输入管理器 → 焦点系统 ————

		//对象接入 / 卸载
		inline constexpr const char* object_linked = "ObjectLinked";
		inline constexpr const char* object_destroyed = "ObjectDestroyed";
		//子对象焦点请求
		inline constexpr const char* child_focus = "ChildFocus";

		// ———— 焦点系统 → 中转站广播 ————

		//焦点变更
		inline constexpr const char* focus_changed = "FocusChanged";
		//文本模式变更
		inline constexpr const char* text_mode = "TextMode";

		// ———— 载荷字段名 ————

		//对象名
		inline constexpr const char* field_object = "object";
		//宿主名
		inline constexpr const char* field_host = "host";
		//子对象名
		inline constexpr const char* field_child = "child";
		//可聚焦
		inline constexpr const char* field_focusable = "focusable";
		//可接收输入
		inline constexpr const char* field_enabled = "enabled";
		//文本接收者
		inline constexpr const char* field_text_sink = "text_sink";
		//帧号与时间步长
		inline constexpr const char* field_frame = "frame";
		inline constexpr const char* field_dt = "dt";
		//键位边沿
		inline constexpr const char* field_keys = "keys";
		inline constexpr const char* field_key = "key";
		inline constexpr const char* field_pressed = "pressed";
		//长按自动重复沿标记（边沿字段与语义命令字段共用）
		inline constexpr const char* field_repeat = "repeat";
		//指针与轴
		inline constexpr const char* field_pointer = "pointer";
		inline constexpr const char* field_axes = "axes";
		//按住键位快照（原始帧可选的权威状态）
		inline constexpr const char* field_held = "held";
		//映射语义与目标
		inline constexpr const char* field_command = "command";
		inline constexpr const char* field_target = "target";
		//文本模式标记
		inline constexpr const char* field_text_mode = "text_mode";
	}

}
