/*
渲染 UI 控件查询测试（第九百二十九轮）
覆盖：按系统输入对象名查找、直接子节点计数与按创建顺序枚举。
纯数据用例直接构造 Ui_Context，不依赖 GPU。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造空颜色（不参与绘制断言）
	Ui_Color no_color(void)
	{
		return Ui_Color{ 0.0f, 0.0f, 0.0f, 1.0f };
	}
}

//渲染 UI 控件查询测试夹具
class Renderer_Ui_Query_Test : public ::testing::Test
{
};

//按输入对象名查找：命中 / 缺失 / 空名 / 销毁后失效
TEST_F(Renderer_Ui_Query_Test, 按输入名查找)
{
	Ui_Context ui;
	const Ui_Handle play = ui.button_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 60.0f, 24.0f }, "play");
	const Ui_Handle stop = ui.button_create(ui.root_get(), Ui_Rect{ 0.0f, 30.0f, 60.0f, 24.0f }, "stop");
	ASSERT_NE(play, 0u);
	ASSERT_NE(stop, 0u);
	ASSERT_TRUE(ui.input_name_set(play, "play"));
	ASSERT_TRUE(ui.input_name_set(stop, "stop"));

	//命中
	EXPECT_EQ(ui.node_find_by_input_name("play"), play);
	EXPECT_EQ(ui.node_find_by_input_name("stop"), stop);
	//缺失与空名
	EXPECT_EQ(ui.node_find_by_input_name("missing"), 0u);
	EXPECT_EQ(ui.node_find_by_input_name(""), 0u);
	//销毁后不再命中
	ASSERT_TRUE(ui.destroy(stop));
	EXPECT_EQ(ui.node_find_by_input_name("stop"), 0u);
}

//同名控件取首个创建者
TEST_F(Renderer_Ui_Query_Test, 重名取首个)
{
	Ui_Context ui;
	const Ui_Handle first = ui.button_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 60.0f, 24.0f }, "a");
	const Ui_Handle second = ui.button_create(ui.root_get(), Ui_Rect{ 0.0f, 30.0f, 60.0f, 24.0f }, "b");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_TRUE(ui.input_name_set(first, "shared"));
	ASSERT_TRUE(ui.input_name_set(second, "shared"));
	EXPECT_EQ(ui.node_find_by_input_name("shared"), first) << "应取创建顺序中的首个匹配";
}

//子节点枚举：只计直接子节点，销毁后顺序收紧
TEST_F(Renderer_Ui_Query_Test, 子节点计数与顺序)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 200.0f, 200.0f }, no_color());
	ASSERT_NE(panel, 0u);
	const Ui_Handle first = ui.button_create(panel, Ui_Rect{ 10.0f, 10.0f, 60.0f, 24.0f }, "1");
	const Ui_Handle second = ui.button_create(panel, Ui_Rect{ 10.0f, 40.0f, 60.0f, 24.0f }, "2");
	const Ui_Handle third = ui.button_create(panel, Ui_Rect{ 10.0f, 70.0f, 60.0f, 24.0f }, "3");
	//孙节点不应计入
	const Ui_Handle grandchild = ui.label_create(first, Ui_Rect{ 0.0f, 0.0f, 20.0f, 10.0f }, "gc");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_NE(third, 0u);
	ASSERT_NE(grandchild, 0u);

	EXPECT_EQ(ui.child_count(panel), 3u);
	EXPECT_EQ(ui.child_count(first), 1u);
	EXPECT_EQ(ui.child_at(panel, 0), first);
	EXPECT_EQ(ui.child_at(panel, 1), second);
	EXPECT_EQ(ui.child_at(panel, 2), third);
	//越界与无效父节点
	EXPECT_EQ(ui.child_at(panel, 3), 0u);
	EXPECT_EQ(ui.child_count(panel + 100), 0u);
	EXPECT_EQ(ui.child_at(panel + 100, 0), 0u);

	//销毁中间项后顺序收紧
	ASSERT_TRUE(ui.destroy(second));
	EXPECT_EQ(ui.child_count(panel), 2u);
	EXPECT_EQ(ui.child_at(panel, 1), third);
}
