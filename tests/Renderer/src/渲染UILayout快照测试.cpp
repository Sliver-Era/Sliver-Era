/*
渲染 UI 布局快照测试（第九百五十四轮）
覆盖：树序（父先于子、同层按创建顺序）、矩形与节点一致、未知句柄清空输出。
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

//渲染 UI 布局快照测试夹具
class Renderer_Ui_Layout_Snapshot_Test : public ::testing::Test
{
};

//树序与矩形一致性
TEST_F(Renderer_Ui_Layout_Snapshot_Test, 树序与矩形一致)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 10.0f, 20.0f, 200.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 4.0f, 2.0f));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "B");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ui.frame_begin(layout_frame(320, 180));

	std::vector<Ui_Layout_Item> items{};
	ASSERT_TRUE(ui.layout_snapshot(panel, items));
	ASSERT_EQ(items.size(), 3u);
	//父先于子、同层按创建顺序
	EXPECT_EQ(items[0].handle, panel);
	EXPECT_EQ(items[1].handle, first);
	EXPECT_EQ(items[2].handle, second);
	EXPECT_EQ(items[1].parent, panel);
	EXPECT_EQ(items[2].parent, panel);
	EXPECT_EQ(items[0].type, Ui_Node_Type::Panel);
	EXPECT_EQ(items[1].type, Ui_Node_Type::Label);

	//矩形与 rect_get 一致
	for (const Ui_Layout_Item& item : items)
	{
		Ui_Rect rect{};
		ASSERT_TRUE(ui.rect_get(item.handle, rect));
		EXPECT_FLOAT_EQ(item.rect.x, rect.x);
		EXPECT_FLOAT_EQ(item.rect.y, rect.y);
		EXPECT_FLOAT_EQ(item.rect.w, rect.w);
		EXPECT_FLOAT_EQ(item.rect.h, rect.h);
	}
}

//未知句柄：返回假并清空输出
TEST_F(Renderer_Ui_Layout_Snapshot_Test, 未知句柄清空输出)
{
	Ui_Context ui;
	std::vector<Ui_Layout_Item> items{ Ui_Layout_Item{} };
	EXPECT_FALSE(ui.layout_snapshot(9999, items));
	EXPECT_TRUE(items.empty());
}
