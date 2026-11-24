/*
渲染 UI 流式换行测试（第九百四十九轮）
覆盖：换行开关存取、横向排满另起一行、纵向排满另起一列。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造零输入帧（用于触发布局重算）
	Input_Frame layout_frame(uint32_t window_width, uint32_t window_height)
	{
		Input_Frame input{};
		input.window_width = window_width;
		input.window_height = window_height;
		return input;
	}
}

//渲染 UI 流式换行测试夹具
class Renderer_Ui_Flow_Wrap_Test : public ::testing::Test
{
};

//换行开关：默认关闭、读写、未知句柄
TEST_F(Renderer_Ui_Flow_Wrap_Test, 换行开关存取)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	EXPECT_FALSE(ui.flow_wrap_get(panel));

	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	EXPECT_TRUE(ui.flow_wrap_get(panel));
	ASSERT_TRUE(ui.flow_wrap_set(panel, false));
	EXPECT_FALSE(ui.flow_wrap_get(panel));

	//未知句柄
	EXPECT_FALSE(ui.flow_wrap_set(panel + 100, true));
	EXPECT_FALSE(ui.flow_wrap_get(panel + 100));
}

//横向换行：父宽 100、子项 40，第三个另起一行
TEST_F(Renderer_Ui_Flow_Wrap_Test, 横向排满另起一行)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "B");
	const Ui_Handle third = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "C");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_NE(third, 0u);

	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect first_rect{};
	Ui_Rect second_rect{};
	Ui_Rect third_rect{};
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 0.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 40.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 0.0f) << "第三个应另起一行";
	EXPECT_FLOAT_EQ(third_rect.y, 20.0f) << "新行按首行高度下移";
}

//纵向换行：父高 100、子项高 40，第三个另起一列
TEST_F(Renderer_Ui_Flow_Wrap_Test, 纵向排满另起一列)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 60.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Vertical, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 30.0f, 40.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 30.0f, 40.0f }, "B");
	const Ui_Handle third = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 30.0f, 40.0f }, "C");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_NE(third, 0u);

	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect first_rect{};
	Ui_Rect second_rect{};
	Ui_Rect third_rect{};
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.y, 0.0f);
	EXPECT_FLOAT_EQ(second_rect.y, 40.0f);
	EXPECT_FLOAT_EQ(third_rect.y, 0.0f) << "第三个应另起一列";
	EXPECT_FLOAT_EQ(third_rect.x, 30.0f) << "新列按首列宽度右移";
}
