/*
渲染 UI 混排字号对齐测试（第九百六十七轮）
覆盖：行内区间改字号时，水平对齐宽度按各段字体求和，右对齐不越出内容区。
像素用例按回读能力门控。
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
	constexpr uint32_t FONT_LARGE_HEIGHT = 48;
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

	//扫描区域内最右侧亮像素横坐标（无亮像素返回 -1）
	int rightmost_lit_col(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t x1,
		uint32_t y0, uint32_t y1, float threshold)
	{
		for (uint32_t x = x1; x > x0; --x)
		{
			for (uint32_t y = y0; y < y1; ++y)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x - 1, y, 320)) >= threshold)
					return static_cast<int>(x - 1);
			}
		}
		return -1;
	}
}

//渲染 UI 混排字号对齐测试夹具
class Renderer_Ui_Mixed_Align_Test : public ::testing::Test
{
};

//混排字号右对齐：行宽按区间字体求和，右边界贴内容区右沿
TEST_F(Renderer_Ui_Mixed_Align_Test, 混排字号右对齐宽度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//加载小 / 大两种字号
	uint64_t small_font = 0;
	uint64_t large_font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(small_font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ASSERT_TRUE(renderer.ui().font_load_file(large_font, FONT_PATH, FONT_LARGE_HEIGHT));
	ui->font_set(small_font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 60.0f }, "AA", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 0.0f, Ui_Align::Start));
	ASSERT_TRUE(ui->text_halign_set(label, Ui_Align::Start));

	//第二个字符改用大字号
	ASSERT_TRUE(ui->text_span_add(label, 1, 2, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }, large_font));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//左对齐：混排应有明显宽度（大字号已生效）
	const int start_right = rightmost_lit_col(pixels, 20, 260, 20, 80, 120.0f);
	ASSERT_GT(start_right, 40) << "混排两字应有可见墨迹";

	//右对齐：行宽按两段字体求和，右边界应贴近内容区右沿（220）而不越界
	ASSERT_TRUE(ui->text_halign_set(label, Ui_Align::End));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const int end_right = rightmost_lit_col(pixels, 20, 260, 20, 80, 120.0f);
	EXPECT_GE(end_right, 205) << "右对齐后墨迹应贴近右沿";
	EXPECT_LE(end_right, 224) << "混排行宽应含大字号段，不得越出内容区右沿";
}
