/*
渲染 UI 图标按钮测试（第九百五十八轮）
覆盖：图标纹理存取、无图标基线、有图标纹理记录 +1。
*/
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//测试字体
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

	//测试字体是否存在
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path(FONT_PATH), code);
	}

	//构造隐藏窗口配置
	Renderer_Config hidden_config(uint32_t width, uint32_t height)
	{
		Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}
}

//渲染 UI 图标按钮测试夹具
class Renderer_Ui_Icon_Button_Test : public ::testing::Test
{
};

//图标存取：默认 0、写入读回、未知句柄
TEST_F(Renderer_Ui_Icon_Button_Test, 图标纹理存取)
{
	Ui_Context ui;
	const Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 30.0f }, "确定");
	ASSERT_NE(button, 0u);
	EXPECT_EQ(ui.button_icon_get(button), 0u);

	ASSERT_TRUE(ui.button_icon_set(button, 12345u));
	EXPECT_EQ(ui.button_icon_get(button), 12345u);
	ASSERT_TRUE(ui.button_icon_set(button, 0u));
	EXPECT_EQ(ui.button_icon_get(button), 0u);

	//未知句柄
	EXPECT_FALSE(ui.button_icon_set(button + 100, 7u));
	EXPECT_EQ(ui.button_icon_get(button + 100), 0u);
}

//绘制：设置图标后纹理记录 +1，清除后恢复
TEST_F(Renderer_Ui_Icon_Button_Test, 图标绘制记录)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle button = ui->button_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 30.0f }, "确定");
	ASSERT_NE(button, 0u);

	ASSERT_TRUE(renderer.frame_render());
	const uint32_t base_textured = renderer.ui().ui_render_stats().texture_draws;

	//设置图标：多一条纹理记录
	ASSERT_TRUE(ui->button_icon_set(button, 12345u));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().texture_draws, base_textured + 1u);

	//清除图标：恢复
	ASSERT_TRUE(ui->button_icon_set(button, 0u));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().texture_draws, base_textured);
}
