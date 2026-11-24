/*
渲染 UI 流式间距测试（第九百五十二轮）
覆盖：行内间距与换行行间距（复用容器 flow_gap）。
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

//渲染 UI 流式间距测试夹具
class Renderer_Ui_Flow_Gap_Test : public ::testing::Test
{
};

//间距 0：行内紧贴、换行按行高
TEST_F(Renderer_Ui_Flow_Gap_Test, 无间距基线)
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
	Ui_Rect second_rect{};
	Ui_Rect third_rect{};
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	EXPECT_FLOAT_EQ(second_rect.x, 40.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 0.0f);
	EXPECT_FLOAT_EQ(third_rect.y, 20.0f);
}

//间距 5：行内间隔 5、换行行间同样 5
TEST_F(Renderer_Ui_Flow_Gap_Test, 行内与行间间距)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 5.0f, 0.0f));
	ASSERT_TRUE(ui.flow_wrap_set(panel, true));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "B");
	const Ui_Handle third = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "C");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_NE(third, 0u);

	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect second_rect{};
	Ui_Rect third_rect{};
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	ASSERT_TRUE(ui.rect_get(third, third_rect));
	//40 + 5 = 45；第三个放不下另起一行
	EXPECT_FLOAT_EQ(second_rect.x, 45.0f);
	EXPECT_FLOAT_EQ(third_rect.x, 0.0f);
	EXPECT_FLOAT_EQ(third_rect.y, 25.0f) << "行间应追加间距";
}
