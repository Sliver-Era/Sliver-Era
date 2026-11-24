/*
渲染 UI 键盘激活测试（第九百六十一轮）
覆盖：回车 / 空格激活焦点按钮、空格切换焦点复选框、文本模式不误触。
*/
#include <gtest/gtest.h>
#include <SDL3/SDL.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造点击帧
	Input_Frame click_frame(float x, float y)
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

	//构造按键帧
	Input_Frame key_frame(int scancode)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.key_pressed.push_back(scancode);
		return input;
	}

	//完整点击（按下帧 ＋ 抬起帧）
	void click_at(Ui_Context& ui, float x, float y)
	{
		Input_Frame press{};
		press.window_width = 320;
		press.window_height = 180;
		press.pointer.x = x;
		press.pointer.y = y;
		press.pointer.button_pressed[0] = true;
		press.pointer.button_down[0] = true;
		ui.frame_begin(press);

		Input_Frame release{};
		release.window_width = 320;
		release.window_height = 180;
		release.pointer.x = x;
		release.pointer.y = y;
		release.pointer.button_released[0] = true;
		ui.frame_begin(release);
	}
}

//渲染 UI 键盘激活测试夹具
class Renderer_Ui_Keyboard_Activate_Test : public ::testing::Test
{
};

//空格 / 回车激活焦点按钮
TEST_F(Renderer_Ui_Keyboard_Activate_Test, 激活按钮)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, "A");
	ASSERT_NE(button, 0u);

	//点击建立焦点
	click_at(ui, 60.0f, 35.0f);
	ASSERT_TRUE(ui.focused_get(button));
	ASSERT_TRUE(ui.clicked_consume(button)) << "点击应先消费一次";

	//空格激活
	ui.frame_begin(key_frame(SDL_SCANCODE_SPACE));
	EXPECT_TRUE(ui.clicked_consume(button)) << "空格应激活按钮";

	//回车激活
	ui.frame_begin(key_frame(SDL_SCANCODE_RETURN));
	EXPECT_TRUE(ui.clicked_consume(button)) << "回车应激活按钮";
}

//空格切换焦点复选框
TEST_F(Renderer_Ui_Keyboard_Activate_Test, 切换复选框)
{
	Ui_Context ui;
	const Ui_Handle box = ui.checkbox_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, "勾选");
	ASSERT_NE(box, 0u);
	ASSERT_FALSE(ui.checked_get(box));

	//点击建立焦点
	click_at(ui, 30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(box));

	//空格切换（点击本身已切换一次，这里只验证键盘切换）
	const bool before = ui.checked_get(box);
	ui.frame_begin(key_frame(SDL_SCANCODE_SPACE));
	EXPECT_EQ(ui.checked_get(box), !before) << "空格应切换复选框";
}

//文本框焦点下空格不触发点击语义
TEST_F(Renderer_Ui_Keyboard_Activate_Test, 文本模式不误触)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 120.0f, 24.0f }, "ab");
	ASSERT_NE(box, 0u);

	//点击建立焦点
	click_at(ui, 30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(box));
	ASSERT_FALSE(ui.clicked_consume(box));

	//空格帧：文本框不产生 clicked
	ui.frame_begin(key_frame(SDL_SCANCODE_SPACE));
	EXPECT_FALSE(ui.clicked_consume(box)) << "文本框不应被视为按钮激活";
}
