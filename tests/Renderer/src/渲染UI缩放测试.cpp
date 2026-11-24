/*
渲染 UI 缩放测试（第九百七十一轮）
覆盖：缩放比例存取、根矩形归一、指针坐标归一后命中、非正比例按 1 处理。
说明：绘制侧按同一比例放大几何，本文件验证交互侧归一行为。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造带缩放与指针的帧
	Input_Frame scaled_frame(float dpi_scale, float x, float y, bool pressed)
	{
		Input_Frame input{};
		input.window_width = 320;
		input.window_height = 180;
		input.dpi_scale = dpi_scale;
		input.pointer.x = x;
		input.pointer.y = y;
		input.pointer.button_pressed[0] = pressed;
		input.pointer.button_down[0] = pressed;
		return input;
	}
}

//渲染 UI 缩放测试夹具
class Renderer_Ui_Scale_Test : public ::testing::Test
{
};

//缩放比例与根矩形归一
TEST_F(Renderer_Ui_Scale_Test, 缩放比例与根尺寸)
{
	Ui_Context ui;
	EXPECT_FLOAT_EQ(ui.ui_scale_get(), 1.0f) << "默认比例为 1";
	EXPECT_FALSE(ui.ui_scale_auto_get()) << "默认不启用自动缩放";

	//未开启自动缩放：帧内比例被忽略
	ui.frame_begin(scaled_frame(2.0f, 0.0f, 0.0f, false));
	EXPECT_FLOAT_EQ(ui.ui_scale_get(), 1.0f) << "未开启时保持既有像素口径";

	//开启自动缩放：2 倍缩放使根矩形按逻辑像素折半
	ASSERT_TRUE(ui.ui_scale_auto_set(true));
	ui.frame_begin(scaled_frame(2.0f, 0.0f, 0.0f, false));
	EXPECT_FLOAT_EQ(ui.ui_scale_get(), 2.0f);
	Ui_Rect root{};
	ASSERT_TRUE(ui.rect_get(ui.root_get(), root));
	EXPECT_FLOAT_EQ(root.w, 160.0f);
	EXPECT_FLOAT_EQ(root.h, 90.0f);

	//非正比例按 1 处理
	ui.frame_begin(scaled_frame(0.0f, 0.0f, 0.0f, false));
	EXPECT_FLOAT_EQ(ui.ui_scale_get(), 1.0f);
	ASSERT_TRUE(ui.rect_get(ui.root_get(), root));
	EXPECT_FLOAT_EQ(root.w, 320.0f);
	EXPECT_FLOAT_EQ(root.h, 180.0f);

	//0.5 倍缩放：根矩形放大一倍
	ui.frame_begin(scaled_frame(0.5f, 0.0f, 0.0f, false));
	EXPECT_FLOAT_EQ(ui.ui_scale_get(), 0.5f);
	ASSERT_TRUE(ui.rect_get(ui.root_get(), root));
	EXPECT_FLOAT_EQ(root.w, 640.0f);
	EXPECT_FLOAT_EQ(root.h, 360.0f);

	//关闭自动缩放：立即回到 1
	ASSERT_TRUE(ui.ui_scale_auto_set(false));
	EXPECT_FLOAT_EQ(ui.ui_scale_get(), 1.0f);
}

//指针坐标按比例归一后命中
TEST_F(Renderer_Ui_Scale_Test, 指针归一命中)
{
	Ui_Context ui;
	ASSERT_TRUE(ui.ui_scale_auto_set(true));
	//逻辑矩形 (80,40,80,30)：0.5 倍缩放下物理坐标约为 (40,20,40,15)
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 80.0f, 40.0f, 80.0f, 30.0f }, "A");
	ASSERT_NE(button, 0u);

	//物理点 (50,30) 归一为逻辑 (100,60)，落在按钮内
	ui.frame_begin(scaled_frame(0.5f, 50.0f, 30.0f, true));
	EXPECT_TRUE(ui.focused_get(button)) << "指针应按缩放归一后命中";

	//抬起到同一物理点：清除按下态
	ui.frame_begin(scaled_frame(0.5f, 50.0f, 30.0f, false));
	EXPECT_TRUE(ui.focused_get(button));
}
