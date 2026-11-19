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

	//构造沿 X 平移的模型矩阵（列主序）
	std::array<float, 16> quad_offset_matrix(float offset_x)
	{
		std::array<float, 16> matrix{};
		matrix[0] = 1.0f;
		matrix[5] = 1.0f;
		matrix[10] = 1.0f;
		matrix[12] = offset_x;
		matrix[15] = 1.0f;
		return matrix;
	}

	//统计某一半画面（0 = 左半、1 = 右半）相对参考帧的变化像素数
	uint32_t half_changed(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, uint32_t half, int threshold)
	{
		uint32_t count = 0;
		uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
		uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = begin; x < end; ++x)
			{
				std::array<uint8_t, 4> a = pixel_get(left, x, y);
				std::array<uint8_t, 4> b = pixel_get(right, x, y);
				int red = std::abs(static_cast<int>(a[0]) - static_cast<int>(b[0]));
				int green = std::abs(static_cast<int>(a[1]) - static_cast<int>(b[1]));
				int blue = std::abs(static_cast<int>(a[2]) - static_cast<int>(b[2]));
				if (red >= threshold || green >= threshold || blue >= threshold)
					++count;
			}
		}
		return count;
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

//镜面抗锯齿：球面高频法线处粗糙度被加宽，关闭态两帧稳定
TEST_F(Renderer_Scene_Test, PBR镜面抗锯齿)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle cube = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, cube, texture);
	//白色金属球（球面法线导数在轮廓处显著）
	uint64_t white = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(white, white_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(white);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	renderer.scene().pbr_metallic_set(1.0f);
	renderer.scene().pbr_roughness_set(0.15f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.0f);
	renderer.scene().specular_set(1.0f, 64.0f);
	//关闭态两帧一致
	renderer.scene().pbr_specular_aa_set(false);
	std::vector<uint8_t> off_first = pbr_param_frame(renderer, sphere);
	std::vector<uint8_t> off_second = pbr_param_frame(renderer, sphere);
	EXPECT_EQ(off_first, off_second) << "镜面抗锯齿关闭态两帧不一致";
	//开启后画面应可区分
	renderer.scene().pbr_specular_aa_set(true);
	EXPECT_TRUE(renderer.scene().pbr_specular_aa());
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	uint32_t changed = byjy_render_test::changed_pixels(off_second, on, 1);
	EXPECT_GT(changed, 0u) << "镜面抗锯齿未改变画面";
	RecordProperty("specular_aa_changed_pixels", static_cast<int>(changed));
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(white);
	renderer.resource().mesh_unload(cube);
	renderer.resource().texture_unload(texture);
}

/*
Fresnel-roughness 与 F90 口径统一（方向九第 718 轮）
黑色介电球 ＋ 白色环境：漫反射为零，轮廓只显示环境镜面项；
开启后 F90 = max(1 − roughness, F0) 使掠射反射收敛，轮廓变暗。
*/
TEST_F(Renderer_Scene_Test, PBR介电掠射菲涅尔收敛)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	//纯黑反照率（漫反射为零，只保留环境镜面）
	uint64_t black = 0;
	std::vector<uint8_t> black_texels(2 * 2 * 4, 0);
	ASSERT_TRUE(renderer.resource().texture_load(black, black_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(black);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	//介电 ＋ 中高粗糙度 ＋ 白色环境
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_metallic_set(0.0f);
	renderer.scene().pbr_roughness_set(0.5f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 0.0f, 0.0f);
	const float environment[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().environment_set(environment, 1.0f);
	//关闭态基线
	renderer.scene().pbr_fresnel_roughness_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, sphere);
	//开启后轮廓掠射区应变暗
	renderer.scene().pbr_fresnel_roughness_set(true);
	EXPECT_TRUE(renderer.scene().pbr_fresnel_roughness());
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	uint32_t dimmed = byjy_render_test::darkened_count(on, off, 3.0f);
	EXPECT_GT(dimmed, 0u) << "介电掠射菲涅尔未收敛";
	RecordProperty("fresnel_dielectric_dimmed", static_cast<int>(dimmed));
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(black);
}

//Fresnel-roughness 与 F90 口径统一（方向九第 718 轮）：白色金属 F0 ≈ 1 → F90 仍为 1，两态逐字节一致
TEST_F(Renderer_Scene_Test, PBR金属菲涅尔口径不变)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle cube = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, cube, texture);
	//纯白反照率使金属 F0 接近 1
	uint64_t white = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(white, white_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(white);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	renderer.scene().pbr_metallic_set(1.0f);
	renderer.scene().pbr_roughness_set(0.5f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.0f);
	renderer.scene().specular_set(1.0f, 64.0f);
	renderer.scene().pbr_fresnel_roughness_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, sphere);
	renderer.scene().pbr_fresnel_roughness_set(true);
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	EXPECT_EQ(off, on) << "白色金属的 F90 口径被改变";
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(white);
	renderer.resource().mesh_unload(cube);
	renderer.resource().texture_unload(texture);
}

/*
环境镜面 split-sum BRDF 近似（方向九第 719 轮）
1) 黑色介电球 ＋ 白色环境：开启后走 DFG 近似，画面与环境菲涅尔路径可区分；
2) 白色金属球：关闭时环境反射与粗糙度无关（两档一致），开启后随粗糙度变化。
*/
TEST_F(Renderer_Scene_Test, PBR环境BRDF近似生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t black = 0;
	std::vector<uint8_t> black_texels(2 * 2 * 4, 0);
	ASSERT_TRUE(renderer.resource().texture_load(black, black_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(black);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_metallic_set(0.0f);
	renderer.scene().pbr_roughness_set(0.5f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 0.0f, 0.0f);
	const float environment[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().environment_set(environment, 1.0f);
	renderer.scene().pbr_env_brdf_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, sphere);
	renderer.scene().pbr_env_brdf_set(true);
	EXPECT_TRUE(renderer.scene().pbr_env_brdf());
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	uint32_t changed = byjy_render_test::changed_pixels(off, on, 2);
	EXPECT_GT(changed, 0u) << "环境 BRDF 近似未改变画面";
	RecordProperty("env_brdf_changed_pixels", static_cast<int>(changed));
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(black);
}

//环境镜面 split-sum BRDF 近似（方向九第 719 轮）：关闭时与粗糙度无关，开启后随粗糙度变化
TEST_F(Renderer_Scene_Test, PBR环境BRDF随粗糙度变化)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t white = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(white, white_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(white);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_metallic_set(1.0f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 0.0f, 0.0f);
	const float environment[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().environment_set(environment, 1.0f);
	auto frame_with = [&](float roughness, bool env_brdf)
	{
		renderer.scene().pbr_roughness_set(roughness);
		renderer.scene().pbr_env_brdf_set(env_brdf);
		return pbr_param_frame(renderer, sphere);
	};
	//关闭：常量环境 ＋ 白色金属，环境项与粗糙度无关
	std::vector<uint8_t> smooth_off = frame_with(0.15f, false);
	std::vector<uint8_t> rough_off = frame_with(0.85f, false);
	EXPECT_EQ(smooth_off, rough_off) << "关闭态环境反射不应随粗糙度变化";
	//开启：DFG 近似引入粗糙度响应
	std::vector<uint8_t> smooth_on = frame_with(0.15f, true);
	std::vector<uint8_t> rough_on = frame_with(0.85f, true);
	uint32_t changed = byjy_render_test::changed_pixels(smooth_on, rough_on, 2);
	EXPECT_GT(changed, 0u) << "环境 BRDF 近似未引入粗糙度响应";
	RecordProperty("env_brdf_roughness_changed", static_cast<int>(changed));
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(white);
}

/*
光泽度 → GGX 粗糙度修正映射（方向九第 721 轮）
旧口径 roughness = sqrt(2/(n+2))，修正口径 roughness = (2/(n+2))^0.25；
关闭绝对口径后，两种映射在同一光泽度下画面可区分。
*/
TEST_F(Renderer_Scene_Test, PBR光泽度映射修正)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, mesh, texture);
	//回到光泽度换算路径
	renderer.scene().pbr_roughness_absolute_set(false);
	renderer.scene().pbr_roughness_set(1.0f);
	renderer.scene().specular_set(1.0f, 64.0f);
	//旧口径基线
	renderer.scene().pbr_gloss_mapping_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, mesh);
	//修正映射后画面应变化
	renderer.scene().pbr_gloss_mapping_set(true);
	EXPECT_TRUE(renderer.scene().pbr_gloss_mapping());
	std::vector<uint8_t> on = pbr_param_frame(renderer, mesh);
	uint32_t changed = byjy_render_test::changed_pixels(off, on, 2);
	EXPECT_GT(changed, 0u) << "光泽度修正映射未改变画面";
	RecordProperty("gloss_mapping_changed", static_cast<int>(changed));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
各向异性 GGX（方向九第 722 轮）
1) 金属球开启各向异性（强度 0.8）后高光形状改变，画面可区分；
2) 强度 0 时与关闭态等价（差异不超过 2 级）。
*/
TEST_F(Renderer_Scene_Test, PBR各向异性改变高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t white = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(white, white_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(white);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_metallic_set(0.9f);
	renderer.scene().pbr_roughness_set(0.3f);
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.0f);
	renderer.scene().specular_set(1.0f, 64.0f);
	//关闭态基线
	renderer.scene().pbr_aniso_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, sphere);
	//开启各向异性
	renderer.scene().pbr_aniso_set(true);
	renderer.scene().pbr_aniso_strength_set(0.8f);
	EXPECT_TRUE(renderer.scene().pbr_aniso());
	EXPECT_FLOAT_EQ(renderer.scene().pbr_aniso_strength(), 0.8f);
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	uint32_t changed = byjy_render_test::changed_pixels(off, on, 2);
	EXPECT_GT(changed, 0u) << "各向异性未改变高光";
	RecordProperty("aniso_changed_pixels", static_cast<int>(changed));
	//强度 0 应与各向同性等价
	renderer.scene().pbr_aniso_strength_set(0.0f);
	std::vector<uint8_t> zero = pbr_param_frame(renderer, sphere);
	EXPECT_LE(byjy_render_test::max_channel_diff(off, zero), 2) << "零强度与各向同性不等价";
	//越界强度被忽略
	renderer.scene().pbr_aniso_strength_set(1.5f);
	EXPECT_FLOAT_EQ(renderer.scene().pbr_aniso_strength(), 0.0f);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(white);
}

/*
皮肤 SSS-lite wrap 漫反射（方向九第 723 轮）
斜射方向光下开启 wrap：明暗交界推向背光侧，背光区域提亮（不会压暗任何像素）。
*/
TEST_F(Renderer_Scene_Test, PBR皮肤wrap漫反射)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, mesh, texture);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	//斜射方向光（光传播方向偏离相机轴）
	const float direction[3] = { 0.6f, -0.5f, -0.8f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.05f);
	//关闭态基线
	renderer.scene().pbr_skin_sss_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, sphere);
	//开启 wrap 后背光侧应变亮
	renderer.scene().pbr_skin_sss_set(true);
	renderer.scene().pbr_skin_wrap_set(0.8f);
	EXPECT_TRUE(renderer.scene().pbr_skin_sss());
	EXPECT_FLOAT_EQ(renderer.scene().pbr_skin_wrap(), 0.8f);
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	uint32_t brightened = byjy_render_test::brightened_count(on, off, 2.0f);
	EXPECT_GT(brightened, 0u) << "wrap 漫反射未提亮背光侧";
	RecordProperty("skin_wrap_brightened", static_cast<int>(brightened));
	//越界 wrap 被忽略
	renderer.scene().pbr_skin_wrap_set(2.0f);
	EXPECT_FLOAT_EQ(renderer.scene().pbr_skin_wrap(), 0.8f);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
布料绒感 sheen（方向九第 724 轮）
球面轮廓按 (1 − N·V)^3 叠加受光绒边；开启后轮廓提亮，关闭态不变。
*/
TEST_F(Renderer_Scene_Test, PBR布料绒感sheen)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	pbr_param_scene_register(renderer, mesh, texture);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
	//关闭态基线
	renderer.scene().pbr_sheen_set(false);
	std::vector<uint8_t> off = pbr_param_frame(renderer, sphere);
	//开启 sheen 后轮廓应提亮
	renderer.scene().pbr_sheen_set(true);
	renderer.scene().pbr_sheen_strength_set(0.8f);
	EXPECT_TRUE(renderer.scene().pbr_sheen());
	EXPECT_FLOAT_EQ(renderer.scene().pbr_sheen_strength(), 0.8f);
	std::vector<uint8_t> on = pbr_param_frame(renderer, sphere);
	uint32_t brightened = byjy_render_test::brightened_count(on, off, 2.0f);
	EXPECT_GT(brightened, 0u) << "sheen 未提亮轮廓";
	RecordProperty("sheen_brightened", static_cast<int>(brightened));
	//越界强度被忽略
	renderer.scene().pbr_sheen_strength_set(-0.5f);
	EXPECT_FLOAT_EQ(renderer.scene().pbr_sheen_strength(), 0.8f);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
写实与二次元双轨同帧（方向九第 728 轮）
左侧写实 PBR、右侧二次元卡通；仅切换右侧材质时左半画面必须逐像素不变。
*/
TEST_F(Renderer_Scene_Test, PBR写实与二次元双轨同帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 24, 12));
	//斜射主光：球面法线变化产生明暗交界，卡通量化才可观测
	const float direction[3] = { 0.4f, -0.25f, -0.9f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.15f);
	renderer.scene().specular_set(0.2f, 32.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	//写实材质与卡通材质
	engine::Material_Data realistic;
	realistic.texture = texture;
	realistic.pbr_override = true;
	realistic.pbr_roughness = 0.4f;
	realistic.pbr_metallic = 0.0f;
	engine::Material_Handle realistic_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(realistic_handle, realistic));
	engine::Material_Data anime = realistic;
	anime.toon_override = true;
	anime.toon_steps = 1.0f;
	anime.toon_rim = 0.6f;
	engine::Material_Handle anime_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(anime_handle, anime));
	//左写实 / 右任意材质的单帧
	auto draw_frame = [&](engine::Material_Handle right_handle)
	{
		std::array<float, 16> left_matrix = quad_offset_matrix(-1.1f);
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		renderer.resource().material_bind_set(realistic_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, left_matrix.data()));
		renderer.resource().material_bind_set(right_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};
	//基线：两侧都写实
	std::vector<uint8_t> both_realistic = draw_frame(realistic_handle);
	//右侧改卡通
	std::vector<uint8_t> right_anime = draw_frame(anime_handle);
	uint32_t right_changed = half_changed(both_realistic, right_anime, 1, 2);
	uint32_t left_changed = half_changed(both_realistic, right_anime, 0, 2);
	EXPECT_GT(right_changed, 0u) << "右侧卡通材质未改变画面";
	EXPECT_EQ(left_changed, 0u) << "右侧材质切换影响了左侧写实物体";
	RecordProperty("dual_track_right_changed", static_cast<int>(right_changed));
	RecordProperty("dual_track_left_changed", static_cast<int>(left_changed));
	renderer.resource().material_unload(anime_handle);
	renderer.resource().material_unload(realistic_handle);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(texture);
}

/*
逐物体 PBR 特性分层（第 777 轮 · 通用引擎化整改）
左侧物体材质沿用全局口径（特性全关），右侧材质以位掩码声明「布料 sheen」；
仅切换右侧材质时左半必须逐像素不变，右半必须出现差异。
另两项判据：空掩码覆盖与普通材质逐字节一致；光泽度映射（bit5）确实改写本材质粗糙度。
*/
TEST_F(Renderer_Scene_Test, PBR特性逐物体分层)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 24, 12));
	//斜射主光：球面法线变化使 sheen 绒边可观测
	const float direction[3] = { 0.4f, -0.25f, -0.9f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.15f);
	renderer.scene().specular_set(0.6f, 48.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	renderer.scene().pbr_metallic_set(0.0f);
	//全局特性全部关闭（材质未覆盖时回落该口径）
	renderer.scene().pbr_sheen_set(false);
	renderer.scene().pbr_sheen_strength_set(0.8f);
	renderer.scene().pbr_aniso_set(false);
	renderer.scene().pbr_skin_sss_set(false);
	//普通材质（不覆盖特性）
	engine::Material_Data plain;
	plain.texture = texture;
	plain.pbr_override = true;
	plain.pbr_roughness = 0.4f;
	plain.pbr_metallic = 0.0f;
	engine::Material_Handle plain_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(plain_handle, plain));
	//布料 sheen 材质（bit4 仅在右侧物体生效）
	engine::Material_Data cloth = plain;
	cloth.pbr_features_override = true;
	cloth.pbr_features = 1u << 4;
	engine::Material_Handle cloth_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(cloth_handle, cloth));
	//光泽度映射材质（bit5 ＋ gloss 1.0 → 本材质粗糙度压到下限）
	engine::Material_Data glossy = plain;
	glossy.pbr_features_override = true;
	glossy.pbr_features = 1u << 5;
	glossy.pbr_gloss = 1.0f;
	engine::Material_Handle glossy_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(glossy_handle, glossy));
	auto draw_frame = [&](engine::Material_Handle left_handle, engine::Material_Handle right_handle)
	{
		std::array<float, 16> left_matrix = quad_offset_matrix(-1.1f);
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		renderer.resource().material_bind_set(left_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, left_matrix.data()));
		renderer.resource().material_bind_set(right_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};
	//基线：两侧均为普通材质
	std::vector<uint8_t> baseline = draw_frame(plain_handle, plain_handle);
	//右侧改为 sheen 材质：右半变化、左半不动
	std::vector<uint8_t> right_cloth = draw_frame(plain_handle, cloth_handle);
	EXPECT_GT(half_changed(baseline, right_cloth, 1, 2), 0u) << "右侧材质的 sheen 特性未生效";
	EXPECT_EQ(half_changed(baseline, right_cloth, 0, 1), 0u) << "右侧材质影响了左侧物体";
	RecordProperty("layer_right_changed", static_cast<int>(half_changed(baseline, right_cloth, 1, 2)));
	//空掩码覆盖：特性全关，与普通材质逐字节一致
	engine::Material_Data empty_mask = plain;
	empty_mask.pbr_features_override = true;
	empty_mask.pbr_features = 0u;
	engine::Material_Handle empty_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(empty_handle, empty_mask));
	EXPECT_EQ(baseline, draw_frame(plain_handle, empty_handle)) << "空掩码覆盖改变了画面";
	//光泽度映射：绑定时本材质粗糙度应被改写为下限
	renderer.resource().material_bind_set(glossy_handle);
	EXPECT_TRUE(renderer.resource().material_pbr_active()) << "光泽度映射未登记为逐物体 PBR 覆盖";
	EXPECT_FLOAT_EQ(renderer.resource().material_pbr_roughness(), 0.05f) << "光泽度映射未改写粗糙度";
	EXPECT_GT(half_changed(baseline, draw_frame(plain_handle, glossy_handle), 1, 2), 0u)
		<< "光泽度映射未改变右侧画面";
	renderer.resource().material_unload(glossy_handle);
	renderer.resource().material_unload(empty_handle);
	renderer.resource().material_unload(cloth_handle);
	renderer.resource().material_unload(plain_handle);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(texture);
}


/*
HDR 默认路径下的双轨色彩（方向九第 762 轮）
左侧写实、右侧二次元，纹理为纯蓝；比较 ACES 与 AgX 对卡通纯色的处理。
AgX 的输入矩阵会把彩色高光去饱和（二次元纯色块变灰），ACES 逐通道则保持纯色。
统计右半区域的最大通道差（饱和度粗量）作为判据。
*/
TEST_F(Renderer_Scene_Test, HDR双轨同帧色彩保真)
{
	//纯蓝纹理（RGBA8，2×2）
	std::vector<uint8_t> blue_texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < blue_texels.size(); index += 4)
	{
		blue_texels[index + 2] = 255;
		blue_texels[index + 3] = 255;
	}
	//按算子出一帧，返回右半区域的最大通道差
	auto right_span_by = [&](uint32_t tone_operator)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.hdr_targets = true;
		config.hdr_tone_operator = tone_operator;
		EXPECT_TRUE(renderer.init(config));
		uint64_t texture = 0;
		EXPECT_TRUE(renderer.resource().texture_load(texture, blue_texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		engine::Mesh_Handle sphere = 0;
		EXPECT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 24, 12));
		const float direction[3] = { 0.4f, -0.25f, -0.9f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.15f);
		renderer.scene().specular_set(0.2f, 32.0f);
		renderer.scene().pbr_enable_set(true);
		renderer.scene().pbr_roughness_absolute_set(true);
		renderer.scene().pbr_roughness_set(0.5f);
		engine::Material_Data realistic;
		realistic.texture = texture;
		realistic.pbr_override = true;
		realistic.pbr_roughness = 0.4f;
		realistic.pbr_metallic = 0.0f;
		engine::Material_Handle realistic_handle = 0;
		EXPECT_TRUE(renderer.resource().material_load(realistic_handle, realistic));
		engine::Material_Data anime = realistic;
		anime.toon_override = true;
		anime.toon_steps = 1.0f;
		anime.toon_rim = 0.6f;
		engine::Material_Handle anime_handle = 0;
		EXPECT_TRUE(renderer.resource().material_load(anime_handle, anime));
		//左写实 / 右卡通
		std::array<float, 16> left_matrix = quad_offset_matrix(-1.1f);
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		renderer.resource().material_bind_set(realistic_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, left_matrix.data()));
		renderer.resource().material_bind_set(anime_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		//扫描右半区域的最大通道差
		int max_span = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = WINDOW_WIDTH / 2; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> pixel = pixel_get(pixels, x, y);
				int high = static_cast<int>(*std::max_element(pixel.begin(), pixel.begin() + 3));
				int low = static_cast<int>(*std::min_element(pixel.begin(), pixel.begin() + 3));
				if (high - low > max_span)
					max_span = high - low;
			}
		}
		renderer.resource().material_unload(anime_handle);
		renderer.resource().material_unload(realistic_handle);
		renderer.resource().mesh_unload(sphere);
		renderer.resource().texture_unload(texture);
		return max_span;
	};
	int aces_span = right_span_by(0);
	int agx_span = right_span_by(4);
	RecordProperty("dual_hdr_aces_span", aces_span);
	RecordProperty("dual_hdr_agx_span", agx_span);
	//ACES：纯蓝卡通块保持高饱和
	EXPECT_GE(aces_span, 120) << "ACES 下卡通纯色饱和度过低：" << aces_span;
	//AgX：彩色高光去饱和，通道差明显收窄
	EXPECT_LT(agx_span, aces_span - 30) << "AgX 未对卡通纯色去饱和：ACES " << aces_span
		<< " / AgX " << agx_span;
}

/*
HDR 双轨区域色调映射（方向九第 773 轮）
口径：hdr_tone_operator = 5 时，左半屏走 AgX（写实侧）、右半屏走逐通道 ACES（二次元侧），
中间 0.1 宽度过渡。两枚纯红四边形分别落在左右半屏，断言：
1) 左半红块的非红通道被 AgX 抬起；2) 右半红块保持纯色；3) 算子 0（ACES）两侧都保持纯色（对照）。
读取通道：交换链为 BGRA，index 0 = 蓝、index 2 = 红。
*/
TEST_F(Renderer_Scene_Test, HDR双轨区域色调映射)
{
	//纯红纹理（RGBA8，2×2；index 0 = R）
	std::vector<uint8_t> red_texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < red_texels.size(); index += 4)
	{
		red_texels[index + 0] = 255;
		red_texels[index + 3] = 255;
	}
	//按算子出一帧，返回左右半屏的（红, 蓝）读数
	auto halves_by = [&](uint32_t tone_operator, int& left_red, int& left_blue,
		int& right_red, int& right_blue)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		config.hdr_tone_operator = tone_operator;
		EXPECT_TRUE(renderer.init(config));
		uint64_t texture = 0;
		EXPECT_TRUE(renderer.resource().texture_load(texture, red_texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.0f);
		engine::Mesh_Handle quad = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
		//左 / 右各一枚四边形（与 762 同几何）
		std::array<float, 16> left_matrix = quad_offset_matrix(-1.1f);
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, left_matrix.data()));
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		std::array<uint8_t, 4> left_pixel = pixel_get(pixels, WINDOW_WIDTH / 4, WINDOW_HEIGHT / 2);
		std::array<uint8_t, 4> right_pixel = pixel_get(pixels, WINDOW_WIDTH * 3 / 4, WINDOW_HEIGHT / 2);
		left_red = static_cast<int>(left_pixel[2]);
		left_blue = static_cast<int>(left_pixel[0]);
		right_red = static_cast<int>(right_pixel[2]);
		right_blue = static_cast<int>(right_pixel[0]);
		renderer.resource().mesh_unload(quad);
		renderer.resource().texture_unload(texture);
	};
	int aces_left_red = 0;
	int aces_left_blue = 0;
	int aces_right_red = 0;
	int aces_right_blue = 0;
	int dual_left_red = 0;
	int dual_left_blue = 0;
	int dual_right_red = 0;
	int dual_right_blue = 0;
	halves_by(0, aces_left_red, aces_left_blue, aces_right_red, aces_right_blue);
	halves_by(5, dual_left_red, dual_left_blue, dual_right_red, dual_right_blue);
	RecordProperty("dual_region_aces_left_blue", aces_left_blue);
	RecordProperty("dual_region_aces_right_blue", aces_right_blue);
	RecordProperty("dual_region_dual_left_blue", dual_left_blue);
	RecordProperty("dual_region_dual_right_blue", dual_right_blue);
	RecordProperty("dual_region_dual_left_red", dual_left_red);
	RecordProperty("dual_region_dual_right_red", dual_right_red);
	//对照：算子 0 两侧都保持纯红（非红通道低）
	EXPECT_LE(aces_left_blue, 30) << "ACES 左半非红通道异常：" << aces_left_blue;
	EXPECT_LE(aces_right_blue, 30) << "ACES 右半非红通道异常：" << aces_right_blue;
	//双轨：左半 AgX 抬起非红通道，右半 ACES 保持纯色
	EXPECT_GE(dual_left_red, 150) << "双轨左半亮度异常：" << dual_left_red;
	EXPECT_GE(dual_right_red, 150) << "双轨右半亮度异常：" << dual_right_red;
	EXPECT_GE(dual_left_blue, 50) << "双轨左半未走 AgX（非红通道未抬起）：" << dual_left_blue;
	EXPECT_LE(dual_right_blue, 30) << "双轨右半未走逐通道 ACES：" << dual_right_blue;
	EXPECT_LT(dual_right_blue, dual_left_blue) << "双轨两侧曲线未区分";
}

//HDR 双轨分轨位置可配（第 774 轮）：split 0.9 使右半也落入 AgX 区域
TEST_F(Renderer_Scene_Test, HDR双轨分轨位置可配)
{
	std::vector<uint8_t> red_texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < red_texels.size(); index += 4)
	{
		red_texels[index + 0] = 255;
		red_texels[index + 3] = 255;
	}
	//按 split 出一帧，返回右半中心像素的蓝通道（0 = ACES 纯色、> 50 = AgX 已去饱和）
	auto right_blue_by = [&](float split) -> int
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		config.hdr_tone_operator = 5;
		config.hdr_dual_tone_split = split;
		EXPECT_TRUE(renderer.init(config));
		uint64_t texture = 0;
		EXPECT_TRUE(renderer.resource().texture_load(texture, red_texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.0f);
		engine::Mesh_Handle quad = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		std::array<uint8_t, 4> pixel = pixel_get(pixels, WINDOW_WIDTH * 3 / 4, WINDOW_HEIGHT / 2);
		renderer.resource().mesh_unload(quad);
		renderer.resource().texture_unload(texture);
		return static_cast<int>(pixel[0]);
	};
	int default_blue = right_blue_by(0.5f);
	int shifted_blue = right_blue_by(0.9f);
	RecordProperty("dual_split_default_blue", default_blue);
	RecordProperty("dual_split_shifted_blue", shifted_blue);
	//默认分轨：右半走 ACES，保持纯色
	EXPECT_LE(default_blue, 30) << "默认分轨右半未走 ACES：" << default_blue;
	//分轨移到 0.9：右半落入 AgX 区域，非红通道被抬起
	EXPECT_GE(shifted_blue, 50) << "分轨 0.9 未使右半走 AgX：" << shifted_blue;
	EXPECT_GT(shifted_blue, default_blue) << "分轨位置未生效";
}

/*
逐材质色调算子同帧（第 779 轮 · 通用引擎化整改）
同一帧左侧材质请求 AgX（4）、右侧材质请求 ACES（0），纹理纯蓝：
AgX 会把彩色高光去饱和（通道差收窄），ACES 保持纯色 —— 证明算子不再是全帧唯一。
另断言：材质不请求覆盖（0xFF）时与显式 ACES 材质逐字节一致（默认路径不变）。
*/
TEST_F(Renderer_Scene_Test, PBR逐材质色调算子同帧)
{
	//纯蓝纹理（RGBA8，2×2；index 0 = 蓝）
	std::vector<uint8_t> blue_texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < blue_texels.size(); index += 4)
	{
		blue_texels[index + 0] = 255;
		blue_texels[index + 3] = 255;
	}
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	config.hdr_targets = true;
	ASSERT_TRUE(renderer.init(config));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, blue_texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const float direction[3] = { 0.4f, -0.25f, -0.9f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.15f);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//三种材质：继承全局（0xFF）/ AgX（4）/ ACES（0）
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Handle inherit_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(inherit_handle, base));
	engine::Material_Data agx_data = base;
	agx_data.tone_operator = 4;
	engine::Material_Handle agx_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(agx_handle, agx_data));
	engine::Material_Data aces_data = base;
	aces_data.tone_operator = 0;
	engine::Material_Handle aces_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(aces_handle, aces_data));
	//左 / 右各一枚四边形的单帧
	auto frame_by = [&](engine::Material_Handle left_handle, engine::Material_Handle right_handle)
	{
		std::array<float, 16> left_matrix = quad_offset_matrix(-1.1f);
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		renderer.resource().material_bind_set(left_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, left_matrix.data()));
		renderer.resource().material_bind_set(right_handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};
	//半屏最大通道差（饱和度粗量）
	auto half_span = [](const std::vector<uint8_t>& pixels, uint32_t half) -> int
	{
		int span = 0;
		uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
		uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = begin; x < end; ++x)
			{
				std::array<uint8_t, 4> pixel = pixel_get(pixels, x, y);
				int high = static_cast<int>(*std::max_element(pixel.begin(), pixel.begin() + 3));
				int low = static_cast<int>(*std::min_element(pixel.begin(), pixel.begin() + 3));
				if (high - low > span)
					span = high - low;
			}
		}
		return span;
	};
	//左 AgX / 右 ACES
	std::vector<uint8_t> mixed = frame_by(agx_handle, aces_handle);
	int left_span = half_span(mixed, 0);
	int right_span = half_span(mixed, 1);
	RecordProperty("mat_tone_left_agx_span", left_span);
	RecordProperty("mat_tone_right_aces_span", right_span);
	//ACES 侧保持纯色，AgX 侧去饱和
	EXPECT_GE(right_span, 120) << "右侧 ACES 材质未保持纯色：" << right_span;
	EXPECT_LT(left_span, right_span - 30) << "左侧 AgX 材质未去饱和：AgX " << left_span
		<< " / ACES " << right_span;
	//继承材质（0xFF）与显式 ACES 材质逐字节一致
	std::vector<uint8_t> inherited = frame_by(inherit_handle, inherit_handle);
	std::vector<uint8_t> explicit_aces = frame_by(aces_handle, aces_handle);
	EXPECT_EQ(inherited, explicit_aces) << "未覆盖时画面与全局默认口径不一致";
	renderer.resource().material_unload(aces_handle);
	renderer.resource().material_unload(agx_handle);
	renderer.resource().material_unload(inherit_handle);
	renderer.resource().mesh_unload(quad);
	renderer.resource().texture_unload(texture);
}

/*
逐材质特性隔离（第 785 轮 · 整改验收补全）
口径：左右两枚球，全局特性全关；每次只让左球材质开启「各向异性 / 布料 sheen / 皮肤 SSS」之一，
断言只有左半画面变化、右半逐像素不变 —— 证明 bit2 / bit3 / bit4 的材质门控互相隔离且不污染其他物体。
*/
TEST_F(Renderer_Scene_Test, PBR特性逐材质隔离)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 24, 12));
	//斜射主光：球面明暗交界与轮廓都可见（sheen / SSS / 各向异性均可观测）
	const float direction[3] = { 0.4f, -0.25f, -0.9f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.15f);
	renderer.scene().specular_set(0.6f, 48.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_roughness_set(0.35f);
	renderer.scene().pbr_metallic_set(0.6f);
	//全局特性全关，强度先给足（仅材质开启时才生效）
	renderer.scene().pbr_aniso_set(false);
	renderer.scene().pbr_aniso_strength_set(0.8f);
	renderer.scene().pbr_sheen_set(false);
	renderer.scene().pbr_sheen_strength_set(0.8f);
	renderer.scene().pbr_skin_sss_set(false);
	renderer.scene().pbr_skin_wrap_set(0.8f);
	//基础材质与三种特性材质
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.35f;
	base.pbr_metallic = 0.6f;
	engine::Material_Handle base_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(base_handle, base));
	engine::Material_Data aniso = base;
	aniso.pbr_features_override = true;
	aniso.pbr_features = 1u << 2;
	engine::Material_Handle aniso_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(aniso_handle, aniso));
	engine::Material_Data sheen = base;
	sheen.pbr_features_override = true;
	sheen.pbr_features = 1u << 4;
	engine::Material_Handle sheen_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(sheen_handle, sheen));
	engine::Material_Data sss = base;
	sss.pbr_features_override = true;
	sss.pbr_features = 1u << 3;
	engine::Material_Handle sss_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(sss_handle, sss));
	//左 / 右两枚球：左球换材质、右球恒为基础材质（右半画面作为隔离对照）
	auto draw_frame = [&](engine::Material_Handle left, engine::Material_Handle right)
	{
		std::array<float, 16> left_matrix = quad_offset_matrix(-1.1f);
		std::array<float, 16> right_matrix = quad_offset_matrix(1.1f);
		renderer.resource().material_bind_set(left);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, left_matrix.data()));
		renderer.resource().material_bind_set(right);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, right_matrix.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};
	//半屏变化像素数（0 = 左半、1 = 右半）
	auto half_changed = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, uint32_t half) -> uint32_t
	{
		uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
		uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = begin; x < end; ++x)
			{
				std::array<uint8_t, 4> pa = pixel_get(a, x, y);
				std::array<uint8_t, 4> pb = pixel_get(b, x, y);
				int diff = std::abs(static_cast<int>(pa[0]) - static_cast<int>(pb[0]))
					+ std::abs(static_cast<int>(pa[1]) - static_cast<int>(pb[1]))
					+ std::abs(static_cast<int>(pa[2]) - static_cast<int>(pb[2]));
				if (diff >= 4)
					++count;
			}
		}
		return count;
	};
	//基线：两枚都用基础材质
	std::vector<uint8_t> baseline = draw_frame(base_handle, base_handle);
	//逐项只在左球启用，右球保持基础材质（右半必须逐像素不变）
	auto check_one = [&](engine::Material_Handle feature_handle, const char* label)
	{
		std::vector<uint8_t> frame = draw_frame(feature_handle, base_handle);
		uint32_t left_changed = half_changed(baseline, frame, 0);
		uint32_t right_changed = half_changed(baseline, frame, 1);
		EXPECT_GT(left_changed, 0u) << label << " 未改变左侧画面";
		EXPECT_EQ(right_changed, 0u) << label << " 串扰到右侧球";
		return left_changed;
	};
	uint32_t aniso_changed = check_one(aniso_handle, "各向异性");
	uint32_t sheen_changed = check_one(sheen_handle, "布料 sheen");
	uint32_t sss_changed = check_one(sss_handle, "皮肤 SSS");
	RecordProperty("two_left_aniso", static_cast<int>(aniso_changed));
	RecordProperty("two_left_sheen", static_cast<int>(sheen_changed));
	RecordProperty("two_left_sss", static_cast<int>(sss_changed));
	renderer.resource().material_unload(sss_handle);
	renderer.resource().material_unload(sheen_handle);
	renderer.resource().material_unload(aniso_handle);
	renderer.resource().material_unload(base_handle);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(texture);
}
/*
逐材质色调算子 · 透明材质安全（第 780 轮）
口径：算子编号借用场景 alpha 传递，只对不透明材质生效；半透明材质即使声明 tone_operator 也不得改写 alpha，
故「半透明材质 ＋ tone 4」与「同材质继承全局」必须逐字节一致（否则混合结果会被破坏）。
*/
TEST_F(Renderer_Scene_Test, PBR逐材质色调算子透明材质安全)
{
	//半透明白纹理（alpha 128）：让混合结果对 alpha 敏感
	std::vector<uint8_t> texels(2 * 2 * 4, 255);
	for (std::size_t index = 0; index < texels.size(); index += 4)
		texels[index + 3] = 128;
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	config.hdr_targets = true;
	ASSERT_TRUE(renderer.init(config));
	BYJY_REQUIRE_READBACK(renderer);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const float direction[3] = { 0.4f, -0.25f, -0.9f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.15f);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//同一半透明材质：继承全局 / 声明 AgX
	engine::Material_Data base;
	base.texture = texture;
	base.blend = engine::Blend_Mode::Alpha;
	engine::Material_Handle inherit_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(inherit_handle, base));
	engine::Material_Data agx_data = base;
	agx_data.tone_operator = 4;
	engine::Material_Handle agx_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(agx_handle, agx_data));
	//各出一帧（同一几何与材质，仅色调声明不同）
	auto frame_by = [&](engine::Material_Handle handle)
	{
		renderer.resource().material_bind_set(handle);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};
	std::vector<uint8_t> inherited = frame_by(inherit_handle);
	std::vector<uint8_t> declared = frame_by(agx_handle);
	EXPECT_EQ(inherited, declared) << "半透明材质改写了 alpha（混合结果被破坏）";
	renderer.resource().material_unload(agx_handle);
	renderer.resource().material_unload(inherit_handle);
	renderer.resource().mesh_unload(quad);
	renderer.resource().texture_unload(texture);
}


