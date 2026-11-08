#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//匿名命名空间（仅本编译单元可见）
	namespace
	{
		//键位名称表（大写名 → 中立编码）
		const std::unordered_map<std::string, Input_Key>& key_name_table(void)
		{
			//静态名称表（编号一律引用 输入类型.h 的命名常量）
			static const std::unordered_map<std::string, Input_Key> table{
				// —— 主键盘：字母 ——
				{"A", INPUT_KEY_A}, {"B", INPUT_KEY_B}, {"C", INPUT_KEY_C}, {"D", INPUT_KEY_D},
				{"E", INPUT_KEY_E}, {"F", INPUT_KEY_F}, {"G", INPUT_KEY_G}, {"H", INPUT_KEY_H},
				{"I", INPUT_KEY_I}, {"J", INPUT_KEY_J}, {"K", INPUT_KEY_K}, {"L", INPUT_KEY_L},
				{"M", INPUT_KEY_M}, {"N", INPUT_KEY_N}, {"O", INPUT_KEY_O}, {"P", INPUT_KEY_P},
				{"Q", INPUT_KEY_Q}, {"R", INPUT_KEY_R}, {"S", INPUT_KEY_S}, {"T", INPUT_KEY_T},
				{"U", INPUT_KEY_U}, {"V", INPUT_KEY_V}, {"W", INPUT_KEY_W}, {"X", INPUT_KEY_X},
				{"Y", INPUT_KEY_Y}, {"Z", INPUT_KEY_Z},
				// —— 主键盘：数字 ——
				{"0", INPUT_KEY_0}, {"1", INPUT_KEY_1}, {"2", INPUT_KEY_2}, {"3", INPUT_KEY_3},
				{"4", INPUT_KEY_4}, {"5", INPUT_KEY_5}, {"6", INPUT_KEY_6}, {"7", INPUT_KEY_7},
				{"8", INPUT_KEY_8}, {"9", INPUT_KEY_9},
				// —— 主键盘：符号（字符本身与英文键名双写法） ——
				{"`", INPUT_KEY_GRAVE}, {"BACKQUOTE", INPUT_KEY_GRAVE}, {"GRAVE", INPUT_KEY_GRAVE},
				{"-", INPUT_KEY_MINUS}, {"MINUS", INPUT_KEY_MINUS},
				{"=", INPUT_KEY_EQUALS}, {"EQUAL", INPUT_KEY_EQUALS}, {"EQUALS", INPUT_KEY_EQUALS},
				{"[", INPUT_KEY_LEFT_BRACKET}, {"LEFT_BRACKET", INPUT_KEY_LEFT_BRACKET},
				{"]", INPUT_KEY_RIGHT_BRACKET}, {"RIGHT_BRACKET", INPUT_KEY_RIGHT_BRACKET},
				{"\\", INPUT_KEY_BACKSLASH}, {"BACKSLASH", INPUT_KEY_BACKSLASH},
				{";", INPUT_KEY_SEMICOLON}, {"SEMICOLON", INPUT_KEY_SEMICOLON},
				{"'", INPUT_KEY_APOSTROPHE}, {"APOSTROPHE", INPUT_KEY_APOSTROPHE},
				{",", INPUT_KEY_COMMA}, {"COMMA", INPUT_KEY_COMMA},
				{".", INPUT_KEY_PERIOD}, {"PERIOD", INPUT_KEY_PERIOD},
				{"/", INPUT_KEY_SLASH}, {"SLASH", INPUT_KEY_SLASH},
				// —— 控制类：基本控制键 ——
				{"ESCAPE", INPUT_KEY_ESCAPE}, {"ESC", INPUT_KEY_ESCAPE},
				{"ENTER", INPUT_KEY_ENTER}, {"RETURN", INPUT_KEY_ENTER},
				{"SPACE", INPUT_KEY_SPACE}, {"TAB", INPUT_KEY_TAB},
				{"BACKSPACE", INPUT_KEY_BACKSPACE},
				{"LEFT", INPUT_KEY_LEFT}, {"RIGHT", INPUT_KEY_RIGHT},
				{"UP", INPUT_KEY_UP}, {"DOWN", INPUT_KEY_DOWN},
				// —— 控制类：功能键 ——
				{"F1", INPUT_KEY_F1}, {"F2", INPUT_KEY_F2}, {"F3", INPUT_KEY_F3}, {"F4", INPUT_KEY_F4},
				{"F5", INPUT_KEY_F5}, {"F6", INPUT_KEY_F6}, {"F7", INPUT_KEY_F7}, {"F8", INPUT_KEY_F8},
				{"F9", INPUT_KEY_F9}, {"F10", INPUT_KEY_F10}, {"F11", INPUT_KEY_F11}, {"F12", INPUT_KEY_F12},
				// —— 控制类：编辑导航键 ——
				{"INSERT", INPUT_KEY_INSERT},
				{"DELETE", INPUT_KEY_DELETE}, {"DEL", INPUT_KEY_DELETE},
				{"HOME", INPUT_KEY_HOME}, {"END", INPUT_KEY_END},
				{"PAGEUP", INPUT_KEY_PAGE_UP}, {"PGUP", INPUT_KEY_PAGE_UP}, {"PAGE_UP", INPUT_KEY_PAGE_UP},
				{"PAGEDOWN", INPUT_KEY_PAGE_DOWN}, {"PGDN", INPUT_KEY_PAGE_DOWN},
				{"PAGE_DOWN", INPUT_KEY_PAGE_DOWN},
				// —— 控制类：锁定与系统键 ——
				{"CAPSLOCK", INPUT_KEY_CAPS_LOCK}, {"CAPS_LOCK", INPUT_KEY_CAPS_LOCK},
				{"PRINTSCREEN", INPUT_KEY_PRINT_SCREEN}, {"PRINT_SCREEN", INPUT_KEY_PRINT_SCREEN},
				{"SCROLLLOCK", INPUT_KEY_SCROLL_LOCK}, {"SCROLL_LOCK", INPUT_KEY_SCROLL_LOCK},
				{"PAUSE", INPUT_KEY_PAUSE},
				{"NUM_LOCK", INPUT_KEY_NUM_LOCK}, {"NUMLOCK", INPUT_KEY_NUM_LOCK},
				{"MENU", INPUT_KEY_MENU},
				// —— 控制类：修饰键 ——
				{"LSHIFT", INPUT_KEY_LSHIFT}, {"RSHIFT", INPUT_KEY_RSHIFT},
				{"LCTRL", INPUT_KEY_LCTRL}, {"RCTRL", INPUT_KEY_RCTRL},
				{"LCONTROL", INPUT_KEY_LCTRL}, {"RCONTROL", INPUT_KEY_RCTRL},
				{"LALT", INPUT_KEY_LALT}, {"RALT", INPUT_KEY_RALT},
				{"LGUI", INPUT_KEY_LGUI}, {"RGUI", INPUT_KEY_RGUI},
				// —— 小键盘 ——
				{"KP0", INPUT_KEY_KP_0}, {"KP1", INPUT_KEY_KP_1}, {"KP2", INPUT_KEY_KP_2},
				{"KP3", INPUT_KEY_KP_3}, {"KP4", INPUT_KEY_KP_4}, {"KP5", INPUT_KEY_KP_5},
				{"KP6", INPUT_KEY_KP_6}, {"KP7", INPUT_KEY_KP_7}, {"KP8", INPUT_KEY_KP_8},
				{"KP9", INPUT_KEY_KP_9},
				{"KP_DECIMAL", INPUT_KEY_KP_DECIMAL}, {"KP_DIVIDE", INPUT_KEY_KP_DIVIDE},
				{"KP_MULTIPLY", INPUT_KEY_KP_MULTIPLY}, {"KP_MINUS", INPUT_KEY_KP_MINUS},
				{"KP_PLUS", INPUT_KEY_KP_PLUS}, {"KP_ENTER", INPUT_KEY_KP_ENTER},
				// —— 鼠标按键 ——
				{"MOUSE_LEFT", INPUT_KEY_MOUSE_LEFT}, {"MOUSE_MIDDLE", INPUT_KEY_MOUSE_MIDDLE},
				{"MOUSE_RIGHT", INPUT_KEY_MOUSE_RIGHT},
				{"MOUSE_X1", INPUT_KEY_MOUSE_X1}, {"MOUSE_X2", INPUT_KEY_MOUSE_X2},
				// —— 滚轮 ——
				{"WHEEL_UP", INPUT_KEY_WHEEL_UP}, {"WHEEL_DOWN", INPUT_KEY_WHEEL_DOWN}
			};
			return table;
		}

		//修饰键名称表（大写名 → 规范化掩码位）
		const std::unordered_map<std::string, uint8_t>& modifier_name_table(void)
		{
			//静态名称表
			static const std::unordered_map<std::string, uint8_t> table{
				{"SHIFT", INPUT_MODIFIER_SHIFT},
				{"LSHIFT", INPUT_MODIFIER_SHIFT},
				{"RSHIFT", INPUT_MODIFIER_SHIFT},
				{"CTRL", INPUT_MODIFIER_CTRL},
				{"CONTROL", INPUT_MODIFIER_CTRL},
				{"LCTRL", INPUT_MODIFIER_CTRL},
				{"RCTRL", INPUT_MODIFIER_CTRL},
				{"LCONTROL", INPUT_MODIFIER_CTRL},
				{"RCONTROL", INPUT_MODIFIER_CTRL},
				{"ALT", INPUT_MODIFIER_ALT},
				{"LALT", INPUT_MODIFIER_ALT},
				{"RALT", INPUT_MODIFIER_ALT},
				{"GUI", INPUT_MODIFIER_GUI},
				{"LGUI", INPUT_MODIFIER_GUI},
				{"RGUI", INPUT_MODIFIER_GUI}
			};
			return table;
		}

		//名称规范化（统一转大写）
		std::string key_name_normalize(const std::string& name)
		{
			//名称缓冲
			std::string buffer = name;
			//逐字符转大写
			for (auto& character : buffer)
				character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
			return buffer;
		}
	}

	//输入映射工具
	namespace input_mapping
	{
	//键位解析
	bool key_parse(const json& spec, Input_Key& key_out)
	{
		//若为无符号整数
		if (spec.is_number_unsigned())
		{
			const uint64_t value = spec.get<uint64_t>();
			//非正数或超出编码宽度视为无效键位
			if (value == 0 || value > UINT32_MAX)
				return false;
			const Input_Key key = static_cast<Input_Key>(value);
			//位域合法性校验（旧平铺数字在此被拒）
			if (!input_key_valid(key))
				return false;
			key_out = key;
			return true;
		}
		//若为有符号整数
		if (spec.is_number_integer())
		{
			const int64_t value = spec.get<int64_t>();
			//非正数视为无效键位
			if (value <= 0)
				return false;
			const Input_Key key = static_cast<Input_Key>(value);
			//位域合法性校验（旧平铺数字在此被拒）
			if (!input_key_valid(key))
				return false;
			key_out = key;
			return true;
		}
		//若为名称
		if (spec.is_string())
		{
			const string name = key_name_normalize(spec.get<string>());
			//查找名称表
			auto it = key_name_table().find(name);
			//若名称未登记
			if (it == key_name_table().end())
				return false;
			key_out = it->second;
			return true;
		}

		return false;
	}

	//物理键位 → 规范化修饰掩码
	uint8_t modifier_mask_of(const Input_Key& key)
	{
		//左右 Shift
		if (key == INPUT_KEY_LSHIFT || key == INPUT_KEY_RSHIFT)
			return INPUT_MODIFIER_SHIFT;
		//左右 Ctrl
		if (key == INPUT_KEY_LCTRL || key == INPUT_KEY_RCTRL)
			return INPUT_MODIFIER_CTRL;
		//左右 Alt
		if (key == INPUT_KEY_LALT || key == INPUT_KEY_RALT)
			return INPUT_MODIFIER_ALT;
		//左右 GUI
		if (key == INPUT_KEY_LGUI || key == INPUT_KEY_RGUI)
			return INPUT_MODIFIER_GUI;
		return 0;
	}

	//组合修饰项解析
	bool modifier_parse(const json& spec, uint8_t& mask_out)
	{
		//缺省视为无修饰
		mask_out = 0;
		//若字段为空值
		if (spec.is_null())
			return true;
		//若字段非数组
		if (!spec.is_array())
		{
			logger.warn("组合修饰项字段非数组");
			return false;
		}

		//逐项解析
		for (const auto& item : spec)
		{
			//若为名称
			if (item.is_string())
			{
				const string name = key_name_normalize(item.get<string>());
				//查找修饰键名称表
				auto it = modifier_name_table().find(name);
				//若名称不是修饰键
				if (it == modifier_name_table().end())
				{
					logger.warn("组合修饰项无法识别：{}", name);
					return false;
				}
				mask_out |= it->second;
				continue;
			}

			//若为数字键位
			Input_Key key = 0;
			//若键位无法识别或不是修饰键
			if (!key_parse(item, key) || modifier_mask_of(key) == 0)
			{
				logger.warn("组合修饰项无法识别");
				return false;
			}
			mask_out |= modifier_mask_of(key);
		}

		return true;
	}

	}
}
