/*
渲染 UI 嵌套裁剪测试（第九百七十二轮）
覆盖：两级裁剪容器相交后子项被最内层裁掉、圆角背景不越出圆角区域。
像素用例按回读能力门控。
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

	//像素是否为明显红色（回读为 BGRA，按通道序无关判定：红通道最大、其余很低）
	bool is_red(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		const std::array<uint8_t, 4> value = byjy_render_test::pixel_get(pixels, x, y, 320);
		const uint8_t max_outer = value[0] > value[2] ? value[0] : value[2];
		const uint8_t min_outer = value[0] < value[2] ? value[0] : value[2];
		return max_outer > 150 && min_outer < 80 && value[1] < 80;
	}
}

//渲染 UI 嵌套裁剪测试夹具
class Renderer_Ui_Nested_Clip_Test : public ::testing::Test
{
};

//两级裁剪容器：子项只保留最内层交集区域
TEST_F(Renderer_Ui_Nested_Clip_Test, 嵌套裁剪相交)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//外层裁剪容器 (20,20,100,100)
	const Ui_Handle outer = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, Ui_Color{ 0.10f, 0.10f, 0.10f, 1.0f });
	ASSERT_NE(outer, 0u);
	ASSERT_TRUE(ui->clip_children_set(outer, true));
	//内层裁剪容器 (60,60,100,100)
	const Ui_Handle inner = ui->panel_create(outer,
		Ui_Rect{ 60.0f, 60.0f, 100.0f, 100.0f }, Ui_Color{ 0.10f, 0.10f, 0.10f, 1.0f });
	ASSERT_NE(inner, 0u);
	ASSERT_TRUE(ui->clip_children_set(inner, true));
	//子项超出内层右下的红色面板
	const Ui_Handle leaf = ui->panel_create(inner,
		Ui_Rect{ 60.0f, 60.0f, 140.0f, 140.0f }, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(leaf, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//交集内可见
	EXPECT_TRUE(is_red(pixels, 70, 70)) << "裁剪交集内应绘制子项";
	EXPECT_TRUE(is_red(pixels, 100, 100)) << "内层区域内应绘制子项";
	//超出内层右边界（120）不可见
	EXPECT_FALSE(is_red(pixels, 130, 70)) << "子项应被内层右边界裁掉";
	//超出外层下边界（120）不可见
	EXPECT_FALSE(is_red(pixels, 70, 130)) << "子项应被外层下边界裁掉";
}

//圆角裁剪：完全落在角部切除区的子项不绘制，中心子项照常绘制
TEST_F(Renderer_Ui_Nested_Clip_Test, 圆角容器角部子项被裁)
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
	//角部小方块：整体落在左上角切除区
	const Ui_Handle corner_leaf = ui->panel_create(panel,
		Ui_Rect{ 21.0f, 21.0f, 4.0f, 4.0f }, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(corner_leaf, 0u);
	//中心子项：不受圆角影响
	const Ui_Handle center_leaf = ui->panel_create(panel,
		Ui_Rect{ 65.0f, 65.0f, 20.0f, 20.0f }, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(center_leaf, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	EXPECT_FALSE(is_red(pixels, 23, 23)) << "角部切除区内的子项应被圆角裁剪跳过";
	EXPECT_TRUE(is_red(pixels, 70, 70)) << "中心子项应照常绘制";
}

//圆角背景：四角不留背景色，中心仍填充
TEST_F(Renderer_Ui_Nested_Clip_Test, 圆角背景不越界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(panel, 0u);
	//半径超出短边一半时按一半处理
	ASSERT_TRUE(ui->corner_radius_set(panel, 200.0f));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心为红色，四角为空（未绘制）
	EXPECT_TRUE(is_red(pixels, 70, 70)) << "圆角矩形中心应填充";
	EXPECT_FALSE(is_red(pixels, 22, 22)) << "左上角应在圆角外";
	EXPECT_FALSE(is_red(pixels, 117, 22)) << "右上角应在圆角外";
	EXPECT_FALSE(is_red(pixels, 22, 117)) << "左下角应在圆角外";
	EXPECT_FALSE(is_red(pixels, 117, 117)) << "右下角应在圆角外";
}
