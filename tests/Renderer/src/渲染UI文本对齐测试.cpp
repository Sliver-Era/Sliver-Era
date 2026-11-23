/*
渲染 UI 文本水平对齐测试（第九百二十一轮）
覆盖：Start / Center / End 三档的像素分布与未知句柄行为。
*/
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//测试字体
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

//渲染 UI 文本水平对齐测试夹具
class Renderer_Ui_Text_Align_Test : public ::testing::Test
{
};

//Start / End：文字应分别贴近矩形左 / 右侧
TEST_F(Renderer_Ui_Text_Align_Test, 左右对齐像素分布)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, 16));
	ui->font_set(font);
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 40.0f, 200.0f, 18.0f }, "MM", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	EXPECT_EQ(ui->text_halign_get(label), Ui_Align::Start);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//左对齐：左侧区域亮于右侧区域
	const float start_left = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 28, 48, 320));
	const float start_right = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 200, 48, 320));
	EXPECT_GT(start_left, start_right + 5.0f) << "左对齐文字应位于矩形左侧";

	//右对齐：右侧区域亮于左侧区域
	ASSERT_TRUE(ui->text_halign_set(label, Ui_Align::End));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const float end_left = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 28, 48, 320));
	const float end_right = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 210, 48, 320));
	EXPECT_GT(end_right, end_left + 5.0f) << "右对齐文字应位于矩形右侧";
}

//居中：中部亮、两端暗；未知句柄读写行为
TEST_F(Renderer_Ui_Text_Align_Test, 居中与未知句柄)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, 16));
	ui->font_set(font);
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 40.0f, 200.0f, 18.0f }, "MM", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_halign_set(label, Ui_Align::Center));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const float center = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 116, 48, 320));
	const float left = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 28, 48, 320));
	const float right = byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, 200, 48, 320));
	EXPECT_GT(center, left + 5.0f) << "居中文字应位于中部";
	EXPECT_GT(center, right + 5.0f);

	//未知句柄
	EXPECT_FALSE(ui->text_halign_set(label + 100, Ui_Align::End));
	EXPECT_EQ(ui->text_halign_get(label + 100), Ui_Align::Start);
}
