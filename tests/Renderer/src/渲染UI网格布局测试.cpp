/*
渲染 UI 网格布局测试（第九百八十七轮）
覆盖：网格存取、两列行列均分与间距、子项矩形写入、关闭网格保持自身矩形。
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

//渲染 UI 网格布局测试夹具
class Renderer_Ui_Grid_Test : public ::testing::Test
{
};

//网格存取与四子项两列均分
TEST_F(Renderer_Ui_Grid_Test, 两列均分)
{
	Ui_Context ui;
	const Ui_Handle host = ui.panel_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 220.0f, 220.0f }, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(host, 0u);

	//默认不启用网格
	uint32_t columns = 9;
	float gap = 9.0f;
	ASSERT_TRUE(ui.grid_get(host, columns, gap));
	EXPECT_EQ(columns, 0u);
	EXPECT_FLOAT_EQ(gap, 0.0f);

	//两列、间距 10
	ASSERT_TRUE(ui.grid_set(host, 2, 10.0f));
	ASSERT_TRUE(ui.grid_get(host, columns, gap));
	EXPECT_EQ(columns, 2u);
	EXPECT_FLOAT_EQ(gap, 10.0f);

	Ui_Handle children[4]{};
	for (int index = 0; index < 4; ++index)
	{
		children[index] = ui.panel_create(host,
			Ui_Rect{ 0.0f, 0.0f, 10.0f, 10.0f }, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
		ASSERT_NE(children[index], 0u);
	}

	//驱动一帧布局：单元格 105 × 105，间距 10
	ui.frame_begin(idle_frame());
	struct Expected { float x; float y; };
	const Expected expected[4] = { {20.0f, 20.0f}, {135.0f, 20.0f}, {20.0f, 135.0f}, {135.0f, 135.0f} };
	for (int index = 0; index < 4; ++index)
	{
		Ui_Rect rect{};
		ASSERT_TRUE(ui.rect_get(children[index], rect));
		EXPECT_FLOAT_EQ(rect.x, expected[index].x) << "列位置";
		EXPECT_FLOAT_EQ(rect.y, expected[index].y) << "行位置";
		EXPECT_FLOAT_EQ(rect.w, 105.0f);
		EXPECT_FLOAT_EQ(rect.h, 105.0f);
	}

	//未知句柄
	EXPECT_FALSE(ui.grid_set(host + 100, 2, 0.0f));
	EXPECT_FALSE(ui.grid_get(host + 100, columns, gap));
}

//关闭网格：子项保持自身矩形
TEST_F(Renderer_Ui_Grid_Test, 关闭网格保持自身矩形)
{
	Ui_Context ui;
	const Ui_Handle host = ui.panel_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 220.0f, 220.0f }, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(host, 0u);
	const Ui_Handle child = ui.panel_create(host,
		Ui_Rect{ 30.0f, 40.0f, 12.0f, 14.0f }, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(child, 0u);

	ASSERT_TRUE(ui.grid_set(host, 0, 8.0f));
	ui.frame_begin(idle_frame());

	Ui_Rect rect{};
	ASSERT_TRUE(ui.rect_get(child, rect));
	EXPECT_FLOAT_EQ(rect.x, 30.0f);
	EXPECT_FLOAT_EQ(rect.y, 40.0f);
	EXPECT_FLOAT_EQ(rect.w, 12.0f);
	EXPECT_FLOAT_EQ(rect.h, 14.0f);
}
