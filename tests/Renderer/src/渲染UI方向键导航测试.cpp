/*
渲染 UI 方向键导航测试（第九百六十轮）
覆盖：点击建立焦点、右键取最近邻、上键无候选保持、上下几何相邻选择。
*/
#include <gtest/gtest.h>
#include <SDL3/SDL.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造零输入帧
	Input_Frame base_frame(uint32_t window_width, uint32_t window_height)
	{
		Input_Frame input{};
		input.window_width = window_width;
		input.window_height = window_height;
		return input;
	}

	//构造点击帧
	Input_Frame click_frame(float x, float y)
	{
		Input_Frame input = base_frame(320, 180);
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_pressed[0] = true;
		input.pointer.button_down[0] = true;
		return input;
	}

	//构造方向键帧
	Input_Frame key_frame(int scancode)
	{
		Input_Frame input = base_frame(320, 180);
		input.key_pressed.push_back(scancode);
		return input;
	}
}

//渲染 UI 方向键导航测试夹具
class Renderer_Ui_Arrow_Nav_Test : public ::testing::Test
{
};

//点击建焦、右移、无候选保持、上移取最近邻
TEST_F(Renderer_Ui_Arrow_Nav_Test, 四向导航)
{
	Ui_Context ui;
	const Ui_Handle left = ui.button_create(ui.root_get(), Ui_Rect{ 10.0f, 10.0f, 60.0f, 30.0f }, "A");
	const Ui_Handle right = ui.button_create(ui.root_get(), Ui_Rect{ 110.0f, 10.0f, 60.0f, 30.0f }, "B");
	const Ui_Handle down = ui.button_create(ui.root_get(), Ui_Rect{ 10.0f, 80.0f, 60.0f, 30.0f }, "C");
	ASSERT_NE(left, 0u);
	ASSERT_NE(right, 0u);
	ASSERT_NE(down, 0u);

	//点击左侧按钮建立焦点
	ui.frame_begin(click_frame(40.0f, 25.0f));
	EXPECT_TRUE(ui.focused_get(left));
	EXPECT_FALSE(ui.focused_get(right));

	//右方向：取右侧最近邻
	ui.frame_begin(key_frame(SDL_SCANCODE_RIGHT));
	EXPECT_TRUE(ui.focused_get(right));
	EXPECT_FALSE(ui.focused_get(left));

	//上方向：无候选，焦点保持
	ui.frame_begin(key_frame(SDL_SCANCODE_UP));
	EXPECT_TRUE(ui.focused_get(right));

	//点击下方按钮，再按上方向：应回到几何最近的左上按钮
	ui.frame_begin(click_frame(40.0f, 95.0f));
	EXPECT_TRUE(ui.focused_get(down));
	ui.frame_begin(key_frame(SDL_SCANCODE_UP));
	EXPECT_TRUE(ui.focused_get(left));
	EXPECT_FALSE(ui.focused_get(right));
}
