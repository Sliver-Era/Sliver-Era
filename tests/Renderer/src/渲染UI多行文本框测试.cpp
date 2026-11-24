/*
渲染 UI 多行文本框测试（第九百六十六轮）
覆盖：多行开关存取、回车插入换行、行首退格跨行合并、单行模式回车不插入。
说明：编辑只由焦点文本框消费键盘，本文件先点击建立焦点再发按键帧。
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
}

//渲染 UI 多行文本框测试夹具
class Renderer_Ui_Multiline_Test : public ::testing::Test
{
};

//多行开关存取
TEST_F(Renderer_Ui_Multiline_Test, 多行开关存取)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 120.0f, 24.0f }, "");
	ASSERT_NE(box, 0u);
	//默认单行
	EXPECT_FALSE(ui.multiline_get(box));

	ASSERT_TRUE(ui.multiline_set(box, true));
	EXPECT_TRUE(ui.multiline_get(box));
	ASSERT_TRUE(ui.multiline_set(box, false));
	EXPECT_FALSE(ui.multiline_get(box));

	//未知句柄
	EXPECT_FALSE(ui.multiline_set(box + 100, true));
	EXPECT_FALSE(ui.multiline_get(box + 100));
}

//回车插入换行
TEST_F(Renderer_Ui_Multiline_Test, 回车插入换行)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 120.0f, 40.0f }, "ab");
	ASSERT_NE(box, 0u);
	ASSERT_TRUE(ui.multiline_set(box, true));

	//点击建立焦点（光标在串尾）
	click_at(ui, 30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(box));
	ASSERT_EQ(ui.caret_get(box), 2u);

	//第一次回车：末尾插入换行
	ui.frame_begin(key_frame(SDL_SCANCODE_RETURN));
	EXPECT_EQ(ui.text_box_text_get(box), "ab\n");
	EXPECT_EQ(ui.caret_get(box), 3u);

	//第二次回车：再插入一个换行
	ui.frame_begin(key_frame(SDL_SCANCODE_KP_ENTER));
	EXPECT_EQ(ui.text_box_text_get(box), "ab\n\n");
	EXPECT_EQ(ui.caret_get(box), 4u);
}

//行首退格跨行合并
TEST_F(Renderer_Ui_Multiline_Test, 行首退格跨行合并)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 120.0f, 40.0f }, "ab\ncd");
	ASSERT_NE(box, 0u);
	ASSERT_TRUE(ui.multiline_set(box, true));

	//点击建立焦点并把光标放到第二行行首
	click_at(ui, 30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(box));
	ASSERT_TRUE(ui.caret_set(box, 3));

	//退格删除换行，两行合并
	ui.frame_begin(key_frame(SDL_SCANCODE_BACKSPACE));
	EXPECT_EQ(ui.text_box_text_get(box), "abcd");
	EXPECT_EQ(ui.caret_get(box), 2u);
}

//单行模式回车不插入
TEST_F(Renderer_Ui_Multiline_Test, 单行模式回车不插入)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 120.0f, 24.0f }, "ab");
	ASSERT_NE(box, 0u);

	//点击建立焦点
	click_at(ui, 30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(box));

	//未开启多行：回车不改变内容
	ui.frame_begin(key_frame(SDL_SCANCODE_RETURN));
	EXPECT_EQ(ui.text_box_text_get(box), "ab");
	EXPECT_EQ(ui.caret_get(box), 2u);
}
