/*
渲染PBR双轨与分层测试（2026-11-21 自「渲染PBR参数测试.cpp」拆出）
覆盖：写实与二次元双轨同帧、PBR 特性逐物体分层、HDR 双轨同帧色彩保真 / 区域色调映射 / 分轨位置可配。
*/

#include "渲染PBR参数测试辅助.h"

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
	cloth.pbr_features = engine::PBR_FEATURE_SHEEN;
	engine::Material_Handle cloth_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(cloth_handle, cloth));
	//光泽度映射材质（bit5 ＋ gloss 1.0 → 本材质粗糙度压到下限）
	engine::Material_Data glossy = plain;
	glossy.pbr_features_override = true;
	glossy.pbr_features = engine::PBR_FEATURE_GLOSS_MAPPING;
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
