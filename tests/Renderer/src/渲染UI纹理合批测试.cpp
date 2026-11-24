/*
渲染 UI 纹理记录合批测试（第九百七十六–九百七十七轮）
覆盖：相邻同纹理同色图片合并、异纹理分离、同字体同色文字合并。
统计用例，不依赖像素回读。
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

	//测试字体是否存在
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path("C:\\Windows\\Fonts\\consola.ttf"), code);
	}
}

//渲染 UI 纹理记录合批测试夹具
class Renderer_Ui_Texture_Batch_Test : public ::testing::Test
{
};

//相邻同纹理图片合并为一条纹理记录
TEST_F(Renderer_Ui_Texture_Batch_Test, 同纹理图片合批)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	ASSERT_NE(texture, 0u);
	ASSERT_NE(ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 40.0f, 30.0f }, texture), 0u);
	ASSERT_NE(ui->image_create(ui->root_get(), Ui_Rect{ 60.0f, 20.0f, 40.0f, 30.0f }, texture), 0u);

	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.texture_draws, 1u) << "同纹理相邻图片应合批";
	EXPECT_EQ(stats.draws, 1u);
	EXPECT_EQ(stats.indices, 12u) << "合批不得减少几何";
}

//不同纹理不得合并
TEST_F(Renderer_Ui_Texture_Batch_Test, 异纹理分离)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	uint64_t green = 0;
	uint64_t red = 0;
	const std::vector<uint8_t> green_texels = byjy_render_test::solid_green();
	const std::vector<uint8_t> red_texels = byjy_render_test::solid_red();
	ASSERT_TRUE(renderer.resource().texture_load(green, green_texels.data(), 2, 2));
	ASSERT_TRUE(renderer.resource().texture_load(red, red_texels.data(), 2, 2));
	ASSERT_NE(ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 40.0f, 30.0f }, green), 0u);
	ASSERT_NE(ui->image_create(ui->root_get(), Ui_Rect{ 60.0f, 20.0f, 40.0f, 30.0f }, red), 0u);

	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.texture_draws, 2u) << "不同纹理不应合批";
}

//同字体同色相邻文字合并为一条文字记录
TEST_F(Renderer_Ui_Texture_Batch_Test, 同字体文字合批)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16));
	ui->font_set(font);

	const Ui_Color color{ 1.0f, 1.0f, 1.0f, 1.0f };
	ASSERT_NE(ui->label_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 40.0f, 20.0f }, "A", color), 0u);
	ASSERT_NE(ui->label_create(ui->root_get(), Ui_Rect{ 60.0f, 20.0f, 40.0f, 20.0f }, "A", color), 0u);

	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 1u) << "同图集同色相邻文字应合批";
}
