/*
PBR 性能基线（2026-11-17 · 方向九阶段 A 第 712 轮）
测量 PBR 开关与实例规模、材质切换的 CPU 帧耗时；只记录数值并做宽松健全性断言，
阈值以 Release 基线为准，Debug 数值仅作相对参考。
*/
#include "渲染场景测试辅助.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
	//测 N 帧的中位耗时（毫秒；含 3 帧预热）
	double median_frame_ms(engine::Renderer& renderer, int frames)
	{
		for (int index = 0; index < 3; ++index)
			renderer.frame_render();
		std::vector<double> samples;
		for (int index = 0; index < frames; ++index)
		{
			auto begin = std::chrono::steady_clock::now();
			renderer.frame_render();
			auto end = std::chrono::steady_clock::now();
			samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
		}
		std::sort(samples.begin(), samples.end());
		return samples[samples.size() / 2];
	}

	//实例规模场景：返回中位帧耗时；初始化失败返回 -1
	//feature_mask 位：0 = 多重散射、1 = 环境 BRDF、2 = 各向异性、3 = 皮肤 wrap、4 = 布料 sheen
	double run_instance_case(uint32_t instances, bool pbr, uint32_t feature_mask = 0)
	{
		engine::Renderer renderer;
		//实例容量按用例规模放宽（默认 256）；关闭 vsync / 校验层以测真实开销
		engine::Renderer_Config config = hidden_config(640, 360);
		config.instance_capacity = instances < 256 ? 256 : instances + 16;
		config.vsync = false;
		config.validation = false;
		if (!renderer.init(config))
			return -1.0;
		engine::Mesh_Handle mesh = 0;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_gray();
		if (!renderer.resource().texture_load(texture, texels.data(), 2, 2))
			return -1.0;
		renderer.resource().texture_bind_set(texture);
		if (!renderer.resource().mesh_load(mesh, cube_data(0.5f)))
			return -1.0;
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.5f);
		renderer.scene().specular_set(1.0f, 64.0f);
		renderer.scene().pbr_enable_set(pbr);
		renderer.scene().pbr_roughness_absolute_set(true);
		renderer.scene().pbr_roughness_set(0.5f);
		renderer.scene().pbr_metallic_set(0.2f);
		//按位开启方向九新增特性
		renderer.scene().pbr_multiscatter_set((feature_mask & 1u) != 0);
		renderer.scene().pbr_env_brdf_set((feature_mask & 2u) != 0);
		renderer.scene().pbr_aniso_set((feature_mask & 4u) != 0);
		renderer.scene().pbr_skin_sss_set((feature_mask & 8u) != 0);
		renderer.scene().pbr_sheen_set((feature_mask & 16u) != 0);
		if (!renderer.resource().mesh_draw_instanced(mesh, nullptr, instances))
			return -1.0;
		double ms = median_frame_ms(renderer, 20);
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
		return ms;
	}

	//材质切换场景：200 个绘制项，单材质或双材质交替
	double run_material_case(bool alternate)
	{
		engine::Renderer renderer;
		//关闭 vsync / 校验层以测真实开销
		engine::Renderer_Config config = hidden_config(320, 180);
		config.vsync = false;
		config.validation = false;
		if (!renderer.init(config))
			return -1.0;
		engine::Mesh_Handle mesh = 0;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_gray();
		if (!renderer.resource().texture_load(texture, texels.data(), 2, 2))
			return -1.0;
		renderer.resource().texture_bind_set(texture);
		if (!renderer.resource().mesh_load(mesh, cube_data(0.5f)))
			return -1.0;
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.5f);
		renderer.scene().specular_set(1.0f, 64.0f);
		renderer.scene().pbr_enable_set(true);
		renderer.scene().pbr_roughness_absolute_set(true);
		engine::Material_Data data_a;
		data_a.pbr_override = true;
		data_a.pbr_roughness = 0.2f;
		data_a.texture = texture;
		engine::Material_Data data_b = data_a;
		data_b.pbr_roughness = 0.8f;
		engine::Material_Handle material_a = 0;
		engine::Material_Handle material_b = 0;
		if (!renderer.resource().material_load(material_a, data_a))
			return -1.0;
		if (!renderer.resource().material_load(material_b, data_b))
			return -1.0;
		for (uint32_t index = 0; index < 200; ++index)
		{
			//始终绑定 A 或 A/B 交替
			renderer.resource().material_bind_set(alternate && (index % 2 == 1) ? material_b : material_a);
			if (!renderer.resource().mesh_draw(mesh, nullptr))
				return -1.0;
		}
		double ms = median_frame_ms(renderer, 20);
		renderer.resource().material_unload(material_b);
		renderer.resource().material_unload(material_a);
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
		return ms;
	}
}

//实例规模：1 / 100 / 1000，PBR 关 / 开
TEST_F(Renderer_Scene_Test, PBR实例规模开销)
{
	//各场景各建独立渲染器（绘制项无清空接口）
	double i1_off = run_instance_case(1, false);
	double i1_on = run_instance_case(1, true);
	double i100_off = run_instance_case(100, false);
	double i100_on = run_instance_case(100, true);
	double i1000_off = run_instance_case(1000, false);
	double i1000_on = run_instance_case(1000, true);
	EXPECT_GT(i1_off, 0.0);
	EXPECT_GT(i1_on, 0.0);
	EXPECT_GT(i100_off, 0.0);
	EXPECT_GT(i100_on, 0.0);
	EXPECT_GT(i1000_off, 0.0);
	EXPECT_GT(i1000_on, 0.0);
	RecordProperty("i1_off_ms", i1_off);
	RecordProperty("i1_on_ms", i1_on);
	RecordProperty("i100_off_ms", i100_off);
	RecordProperty("i100_on_ms", i100_on);
	RecordProperty("i1000_off_ms", i1000_off);
	RecordProperty("i1000_on_ms", i1000_on);
}

//材质切换：200 项单材质 vs 双材质交替
TEST_F(Renderer_Scene_Test, PBR材质切换开销)
{
	double single = run_material_case(false);
	double alternating = run_material_case(true);
	EXPECT_GT(single, 0.0);
	EXPECT_GT(alternating, 0.0);
	RecordProperty("material_single_ms", single);
	RecordProperty("material_alternating_ms", alternating);
}

/*
方向九特性组合开销（第 725 轮）：1000 个实例、PBR 开启，逐项与全开对比基线。
仅记录数值并做健全性断言；不设硬阈值（Debug/Release 与机型差异较大）。
*/
TEST_F(Renderer_Scene_Test, PBR特性组合开销)
{
	double baseline = run_instance_case(1000, true, 0);
	double multiscatter = run_instance_case(1000, true, 1);
	double env_brdf = run_instance_case(1000, true, 2);
	double aniso = run_instance_case(1000, true, 4);
	double all = run_instance_case(1000, true, 31);
	EXPECT_GT(baseline, 0.0);
	EXPECT_GT(multiscatter, 0.0);
	EXPECT_GT(env_brdf, 0.0);
	EXPECT_GT(aniso, 0.0);
	EXPECT_GT(all, 0.0);
	RecordProperty("feature_baseline_ms", baseline);
	RecordProperty("feature_multiscatter_ms", multiscatter);
	RecordProperty("feature_env_brdf_ms", env_brdf);
	RecordProperty("feature_aniso_ms", aniso);
	RecordProperty("feature_all_ms", all);
}

/*
色调映射算子开销（方向九第 749 轮）
口径：640×360、单级后处理链、白四边形；逐算子测 CPU 帧耗时中位数（含每帧重新登记绘制）。
仅记录数值并做健全性断言，不设硬阈值（机型 / Debug-Release 差异较大）。
*/
namespace
{
	//单算子场景：返回中位帧耗时（毫秒；失败返回 -1）
	double run_tonemap_case(const char* fragment_shader)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config(640, 360);
		config.vsync = false;
		config.validation = false;
		if (!renderer.init(config))
			return -1.0;
		uint64_t scene_target = 0;
		if (!renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()))
			return -1.0;
		uint64_t stage = 0;
		if (!renderer.resource().pipeline_load(stage, "后处理.vert.spv", fragment_shader))
			return -1.0;
		renderer.device().post_chain_set(&stage, 1);
		renderer.resource().texture_bind_set(0);
		engine::Mesh_Handle handle = 0;
		if (!renderer.resource().mesh_load(handle, quad_data()))
			return -1.0;
		renderer.device().render_target_present_set(scene_target);
		//预热 3 帧
		for (int index = 0; index < 3; ++index)
		{
			if (!renderer.resource().mesh_draw(handle, nullptr) || !renderer.frame_render())
				return -1.0;
		}
		std::vector<double> samples;
		for (int index = 0; index < 20; ++index)
		{
			auto begin = std::chrono::steady_clock::now();
			//每帧重新登记绘制（绘制项不跨帧保留）
			if (!renderer.resource().mesh_draw(handle, nullptr) || !renderer.frame_render())
				return -1.0;
			auto end = std::chrono::steady_clock::now();
			samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
		}
		std::sort(samples.begin(), samples.end());
		double ms = samples[samples.size() / 2];
		renderer.resource().mesh_unload(handle);
		renderer.resource().pipeline_unload(stage);
		renderer.resource().render_target_destroy(scene_target);
		return ms;
	}
}

TEST_F(Renderer_Scene_Test, Tonemap算子开销)
{
	double aces = run_tonemap_case("色调映射.frag.spv");
	double filmic = run_tonemap_case("胶片色调映射.frag.spv");
	double reinhard = run_tonemap_case("Reinhard扩展色调映射.frag.spv");
	double exponent = run_tonemap_case("指数色调映射.frag.spv");
	EXPECT_GT(aces, 0.0);
	EXPECT_GT(filmic, 0.0);
	EXPECT_GT(reinhard, 0.0);
	EXPECT_GT(exponent, 0.0);
	RecordProperty("tonemap_aces_ms", aces);
	RecordProperty("tonemap_filmic_ms", filmic);
	RecordProperty("tonemap_reinhard_ms", reinhard);
	RecordProperty("tonemap_exponent_ms", exponent);
}

/*
强光照压力场景（第 726 轮）：32 盏点光 ＋ 1000 实例 ＋ 环境光照 ＋ 全部 PBR 特性。
核对可渲染并记录帧耗时；用于阶段 F「强光照 / 多物体」的最低验收。
*/
TEST_F(Renderer_Scene_Test, PBR强光照压力场景)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config(640, 360);
	config.instance_capacity = 1024;
	config.vsync = false;
	config.validation = false;
	ASSERT_TRUE(renderer.init(config));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(0.5f)));
	//32 盏点光环绕场景
	const float light_color[3] = { 1.0f, 0.85f, 0.7f };
	for (uint32_t index = 0; index < 32; ++index)
	{
		float angle = 6.2831853f * static_cast<float>(index) / 32.0f;
		const float position[3] = { 3.0f * std::cos(angle), 1.5f, 3.0f * std::sin(angle) };
		renderer.scene().light_point_set(position, light_color, 6.0f, 0.35f, 0.0f);
	}
	const float direction[3] = { 0.0f, -0.5f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 0.4f, 0.05f);
	//PBR ＋ 环境光照 ＋ 全特性
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_absolute_set(true);
	renderer.scene().pbr_roughness_set(0.4f);
	renderer.scene().pbr_metallic_set(0.3f);
	const float environment[3] = { 0.4f, 0.45f, 0.5f };
	renderer.scene().environment_set(environment, 0.6f);
	renderer.scene().pbr_multiscatter_set(true);
	renderer.scene().pbr_env_brdf_set(true);
	renderer.scene().pbr_env_irradiance_set(true);
	renderer.scene().pbr_aniso_set(true);
	renderer.scene().pbr_aniso_strength_set(0.6f);
	renderer.scene().pbr_skin_sss_set(true);
	renderer.scene().pbr_sheen_set(true);
	renderer.scene().pbr_sheen_strength_set(0.4f);
	ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, nullptr, 1000));
	double ms = median_frame_ms(renderer, 10);
	EXPECT_GT(ms, 0.0) << "强光照压力场景未能出帧";
	RecordProperty("stress_32lights_1000_instances_ms", ms);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
