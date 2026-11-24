/*
渲染 UI 工具提示测试（第九百六十三轮）
覆盖：提示文本存取、悬停延时判定、移出恢复。
说明：绘制路径在 renderer 内部帧推进中以调用方帧时间计算，本文件只验证交互侧判定。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造悬停帧
	Input_Frame hover_frame(float x, float y, double dt)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dt = dt;
		input.pointer.x = x;
		input.pointer.y = y;
		return input;
	}

	//构造移出帧
	Input_Frame away_frame(void)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dt = 0.3;
		input.pointer.x = 300.0f;
		input.pointer.y = 170.0f;
		return input;
	}
}

//渲染 UI 工具提示测试夹具
class Renderer_Ui_Tooltip_Test : public ::testing::Test
{
};

//提示文本存取
TEST_F(Renderer_Ui_Tooltip_Test, 提示文本存取)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 80.0f, 30.0f }, "A");
	ASSERT_NE(button, 0u);
	EXPECT_TRUE(ui.tooltip_get(button).empty());

	ASSERT_TRUE(ui.tooltip_set(button, "提示"));
	EXPECT_EQ(ui.tooltip_get(button), "提示");
	ASSERT_TRUE(ui.tooltip_set(button, ""));
	EXPECT_TRUE(ui.tooltip_get(button).empty());

	//未知句柄
	EXPECT_FALSE(ui.tooltip_set(button + 100, "x"));
	EXPECT_TRUE(ui.tooltip_get(button + 100).empty());
	EXPECT_FALSE(ui.tooltip_visible(button + 100));
}

//悬停延时：0.3 秒未达、0.6 秒达到、移出归零
TEST_F(Renderer_Ui_Tooltip_Test, 悬停延时判定)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, "");
	ASSERT_NE(button, 0u);
	ASSERT_TRUE(ui.tooltip_set(button, "提示"));

	//悬停 0.3 秒：未达延时
	ui.frame_begin(hover_frame(60.0f, 35.0f, 0.3));
	EXPECT_FALSE(ui.tooltip_visible(button)) << "延时前不应显示";

	//再悬停 0.3 秒（累计 0.6）：达到显示条件
	ui.frame_begin(hover_frame(60.0f, 35.0f, 0.3));
	EXPECT_TRUE(ui.tooltip_visible(button)) << "延时后应可显示";

	//移出：计时归零
	ui.frame_begin(away_frame());
	EXPECT_FALSE(ui.tooltip_visible(button));
}
