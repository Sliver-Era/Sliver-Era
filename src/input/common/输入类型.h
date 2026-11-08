#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
	/*
	输入键位（引擎中立编码，位域布局）
	    bit 31..28  键类（见 Input_Key_Class）
	    bit 27..16  保留，恒 0
	    bit 15..0   类内编号（1 起，0 表示无效）

	渲染层负责把平台扫描码翻译到此；映射配置可写位域数字，也可写名称
	（名称与别名表见 Input_Mapping/core/键位解析.cpp，本文件只提供唯一编号真源）。
	*/
	using Input_Key = uint32_t;

	//键类位移
	inline constexpr uint32_t INPUT_KEY_CLASS_SHIFT = 28;
	//键类掩码
	inline constexpr uint32_t INPUT_KEY_CLASS_MASK = 0xF0000000u;
	//类内编号掩码
	inline constexpr uint32_t INPUT_KEY_CODE_MASK = 0x0000FFFFu;
	//合法位域总掩码
	inline constexpr uint32_t INPUT_KEY_VALID_MASK = INPUT_KEY_CLASS_MASK | INPUT_KEY_CODE_MASK;

	//键类
	enum class Input_Key_Class : uint32_t
	{
		//主键盘常规键（字母 / 数字 / 符号）
		main = 0x1,
		//控制 / 功能 / 导航 / 修饰键
		control = 0x2,
		//小键盘
		keypad = 0x3,
		//鼠标按键
		mouse = 0x4,
		//滚轮
		wheel = 0x5
	};

	//键位编码拼装
	constexpr Input_Key input_key_make(const Input_Key_Class& key_class, const uint32_t& code)
	{
		return (static_cast<uint32_t>(key_class) << INPUT_KEY_CLASS_SHIFT) | (code & INPUT_KEY_CODE_MASK);
	}

	//键类提取
	constexpr Input_Key_Class input_key_class_get(const Input_Key& key)
	{
		return static_cast<Input_Key_Class>((key & INPUT_KEY_CLASS_MASK) >> INPUT_KEY_CLASS_SHIFT);
	}

	//类内编号提取
	constexpr uint32_t input_key_code_get(const Input_Key& key)
	{
		return key & INPUT_KEY_CODE_MASK;
	}

	//键位合法性判定
	constexpr bool input_key_valid(const Input_Key& key)
	{
		//保留位必须为零
		if ((key & ~INPUT_KEY_VALID_MASK) != 0)
			return false;
		//键类必须在登记范围内
		const uint32_t key_class = (key & INPUT_KEY_CLASS_MASK) >> INPUT_KEY_CLASS_SHIFT;
		if (key_class < 1 || key_class > 5)
			return false;
		//类内编号不得为零
		return (key & INPUT_KEY_CODE_MASK) != 0;
	}

	// -------------------------------------------------------------------------
	// 命名键常量 —— 全项目唯一编号真源
	// 名称表（键位解析.cpp）、修饰键判定（modifier_mask_of）、平台翻译（渲染层桥）
	// 一律引用本组常量，编号一旦调整即产生编译错误而非静默错键。
	// -------------------------------------------------------------------------

	// —— 主键盘：字母（类内编号连续，便于平台侧按差值换算） ——
	inline constexpr Input_Key INPUT_KEY_A = input_key_make(Input_Key_Class::main, 1);
	inline constexpr Input_Key INPUT_KEY_B = input_key_make(Input_Key_Class::main, 2);
	inline constexpr Input_Key INPUT_KEY_C = input_key_make(Input_Key_Class::main, 3);
	inline constexpr Input_Key INPUT_KEY_D = input_key_make(Input_Key_Class::main, 4);
	inline constexpr Input_Key INPUT_KEY_E = input_key_make(Input_Key_Class::main, 5);
	inline constexpr Input_Key INPUT_KEY_F = input_key_make(Input_Key_Class::main, 6);
	inline constexpr Input_Key INPUT_KEY_G = input_key_make(Input_Key_Class::main, 7);
	inline constexpr Input_Key INPUT_KEY_H = input_key_make(Input_Key_Class::main, 8);
	inline constexpr Input_Key INPUT_KEY_I = input_key_make(Input_Key_Class::main, 9);
	inline constexpr Input_Key INPUT_KEY_J = input_key_make(Input_Key_Class::main, 10);
	inline constexpr Input_Key INPUT_KEY_K = input_key_make(Input_Key_Class::main, 11);
	inline constexpr Input_Key INPUT_KEY_L = input_key_make(Input_Key_Class::main, 12);
	inline constexpr Input_Key INPUT_KEY_M = input_key_make(Input_Key_Class::main, 13);
	inline constexpr Input_Key INPUT_KEY_N = input_key_make(Input_Key_Class::main, 14);
	inline constexpr Input_Key INPUT_KEY_O = input_key_make(Input_Key_Class::main, 15);
	inline constexpr Input_Key INPUT_KEY_P = input_key_make(Input_Key_Class::main, 16);
	inline constexpr Input_Key INPUT_KEY_Q = input_key_make(Input_Key_Class::main, 17);
	inline constexpr Input_Key INPUT_KEY_R = input_key_make(Input_Key_Class::main, 18);
	inline constexpr Input_Key INPUT_KEY_S = input_key_make(Input_Key_Class::main, 19);
	inline constexpr Input_Key INPUT_KEY_T = input_key_make(Input_Key_Class::main, 20);
	inline constexpr Input_Key INPUT_KEY_U = input_key_make(Input_Key_Class::main, 21);
	inline constexpr Input_Key INPUT_KEY_V = input_key_make(Input_Key_Class::main, 22);
	inline constexpr Input_Key INPUT_KEY_W = input_key_make(Input_Key_Class::main, 23);
	inline constexpr Input_Key INPUT_KEY_X = input_key_make(Input_Key_Class::main, 24);
	inline constexpr Input_Key INPUT_KEY_Y = input_key_make(Input_Key_Class::main, 25);
	inline constexpr Input_Key INPUT_KEY_Z = input_key_make(Input_Key_Class::main, 26);

	// —— 主键盘：数字 ——
	inline constexpr Input_Key INPUT_KEY_0 = input_key_make(Input_Key_Class::main, 27);
	inline constexpr Input_Key INPUT_KEY_1 = input_key_make(Input_Key_Class::main, 28);
	inline constexpr Input_Key INPUT_KEY_2 = input_key_make(Input_Key_Class::main, 29);
	inline constexpr Input_Key INPUT_KEY_3 = input_key_make(Input_Key_Class::main, 30);
	inline constexpr Input_Key INPUT_KEY_4 = input_key_make(Input_Key_Class::main, 31);
	inline constexpr Input_Key INPUT_KEY_5 = input_key_make(Input_Key_Class::main, 32);
	inline constexpr Input_Key INPUT_KEY_6 = input_key_make(Input_Key_Class::main, 33);
	inline constexpr Input_Key INPUT_KEY_7 = input_key_make(Input_Key_Class::main, 34);
	inline constexpr Input_Key INPUT_KEY_8 = input_key_make(Input_Key_Class::main, 35);
	inline constexpr Input_Key INPUT_KEY_9 = input_key_make(Input_Key_Class::main, 36);

	// —— 主键盘：符号 ——
	inline constexpr Input_Key INPUT_KEY_GRAVE = input_key_make(Input_Key_Class::main, 37);
	inline constexpr Input_Key INPUT_KEY_MINUS = input_key_make(Input_Key_Class::main, 38);
	inline constexpr Input_Key INPUT_KEY_EQUALS = input_key_make(Input_Key_Class::main, 39);
	inline constexpr Input_Key INPUT_KEY_LEFT_BRACKET = input_key_make(Input_Key_Class::main, 40);
	inline constexpr Input_Key INPUT_KEY_RIGHT_BRACKET = input_key_make(Input_Key_Class::main, 41);
	inline constexpr Input_Key INPUT_KEY_BACKSLASH = input_key_make(Input_Key_Class::main, 42);
	inline constexpr Input_Key INPUT_KEY_SEMICOLON = input_key_make(Input_Key_Class::main, 43);
	inline constexpr Input_Key INPUT_KEY_APOSTROPHE = input_key_make(Input_Key_Class::main, 44);
	inline constexpr Input_Key INPUT_KEY_COMMA = input_key_make(Input_Key_Class::main, 45);
	inline constexpr Input_Key INPUT_KEY_PERIOD = input_key_make(Input_Key_Class::main, 46);
	inline constexpr Input_Key INPUT_KEY_SLASH = input_key_make(Input_Key_Class::main, 47);

	// —— 控制类：基本控制键 ——
	inline constexpr Input_Key INPUT_KEY_ESCAPE = input_key_make(Input_Key_Class::control, 1);
	inline constexpr Input_Key INPUT_KEY_ENTER = input_key_make(Input_Key_Class::control, 2);
	inline constexpr Input_Key INPUT_KEY_SPACE = input_key_make(Input_Key_Class::control, 3);
	inline constexpr Input_Key INPUT_KEY_TAB = input_key_make(Input_Key_Class::control, 4);
	inline constexpr Input_Key INPUT_KEY_BACKSPACE = input_key_make(Input_Key_Class::control, 5);
	inline constexpr Input_Key INPUT_KEY_LEFT = input_key_make(Input_Key_Class::control, 6);
	inline constexpr Input_Key INPUT_KEY_RIGHT = input_key_make(Input_Key_Class::control, 7);
	inline constexpr Input_Key INPUT_KEY_UP = input_key_make(Input_Key_Class::control, 8);
	inline constexpr Input_Key INPUT_KEY_DOWN = input_key_make(Input_Key_Class::control, 9);

	// —— 控制类：功能键（类内编号连续） ——
	inline constexpr Input_Key INPUT_KEY_F1 = input_key_make(Input_Key_Class::control, 10);
	inline constexpr Input_Key INPUT_KEY_F2 = input_key_make(Input_Key_Class::control, 11);
	inline constexpr Input_Key INPUT_KEY_F3 = input_key_make(Input_Key_Class::control, 12);
	inline constexpr Input_Key INPUT_KEY_F4 = input_key_make(Input_Key_Class::control, 13);
	inline constexpr Input_Key INPUT_KEY_F5 = input_key_make(Input_Key_Class::control, 14);
	inline constexpr Input_Key INPUT_KEY_F6 = input_key_make(Input_Key_Class::control, 15);
	inline constexpr Input_Key INPUT_KEY_F7 = input_key_make(Input_Key_Class::control, 16);
	inline constexpr Input_Key INPUT_KEY_F8 = input_key_make(Input_Key_Class::control, 17);
	inline constexpr Input_Key INPUT_KEY_F9 = input_key_make(Input_Key_Class::control, 18);
	inline constexpr Input_Key INPUT_KEY_F10 = input_key_make(Input_Key_Class::control, 19);
	inline constexpr Input_Key INPUT_KEY_F11 = input_key_make(Input_Key_Class::control, 20);
	inline constexpr Input_Key INPUT_KEY_F12 = input_key_make(Input_Key_Class::control, 21);

	// —— 控制类：编辑导航键 ——
	inline constexpr Input_Key INPUT_KEY_INSERT = input_key_make(Input_Key_Class::control, 22);
	inline constexpr Input_Key INPUT_KEY_DELETE = input_key_make(Input_Key_Class::control, 23);
	inline constexpr Input_Key INPUT_KEY_HOME = input_key_make(Input_Key_Class::control, 24);
	inline constexpr Input_Key INPUT_KEY_END = input_key_make(Input_Key_Class::control, 25);
	inline constexpr Input_Key INPUT_KEY_PAGE_UP = input_key_make(Input_Key_Class::control, 26);
	inline constexpr Input_Key INPUT_KEY_PAGE_DOWN = input_key_make(Input_Key_Class::control, 27);

	// —— 控制类：锁定与系统键 ——
	inline constexpr Input_Key INPUT_KEY_CAPS_LOCK = input_key_make(Input_Key_Class::control, 28);
	inline constexpr Input_Key INPUT_KEY_PRINT_SCREEN = input_key_make(Input_Key_Class::control, 29);
	inline constexpr Input_Key INPUT_KEY_SCROLL_LOCK = input_key_make(Input_Key_Class::control, 30);
	inline constexpr Input_Key INPUT_KEY_PAUSE = input_key_make(Input_Key_Class::control, 31);
	inline constexpr Input_Key INPUT_KEY_NUM_LOCK = input_key_make(Input_Key_Class::control, 32);
	inline constexpr Input_Key INPUT_KEY_MENU = input_key_make(Input_Key_Class::control, 33);

	// —— 控制类：修饰键（左右可区分，规范化掩码一致） ——
	inline constexpr Input_Key INPUT_KEY_LSHIFT = input_key_make(Input_Key_Class::control, 34);
	inline constexpr Input_Key INPUT_KEY_RSHIFT = input_key_make(Input_Key_Class::control, 35);
	inline constexpr Input_Key INPUT_KEY_LCTRL = input_key_make(Input_Key_Class::control, 36);
	inline constexpr Input_Key INPUT_KEY_RCTRL = input_key_make(Input_Key_Class::control, 37);
	inline constexpr Input_Key INPUT_KEY_LALT = input_key_make(Input_Key_Class::control, 38);
	inline constexpr Input_Key INPUT_KEY_RALT = input_key_make(Input_Key_Class::control, 39);
	inline constexpr Input_Key INPUT_KEY_LGUI = input_key_make(Input_Key_Class::control, 40);
	inline constexpr Input_Key INPUT_KEY_RGUI = input_key_make(Input_Key_Class::control, 41);

	// —— 小键盘 ——
	inline constexpr Input_Key INPUT_KEY_KP_0 = input_key_make(Input_Key_Class::keypad, 1);
	inline constexpr Input_Key INPUT_KEY_KP_1 = input_key_make(Input_Key_Class::keypad, 2);
	inline constexpr Input_Key INPUT_KEY_KP_2 = input_key_make(Input_Key_Class::keypad, 3);
	inline constexpr Input_Key INPUT_KEY_KP_3 = input_key_make(Input_Key_Class::keypad, 4);
	inline constexpr Input_Key INPUT_KEY_KP_4 = input_key_make(Input_Key_Class::keypad, 5);
	inline constexpr Input_Key INPUT_KEY_KP_5 = input_key_make(Input_Key_Class::keypad, 6);
	inline constexpr Input_Key INPUT_KEY_KP_6 = input_key_make(Input_Key_Class::keypad, 7);
	inline constexpr Input_Key INPUT_KEY_KP_7 = input_key_make(Input_Key_Class::keypad, 8);
	inline constexpr Input_Key INPUT_KEY_KP_8 = input_key_make(Input_Key_Class::keypad, 9);
	inline constexpr Input_Key INPUT_KEY_KP_9 = input_key_make(Input_Key_Class::keypad, 10);
	inline constexpr Input_Key INPUT_KEY_KP_DECIMAL = input_key_make(Input_Key_Class::keypad, 11);
	inline constexpr Input_Key INPUT_KEY_KP_DIVIDE = input_key_make(Input_Key_Class::keypad, 12);
	inline constexpr Input_Key INPUT_KEY_KP_MULTIPLY = input_key_make(Input_Key_Class::keypad, 13);
	inline constexpr Input_Key INPUT_KEY_KP_MINUS = input_key_make(Input_Key_Class::keypad, 14);
	inline constexpr Input_Key INPUT_KEY_KP_PLUS = input_key_make(Input_Key_Class::keypad, 15);
	inline constexpr Input_Key INPUT_KEY_KP_ENTER = input_key_make(Input_Key_Class::keypad, 16);

	// —— 鼠标按键 ——
	inline constexpr Input_Key INPUT_KEY_MOUSE_LEFT = input_key_make(Input_Key_Class::mouse, 1);
	inline constexpr Input_Key INPUT_KEY_MOUSE_MIDDLE = input_key_make(Input_Key_Class::mouse, 2);
	inline constexpr Input_Key INPUT_KEY_MOUSE_RIGHT = input_key_make(Input_Key_Class::mouse, 3);
	inline constexpr Input_Key INPUT_KEY_MOUSE_X1 = input_key_make(Input_Key_Class::mouse, 4);
	inline constexpr Input_Key INPUT_KEY_MOUSE_X2 = input_key_make(Input_Key_Class::mouse, 5);

	// —— 滚轮 ——
	inline constexpr Input_Key INPUT_KEY_WHEEL_UP = input_key_make(Input_Key_Class::wheel, 1);
	inline constexpr Input_Key INPUT_KEY_WHEEL_DOWN = input_key_make(Input_Key_Class::wheel, 2);

	//修饰键规范化掩码位
	inline constexpr uint8_t INPUT_MODIFIER_SHIFT = 0x01;
	inline constexpr uint8_t INPUT_MODIFIER_CTRL = 0x02;
	inline constexpr uint8_t INPUT_MODIFIER_ALT = 0x04;
	inline constexpr uint8_t INPUT_MODIFIER_GUI = 0x08;

	//输入映射绑定
	struct Input_Binding
	{
		//映射语义
		std::string command{};
		//语义目标对象名（Enter 必需，其余可空）
		std::string target{};
		//组合修饰掩码（0 = 普通单键绑定）
		uint8_t modifier_mask = 0;
		//响应长按自动重复（组合与单键均适用）
		bool allow_repeat = false;
	};

	//输入轴绑定
	struct Axis_Binding
	{
		//来源（pointer.dx / pointer.dy / pointer.x / pointer.y / 原始轴名）
		std::string source{};
		//速率型（真）/ 位移型（假）
		bool rate = false;
		//缩放系数
		double scale = 1.0;
		//死区
		double deadzone = 0.0;
		//钳制上限
		double clamp = 1.0;
		//反相
		bool invert = false;
	};

	//指针快照（系统侧缓存的最近一帧原始指针状态）
	struct Input_Cursor_Snapshot
	{
		//窗口客户区像素坐标
		double x = 0.0;
		double y = 0.0;
		//本帧位移增量
		double dx = 0.0;
		double dy = 0.0;
		//本帧滚轮增量（刻度）
		double wheel_x = 0.0;
		double wheel_y = 0.0;
	};

}
