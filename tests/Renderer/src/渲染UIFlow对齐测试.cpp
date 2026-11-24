/*
渲染 UI 流式交叉轴对齐测试（第九百五十轮）
覆盖：换行后行内子项按 Start / Center / End 对齐、Stretch 拉满行高。
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

//渲染 UI 流式交叉轴对齐测试夹具
class Renderer_Ui_Flow_Align_Test : public ::testing::Test
{
};

//Start：行内子项贴行顶
TEST_F(Renderer_Ui_Flow_Align_Test, 起点对齐)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	const Ui_Handle short_first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "A");
	const Ui_Handle tall = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 40.0f }, "B");
	const Ui_Handle short_third = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "C");
	ASSERT_NE(short_first, 0u);
	ASSERT_NE(tall, 0u);
	ASSERT_NE(short_third, 0u);

	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect first_rect{};
	Ui_Rect tall_rect{};
	Ui_Rect third_rect{};
	ASSERT_TRUE(ui.rect_get(short_first, first_rect));
	ASSERT_TRUE(ui.rect_get(tall, tall_rect));
	ASSERT_TRUE(ui.rect_get(short_third, third_rect));
	EXPECT_FLOAT_EQ(first_rect.y, 0.0f);
	EXPECT_FLOAT_EQ(tall_rect.y, 0.0f);
	EXPECT_FLOAT_EQ(third_rect.y, 40.0f) << "换行后按首行最高子项下移";
}

//Center：行内矮子项在行高中居中
TEST_F(Renderer_Ui_Flow_Align_Test, 居中对齐)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	ASSERT_TRUE(ui.flow_align_set(panel, Ui_Align::Center));
	const Ui_Handle short_first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "A");
	const Ui_Handle tall = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 40.0f }, "B");
	ASSERT_NE(short_first, 0u);
	ASSERT_NE(tall, 0u);

	ui.frame_begin(layout_frame(321, 180));
	Ui_Rect first_rect{};
	Ui_Rect tall_rect{};
	ASSERT_TRUE(ui.rect_get(short_first, first_rect));
	ASSERT_TRUE(ui.rect_get(tall, tall_rect));
	EXPECT_FLOAT_EQ(first_rect.y, 10.0f) << "矮子项应在 40 高行内居中";
	EXPECT_FLOAT_EQ(tall_rect.y, 0.0f);
}

//End：行内矮子项贴行底
TEST_F(Renderer_Ui_Flow_Align_Test, 末端对齐)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	ASSERT_TRUE(ui.flow_align_set(panel, Ui_Align::End));
	const Ui_Handle short_first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "A");
	const Ui_Handle tall = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 40.0f }, "B");
	ASSERT_NE(short_first, 0u);
	ASSERT_NE(tall, 0u);

	ui.frame_begin(layout_frame(322, 180));
	Ui_Rect first_rect{};
	Ui_Rect tall_rect{};
	ASSERT_TRUE(ui.rect_get(short_first, first_rect));
	ASSERT_TRUE(ui.rect_get(tall, tall_rect));
	EXPECT_FLOAT_EQ(first_rect.y, 20.0f) << "矮子项应贴行底";
	EXPECT_FLOAT_EQ(tall_rect.y, 0.0f);
}

//Stretch：行内子项拉满行高
TEST_F(Renderer_Ui_Flow_Align_Test, 拉伸对齐)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	ASSERT_TRUE(ui.flow_align_set(panel, Ui_Align::Stretch));
	const Ui_Handle short_first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "A");
	const Ui_Handle tall = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 40.0f }, "B");
	ASSERT_NE(short_first, 0u);
	ASSERT_NE(tall, 0u);

	ui.frame_begin(layout_frame(323, 180));
	Ui_Rect first_rect{};
	Ui_Rect tall_rect{};
	ASSERT_TRUE(ui.rect_get(short_first, first_rect));
	ASSERT_TRUE(ui.rect_get(tall, tall_rect));
	EXPECT_FLOAT_EQ(first_rect.h, 40.0f) << "矮子项应被拉伸到行高";
	EXPECT_FLOAT_EQ(tall_rect.h, 40.0f);
}
