/*
PBR 基线测试（2026-11-17 · 方向九阶段 A 第 710 轮）
建立 PBR 参数的可测基线：关闭态稳定、开启可区分、粗糙度 / 金属度改变高光。
断言只用结构化像素指标（中心亮度、差异像素），不硬编码单点颜色。
*/
#include "渲染场景测试辅助.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//PBR 测试统一窗口（沿用公共辅助）
	//构造中性灰立方体场景：方向光正对相机，高光落在中心
	void pbr_scene_register(engine::Renderer& renderer, engine::Mesh_Handle& mesh, uint64_t& texture)
	{
		//中性灰避免漫反射先饱和
		std::vector<uint8_t> texels = solid_gray();
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(1.0f)));
		//方向光沿 -Z 传播（相机在 +Z），镜面高光正对相机
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.2f);
		renderer.scene().specular_set(1.0f, 64.0f);
	}

	//画一帧并回读
	std::vector<uint8_t> pbr_frame(engine::Renderer& renderer, engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	}
}

//PBR 关闭时连续两帧应逐字节一致（默认真实性基线）
TEST_F(Renderer_Scene_Test, PBR关闭默认帧稳定)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_scene_register(renderer, mesh, texture);
	renderer.scene().pbr_enable_set(false);
	std::vector<uint8_t> first = pbr_frame(renderer, mesh);
	std::vector<uint8_t> second = pbr_frame(renderer, mesh);
	EXPECT_EQ(first, second) << "PBR 关闭态两帧不一致";
	RecordProperty("pbr_off_center", static_cast<int>(center_brightness(first)));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//PBR 开启与关闭应产生可区分画面
TEST_F(Renderer_Scene_Test, PBR开启与关闭可区分)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_scene_register(renderer, mesh, texture);
	//关闭态
	renderer.scene().pbr_enable_set(false);
	std::vector<uint8_t> off = pbr_frame(renderer, mesh);
	//开启态（绝对粗糙度口径，避免光泽度换算干扰）
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	renderer.scene().pbr_metallic_set(0.0f);
	std::vector<uint8_t> on = pbr_frame(renderer, mesh);
	uint32_t changed = byjy_render_test::changed_pixels(off, on, 6);
	EXPECT_GT(changed, 0u) << "PBR 开关未改变画面";
	RecordProperty("pbr_toggle_changed_pixels", static_cast<int>(changed));
	RecordProperty("pbr_on_center", static_cast<int>(center_brightness(on)));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//绝对粗糙度口径：低粗糙度的高光更集中、中心更亮
TEST_F(Renderer_Scene_Test, PBR粗糙度改变高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_scene_register(renderer, mesh, texture);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_metallic_set(0.0f);
	//低粗糙度（镜面）
	renderer.scene().pbr_roughness_set(0.05f);
	std::vector<uint8_t> smooth = pbr_frame(renderer, mesh);
	//高粗糙度（漫反射为主）
	renderer.scene().pbr_roughness_set(0.9f);
	std::vector<uint8_t> rough = pbr_frame(renderer, mesh);
	uint32_t changed = byjy_render_test::changed_pixels(smooth, rough, 6);
	EXPECT_GT(changed, 0u) << "粗糙度未改变画面";
	uint32_t smooth_center = center_brightness(smooth);
	uint32_t rough_center = center_brightness(rough);
	EXPECT_GT(smooth_center, rough_center) << "低粗糙度中心未更亮";
	RecordProperty("pbr_smooth_center", static_cast<int>(smooth_center));
	RecordProperty("pbr_rough_center", static_cast<int>(rough_center));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//金属度改变镜面基色与漫反射占比，画面应可区分
TEST_F(Renderer_Scene_Test, PBR金属度改变镜面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_scene_register(renderer, mesh, texture);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_roughness_set(0.2f);
	//介电
	renderer.scene().pbr_metallic_set(0.0f);
	std::vector<uint8_t> dielectric = pbr_frame(renderer, mesh);
	//金属
	renderer.scene().pbr_metallic_set(1.0f);
	std::vector<uint8_t> metal = pbr_frame(renderer, mesh);
	uint32_t changed = byjy_render_test::changed_pixels(dielectric, metal, 6);
	EXPECT_GT(changed, 0u) << "金属度未改变画面";
	RecordProperty("pbr_dielectric_center", static_cast<int>(center_brightness(dielectric)));
	RecordProperty("pbr_metal_center", static_cast<int>(center_brightness(metal)));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
