/*
渲染 UI 弹性权重测试（第九百四十八轮）
覆盖：权重存取与负值归零、按 1:2 分配剩余主轴、单项独占剩余空间。
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

//渲染 UI 弹性权重测试夹具
class Renderer_Ui_Stretch_Test : public ::testing::Test
{
};

//权重存取：默认 0、负值归零、未知句柄
TEST_F(Renderer_Ui_Stretch_Test, 弹性权重存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "A");
	ASSERT_NE(label, 0u);
	EXPECT_FLOAT_EQ(ui.stretch_get(label), 0.0f);

	ASSERT_TRUE(ui.stretch_set(label, 1.5f));
	EXPECT_FLOAT_EQ(ui.stretch_get(label), 1.5f);
	ASSERT_TRUE(ui.stretch_set(label, -2.0f));
	EXPECT_FLOAT_EQ(ui.stretch_get(label), 0.0f);

	//未知句柄
	EXPECT_FALSE(ui.stretch_set(label + 100, 1.0f));
	EXPECT_FLOAT_EQ(ui.stretch_get(label + 100), 0.0f);
}

//分配：两个弹性子项按 1:2 分完父宽
TEST_F(Renderer_Ui_Stretch_Test, 按权重分配剩余主轴)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 300.0f, 40.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "B");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_TRUE(ui.stretch_set(first, 1.0f));
	ASSERT_TRUE(ui.stretch_set(second, 2.0f));

	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect first_rect{};
	Ui_Rect second_rect{};
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	//基准 60 + 60，剩余 180 按 1:2 分配
	EXPECT_FLOAT_EQ(first_rect.w, 120.0f);
	EXPECT_FLOAT_EQ(second_rect.w, 180.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 120.0f);
}

//单项弹性：独占全部剩余空间
TEST_F(Renderer_Ui_Stretch_Test, 单项独占剩余)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 300.0f, 40.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "B");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_TRUE(ui.stretch_set(first, 1.0f));

	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect first_rect{};
	Ui_Rect second_rect{};
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	EXPECT_FLOAT_EQ(first_rect.w, 240.0f) << "单项应独占剩余空间";
	EXPECT_FLOAT_EQ(second_rect.w, 60.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 240.0f);
}
