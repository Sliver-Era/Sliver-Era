/*
渲染 UI 选中模型测试（第九百六十二轮）
覆盖：选中态 / 多选标记存取、选中高亮纯色记录 +1。
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

//渲染 UI 选中模型测试夹具
class Renderer_Ui_Selection_Test : public ::testing::Test
{
};

//选中态与多选标记存取
TEST_F(Renderer_Ui_Selection_Test, 选中态存取)
{
	Ui_Context ui;
	const Ui_Handle item = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, "A");
	ASSERT_NE(item, 0u);
	EXPECT_FALSE(ui.selected_get(item));
	EXPECT_FALSE(ui.select_mode_get(item));

	ASSERT_TRUE(ui.selected_set(item, true));
	EXPECT_TRUE(ui.selected_get(item));
	ASSERT_TRUE(ui.select_mode_set(item, true));
	EXPECT_TRUE(ui.select_mode_get(item));

	//未知句柄
	EXPECT_FALSE(ui.selected_set(item + 100, true));
	EXPECT_FALSE(ui.selected_get(item + 100));
	EXPECT_FALSE(ui.select_mode_set(item + 100, true));
	EXPECT_FALSE(ui.select_mode_get(item + 100));
}

//选中高亮：纯色记录 +1，取消后恢复
TEST_F(Renderer_Ui_Selection_Test, 选中高亮绘制)
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

	const Ui_Handle item = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 24.0f }, "A");
	ASSERT_NE(item, 0u);

	ASSERT_TRUE(renderer.frame_render());
	const uint32_t base_solid = renderer.ui().ui_render_stats().solid_draws;

	//选中：多一条高亮
	ASSERT_TRUE(ui->selected_set(item, true));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().solid_draws, base_solid + 1u);

	//取消选中：恢复
	ASSERT_TRUE(ui->selected_set(item, false));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().solid_draws, base_solid);
}
