/*
渲染 UI 下拉框滚动测试（第九百八十轮）
覆盖：展开区滚轮滚动改变可见起始行、点选按滚动偏移换算下标、键盘移动时保持当前项可见。
说明：可视行数固定为 8；超过部分通过滚轮或键盘移动滚动。
*/
#include <gtest/gtest.h>
#include <SDL3/SDL.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造按下帧
	Input_Frame press_frame(float x, float y)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_pressed[0] = true;
		input.pointer.button_down[0] = true;
		return input;
	}

	//构造抬起帧
	Input_Frame release_frame(float x, float y)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_released[0] = true;
		return input;
	}

	//构造滚轮帧（wheel_y < 0 向下滚动）
	Input_Frame wheel_frame(float x, float y, float wheel_y)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.wheel_y = wheel_y;
		return input;
	}

	//构造按键帧
	Input_Frame key_frame(int scancode)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.key_pressed.push_back(scancode);
		return input;
	}

	//点击建立焦点
	void click_at(Ui_Context& ui, float x, float y)
	{
		ui.frame_begin(press_frame(x, y));
		ui.frame_begin(release_frame(x, y));
	}

	//构造 20 项选项
	std::vector<std::string> many_items(void)
	{
		std::vector<std::string> items{};
		for (int index = 0; index < 20; ++index)
			items.push_back("项" + std::to_string(index));
		return items;
	}
}

//渲染 UI 下拉框滚动测试夹具
class Renderer_Ui_Combo_Scroll_Test : public ::testing::Test
{
};

//滚轮滚动后点选首行：下标按滚动偏移换算
TEST_F(Renderer_Ui_Combo_Scroll_Test, 滚轮滚动与点选换算)
{
	Ui_Context ui;
	const Ui_Handle combo = ui.combo_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, many_items());
	ASSERT_NE(combo, 0u);

	//点击建立焦点（顺带展开）后强制展开
	click_at(ui, 50.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(combo));
	ASSERT_TRUE(ui.combo_open_set(combo, true));

	//在展开区向下滚动一行（列表首行 y = 44）
	ui.frame_begin(wheel_frame(50.0f, 54.0f, -1.0f));
	//滚动帧后仍应保持展开
	ASSERT_TRUE(ui.combo_open_get(combo));
	//点首行：下标应为滚动偏移 1 + 行 0
	ui.frame_begin(press_frame(50.0f, 56.0f));
	EXPECT_EQ(ui.combo_index_get(combo), 1u) << "点选应按滚动偏移换算";
	EXPECT_FALSE(ui.combo_open_get(combo));
}

//键盘下移越过可视区：滚动自动跟随，点选首行反映当前滚动
TEST_F(Renderer_Ui_Combo_Scroll_Test, 键盘移动保持可见)
{
	Ui_Context ui;
	const Ui_Handle combo = ui.combo_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, many_items());
	ASSERT_NE(combo, 0u);

	//建立焦点并复位展开态
	click_at(ui, 50.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(combo));
	ASSERT_TRUE(ui.combo_open_set(combo, false));

	//回车展开
	ui.frame_begin(key_frame(SDL_SCANCODE_RETURN));
	ASSERT_TRUE(ui.combo_open_get(combo));

	//下移 9 次：下标 0 → 9，滚动应为 2（9 - 7）
	for (int index = 0; index < 9; ++index)
		ui.frame_begin(key_frame(SDL_SCANCODE_DOWN));
	EXPECT_EQ(ui.combo_index_get(combo), 9u);

	//点首行：下标 = 滚动 2 + 行 0
	ui.frame_begin(press_frame(50.0f, 56.0f));
	EXPECT_EQ(ui.combo_index_get(combo), 2u) << "键盘移动后滚动应跟随当前项";
}
