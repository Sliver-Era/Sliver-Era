/*
渲染 UI 下拉框键盘测试（第九百七十九轮）
覆盖：回车展开 / 收起、上下键移动选项并夹紧、回车确认、Esc 收起、焦点建立。
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

//渲染 UI 下拉框键盘测试夹具
class Renderer_Ui_Combo_Key_Test : public ::testing::Test
{
};

//回车展开、上下键移动、回车确认、Esc 收起
TEST_F(Renderer_Ui_Combo_Key_Test, 键盘操作)
{
	Ui_Context ui;
	const Ui_Handle combo = ui.combo_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, { "甲", "乙", "丙" });
	ASSERT_NE(combo, 0u);

	//点击建立焦点（按下沿展开，再收起以验证键盘路径）
	click_at(ui, 50.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(combo));
	ASSERT_TRUE(ui.combo_open_set(combo, false));

	//收起态回车：展开
	ui.frame_begin(key_frame(SDL_SCANCODE_RETURN));
	EXPECT_TRUE(ui.combo_open_get(combo));

	//下键两次：下标 0 → 2
	ui.frame_begin(key_frame(SDL_SCANCODE_DOWN));
	EXPECT_EQ(ui.combo_index_get(combo), 1u);
	ui.frame_begin(key_frame(SDL_SCANCODE_DOWN));
	EXPECT_EQ(ui.combo_index_get(combo), 2u);
	//越界保持
	ui.frame_begin(key_frame(SDL_SCANCODE_DOWN));
	EXPECT_EQ(ui.combo_index_get(combo), 2u);
	//上键一次：2 → 1
	ui.frame_begin(key_frame(SDL_SCANCODE_UP));
	EXPECT_EQ(ui.combo_index_get(combo), 1u);

	//回车确认：收起并消费一次点击
	ui.frame_begin(key_frame(SDL_SCANCODE_RETURN));
	EXPECT_FALSE(ui.combo_open_get(combo));
	EXPECT_EQ(ui.combo_index_get(combo), 1u);
	EXPECT_TRUE(ui.clicked_consume(combo));

	//再次展开后 Esc 收起（不产生点击）
	ui.frame_begin(key_frame(SDL_SCANCODE_SPACE));
	ASSERT_TRUE(ui.combo_open_get(combo));
	ui.frame_begin(key_frame(SDL_SCANCODE_ESCAPE));
	EXPECT_FALSE(ui.combo_open_get(combo));
	EXPECT_FALSE(ui.clicked_consume(combo));
}
