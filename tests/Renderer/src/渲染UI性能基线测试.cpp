/*
渲染 UI 性能基线测试（第九百零五轮）
覆盖：文本密集界面的整树重建均值、稳定帧均值与缓存命中计数。
基线只做宽松上限守卫，数值打印到控制台供进度文档记录。
*/
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>

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

	/*
	构造文本密集界面
	圆角面板 + 32 个自动换行标签（固定文本），返回面板句柄供逐帧改色使指纹失效。
	*/
	Ui_Handle build_text_dense_ui(Ui_Context& ui)
	{
		const Ui_Handle panel = ui.panel_create(ui.root_get(),
			Ui_Rect{ 4.0f, 4.0f, 312.0f, 172.0f }, Ui_Color{ 0.12f, 0.12f, 0.16f, 1.0f });
		ui.corner_radius_set(panel, 6.0f);
		for (uint32_t index = 0; index < 32; ++index)
		{
			const float y = 8.0f + static_cast<float>(index % 8) * 20.0f;
			const float x = 8.0f + static_cast<float>(index / 8) * 76.0f;
			const Ui_Handle label = ui.label_create(panel,
				Ui_Rect{ x, y, 70.0f, 18.0f }, "ABC DEFGH 0123");
			ui.text_layout_set(label, true, 0.0f, Ui_Align::Start);
		}
		return panel;
	}
}

//渲染 UI 性能基线测试夹具
class Renderer_Ui_Perf_Test : public ::testing::Test
{
};

//整树重建基线：每帧改色使几何缓存失效，文本块布局缓存仍应命中
TEST_F(Renderer_Ui_Perf_Test, 文本密集整树重建基线)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	const Ui_Handle panel = build_text_dense_ui(*ui);
	ASSERT_NE(panel, 0u);

	//预热一帧（烘焙字形并填充两级缓存）
	ASSERT_TRUE(renderer.frame_render());

	const int iterations = 100;
	const auto start = std::chrono::steady_clock::now();
	for (int index = 0; index < iterations; ++index)
	{
		//逐帧改色，绕开稳定帧几何缓存
		ASSERT_TRUE(ui->color_set(panel,
			Ui_Color{ 0.12f + 0.0005f * static_cast<float>(index), 0.12f, 0.16f, 1.0f }));
		ASSERT_TRUE(renderer.frame_render());
	}
	const auto end = std::chrono::steady_clock::now();
	const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
	const double average_ms = total_ms / static_cast<double>(iterations);

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	std::cout << "[UI 文本基线] 32 标签整树重建 " << iterations << " 帧：" << average_ms
		<< " ms/帧（文本命中 " << stats.text_cache_hits
		<< "，块命中 " << stats.block_cache_hits << "）" << std::endl;
	//缓存应持续命中，且确有文字绘制
	EXPECT_GT(stats.text_cache_hits, 0u);
	EXPECT_GT(stats.block_cache_hits, 0u);
	EXPECT_GT(stats.text_draws, 0u);
	//宽松上限守卫（4 倍余量）
	EXPECT_LT(average_ms, 12.0);
}

//稳定帧基线：不改动控件时整树几何缓存命中，均值应显著低于重建
TEST_F(Renderer_Ui_Perf_Test, 文本密集稳定帧基线)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	ASSERT_NE(build_text_dense_ui(*ui), 0u);
	ASSERT_TRUE(renderer.frame_render());

	const int iterations = 60;
	bool all_hit = true;
	const auto start = std::chrono::steady_clock::now();
	for (int index = 0; index < iterations; ++index)
	{
		ASSERT_TRUE(renderer.frame_render());
		if (renderer.ui().ui_render_stats().cache_hits != 1u)
			all_hit = false;
	}
	const auto end = std::chrono::steady_clock::now();
	const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
	const double average_ms = total_ms / static_cast<double>(iterations);

	std::cout << "[UI 稳定基线] 32 标签稳定帧 " << iterations << " 帧：" << average_ms
		<< " ms/帧（含绘制）" << std::endl;
	//稳定帧应全部命中几何缓存
	EXPECT_TRUE(all_hit) << "稳定帧不应重建几何";
	//宽松上限守卫
	EXPECT_LT(average_ms, 10.0);
}

//九宫格绘制基线：64 个九宫格面板，合批不应回退为多记录
TEST_F(Renderer_Ui_Perf_Test, 九宫格稳定帧基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));

	//64 个九宫格面板铺成网格
	for (int index = 0; index < 64; ++index)
	{
		const int column = index % 8;
		const int row = index / 8;
		const Ui_Rect rect{ static_cast<float>(column) * 40.0f, static_cast<float>(row) * 22.0f, 36.0f, 20.0f };
		const Ui_Handle panel = ui->panel_create(ui->root_get(), rect,
			Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
		ASSERT_NE(panel, 0u);
		ASSERT_TRUE(ui->panel_texture_set(panel, texture));
		ASSERT_TRUE(ui->image_slice_set(panel, 0.25f, 0.25f, 0.25f, 0.25f));
	}

	//几何规模：每面板 1 条记录 / 36 顶点 / 54 索引
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& built = renderer.ui().ui_render_stats();
	EXPECT_EQ(built.draws, 64u);
	EXPECT_EQ(built.vertices, 64u * 36u);
	EXPECT_EQ(built.indices, 64u * 54u);

	const int iterations = 60;
	const auto start = std::chrono::steady_clock::now();
	for (int index = 0; index < iterations; ++index)
		ASSERT_TRUE(renderer.frame_render());
	const auto end = std::chrono::steady_clock::now();
	const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
	const double average_ms = total_ms / static_cast<double>(iterations);

	std::cout << "[UI 九宫格基线] 64 面板稳定帧 " << iterations << " 帧：" << average_ms
		<< " ms/帧（含绘制）" << std::endl;
	EXPECT_LT(average_ms, 12.0);
	renderer.resource().texture_unload(texture);
}
