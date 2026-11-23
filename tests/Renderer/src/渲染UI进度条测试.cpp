/*
渲染 UI 进度条测试（第九百一十五轮）
覆盖：取值 0 / 0.5 / 1 的绘制规模、越界钳制与居中标签。
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

//渲染 UI 进度条测试夹具
class Renderer_Ui_Progress_Test : public ::testing::Test
{
};

//取值 0：只有背景，不产生前景
TEST_F(Renderer_Ui_Progress_Test, 零值只有背景)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle progress = ui->progress_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, 0.0f);
	ASSERT_NE(progress, 0u);
	float value = 1.0f;
	ASSERT_TRUE(ui->value_get(progress, value));
	EXPECT_FLOAT_EQ(value, 0.0f);
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.draws, 1u) << "零值只绘制背景";
	EXPECT_EQ(stats.vertices, 4u);
}

//取值 0.5：背景 + 半宽前景
TEST_F(Renderer_Ui_Progress_Test, 半值前景与背景)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle progress = ui->progress_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, 0.5f);
	ASSERT_NE(progress, 0u);
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.draws, 2u) << "背景 + 前景";
	EXPECT_EQ(stats.vertices, 8u);
	EXPECT_EQ(stats.solid_draws, 2u);
}

//越界取值：负值按 0、超过 1 按 1；值 1 时前景与背景同宽
TEST_F(Renderer_Ui_Progress_Test, 越界钳制与满值)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle progress = ui->progress_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, 1.5f);
	ASSERT_NE(progress, 0u);
	float value = 0.0f;
	ASSERT_TRUE(ui->value_get(progress, value));
	EXPECT_FLOAT_EQ(value, 1.0f) << "超上界应夹紧到 1";

	//负值夹紧到 0：只剩背景
	ASSERT_TRUE(ui->value_set(progress, -0.5f));
	ASSERT_TRUE(ui->value_get(progress, value));
	EXPECT_FLOAT_EQ(value, 0.0f);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().draws, 1u);

	//满值：前景与背景同宽
	ASSERT_TRUE(ui->value_set(progress, 1.0f));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& full = renderer.ui().ui_render_stats();
	EXPECT_EQ(full.draws, 2u);
	EXPECT_EQ(full.vertices, 8u);
}

//标签：居中绘制且不影响前景规模
TEST_F(Renderer_Ui_Progress_Test, 居中标签)
{
	if (!std::filesystem::exists(std::filesystem::path("C:\\Windows\\Fonts\\consola.ttf")))
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16));
	ui->font_set(font);
	const Ui_Handle progress = ui->progress_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, 0.5f, "50%");
	ASSERT_NE(progress, 0u);
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 1u) << "标签应绘制一条文字记录";
	EXPECT_EQ(stats.draws, 3u) << "背景 + 前景 + 文字";
}

//像素：半值左半为强调色、右半为背景，满值右半也被覆盖
TEST_F(Renderer_Ui_Progress_Test, 前景像素覆盖)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//矩形横跨 20..220，垂直 20..44
	const Ui_Handle progress = ui->progress_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, 0.5f);
	ASSERT_NE(progress, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//左半（前景）：蓝通道占优且亮度高于背景（回读通道序为 BGRA）
	const std::array<uint8_t, 4> filled = byjy_render_test::pixel_get(pixels, 60, 32, 320);
	const std::array<uint8_t, 4> empty = byjy_render_test::pixel_get(pixels, 180, 32, 320);
	EXPECT_GT(static_cast<int>(filled[0]), static_cast<int>(filled[2])) << "前景应为蓝色强调色";
	EXPECT_GT(byjy_render_test::luma_of(filled), byjy_render_test::luma_of(empty) + 10.0f)
		<< "前景亮度应明显高于背景";

	//满值：右半也被前景覆盖
	ASSERT_TRUE(ui->value_set(progress, 1.0f));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const std::array<uint8_t, 4> full_right = byjy_render_test::pixel_get(pixels, 180, 32, 320);
	EXPECT_GT(static_cast<int>(full_right[0]), static_cast<int>(full_right[2])) << "满值右半应为前景色";
}
