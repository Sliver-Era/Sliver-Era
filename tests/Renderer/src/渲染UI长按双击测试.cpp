/*
渲染 UI 长按与双击测试（第九百八十二轮）
覆盖：按住累计达 0.6 秒置长按、标记消费一次即清除、两次点击间隔内置双击。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造按下帧
	Input_Frame press_frame(float x, float y, double dt)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dt = dt;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_pressed[0] = true;
		input.pointer.button_down[0] = true;
		return input;
	}

	//构造按住帧（无按下沿，仅保持按下）
	Input_Frame hold_frame(float x, float y, double dt)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dt = dt;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_down[0] = true;
		return input;
	}

	//构造抬起帧
	Input_Frame release_frame(float x, float y, double dt)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dt = dt;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_released[0] = true;
		return input;
	}
}

//渲染 UI 长按与双击测试夹具
class Renderer_Ui_Gesture_Test : public ::testing::Test
{
};

//长按：按住累计达阈值，消费一次即清除
TEST_F(Renderer_Ui_Gesture_Test, 长按判定与消费)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, "A");
	ASSERT_NE(button, 0u);

	//按下 0.4 秒：未达阈值
	ui.frame_begin(press_frame(60.0f, 35.0f, 0.4));
	EXPECT_FALSE(ui.long_pressed_consume(button));

	//再按住 0.3 秒（累计 0.7）：触发长按
	ui.frame_begin(hold_frame(60.0f, 35.0f, 0.3));
	EXPECT_TRUE(ui.long_pressed_consume(button));
	EXPECT_FALSE(ui.long_pressed_consume(button)) << "标记只消费一次";

	//抬起
	ui.frame_begin(release_frame(60.0f, 35.0f, 0.1));
	EXPECT_FALSE(ui.long_pressed_consume(button));
}

//双击：间隔 0.1 秒的两次点击置双击，消费一次即清除
TEST_F(Renderer_Ui_Gesture_Test, 双击判定与消费)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, "A");
	ASSERT_NE(button, 0u);

	//第一次点击
	ui.frame_begin(press_frame(60.0f, 35.0f, 0.1));
	ui.frame_begin(release_frame(60.0f, 35.0f, 0.1));
	EXPECT_FALSE(ui.double_clicked_consume(button)) << "首次点击不构成双击";

	//第二次点击（间隔 0.1 秒）
	ui.frame_begin(press_frame(60.0f, 35.0f, 0.1));
	ui.frame_begin(release_frame(60.0f, 35.0f, 0.1));
	EXPECT_TRUE(ui.double_clicked_consume(button));
	EXPECT_FALSE(ui.double_clicked_consume(button)) << "标记只消费一次";
}
