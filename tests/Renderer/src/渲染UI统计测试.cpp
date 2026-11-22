/*
渲染 UI 绘制统计测试（第八百九十九轮）
覆盖：纯色面板的顶点 / 索引 / 记录数、文字绘制记录与纹理重绑计数、
      图片绘制的纹理重绑计数。
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

//渲染 UI 统计测试夹具
class Renderer_Ui_Stats_Test : public ::testing::Test
{
};

//单个纯色面板：4 顶点 / 6 索引 / 1 记录
TEST_F(Renderer_Ui_Stats_Test, 纯色面板统计)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 10.0f, 10.0f, 100.0f, 50.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.vertices, 4u);
	EXPECT_EQ(stats.indices, 6u);
	EXPECT_EQ(stats.draws, 1u);
	EXPECT_EQ(stats.text_draws, 0u);
	EXPECT_EQ(stats.texture_binds, 0u);
}

//文字：至少一条文字记录且发生一次纹理重绑
TEST_F(Renderer_Ui_Stats_Test, 文字绘制统计)
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
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, "AB");
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 1u);
	EXPECT_GE(stats.texture_binds, 1u);
	EXPECT_GT(stats.indices, 6u) << "文字应产生额外几何";
}

//图片：一条纹理记录且发生一次纹理重绑，文字计数为零
TEST_F(Renderer_Ui_Stats_Test, 图片绘制统计)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	const Ui_Handle image = ui->image_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, texture);
	ASSERT_NE(image, 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 0u);
	EXPECT_EQ(stats.draws, 1u);
	EXPECT_EQ(stats.texture_binds, 1u);
	EXPECT_EQ(stats.vertices, 4u);
	EXPECT_EQ(stats.indices, 6u);
	renderer.resource().texture_unload(texture);
}
