/*
渲染 UI 字号测试（第八百九十六轮）
覆盖：控件专用字体句柄存取与未知句柄、同一文本在大 / 小字号下的绘制像素差异。
像素用例按回读能力与字体可用性门控。
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

	//统计矩形区域内亮度超过阈值的像素数
	uint32_t bright_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0,
		uint32_t x1, uint32_t y1, uint32_t row_width, float threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1; ++y)
		{
			for (uint32_t x = x0; x < x1; ++x)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y, row_width)) >= threshold)
					++count;
			}
		}
		return count;
	}
}

//渲染 UI 字号测试夹具
class Renderer_Ui_Font_Size_Test : public ::testing::Test
{
};

//控件字体句柄：默认 0、可设置读回、未知句柄拒绝
TEST_F(Renderer_Ui_Font_Size_Test, 字体句柄存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 80.0f, 24.0f }, "文本");
	ASSERT_NE(label, 0u);
	//默认使用全局字体
	EXPECT_EQ(ui.text_font_get(label), 0u);
	ASSERT_TRUE(ui.text_font_set(label, 42u));
	EXPECT_EQ(ui.text_font_get(label), 42u);
	ASSERT_TRUE(ui.text_font_set(label, 0u));
	EXPECT_EQ(ui.text_font_get(label), 0u);
	//未知句柄
	EXPECT_FALSE(ui.text_font_set(label + 100, 42u));
	EXPECT_EQ(ui.text_font_get(label + 100), 0u);
}

//同一文本：大字号覆盖的亮像素多于小字号
TEST_F(Renderer_Ui_Font_Size_Test, 字号分级绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//加载两种字号
	uint64_t small_font = 0;
	uint64_t large_font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(small_font, FONT_PATH, 16));
	ASSERT_TRUE(renderer.ui().font_load_file(large_font, FONT_PATH, 28));
	ASSERT_NE(small_font, 0u);
	ASSERT_NE(large_font, 0u);
	ASSERT_NE(small_font, large_font);

	//同一文本分别使用两种字号
	const Ui_Handle small_label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 50.0f }, "HHHH");
	const Ui_Handle large_label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 90.0f, 200.0f, 50.0f }, "HHHH");
	ASSERT_NE(small_label, 0u);
	ASSERT_NE(large_label, 0u);
	ASSERT_TRUE(ui->text_font_set(small_label, small_font));
	ASSERT_TRUE(ui->text_font_set(large_label, large_font));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const uint32_t small_pixels = bright_count(pixels, 20, 20, 220, 70, 320, 120.0f);
	const uint32_t large_pixels = bright_count(pixels, 20, 90, 220, 140, 320, 120.0f);
	EXPECT_GT(small_pixels, 0u) << "小字号应绘制文本";
	EXPECT_GT(large_pixels, small_pixels) << "大字号应覆盖更多亮像素";
}
