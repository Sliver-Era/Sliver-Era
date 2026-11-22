/*
渲染 UI 裁剪测试（第八百八十八轮）
覆盖：裁剪子节点开关存取、父裁剪开启时子节点溢出部分被挖空、
      两级裁剪取交、父不裁剪时子节点可溢出。
纯数据用例直接构造 Ui_Context；像素用例按回读能力门控。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

//渲染 UI 裁剪测试夹具
class Renderer_Ui_Clip_Test : public ::testing::Test
{
};

//裁剪开关默认关闭、可设置读回、未知句柄拒绝
TEST_F(Renderer_Ui_Clip_Test, 裁剪开关存取)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 40.0f, 40.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	//默认不裁剪子节点
	EXPECT_FALSE(ui.clip_children_get(panel));
	//开启后读回
	ASSERT_TRUE(ui.clip_children_set(panel, true));
	EXPECT_TRUE(ui.clip_children_get(panel));
	//关闭后读回
	ASSERT_TRUE(ui.clip_children_set(panel, false));
	EXPECT_FALSE(ui.clip_children_get(panel));
	//未知句柄
	EXPECT_FALSE(ui.clip_children_set(panel + 100, true));
	EXPECT_FALSE(ui.clip_children_get(panel + 100));
}

//父裁剪开启时子节点超出父矩形的部分不绘制
TEST_F(Renderer_Ui_Clip_Test, 父裁剪子溢出)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color parent_color{ 0.20f, 0.20f, 0.20f, 1.0f };
	const Ui_Color child_color{ 0.85f, 0.85f, 0.85f, 1.0f };
	//左：父裁剪开启；右：父不裁剪（对照）
	const Ui_Handle left_parent = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 60.0f, 60.0f }, parent_color);
	const Ui_Handle left_child = ui->panel_create(left_parent,
		Ui_Rect{ 20.0f, 20.0f, 120.0f, 60.0f }, child_color);
	const Ui_Handle right_parent = ui->panel_create(ui->root_get(),
		Ui_Rect{ 140.0f, 20.0f, 60.0f, 60.0f }, parent_color);
	const Ui_Handle right_child = ui->panel_create(right_parent,
		Ui_Rect{ 140.0f, 20.0f, 120.0f, 60.0f }, child_color);
	ASSERT_NE(left_parent, 0u);
	ASSERT_NE(left_child, 0u);
	ASSERT_NE(right_parent, 0u);
	ASSERT_NE(right_child, 0u);
	ASSERT_TRUE(ui->clip_children_set(left_parent, true));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//父内区域两例一致（子色覆盖父色）
	const std::array<uint8_t, 4> left_inside = byjy_render_test::pixel_get(pixels, 40, 40, 320);
	const std::array<uint8_t, 4> right_inside = byjy_render_test::pixel_get(pixels, 160, 40, 320);
	EXPECT_EQ(left_inside, right_inside) << "父内区域应同为子节点填充色";
	//父外、子内区域：开启裁剪一侧为背景，未裁剪一侧为子色
	const std::array<uint8_t, 4> left_outside = byjy_render_test::pixel_get(pixels, 100, 40, 320);
	const std::array<uint8_t, 4> right_outside = byjy_render_test::pixel_get(pixels, 220, 40, 320);
	EXPECT_NE(left_outside, right_outside) << "父裁剪开启时子节点溢出部分应被挖空";
	//开启裁剪一侧的溢出处应与空白背景一致
	const std::array<uint8_t, 4> background = byjy_render_test::pixel_get(pixels, 300, 160, 320);
	EXPECT_EQ(left_outside, background) << "被挖空区域应回到背景色";
}

//两级嵌套裁剪取交集：孙节点同时受父与祖父约束
TEST_F(Renderer_Ui_Clip_Test, 两级裁剪取交)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color child_color{ 0.85f, 0.85f, 0.85f, 1.0f };
	//祖父 80×80 → 父 60×60 → 子 100×100，三层全部开启裁剪
	const Ui_Handle grand = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 80.0f }, Ui_Color{ 0.3f, 0.3f, 0.3f, 1.0f });
	const Ui_Handle middle = ui->panel_create(grand,
		Ui_Rect{ 20.0f, 20.0f, 60.0f, 60.0f }, Ui_Color{ 0.4f, 0.4f, 0.4f, 1.0f });
	const Ui_Handle leaf = ui->panel_create(middle,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, child_color);
	ASSERT_NE(grand, 0u);
	ASSERT_NE(middle, 0u);
	ASSERT_NE(leaf, 0u);
	ASSERT_TRUE(ui->clip_children_set(grand, true));
	ASSERT_TRUE(ui->clip_children_set(middle, true));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//交集内（20..80 区域）为孙节点填充色（明显亮于背景）
	const std::array<uint8_t, 4> inside = byjy_render_test::pixel_get(pixels, 70, 70, 320);
	const std::array<uint8_t, 4> background = byjy_render_test::pixel_get(pixels, 300, 160, 320);
	EXPECT_GT(byjy_render_test::luma_of(inside), byjy_render_test::luma_of(background) + 40.0f)
		<< "交集内应为孙节点填充色";
	//父内但祖父外（x 在 80..140）应为祖父面板色（孙节点被父裁剪）
	const std::array<uint8_t, 4> outside = byjy_render_test::pixel_get(pixels, 90, 40, 320);
	EXPECT_GT(byjy_render_test::luma_of(outside), byjy_render_test::luma_of(background) + 20.0f)
		<< "祖父矩形内应可见祖父面板色";
	EXPECT_GT(byjy_render_test::luma_of(inside), byjy_render_test::luma_of(outside) + 40.0f)
		<< "超出父矩形部分的孙节点应被挖空";
}

//父不裁剪时子节点允许溢出父矩形
TEST_F(Renderer_Ui_Clip_Test, 父不裁剪允许溢出)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color child_color{ 0.85f, 0.85f, 0.85f, 1.0f };
	//父 40×40 不裁剪，子 100×100 溢出
	const Ui_Handle parent = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 40.0f, 40.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	const Ui_Handle child = ui->panel_create(parent,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 100.0f }, child_color);
	ASSERT_NE(parent, 0u);
	ASSERT_NE(child, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//父外但子内像素应为子节点填充色（明显亮于背景）
	const std::array<uint8_t, 4> overflow = byjy_render_test::pixel_get(pixels, 100, 100, 320);
	const std::array<uint8_t, 4> background = byjy_render_test::pixel_get(pixels, 300, 160, 320);
	EXPECT_GT(byjy_render_test::luma_of(overflow), byjy_render_test::luma_of(background) + 40.0f)
		<< "父未开启裁剪时子节点应允许溢出";
}
