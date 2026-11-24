/*
渲染 UI 列表控件测试（第九百八十一轮）
覆盖：创建与选项存取、下标夹紧、点选行更新当前项、未知句柄与空选项。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造按下帧
	Input_Frame press_frame(float x, float y)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_pressed[0] = true;
		input.pointer.button_down[0] = true;
		return input;
	}
}

//渲染 UI 列表控件测试夹具
class Renderer_Ui_List_Box_Test : public ::testing::Test
{
};

//创建与选项存取
TEST_F(Renderer_Ui_List_Box_Test, 创建与选项存取)
{
	Ui_Context ui;
	//空选项创建失败
	EXPECT_EQ(ui.list_box_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 90.0f }, {}), 0u);

	const Ui_Handle list = ui.list_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 90.0f }, { "甲", "乙", "丙" }, 1);
	ASSERT_NE(list, 0u);
	EXPECT_EQ(ui.list_box_index_get(list), 1u);

	//越界夹紧到末项
	ASSERT_TRUE(ui.list_box_index_set(list, 99));
	EXPECT_EQ(ui.list_box_index_get(list), 2u);
	ASSERT_TRUE(ui.list_box_index_set(list, 0));
	EXPECT_EQ(ui.list_box_index_get(list), 0u);

	//未知句柄
	EXPECT_FALSE(ui.list_box_index_set(list + 100, 1));
	EXPECT_EQ(ui.list_box_index_get(list + 100), 0u);
}

//点选行更新当前项（三行等分，行高 30）
TEST_F(Renderer_Ui_List_Box_Test, 点选行)
{
	Ui_Context ui;
	const Ui_Handle list = ui.list_box_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 90.0f }, { "甲", "乙", "丙" });
	ASSERT_NE(list, 0u);
	ASSERT_EQ(ui.list_box_index_get(list), 0u);

	//点第三行：y 落在 80..110
	ui.frame_begin(press_frame(30.0f, 95.0f));
	EXPECT_EQ(ui.list_box_index_get(list), 2u);

	//点第一行：y 落在 20..50
	ui.frame_begin(press_frame(30.0f, 30.0f));
	EXPECT_EQ(ui.list_box_index_get(list), 0u);
}
