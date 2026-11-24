/*
渲染 UI 内容自适应尺寸测试（第九百八十八轮）
覆盖：自适应开关存取、按子项包围盒撑开（含内边距）、嵌套时子先于父。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造空帧
	Input_Frame idle_frame(void)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		return input;
	}
}

//渲染 UI 内容自适应尺寸测试夹具
class Renderer_Ui_Fit_Test : public ::testing::Test
{
};

//按子项包围盒撑开并计入内边距
TEST_F(Renderer_Ui_Fit_Test, 单层撑开)
{
	Ui_Context ui;
	const Ui_Handle host = ui.panel_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 50.0f, 50.0f }, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(host, 0u);
	EXPECT_FALSE(ui.fit_children_get(host));
	ASSERT_TRUE(ui.fit_children_set(host, true));
	EXPECT_TRUE(ui.fit_children_get(host));
	ASSERT_TRUE(ui.padding_set(host, 4.0f));

	//子项 100 × 40：容器宽撑到 104、高保持 50
	ASSERT_NE(ui.panel_create(host,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 40.0f }, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }), 0u);
	ui.frame_begin(idle_frame());

	Ui_Rect rect{};
	ASSERT_TRUE(ui.rect_get(host, rect));
	EXPECT_FLOAT_EQ(rect.w, 104.0f);
	EXPECT_FLOAT_EQ(rect.h, 50.0f);

	//未知句柄
	EXPECT_FALSE(ui.fit_children_set(host + 100, true));
	EXPECT_FALSE(ui.fit_children_get(host + 100));
}

//嵌套自适应：内层先撑开，外层再按内层撑开
TEST_F(Renderer_Ui_Fit_Test, 嵌套撑开)
{
	Ui_Context ui;
	const Ui_Handle outer = ui.panel_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 10.0f, 10.0f }, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(outer, 0u);
	ASSERT_TRUE(ui.fit_children_set(outer, true));
	const Ui_Handle inner = ui.panel_create(outer,
		Ui_Rect{ 20.0f, 20.0f, 10.0f, 10.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	ASSERT_NE(inner, 0u);
	ASSERT_TRUE(ui.fit_children_set(inner, true));
	//最内层子项 80 × 30
	ASSERT_NE(ui.panel_create(inner,
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }), 0u);

	ui.frame_begin(idle_frame());

	Ui_Rect inner_rect{};
	Ui_Rect outer_rect{};
	ASSERT_TRUE(ui.rect_get(inner, inner_rect));
	ASSERT_TRUE(ui.rect_get(outer, outer_rect));
	EXPECT_FLOAT_EQ(inner_rect.w, 80.0f) << "内层按最内子项撑开";
	EXPECT_FLOAT_EQ(inner_rect.h, 30.0f);
	EXPECT_FLOAT_EQ(outer_rect.w, 80.0f) << "外层按内层撑开";
	EXPECT_FLOAT_EQ(outer_rect.h, 30.0f);
}
