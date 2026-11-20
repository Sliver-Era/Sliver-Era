/*
渲染后处理 HDR 组合测试（2026-11-21 自「渲染后处理链测试.cpp」拆出）
覆盖：默认路径 MSAA 组合、泛光组合与算子矩阵、不隐式介入用户链、无残留链、
      显式泛光参数在尺寸重建后保持、半分辨率重建、算子查询与越界回落、浮点读回、多级链、场景目标保留高光。
*/
#include "渲染后处理链测试辅助.h"
#include "渲染后处理链装配辅助.h"
#include "测试辅助/渲染测试辅助.h"

/*
HDR 默认路径 · MSAA 组合（方向九第 766 轮）
口径：hdr_targets 开启时内部 16F 场景目标须跟随设备 MSAA 采样数建立多重采样附件，
场景 pass 结束时解析到单采样颜色，色调映射级照常采样该单采样结果。
断言：1) 帧统计报告的采样数为 4；2) 与单采样帧相比曲面边缘存在可观测差异（AA 生效）；
      3) 两级链路仍成立（HDR 默认路径只有一级色调映射）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR默认路径MSAA组合)
{
	//按采样数出一帧（球面轮廓提供曲面边缘）
	auto frame_by = [](uint32_t samples, std::vector<uint8_t>& pixels, uint32_t& reported_samples,
		uint32_t& reported_stages) -> bool
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		config.msaa_samples = samples;
		if (!renderer.init(config))
			return false;
		renderer.resource().texture_bind_set(0);
		EXPECT_TRUE(renderer.scene().exposure_set(1.0f));
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.3f);
		engine::Mesh_Handle sphere = 0;
		EXPECT_TRUE(renderer.resource().mesh_sphere_load(sphere, 1.0f, 32, 16));
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
		pixels.assign(pixel_bytes(), 0);
		bool rendered = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		reported_samples = renderer.frame_stats_get().msaa_samples;
		reported_stages = renderer.frame_stats_get().post_stages;
		renderer.resource().mesh_unload(sphere);
		return rendered;
	};
	//能力探测（不支持 4x 则跳过）
	{
		engine::Renderer probe;
		ASSERT_TRUE(probe.init(hidden_config()));
		if (probe.device().msaa_max_samples() < 4)
			GTEST_SKIP() << "本机 MSAA 上限不足 4";
	}
	std::vector<uint8_t> single;
	std::vector<uint8_t> multi;
	uint32_t single_samples = 0;
	uint32_t single_stages = 0;
	uint32_t multi_samples = 0;
	uint32_t multi_stages = 0;
	ASSERT_TRUE(frame_by(1, single, single_samples, single_stages));
	ASSERT_TRUE(frame_by(4, multi, multi_samples, multi_stages));
	uint32_t changed = byjy_render_test::changed_pixels(single, multi, 2);
	RecordProperty("hdr_msaa_single_samples", static_cast<int>(single_samples));
	RecordProperty("hdr_msaa_multi_samples", static_cast<int>(multi_samples));
	RecordProperty("hdr_msaa_stages", static_cast<int>(multi_stages));
	RecordProperty("hdr_msaa_changed_pixels", static_cast<int>(changed));
	//HDR 场景目标跟随设备采样数
	EXPECT_EQ(single_samples, 1u);
	EXPECT_EQ(multi_samples, 4u) << "HDR 场景目标未跟随 MSAA 采样数";
	//HDR 默认路径为单级色调映射
	EXPECT_EQ(single_stages, 1u);
	EXPECT_EQ(multi_stages, 1u);
	//曲面边缘应因 MSAA 产生差异（球体轮廓像素数远大于 0）
	EXPECT_GT(changed, 0u) << "MSAA 未改变曲面边缘";
}

/*
HDR 组合矩阵（方向九第 767 轮；第 778 轮改为显式装配）
口径：把「显式泛光 × 半分辨率 × MSAA」六种组合各出一帧，验证
1) 链路级数符合该配置（泛光 → 4 级、否则单级）；
2) 主体（四边形中心）亮度在全部组合间一致（±6）：泛光只动边缘、MSAA 只动轮廓、半分辨率只改中间层分辨率；
3) 全部组合都能正常出帧。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光组合矩阵)
{
	struct Combo
	{
		uint32_t samples;
		bool bloom;
		bool half_res;
	};
	const Combo combos[] = {
		{ 1, false, false }, { 4, false, false },
		{ 1, true, false }, { 4, true, false },
		{ 1, true, true }, { 4, true, true },
	};
	//能力探测（不支持 4x 时只跑单采样组合）
	uint32_t max_samples = 1;
	{
		engine::Renderer probe;
		ASSERT_TRUE(probe.init(hidden_config()));
		max_samples = probe.device().msaa_max_samples();
	}
	int reference_center = -1;
	for (const Combo& combo : combos)
	{
		//跳过本机不支持的采样数
		if (combo.samples > max_samples)
			continue;
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.msaa_samples = combo.samples;
		//无泛光组合走 HDR 默认路径（单级色调映射）；泛光组合由调用方显式装配
		config.hdr_targets = !combo.bloom;
		ASSERT_TRUE(renderer.init(config)) << "组合初始化失败（采样 " << combo.samples << "）";
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		Explicit_Bloom_Chain chain;
		if (combo.bloom)
		{
			engine::Bloom_Preset_Desc desc;
			desc.half_res = combo.half_res;
			ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc, combo.samples))
				<< "组合泛光预设建立失败（采样 " << combo.samples << "）";
			renderer.resource().pipeline_bind_set(chain.scene_pipeline);
		}
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
			<< "组合出帧失败（采样 " << combo.samples << "）";
		engine::Frame_Stats stats = renderer.frame_stats_get();
		int center = static_cast<int>(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
		RecordProperty("matrix_" + std::to_string(combo.samples) + "_" + (combo.bloom ? "bloom" : "plain")
			+ (combo.half_res ? "_half" : "") + "_center", center);
		RecordProperty("matrix_" + std::to_string(combo.samples) + "_" + (combo.bloom ? "bloom" : "plain")
			+ (combo.half_res ? "_half" : "") + "_stages", static_cast<int>(stats.post_stages));
		//链路级数：泛光 4 级、否则单级
		EXPECT_EQ(stats.post_stages, combo.bloom ? 4u : 1u)
			<< "组合级数不符（采样 " << combo.samples << "，泛光 " << combo.bloom << "）";
		//采样数由目标如实上报
		EXPECT_EQ(stats.msaa_samples, combo.samples)
			<< "组合采样数未生效（期望 " << combo.samples << "）";
		//主体亮度基线
		if (reference_center < 0)
			reference_center = center;
		else
			EXPECT_LE(std::abs(center - reference_center), 6) << "组合改变了主体亮度："
				<< center << " vs " << reference_center;
		if (combo.bloom)
			explicit_bloom_chain_destroy(renderer, chain);
		renderer.resource().mesh_unload(handle);
	}
	ASSERT_GE(reference_center, 0);
}

/*
HDR 泛光不损失高光 · 算子矩阵（方向九第 768 轮）
口径：泛光合成去掉上钳后（767），开泛光不应让主体变暗 —— 对每个色调算子各出一对帧（关 / 开泛光），
断言「开泛光中心 ≥ 关泛光中心 − 2」，并核对链路级数（1 / 4）。
覆盖算子：0 ACES、1 胶片、2 指数、3 扩展 Reinhard、4 AgX。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光不损失高光算子矩阵)
{
	//按（算子, 是否泛光）出一帧，返回中心亮度与链路级数
	auto frame_by = [](uint32_t tone_operator, bool bloom, int& center_out, uint32_t& stages_out) -> bool
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_tone_operator = tone_operator;
		//无泛光：HDR 默认路径（内置算子选择）；泛光：调用方显式装配同算子末级
		config.hdr_targets = !bloom;
		if (!renderer.init(config))
			return false;
		renderer.resource().texture_bind_set(0);
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		Explicit_Bloom_Chain chain;
		if (bloom)
		{
			engine::Bloom_Preset_Desc desc;
			if (!explicit_bloom_chain_build(renderer, chain, desc, 1, tone_operator))
				return false;
			renderer.resource().pipeline_bind_set(chain.scene_pipeline);
		}
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		if (!renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
			return false;
		center_out = static_cast<int>(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
		stages_out = renderer.frame_stats_get().post_stages;
		if (bloom)
			explicit_bloom_chain_destroy(renderer, chain);
		renderer.resource().mesh_unload(handle);
		return true;
	};
	for (uint32_t tone_operator = 0; tone_operator <= 4; ++tone_operator)
	{
		int plain_center = 0;
		int bloom_center = 0;
		uint32_t plain_stages = 0;
		uint32_t bloom_stages = 0;
		ASSERT_TRUE(frame_by(tone_operator, false, plain_center, plain_stages))
			<< "算子 " << tone_operator << " 关泛光出帧失败";
		ASSERT_TRUE(frame_by(tone_operator, true, bloom_center, bloom_stages))
			<< "算子 " << tone_operator << " 开泛光出帧失败";
		RecordProperty("highlight_op" + std::to_string(tone_operator) + "_plain", plain_center);
		RecordProperty("highlight_op" + std::to_string(tone_operator) + "_bloom", bloom_center);
		//链路级数
		EXPECT_EQ(plain_stages, 1u) << "算子 " << tone_operator << " 关泛光未走单级路径";
		EXPECT_EQ(bloom_stages, 4u) << "算子 " << tone_operator << " 开泛光未走四级链";
		//开泛光不得压低主体高光
		EXPECT_GE(bloom_center, plain_center - 2) << "算子 " << tone_operator
			<< " 开泛光后主体变暗：" << bloom_center << " vs " << plain_center;
	}
}

/*
HDR 不隐式介入用户链（第 778 轮；取代方向九第 770 轮的「自动泛光不覆盖用户链」）
口径：引擎不再自动填充后处理链。用户设两级链（拷贝 / 色调映射）但不设额外目标时，
本帧按既有规则回落单级，链本身保持 2 级且多帧不被任何隐式逻辑改写。
*/
TEST_F(Renderer_Post_Chain_Test, HDR不隐式介入用户链)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	config.hdr_targets = true;
	ASSERT_TRUE(renderer.init(config));
	//用户自设两级链（拷贝 / 色调映射）；不设额外目标，渲染时按既有规则回落单级
	uint64_t copy_stage = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t tone_stage = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(copy_stage, 0u);
	ASSERT_NE(tone_stage, 0u);
	uint64_t stages[2] = { copy_stage, tone_stage };
	ASSERT_TRUE(renderer.device().post_chain_set(stages, 2));
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	uint32_t chain_after = renderer.device().post_chain_count();
	uint32_t stages_after = renderer.frame_stats_get().post_stages;
	RecordProperty("user_chain_count", static_cast<int>(chain_after));
	RecordProperty("user_chain_stages", static_cast<int>(stages_after));
	EXPECT_EQ(chain_after, 2u) << "引擎隐式改写了用户后处理链";
	EXPECT_EQ(stages_after, 1u) << "用户两级链（无额外目标）未按规则回落单级";
	//第二帧仍保持用户链
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.device().post_chain_count(), 2u) << "多帧后用户链被改写";
	renderer.resource().pipeline_unload(tone_stage);
	renderer.resource().pipeline_unload(copy_stage);
	renderer.resource().mesh_unload(handle);
}

//HDR 默认路径不残留链：未显式装配时链恒为空、多帧稳定为单级路径
TEST_F(Renderer_Post_Chain_Test, HDR默认路径无残留链)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	config.hdr_targets = true;
	ASSERT_TRUE(renderer.init(config));
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
			<< "第 " << frame << " 帧出帧失败";
		EXPECT_EQ(renderer.device().post_chain_count(), 0u) << "第 " << frame << " 帧出现残留链";
		EXPECT_EQ(renderer.frame_stats_get().post_stages, 1u) << "第 " << frame << " 帧未走单级路径";
	}
	renderer.resource().mesh_unload(handle);
}

/*
HDR 显式泛光参数在尺寸重建后保持（第 778 轮；取代方向九第 771 轮）
口径：尺寸变化由调用方释放并按新尺寸重建预设，重建时重新下发同一描述。
两种阈值（默认 0.6 生效 / 5.0 关闭）各跑「出帧 → 改尺寸 → 重建 → 再出帧」，
断言两尺寸下的边界外侧亮度都与该阈值对应的预期一致（0.6 → ≥150；5.0 → ≤120）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式泛光参数在尺寸重建后保持)
{
	//按阈值出一轮「改尺寸前后」两帧，返回两次边界外侧亮度
	auto probe_by = [](float threshold, int& before_out, int& after_out) -> bool
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		if (!renderer.init(config))
			return false;
		renderer.resource().texture_bind_set(0);
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		//边界外侧亮度：首个 < 220 的像素（主体 232），取其值
		auto halo_of = [](const std::vector<uint8_t>& image, uint32_t width, uint32_t height) -> int
		{
			for (uint32_t x = width / 2; x < width; ++x)
				if (pixel_get(image, x, height / 2, width)[0] < 220)
					return static_cast<int>(pixel_get(image, x, height / 2, width)[0]);
			return -1;
		};
		engine::Bloom_Preset_Desc desc;
		desc.threshold = threshold;
		Explicit_Bloom_Chain chain;
		if (!explicit_bloom_chain_build(renderer, chain, desc))
			return false;
		renderer.resource().pipeline_bind_set(chain.scene_pipeline);
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> before(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(before.data(), static_cast<uint32_t>(before.size())));
		before_out = halo_of(before, WINDOW_WIDTH, WINDOW_HEIGHT);
		//改尺寸：先释放旧链 → 触发交换链重建 → 按新尺寸重建预设（同描述重新下发参数）
		EXPECT_TRUE(renderer.device().window_size_set(96, 54));
		explicit_bloom_chain_destroy(renderer, chain);
		EXPECT_TRUE(renderer.frame_render());
		if (!explicit_bloom_chain_build(renderer, chain, desc))
			return false;
		renderer.resource().pipeline_bind_set(chain.scene_pipeline);
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> after(pixel_bytes(96, 54));
		EXPECT_TRUE(renderer.frame_render(after.data(), static_cast<uint32_t>(after.size())));
		after_out = halo_of(after, 96, 54);
		explicit_bloom_chain_destroy(renderer, chain);
		renderer.resource().mesh_unload(handle);
		return true;
	};
	int default_before = 0;
	int default_after = 0;
	int high_before = 0;
	int high_after = 0;
	ASSERT_TRUE(probe_by(0.6f, default_before, default_after));
	ASSERT_TRUE(probe_by(5.0f, high_before, high_after));
	RecordProperty("param_resize_default_before", default_before);
	RecordProperty("param_resize_default_after", default_after);
	RecordProperty("param_resize_high_before", high_before);
	RecordProperty("param_resize_high_after", high_after);
	//默认阈值：两尺寸下都有泛光
	EXPECT_GE(default_before, 150) << "默认阈值在默认尺寸下未生效：" << default_before;
	EXPECT_GE(default_after, 150) << "默认阈值在改尺寸后丢失：" << default_after;
	//高阈值：两尺寸下都无泛光（边界即清屏色）
	ASSERT_GE(high_before, 0);
	ASSERT_GE(high_after, 0);
	EXPECT_LE(high_before, 120) << "高阈值在默认尺寸下未关闭泛光：" << high_before;
	EXPECT_LE(high_after, 120) << "高阈值在改尺寸后失效：" << high_after;
}

/*
HDR 显式泛光半分辨率随尺寸重建（第 778 轮；取代方向九第 772 轮）
口径：半分辨率描述与「调用方按新尺寸重建预设」组合 —— 重建后提取 / 模糊两个 1/2 边长目标按新尺寸建立，
叠加级保持全尺寸。断言：两尺寸下链路级数 4、边界外侧亮度满足 150 / 120。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式泛光半分辨率随尺寸重建)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	engine::Bloom_Preset_Desc desc;
	desc.half_res = true;
	Explicit_Bloom_Chain chain;
	ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
	renderer.resource().pipeline_bind_set(chain.scene_pipeline);
	//边界外侧两点（首个 < 220 的像素与其后一个）
	auto halo_pair = [](const std::vector<uint8_t>& image, uint32_t width, uint32_t height,
		int& inner_out, int& outer_out)
	{
		for (uint32_t x = width / 2; x < width; ++x)
		{
			if (pixel_get(image, x, height / 2, width)[0] < 220)
			{
				inner_out = static_cast<int>(pixel_get(image, x, height / 2, width)[0]);
				outer_out = x + 1 < width
					? static_cast<int>(pixel_get(image, x + 1, height / 2, width)[0]) : inner_out;
				return;
			}
		}
		inner_out = -1;
		outer_out = -1;
	};
	//默认尺寸
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> before(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(before.data(), static_cast<uint32_t>(before.size())));
	int before_inner = 0;
	int before_outer = 0;
	halo_pair(before, WINDOW_WIDTH, WINDOW_HEIGHT, before_inner, before_outer);
	uint32_t before_stages = renderer.frame_stats_get().post_stages;
	//改尺寸：释放旧链 → 触发交换链重建 → 按新尺寸重建同描述预设
	ASSERT_TRUE(renderer.device().window_size_set(96, 54));
	explicit_bloom_chain_destroy(renderer, chain);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
	renderer.resource().pipeline_bind_set(chain.scene_pipeline);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> after(pixel_bytes(96, 54));
	uint32_t read_width = 0;
	uint32_t read_height = 0;
	ASSERT_TRUE(renderer.frame_render(after.data(), static_cast<uint32_t>(after.size()), &read_width, &read_height));
	int after_inner = 0;
	int after_outer = 0;
	halo_pair(after, 96, 54, after_inner, after_outer);
	uint32_t after_stages = renderer.frame_stats_get().post_stages;
	RecordProperty("half_resize_before_halo", before_inner * 1000 + before_outer);
	RecordProperty("half_resize_after_halo", after_inner * 1000 + after_outer);
	RecordProperty("half_resize_before_stages", static_cast<int>(before_stages));
	RecordProperty("half_resize_after_stages", static_cast<int>(after_stages));
	//两尺寸均走四级链
	EXPECT_EQ(before_stages, 4u);
	EXPECT_EQ(after_stages, 4u) << "改尺寸后半分辨率未走四级链";
	//两尺寸下都有泛光（半分辨率特征：两点接近）
	EXPECT_GE(before_inner, 150) << "默认尺寸半分辨率无泛光：" << before_inner;
	EXPECT_GE(before_outer, 120) << "默认尺寸半分辨率扩散不足：" << before_outer;
	EXPECT_GE(after_inner, 150) << "改尺寸后半分辨率无泛光：" << after_inner;
	EXPECT_GE(after_outer, 120) << "改尺寸后半分辨率扩散不足：" << after_outer;
	explicit_bloom_chain_destroy(renderer, chain);
	renderer.resource().mesh_unload(handle);
}

/*
HDR 算子查询与越界回落（方向九第 776 轮）
口径：device().hdr_tone_operator() 返回生效值；配置越界（6）回落 0；算子 5 原样返回。
*/
TEST_F(Renderer_Post_Chain_Test, HDR算子查询与越界回落)
{
	//按配置算子建渲染器并回读生效值
	auto operator_by = [](uint32_t configured) -> uint32_t
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		config.hdr_tone_operator = configured;
		EXPECT_TRUE(renderer.init(config));
		return renderer.device().hdr_tone_operator();
	};
	uint32_t dual = operator_by(5);
	uint32_t overflow = operator_by(6);
	uint32_t ac_s = operator_by(0);
	RecordProperty("tone_query_dual", static_cast<int>(dual));
	RecordProperty("tone_query_overflow", static_cast<int>(overflow));
	//算子 5 原样返回
	EXPECT_EQ(dual, 5u) << "双轨算子未如实回读";
	//越界回落 ACES
	EXPECT_EQ(overflow, 0u) << "越界算子未回落 0";
	EXPECT_EQ(ac_s, 0u);
}

/*
HDR 浮点读回（方向九第 752 轮）
口径：16F 场景目标（曝光 +2）经浮点读回可读到 >1 的原始线性值；
8 位目标读回按 /255 归一，恒 ≤1。
*/
TEST_F(Renderer_Post_Chain_Test, HDR浮点读回)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config(320, 180);
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	//16F 目标 ＋ 16F 管线，离屏渲染
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	uint64_t hdr_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(hdr_target, width, height, hdr_format, depth_format, 1));
	uint64_t hdr_pipeline = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(hdr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
	renderer.resource().pipeline_bind_set(hdr_pipeline);
	renderer.device().render_target_set(hdr_target);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	std::vector<float> hdr_values(static_cast<std::size_t>(width) * height * 4);
	uint32_t read_width = 0;
	uint32_t read_height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_float(hdr_target, hdr_values.data(),
		static_cast<uint32_t>(hdr_values.size()), &read_width, &read_height));
	ASSERT_EQ(read_width, width);
	ASSERT_EQ(read_height, height);
	const std::size_t center = (static_cast<std::size_t>(height / 2) * width + width / 2) * 4;
	EXPECT_GT(hdr_values[center], 1.5f) << "16F 浮点读回未保留 >1 的线性值（实测 " << hdr_values[center] << "）";
	EXPECT_NEAR(hdr_values[center], 4.0f, 0.5f) << "16F 浮点读回数值偏离曝光 +2 的预期";
	//8 位目标：归一后 ≤1
	renderer.resource().pipeline_bind_set(0);
	uint64_t ldr_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(ldr_target, width, height));
	renderer.device().render_target_set(ldr_target);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	std::vector<float> ldr_values(static_cast<std::size_t>(width) * height * 4);
	ASSERT_TRUE(renderer.resource().render_target_readback_float(ldr_target, ldr_values.data(),
		static_cast<uint32_t>(ldr_values.size()), &read_width, &read_height));
	const std::size_t ldr_center = (static_cast<std::size_t>(read_height / 2) * read_width + read_width / 2) * 4;
	EXPECT_LE(ldr_values[ldr_center], 1.0f) << "8 位浮点读回未归一";
	EXPECT_GE(ldr_values[ldr_center], 0.9f) << "8 位浮点读回数值异常（实测 " << ldr_values[ldr_center] << "）";
	RecordProperty("hdr_float_center", hdr_values[center]);
	RecordProperty("ldr_float_center", ldr_values[ldr_center]);
	renderer.device().render_target_set(0);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(hdr_pipeline);
	renderer.resource().render_target_destroy(ldr_target);
	renderer.resource().render_target_destroy(hdr_target);
}

/*
HDR 多级链传递（方向九第 746 轮）
口径：场景 16F → 第 0 级拷贝到中间目标 → 第 1 级 ACES → 交换链。
中间目标为 16F 时高光穿过两级仍保留（中心 ≥ 242）；
中间目标为 8 位时在第 0 级写入处被截断（约 232）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR多级链传递)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t ldr_format = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	//场景目标（16F）与两条场景管线变体
	uint64_t scene_hdr = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_hdr,
		renderer.device().extent_width(), renderer.device().extent_height(), hdr_format, depth_format, 1));
	uint64_t scene_pipeline = 0;
	ASSERT_TRUE(renderer.resource().scene_pipeline_variant_load(scene_pipeline, hdr_format, 1, depth_format));
	//中间目标：16F 与 8 位各一
	uint64_t mid_hdr = 0;
	uint64_t mid_ldr = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(mid_hdr,
		renderer.device().extent_width(), renderer.device().extent_height(), hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(mid_ldr,
		renderer.device().extent_width(), renderer.device().extent_height(), ldr_format, depth_format, 1));
	//第 0 级：拷贝（16F 与 8 位两条变体）；第 1 级：ACES（8 位，写交换链）
	uint64_t copy_hdr = 0;
	uint64_t copy_ldr = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(copy_hdr, "后处理.vert.spv", "后处理.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, hdr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(copy_ldr, "后处理.vert.spv", "后处理.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, ldr_format));
	uint64_t tone = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	renderer.device().render_target_present_set(scene_hdr);
	renderer.resource().pipeline_bind_set(scene_pipeline);
	//按「第 0 级拷贝管线 ＋ 中间目标」出帧
	auto frame_by = [&](uint64_t copy_stage, uint64_t intermediate, std::vector<uint8_t>& pixels)
	{
		uint64_t stages[2] = { copy_stage, tone };
		EXPECT_TRUE(renderer.device().post_chain_set(stages, 2));
		uint64_t chain_targets[2] = { scene_hdr, intermediate };
		EXPECT_TRUE(renderer.device().render_target_present_chain_set(chain_targets, 2));
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	std::vector<uint8_t> hdr_pixels(pixel_bytes());
	std::vector<uint8_t> ldr_pixels(pixel_bytes());
	frame_by(copy_hdr, mid_hdr, hdr_pixels);
	frame_by(copy_ldr, mid_ldr, ldr_pixels);
	int hdr_center = static_cast<int>(pixel_get(hdr_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	int ldr_center = static_cast<int>(pixel_get(ldr_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	EXPECT_GE(ldr_center, 220) << "8 位中间态基线异常：" << ldr_center;
	EXPECT_LE(ldr_center, 240) << "8 位中间态基线异常：" << ldr_center;
	EXPECT_GE(hdr_center, 242) << "16F 中间目标未保留高光（8 位 " << ldr_center
		<< " / 16F " << hdr_center << "）";
	EXPECT_GT(hdr_center, ldr_center) << "中间目标格式未产生差异";
	RecordProperty("chain_ldr_center", ldr_center);
	RecordProperty("chain_hdr_center", hdr_center);
	renderer.resource().pipeline_bind_set(0);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().pipeline_unload(copy_ldr);
	renderer.resource().pipeline_unload(copy_hdr);
	renderer.resource().pipeline_unload(scene_pipeline);
	renderer.resource().render_target_destroy(mid_ldr);
	renderer.resource().render_target_destroy(mid_hdr);
	renderer.resource().render_target_destroy(scene_hdr);
}
