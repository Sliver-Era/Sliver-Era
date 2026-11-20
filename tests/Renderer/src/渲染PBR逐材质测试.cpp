/*
渲染PBR逐材质测试（2026-11-21 自「渲染PBR参数测试.cpp」拆出）
覆盖：逐材质色调算子同帧、PBR 特性逐材质隔离、透明材质安全、关闭 HDR 时忽略覆盖。
*/

#include "渲染PBR参数测试辅助.h"

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
	aniso.pbr_features = engine::PBR_FEATURE_ANISO;
	engine::Material_Handle aniso_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(aniso_handle, aniso));
	engine::Material_Data sheen = base;
	sheen.pbr_features_override = true;
	sheen.pbr_features = engine::PBR_FEATURE_SHEEN;
	engine::Material_Handle sheen_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(sheen_handle, sheen));
	engine::Material_Data sss = base;
	sss.pbr_features_override = true;
	sss.pbr_features = engine::PBR_FEATURE_SKIN_SSS;
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
/*
逐材质色调算子 · 关闭 HDR 时忽略覆盖（第 794 轮）
口径：编号借用 16F 场景 alpha 传递，8 位呈现路径承载不了编号；故 `hdr_targets = false` 时
「声明 AgX 的不透明材质」与「同材质继承全局」必须逐字节一致（能力关闭即回到引入前画面）。
*/
TEST_F(Renderer_Scene_Test, PBR逐材质色调算子关闭HDR时忽略覆盖)
{
	std::vector<uint8_t> texels(2 * 2 * 4, 255);
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	//8 位默认路径（不启用 HDR 场景目标）
	config.hdr_targets = false;
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
	engine::Material_Data base;
	base.texture = texture;
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
	/*
	8 位路径下编号会被量化（(id+1)×0.0005 → 0），仍写进未呈现的 alpha 通道，
	故只比较可见的 RGB 三通道：覆盖在关闭 HDR 时不得改变任何可见像素。
	*/
	ASSERT_EQ(inherited.size(), declared.size());
	int rgb_difference = 0;
	for (std::size_t index = 0; index + 3 < inherited.size(); index += 4)
	{
		for (std::size_t channel = 0; channel < 3; ++channel)
		{
			int difference = std::abs(static_cast<int>(inherited[index + channel])
				- static_cast<int>(declared[index + channel]));
			rgb_difference = (std::max)(rgb_difference, difference);
		}
	}
	EXPECT_EQ(rgb_difference, 0) << "关闭 HDR 时逐材质色调覆盖改变了可见画面（最大 RGB 差 "
		<< rgb_difference << "）";
	renderer.resource().material_unload(agx_handle);
	renderer.resource().material_unload(inherit_handle);
	renderer.resource().mesh_unload(quad);
	renderer.resource().texture_unload(texture);
}
