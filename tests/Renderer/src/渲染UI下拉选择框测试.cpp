/*
渲染 UI 下拉选择框测试（第九百六十五轮）
覆盖：创建与选项存取、下标夹紧、展开状态存取、点击展开收起、选项点选。
说明：按下沿处理展开与点选，本文件只发送按下帧。
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
		input.dt = 0.016;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_pressed[0] = true;
		return input;
	}

	//构造空帧（无按键）
	Input_Frame idle_frame(void)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dt = 0.016;
		return input;
	}
}

//渲染 UI 下拉选择框测试夹具
class Renderer_Ui_Combo_Test : public ::testing::Test
{
};

//创建与选项存取
TEST_F(Renderer_Ui_Combo_Test, 创建与选项存取)
{
	Ui_Context ui;
	//空选项列表创建失败
	EXPECT_EQ(ui.combo_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 24.0f }, {}), 0u);

	//三项选项，默认取首项
	const Ui_Handle combo = ui.combo_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, { "甲", "乙", "丙" });
	ASSERT_NE(combo, 0u);
	EXPECT_EQ(ui.combo_index_get(combo), 0u);
	EXPECT_FALSE(ui.combo_open_get(combo));

	//指定下标生效
	ASSERT_TRUE(ui.combo_index_set(combo, 2));
	EXPECT_EQ(ui.combo_index_get(combo), 2u);

	//越界下标夹紧到末项
	ASSERT_TRUE(ui.combo_index_set(combo, 99));
	EXPECT_EQ(ui.combo_index_get(combo), 2u);

	//展开状态存取
	ASSERT_TRUE(ui.combo_open_set(combo, true));
	EXPECT_TRUE(ui.combo_open_get(combo));
	ASSERT_TRUE(ui.combo_open_set(combo, false));
	EXPECT_FALSE(ui.combo_open_get(combo));

	//未知句柄
	EXPECT_FALSE(ui.combo_index_set(combo + 100, 1));
	EXPECT_EQ(ui.combo_index_get(combo + 100), 0u);
	EXPECT_FALSE(ui.combo_open_set(combo + 100, true));
	EXPECT_FALSE(ui.combo_open_get(combo + 100));

	//销毁后句柄失效
	ASSERT_TRUE(ui.destroy(combo));
	EXPECT_EQ(ui.combo_index_get(combo), 0u);
	EXPECT_FALSE(ui.combo_open_get(combo));
}

//点击控件切换展开状态
TEST_F(Renderer_Ui_Combo_Test, 点击展开收起)
{
	Ui_Context ui;
	const Ui_Handle combo = ui.combo_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, { "甲", "乙" });
	ASSERT_NE(combo, 0u);

	//点在控件内：展开
	ui.frame_begin(press_frame(50.0f, 30.0f));
	EXPECT_TRUE(ui.combo_open_get(combo)) << "首次按下应展开";

	//同位置再按下：收起
	ui.frame_begin(press_frame(50.0f, 30.0f));
	EXPECT_FALSE(ui.combo_open_get(combo)) << "再次按下应收起";
}

//展开时点选选项更新下标并收起
TEST_F(Renderer_Ui_Combo_Test, 展开点选选项)
{
	Ui_Context ui;
	const Ui_Handle combo = ui.combo_create(ui.root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, { "甲", "乙", "丙" });
	ASSERT_NE(combo, 0u);
	ASSERT_TRUE(ui.combo_open_set(combo, true));

	//选项列表紧接控件下方，每项高等于控件高；点第二项
	ui.frame_begin(press_frame(50.0f, 20.0f + 24.0f + 24.0f + 12.0f));
	EXPECT_EQ(ui.combo_index_get(combo), 1u);
	EXPECT_FALSE(ui.combo_open_get(combo)) << "点选后应收起";

	//点列表外空白：不改变下标与展开状态
	ASSERT_TRUE(ui.combo_open_set(combo, true));
	ui.frame_begin(press_frame(300.0f, 170.0f));
	EXPECT_EQ(ui.combo_index_get(combo), 1u);
	EXPECT_TRUE(ui.combo_open_get(combo));

	//空帧不产生变化
	ui.frame_begin(idle_frame());
	EXPECT_EQ(ui.combo_index_get(combo), 1u);
	EXPECT_TRUE(ui.combo_open_get(combo));
}
