/*
渲染 UI 圆角裁剪测试（第九百七十五轮）
覆盖：圆角裁剪容器的子项在弧形边界外被片元丢弃、弧内与中心照常绘制。
像素用例按回读能力门控（回读为 BGRA，红判定按通道序无关）。
*/
#include <gtest/gtest.h>

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

	//像素是否为明显红色（红通道最大、其余很低）
	bool is_red(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		const std::array<uint8_t, 4> value = byjy_render_test::pixel_get(pixels, x, y, 320);
		const uint8_t max_outer = value[0] > value[2] ? value[0] : value[2];
		const uint8_t min_outer = value[0] < value[2] ? value[0] : value[2];
		return max_outer > 150 && min_outer < 80 && value[1] < 80;
	}
}

//渲染 UI 圆角裁剪测试夹具
class Renderer_Ui_Round_Clip_Test : public ::testing::Test
{
};

//圆角容器子项：弧形外丢弃、弧内与中心保留
TEST_F(Renderer_Ui_Round_Clip_Test, 弧形边界裁剪)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//圆角裁剪容器 (20,20,100,100)，半径 40
	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, Ui_Color{ 0.10f, 0.10f, 0.10f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->corner_radius_set(panel, 40.0f));
	ASSERT_TRUE(ui->clip_children_set(panel, true));
	//铺满容器的红色子项：矩形几何会溢出圆角，需由片元裁剪
	const Ui_Handle leaf = ui->panel_create(panel,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(leaf, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心与弧内保留
	EXPECT_TRUE(is_red(pixels, 70, 70)) << "中心应绘制子项";
	EXPECT_TRUE(is_red(pixels, 46, 46)) << "弧形内侧应绘制子项";
	//四角在弧形外：片元丢弃
	EXPECT_FALSE(is_red(pixels, 22, 22)) << "左上角应在弧外被丢弃";
	EXPECT_FALSE(is_red(pixels, 117, 22)) << "右上角应在弧外被丢弃";
	EXPECT_FALSE(is_red(pixels, 22, 117)) << "左下角应在弧外被丢弃";
	EXPECT_FALSE(is_red(pixels, 117, 117)) << "右下角应在弧外被丢弃";
}

//非圆角容器不受影响：子项角落照常绘制
TEST_F(Renderer_Ui_Round_Clip_Test, 非圆角容器不裁剪)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, Ui_Color{ 0.10f, 0.10f, 0.10f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->clip_children_set(panel, true));
	const Ui_Handle leaf = ui->panel_create(panel,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(leaf, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//无圆角：角落保持矩形
	EXPECT_TRUE(is_red(pixels, 22, 22));
	EXPECT_TRUE(is_red(pixels, 117, 117));
}
