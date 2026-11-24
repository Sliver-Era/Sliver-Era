/*
渲染 UI 文本块缓存键量化测试（第九百八十三轮）
覆盖：区域宽度亚像素变化仍命中缓存、跨 0.5 像素边界失效重建。
*/
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//测试字体
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

	//构造隐藏窗口配置
	Renderer_Config hidden_config(uint32_t width, uint32_t height)
	{
		Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}

	//测试字体是否存在
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path(FONT_PATH), code);
	}
}

//渲染 UI 文本块缓存键量化测试夹具
class Renderer_Ui_Cache_Key_Test : public ::testing::Test
{
};

//区域宽度亚像素抖动仍命中
TEST_F(Renderer_Ui_Cache_Key_Test, 宽度量化命中)
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

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.1f, 40.0f }, "AB", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 0.0f, Ui_Align::Start));

	//首帧：布局缓存未命中
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.ui().ui_render_stats().block_cache_misses, 1u);

	//宽度 100.1 → 100.2：量化后同为 100.0，应命中
	ASSERT_TRUE(ui->rect_set(label, Ui_Rect{ 20.0f, 20.0f, 100.2f, 40.0f }));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.ui().ui_render_stats().block_cache_hits, 1u) << "亚像素宽度变化应命中";
	EXPECT_EQ(renderer.ui().ui_render_stats().block_cache_misses, 0u);

	//宽度 101.5：跨量化边界，应重建
	ASSERT_TRUE(ui->rect_set(label, Ui_Rect{ 20.0f, 20.0f, 101.5f, 40.0f }));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.ui().ui_render_stats().block_cache_misses, 1u) << "跨量化边界应失效";
}
