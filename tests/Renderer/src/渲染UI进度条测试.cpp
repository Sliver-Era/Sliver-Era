/*
渲染 UI 进度条测试（第九百一十五轮）
覆盖：取值 0 / 0.5 / 1 的绘制规模、越界钳制与居中标签。
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
