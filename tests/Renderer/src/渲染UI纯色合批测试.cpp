/*
渲染 UI 相邻纯色合批测试（第九百六十九轮）
覆盖：相邻同色矩形合并为一条纯色记录、颜色不同不合并、几何量不变。
*/
#include <gtest/gtest.h>

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

//渲染 UI 相邻纯色合批测试夹具
class Renderer_Ui_Solid_Batch_Test : public ::testing::Test
{
};

//相邻同色面板合并为一条纯色记录
TEST_F(Renderer_Ui_Solid_Batch_Test, 相邻同色合并)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color color{ 0.20f, 0.20f, 0.40f, 1.0f };
	const Ui_Handle left = ui->panel_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 40.0f, 30.0f }, color);
	const Ui_Handle right = ui->panel_create(ui->root_get(), Ui_Rect{ 60.0f, 20.0f, 40.0f, 30.0f }, color);
	ASSERT_NE(left, 0u);
	ASSERT_NE(right, 0u);

	//同色相邻：两条矩形合并为一条记录，几何量不变
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& merged = renderer.ui().ui_render_stats();
	EXPECT_EQ(merged.solid_draws, 1u) << "同色相邻矩形应合批";
	EXPECT_EQ(merged.draws, 1u);
	EXPECT_EQ(merged.indices, 12u) << "合批不得减少几何";
	EXPECT_EQ(merged.batch_merges, 1u) << "合批次数应计数";

	//改变后一个面板颜色：不得合并
	ASSERT_TRUE(ui->color_set(right, Ui_Color{ 0.60f, 0.20f, 0.20f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& split = renderer.ui().ui_render_stats();
	EXPECT_EQ(split.solid_draws, 2u) << "颜色不同不应合批";
	EXPECT_EQ(split.indices, 12u);
	EXPECT_EQ(split.batch_merges, 0u) << "未合批时计数应为 0";
}
