/*
渲染 UI 禁用态收口测试（第九百五十九轮）
覆盖：禁用控件不响应点击、禁用态与常态主题色可区分。
*/
#include <gtest/gtest.h>

#include <cmath>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造零输入帧
	Input_Frame frame_make(float x, float y)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.pointer.x = x;
		input.pointer.y = y;
		return input;
	}

	//完整点击（按下帧 ＋ 抬起帧）
	void click_at(Ui_Context& ui, float x, float y)
	{
		Input_Frame press = frame_make(x, y);
		press.pointer.button_pressed[0] = true;
		press.pointer.button_down[0] = true;
		ui.frame_begin(press);

		Input_Frame release = frame_make(x, y);
		release.pointer.button_released[0] = true;
		ui.frame_begin(release);
	}

	//颜色亮度（Rec.709）
	float luma_of(const Ui_Color& color)
	{
		return color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
	}
}

//渲染 UI 禁用态测试夹具
class Renderer_Ui_Disabled_Test : public ::testing::Test
{
};

//禁用控件不响应点击，恢复后正常点击
TEST_F(Renderer_Ui_Disabled_Test, 禁用不响应点击)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, "A");
	ASSERT_NE(button, 0u);
	ASSERT_TRUE(ui.enabled_set(button, false));

	//禁用态点击：不产生 clicked
	click_at(ui, 60.0f, 35.0f);
	EXPECT_FALSE(ui.clicked_consume(button));

	//恢复可用：点击产生一次 clicked
	ASSERT_TRUE(ui.enabled_set(button, true));
	click_at(ui, 60.0f, 35.0f);
	EXPECT_TRUE(ui.clicked_consume(button));
	EXPECT_FALSE(ui.clicked_consume(button)) << "单击只应消费一次";
}

//禁用态主题色与常态可区分
TEST_F(Renderer_Ui_Disabled_Test, 禁用态主题色可区分)
{
	Ui_Context ui;
	const Ui_Theme& theme = ui.theme_get();
	const Ui_Color normal = ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Normal);
	const Ui_Color disabled = ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Disabled);
	const float normal_luma = luma_of(normal);
	const float disabled_luma = luma_of(disabled);
	EXPECT_GT(std::abs(normal_luma - disabled_luma), 0.02f) << "禁用态应与常态可区分";
}
