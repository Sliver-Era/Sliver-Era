/*
渲染 UI 流式主轴分布测试（第九百五十一轮）
覆盖：分布存取与默认值、Start / Center / End / Space_Between 四档行内位置。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造零输入帧（窗口宽度可变，用于触发布局重算）
	Input_Frame layout_frame(uint32_t window_width, uint32_t window_height)
	{
		Input_Frame input{};
		input.window_width = window_width;
		input.window_height = window_height;
		return input;
	}
}

//渲染 UI 流式主轴分布测试夹具
class Renderer_Ui_Flow_Justify_Test : public ::testing::Test
{
};

//分布存取：默认 Start、四档读写、未知句柄
TEST_F(Renderer_Ui_Flow_Justify_Test, 分布存取)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	EXPECT_EQ(ui.flow_justify_get(panel), Ui_Justify::Start);

	ASSERT_TRUE(ui.flow_justify_set(panel, Ui_Justify::Center));
	EXPECT_EQ(ui.flow_justify_get(panel), Ui_Justify::Center);
	ASSERT_TRUE(ui.flow_justify_set(panel, Ui_Justify::End));
	EXPECT_EQ(ui.flow_justify_get(panel), Ui_Justify::End);
	ASSERT_TRUE(ui.flow_justify_set(panel, Ui_Justify::Space_Between));
	EXPECT_EQ(ui.flow_justify_get(panel), Ui_Justify::Space_Between);

	//未知句柄
	EXPECT_FALSE(ui.flow_justify_set(panel + 100, Ui_Justify::Center));
	EXPECT_EQ(ui.flow_justify_get(panel + 100), Ui_Justify::Start);
}

//四档分布：父宽 100、三个 20 宽子项，剩余 40
TEST_F(Renderer_Ui_Flow_Justify_Test, 四档行内位置)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 20.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 20.0f, 20.0f }, "B");
	const Ui_Handle third = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 20.0f, 20.0f }, "C");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_NE(third, 0u);

	Ui_Rect first_rect{};
	Ui_Rect second_rect{};
	Ui_Rect third_rect{};

	//Start：0 / 20 / 40
	ui.frame_begin(layout_frame(320, 180));
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 0.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 20.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 40.0f);

	//Center：20 / 40 / 60
	ASSERT_TRUE(ui.flow_justify_set(panel, Ui_Justify::Center));
	ui.frame_begin(layout_frame(321, 180));
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 20.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 40.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 60.0f);

	//End：40 / 60 / 80
	ASSERT_TRUE(ui.flow_justify_set(panel, Ui_Justify::End));
	ui.frame_begin(layout_frame(322, 180));
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 40.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 60.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 80.0f);

	//Space_Between：0 / 40 / 80
	ASSERT_TRUE(ui.flow_justify_set(panel, Ui_Justify::Space_Between));
	ui.frame_begin(layout_frame(323, 180));
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 0.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 40.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 80.0f);
}
