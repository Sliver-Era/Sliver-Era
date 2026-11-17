/*
PBR 参数口径（2026-11-17 · 方向九阶段 A 第 711 轮）
覆盖：全局 PBR 参数非法值被忽略；逐物体覆盖优先于全局参数；
断言用结构化指标与整帧差，不硬编码单点颜色。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造中性灰立方体场景（方向光正对相机）
	void pbr_param_scene_register(engine::Renderer& renderer, engine::Mesh_Handle& mesh, uint64_t& texture)
	{
		std::vector<uint8_t> texels = solid_gray();
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(1.0f)));
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.2f);
		renderer.scene().specular_set(1.0f, 64.0f);
		//统一走绝对粗糙度口径，避免光泽度换算干扰
		renderer.scene().pbr_enable_set(true);
		renderer.scene().pbr_roughness_absolute_set(true);
		renderer.scene().pbr_metallic_set(0.0f);
	}

	//画一帧并回读
	std::vector<uint8_t> pbr_param_frame(engine::Renderer& renderer, engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	}
}

//全局 PBR 参数越界时被忽略：画面与前一合法值完全一致
TEST_F(Renderer_Scene_Test, PBR全局非法参数被忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, mesh, texture);
	//合法基线
	renderer.scene().pbr_roughness_set(0.5f);
	renderer.scene().pbr_metallic_set(0.2f);
	std::vector<uint8_t> baseline = pbr_param_frame(renderer, mesh);
	//粗糙度越界（0 与 9）应被忽略
	renderer.scene().pbr_roughness_set(0.0f);
	std::vector<uint8_t> low = pbr_param_frame(renderer, mesh);
	EXPECT_EQ(low, baseline) << "粗糙度下越界未被忽略";
	renderer.scene().pbr_roughness_set(9.0f);
	std::vector<uint8_t> high = pbr_param_frame(renderer, mesh);
	EXPECT_EQ(high, baseline) << "粗糙度上越界未被忽略";
	//金属度越界（-0.1 与 1.5）应被忽略
	renderer.scene().pbr_metallic_set(-0.1f);
	std::vector<uint8_t> metal_low = pbr_param_frame(renderer, mesh);
	EXPECT_EQ(metal_low, baseline) << "金属度下越界未被忽略";
	renderer.scene().pbr_metallic_set(1.5f);
	std::vector<uint8_t> metal_high = pbr_param_frame(renderer, mesh);
	EXPECT_EQ(metal_high, baseline) << "金属度上越界未被忽略";
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
逐物体 PBR 覆盖优先于全局参数：
1) 同一覆盖材质在全局粗糙度变化时画面保持一致（全局被覆盖）；
2) 两个覆盖材质（0.2 / 0.8）画面可区分（覆盖参数确实参与着色）。
*/
TEST_F(Renderer_Scene_Test, PBR逐物体覆盖优先于全局)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, mesh, texture);
	//覆盖材质：偏光滑（0.2）与偏粗糙（0.8）
	engine::Material_Data smooth_data;
	smooth_data.pbr_override = true;
	smooth_data.pbr_roughness = 0.2f;
	smooth_data.pbr_metallic = 0.0f;
	smooth_data.texture = texture;
	engine::Material_Handle smooth_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(smooth_handle, smooth_data));
	engine::Material_Data rough_data = smooth_data;
	rough_data.pbr_roughness = 0.8f;
	engine::Material_Handle rough_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(rough_handle, rough_data));

	//全局粗糙度 0.9：覆盖材质画面
	renderer.scene().pbr_roughness_set(0.9f);
	renderer.resource().material_bind_set(smooth_handle);
	EXPECT_TRUE(renderer.resource().material_pbr_active()) << "逐物体 PBR 覆盖未登记";
	EXPECT_FLOAT_EQ(renderer.resource().material_pbr_roughness(), 0.2f);
	std::vector<uint8_t> smooth_global_high = pbr_param_frame(renderer, mesh);
	//全局改为 0.1：同一覆盖材质画面应不变（全局被覆盖）
	renderer.scene().pbr_roughness_set(0.1f);
	std::vector<uint8_t> smooth_global_low = pbr_param_frame(renderer, mesh);
	EXPECT_LE(byjy_render_test::max_channel_diff(smooth_global_high, smooth_global_low), 2)
		<< "全局粗糙度变化影响了逐物体覆盖";
	//换偏粗糙覆盖材质：画面应可区分
	renderer.scene().pbr_roughness_set(0.9f);
	renderer.resource().material_bind_set(rough_handle);
	std::vector<uint8_t> rough_frame = pbr_param_frame(renderer, mesh);
	EXPECT_GT(byjy_render_test::changed_pixels(smooth_global_high, rough_frame, 6), 0u)
		<< "逐物体粗糙度覆盖未改变画面";
	renderer.resource().material_unload(rough_handle);
	renderer.resource().material_unload(smooth_handle);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//多重散射能量补偿：高粗糙金属提亮，低粗糙几乎不变
TEST_F(Renderer_Scene_Test, PBR多重散射补偿)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, mesh, texture);
	//纯白反照率使金属 F0 接近 1，放大补偿差异
	uint64_t white = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(white, white_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(white);
	renderer.scene().pbr_metallic_set(1.0f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.0f);
	renderer.scene().specular_set(0.5f, 64.0f);
	//按粗糙度与开关取帧
	auto frame_with = [&](float roughness, bool multiscatter)
	{
		renderer.scene().pbr_roughness_set(roughness);
		renderer.scene().pbr_multiscatter_set(multiscatter);
		return pbr_param_frame(renderer, mesh);
	};
	std::vector<uint8_t> high_off = frame_with(0.9f, false);
	std::vector<uint8_t> high_on = frame_with(0.9f, true);
	EXPECT_TRUE(renderer.scene().pbr_multiscatter()) << "多重散射开关未登记";
	std::vector<uint8_t> low_off = frame_with(0.1f, false);
	std::vector<uint8_t> low_on = frame_with(0.1f, true);
	uint32_t high_changed = byjy_render_test::changed_pixels(high_off, high_on, 3);
	uint32_t low_changed = byjy_render_test::changed_pixels(low_off, low_on, 3);
	EXPECT_GT(high_changed, 0u) << "高粗糙金属未获得能量补偿";
	EXPECT_LE(low_changed, high_changed) << "低粗糙补偿强于高粗糙";
	RecordProperty("multiscatter_high_changed", static_cast<int>(high_changed));
	RecordProperty("multiscatter_low_changed", static_cast<int>(low_changed));
	RecordProperty("multiscatter_high_off_center", static_cast<int>(center_brightness(high_off)));
	RecordProperty("multiscatter_high_on_center", static_cast<int>(center_brightness(high_on)));
	renderer.resource().texture_unload(white);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
