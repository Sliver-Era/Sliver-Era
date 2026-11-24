/*
渲染 UI 布局脏标记测试（第九百五十三轮）
覆盖：稳定帧不重建、布局属性修改自动置脏、显式置脏入口。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造零输入帧
	Input_Frame layout_frame(uint32_t window_width, uint32_t window_height)
	{
		Input_Frame input{};
		input.window_width = window_width;
		input.window_height = window_height;
		return input;
	}
}

//渲染 UI 布局脏标记测试夹具
class Renderer_Ui_Layout_Dirty_Test : public ::testing::Test
{
};

//稳定帧不重建、属性修改与显式标记各触发一次重建
TEST_F(Renderer_Ui_Layout_Dirty_Test, 重建计数口径)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 200.0f, 100.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	const Ui_Handle label = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 80.0f, 20.0f }, "A");
	ASSERT_NE(panel, 0u);
	ASSERT_NE(label, 0u);

	//首帧：创建节点后必重建一次
	const Input_Frame frame = layout_frame(320, 180);
	ui.frame_begin(frame);
	const uint64_t after_first = ui.layout_apply_count();
	EXPECT_EQ(after_first, 1u);

	//同输入再跑一帧：不重建
	ui.frame_begin(frame);
	EXPECT_EQ(ui.layout_apply_count(), after_first);

	//修改内边距：自动置脏，下一帧重建
	ASSERT_TRUE(ui.padding_set(label, 4.0f));
	ui.frame_begin(frame);
	EXPECT_EQ(ui.layout_apply_count(), after_first + 1u);

	//显式置脏：下一帧再重建
	ui.layout_dirty_mark();
	ui.frame_begin(frame);
	EXPECT_EQ(ui.layout_apply_count(), after_first + 2u);
}
