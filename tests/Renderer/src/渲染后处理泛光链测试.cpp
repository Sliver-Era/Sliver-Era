/*
渲染后处理泛光链测试（2026-11-21 自「渲染后处理链测试.cpp」拆出）
覆盖：显式泛光预设、预设释放摘引用、显式尺寸、参数化、随尺寸重建、半分辨率。
*/
#include "渲染后处理链测试辅助.h"
#include "渲染后处理链装配辅助.h"
#include "测试辅助/渲染测试辅助.h"

/*
HDR 显式泛光预设（第 778 轮 · 通用引擎化整改；取代方向九第 761 轮的自动泛光）
口径：引擎不再在背后建泛光资源；调用方用 post_bloom_preset_build 建三目标三管线并自行订阅链。
断言：1) 未装配泛光时 HDR 默认路径只有一级色调映射（链为空）；2) 显式装配后四级链成立且边界外被提亮。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式泛光预设)
{
	//基线：只走 HDR 默认路径（不装配泛光）
	std::vector<uint8_t> without(pixel_bytes());
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(without.data(), static_cast<uint32_t>(without.size())));
		EXPECT_EQ(renderer.device().post_chain_count(), 0u) << "未装配泛光时链应恒为空";
		EXPECT_EQ(renderer.frame_stats_get().post_stages, 1u) << "未装配泛光时应只有一级色调映射";
		renderer.resource().mesh_unload(handle);
	}
	//显式装配泛光链
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	Explicit_Bloom_Chain chain;
	engine::Bloom_Preset_Desc desc;
	ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
	renderer.resource().pipeline_bind_set(chain.scene_pipeline);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> with(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(with.data(), static_cast<uint32_t>(with.size())));
	EXPECT_EQ(renderer.frame_stats_get().post_stages, 4u) << "显式泛光链应为四级";
	//从中心沿 +X 扫描第一个暗像素（无泛光帧，边界确定）
	uint32_t edge_x = WINDOW_WIDTH;
	for (uint32_t x = WINDOW_WIDTH / 2; x < WINDOW_WIDTH; ++x)
	{
		if (pixel_get(without, x, WINDOW_HEIGHT / 2)[0] < 100)
		{
			edge_x = x;
			break;
		}
	}
	ASSERT_LT(edge_x, WINDOW_WIDTH) << "未找到亮暗边界（四边形可能铺满视口）";
	uint32_t probe_x = edge_x + 2;
	if (probe_x >= WINDOW_WIDTH)
		probe_x = WINDOW_WIDTH - 1;
	int plain_probe = static_cast<int>(pixel_get(without, probe_x, WINDOW_HEIGHT / 2)[0]);
	int bloom_probe = static_cast<int>(pixel_get(with, probe_x, WINDOW_HEIGHT / 2)[0]);
	RecordProperty("bloom_explicit_edge_x", static_cast<int>(edge_x));
	RecordProperty("bloom_explicit_plain_probe", plain_probe);
	RecordProperty("bloom_explicit_bloom_probe", bloom_probe);
	EXPECT_GE(bloom_probe, plain_probe + 10) << "显式泛光未扩散到边界外：关闭 " << plain_probe
		<< " / 开启 " << bloom_probe;
	explicit_bloom_chain_destroy(renderer, chain);
	renderer.resource().mesh_unload(handle);
}

/*
泛光预设释放时摘除引用（第 787 轮）
口径：预设的目标仍挂在目标组里时释放，必须先把目标组清空再归还资源，
否则下一帧会引用已销毁目标。断言：链计数从 4 降到 1（仅末级保留），且释放后仍能正常出帧。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光预设释放摘引用)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	//场景目标 ＋ 场景管线 ＋ 末级色调
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	uint64_t scene_target = 0;
	uint64_t scene_pipeline = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_target, width, height,
		hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().scene_pipeline_variant_load(scene_pipeline, hdr_format, 1, depth_format));
	uint64_t tone_pipeline = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone_pipeline, 0u);
	engine::Bloom_Preset preset;
	engine::Bloom_Preset_Desc desc;
	ASSERT_TRUE(renderer.device().post_bloom_preset_build(preset, desc));
	uint64_t stages[4] = { preset.pipelines[0], preset.pipelines[1], preset.pipelines[2], tone_pipeline };
	ASSERT_TRUE(renderer.device().post_chain_set(stages, 4));
	uint64_t targets[4] = { scene_target, preset.targets[0], preset.targets[1], preset.targets[2] };
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(targets, 4));
	EXPECT_EQ(renderer.device().post_chain_count(), 4u);
	//释放预设：目标组应被清空、三条管线摘除，只剩末级
	renderer.device().post_bloom_preset_release(preset);
	EXPECT_EQ(renderer.device().post_chain_count(), 1u) << "释放后链中仍残留预设管线";
	std::vector<uint8_t> pixels(pixel_bytes());
	EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
		<< "释放预设后出帧失败（可能引用了已销毁目标）";
	renderer.resource().pipeline_unload(tone_pipeline);
	renderer.resource().pipeline_unload(scene_pipeline);
	renderer.resource().render_target_destroy(scene_target);
}


/*
泛光预设显式尺寸（第 783 轮）
口径：build 的 width / height 非 0 时按该尺寸建立三个中间目标，0 时取当前客户区；
半分辨率描述下提取 / 模糊取 1/2 边长、叠加级仍取全尺寸。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光预设显式尺寸)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	engine::Bloom_Preset preset;
	engine::Bloom_Preset_Desc desc;
	auto target_size = [&](uint64_t handle, uint32_t& width_out, uint32_t& height_out)
	{
		engine::Render_Target_Native_Info info{};
		ASSERT_TRUE(renderer.resource().render_target_native_get(handle, info));
		width_out = info.width;
		height_out = info.height;
	};
	//全分辨率显式 32×18
	ASSERT_TRUE(renderer.device().post_bloom_preset_build(preset, desc, 32, 18));
	for (std::size_t index = 0; index < 3; ++index)
	{
		uint32_t width = 0;
		uint32_t height = 0;
		target_size(preset.targets[index], width, height);
		EXPECT_EQ(width, 32u) << "第 " << index << " 个目标宽度不符";
		EXPECT_EQ(height, 18u) << "第 " << index << " 个目标高度不符";
	}
	//半分辨率：提取 / 模糊 16×9，叠加 32×18
	renderer.device().post_bloom_preset_release(preset);
	engine::Bloom_Preset_Desc half_desc;
	half_desc.half_res = true;
	ASSERT_TRUE(renderer.device().post_bloom_preset_build(preset, half_desc, 32, 18));
	uint32_t width = 0;
	uint32_t height = 0;
	target_size(preset.targets[0], width, height);
	EXPECT_EQ(width, 16u) << "半分辨率提取目标宽度不符";
	EXPECT_EQ(height, 9u) << "半分辨率提取目标高度不符";
	target_size(preset.targets[2], width, height);
	EXPECT_EQ(width, 32u) << "叠加级应保持全尺寸宽度";
	EXPECT_EQ(height, 18u) << "叠加级应保持全尺寸高度";
	//显式尺寸为 0：取当前客户区
	renderer.device().post_bloom_preset_release(preset);
	ASSERT_TRUE(renderer.device().post_bloom_preset_build(preset, desc));
	target_size(preset.targets[0], width, height);
	EXPECT_EQ(width, renderer.device().extent_width()) << "默认尺寸未取客户区宽度";
	EXPECT_EQ(height, renderer.device().extent_height()) << "默认尺寸未取客户区高度";
	renderer.device().post_bloom_preset_release(preset);
}

/*
HDR 显式泛光预设参数化（第 778 轮；取代方向九第 763 轮的自动泛光参数化）
口径：阈值 / 半径 / 强度随 Bloom_Preset_Desc 下发。阈值 5.0（高于 16F 高光 4.0）应使提取权重为 0、
测点回到无泛光水平；强度传 0（非法）应回落 1.0，画面与默认参数一致。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式泛光预设参数化)
{
	//按（阈值, 强度）出一帧，返回中心与边界外测点
	auto probes_by = [](float threshold, float intensity, int& center_out, int& probe_out)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		ASSERT_TRUE(renderer.init(config));
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		Explicit_Bloom_Chain chain;
		engine::Bloom_Preset_Desc desc;
		desc.threshold = threshold;
		desc.intensity = intensity;
		ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
		renderer.resource().pipeline_bind_set(chain.scene_pipeline);
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		center_out = static_cast<int>(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
		//边界沿用第 761 轮实测位置（同一几何）
		probe_out = static_cast<int>(pixel_get(pixels, 44, WINDOW_HEIGHT / 2)[0]);
		explicit_bloom_chain_destroy(renderer, chain);
		renderer.resource().mesh_unload(handle);
	};
	int default_center = 0;
	int default_probe = 0;
	int high_center = 0;
	int high_probe = 0;
	int fallback_center = 0;
	int fallback_probe = 0;
	probes_by(0.6f, 1.0f, default_center, default_probe);
	probes_by(5.0f, 1.0f, high_center, high_probe);
	probes_by(0.6f, 0.0f, fallback_center, fallback_probe);
	RecordProperty("bloom_param_default_probe", default_probe);
	RecordProperty("bloom_param_high_threshold_probe", high_probe);
	RecordProperty("bloom_param_fallback_probe", fallback_probe);
	//默认参数：边界外应有泛光
	EXPECT_GE(default_probe, 150) << "默认参数下显式泛光未生效：" << default_probe;
	//阈值高于高光：泛光消失（回到无泛光水平）
	EXPECT_LE(high_probe, default_probe - 50) << "阈值 5.0 未能关闭泛光：" << high_probe
		<< " / " << default_probe;
	//非法强度回落 1.0：与默认一致（±3）
	EXPECT_LE(std::abs(fallback_probe - default_probe), 3) << "非法强度未回落默认："
		<< fallback_probe << " / " << default_probe;
}

/*
HDR 显式泛光预设随尺寸重建（第 778 轮；取代方向九第 764 轮的自动随尺寸重建）
口径：预设持有的中间目标是显式资源，尺寸变化由调用方释放并按新尺寸重建，
引擎不再在帧内自动重建；重建后泛光仍生效（亮度沿边缘向外扩散）。
判读方式：主体亮度约 232，故以「首个亮度 < 220 的像素」定位亮区边界（即首个光晕像素），
再测其后一个像素；无泛光时边界处直接落入清屏色（≈99），有泛光时依次约为 213 / 167。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式泛光预设随尺寸重建)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//核心亮区边界：自中心沿 +X 找到首个亮度 < 220 的像素（列宽可不同；主体约 232）
	auto core_edge_x = [](const std::vector<uint8_t>& image, uint32_t width, uint32_t height) -> uint32_t
	{
		for (uint32_t x = width / 2; x < width; ++x)
		{
			if (pixel_get(image, x, height / 2, width)[0] < 220)
				return x;
		}
		return width;
	};
	//默认尺寸下装配并出两帧
	Explicit_Bloom_Chain chain;
	engine::Bloom_Preset_Desc desc;
	ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
	renderer.resource().pipeline_bind_set(chain.scene_pipeline);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> first(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(first.data(), static_cast<uint32_t>(first.size())));
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> second(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(second.data(), static_cast<uint32_t>(second.size())));
	uint32_t base_edge = core_edge_x(second, WINDOW_WIDTH, WINDOW_HEIGHT);
	ASSERT_LT(base_edge + 1, WINDOW_WIDTH) << "默认尺寸下未找到核心亮区边界";
	int base_halo_inner = static_cast<int>(pixel_get(second, base_edge, WINDOW_HEIGHT / 2)[0]);
	int base_halo_outer = static_cast<int>(pixel_get(second, base_edge + 1, WINDOW_HEIGHT / 2)[0]);
	RecordProperty("bloom_resize_base_edge", static_cast<int>(base_edge));
	RecordProperty("bloom_resize_base_halo_inner", base_halo_inner);
	RecordProperty("bloom_resize_base_halo_outer", base_halo_outer);
	EXPECT_GE(base_halo_inner, 150) << "同尺寸第二帧边界外侧无泛光：" << base_halo_inner;
	EXPECT_GE(base_halo_outer, 120) << "同尺寸第二帧泛光扩散不足：" << base_halo_outer;
	//改尺寸：调用方先释放旧链，再让交换链按新尺寸重建，然后按新尺寸重建预设
	ASSERT_TRUE(renderer.device().window_size_set(96, 54));
	explicit_bloom_chain_destroy(renderer, chain);
	ASSERT_TRUE(renderer.frame_render());
	const uint32_t new_width = 96;
	const uint32_t new_height = 54;
	ASSERT_EQ(renderer.device().extent_width(), new_width);
	ASSERT_EQ(renderer.device().extent_height(), new_height);
	ASSERT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
	renderer.resource().pipeline_bind_set(chain.scene_pipeline);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> resized(pixel_bytes(new_width, new_height));
	uint32_t read_width = 0;
	uint32_t read_height = 0;
	ASSERT_TRUE(renderer.frame_render(resized.data(), static_cast<uint32_t>(resized.size()), &read_width, &read_height));
	EXPECT_EQ(read_width, new_width);
	EXPECT_EQ(read_height, new_height);
	EXPECT_EQ(renderer.frame_stats_get().post_stages, 4u) << "尺寸变化后未走四级显式泛光链";
	uint32_t edge_x = core_edge_x(resized, new_width, new_height);
	ASSERT_LT(edge_x + 1, new_width) << "新尺寸下未找到核心亮区边界";
	int halo_inner = static_cast<int>(pixel_get(resized, edge_x, new_height / 2, new_width)[0]);
	int halo_outer = static_cast<int>(pixel_get(resized, edge_x + 1, new_height / 2, new_width)[0]);
	RecordProperty("bloom_resize_edge_x", static_cast<int>(edge_x));
	RecordProperty("bloom_resize_halo_inner", halo_inner);
	RecordProperty("bloom_resize_halo_outer", halo_outer);
	EXPECT_GE(halo_inner, 150) << "尺寸变化后边界外侧无泛光：" << halo_inner;
	EXPECT_GE(halo_outer, 120) << "尺寸变化后泛光扩散不足：" << halo_outer;
	//新尺寸第二帧：泛光链持续生效（排除过渡帧偶然）
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> resized_second(pixel_bytes(new_width, new_height));
	uint32_t second_width = 0;
	uint32_t second_height = 0;
	ASSERT_TRUE(renderer.frame_render(resized_second.data(), static_cast<uint32_t>(resized_second.size()),
		&second_width, &second_height));
	uint32_t second_edge = core_edge_x(resized_second, new_width, new_height);
	ASSERT_LT(second_edge + 1, new_width) << "新尺寸第二帧未找到核心亮区边界";
	int second_halo_inner = static_cast<int>(pixel_get(resized_second, second_edge, new_height / 2, new_width)[0]);
	int second_halo_outer = static_cast<int>(pixel_get(resized_second, second_edge + 1, new_height / 2, new_width)[0]);
	RecordProperty("bloom_resize_second_edge", static_cast<int>(second_edge));
	RecordProperty("bloom_resize_second_halo_inner", second_halo_inner);
	RecordProperty("bloom_resize_second_halo_outer", second_halo_outer);
	EXPECT_EQ(renderer.frame_stats_get().post_stages, 4u) << "新尺寸第二帧未走四级显式泛光链";
	EXPECT_GE(second_halo_inner, 150) << "新尺寸第二帧边界外侧无泛光：" << second_halo_inner;
	EXPECT_GE(second_halo_outer, 120) << "新尺寸第二帧泛光扩散不足：" << second_halo_outer;
	explicit_bloom_chain_destroy(renderer, chain);
	renderer.resource().mesh_unload(handle);
}

/*
HDR 显式泛光预设 · 半分辨率（第 778 轮；取代方向九第 765 轮的自动泛光半分辨率）
口径：Bloom_Preset_Desc.half_res 开启后「亮部提取 / 模糊」两个中间目标取 1/2 边长，「泛光叠加」仍按场景尺寸输出。
断言：1) 半分辨率下仍走四级链且边界外有泛光；2) 画面与全分辨率不同（证明开关生效）；
      3) 中心亮度与全分辨率接近（泛光只影响边缘扩散，不改写主体亮度）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式泛光预设半分辨率)
{
	//按半分辨率开关出一帧，返回像素与链级数
	auto frame_by = [](bool half_res, std::vector<uint8_t>& pixels) -> int
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		EXPECT_TRUE(renderer.init(config));
		renderer.resource().texture_bind_set(0);
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		Explicit_Bloom_Chain chain;
		engine::Bloom_Preset_Desc desc;
		desc.half_res = half_res;
		EXPECT_TRUE(explicit_bloom_chain_build(renderer, chain, desc));
		renderer.resource().pipeline_bind_set(chain.scene_pipeline);
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		pixels.assign(pixel_bytes(), 0);
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		int stages = static_cast<int>(renderer.frame_stats_get().post_stages);
		explicit_bloom_chain_destroy(renderer, chain);
		renderer.resource().mesh_unload(handle);
		return stages;
	};
	std::vector<uint8_t> full;
	std::vector<uint8_t> half;
	int full_stages = frame_by(false, full);
	int half_stages = frame_by(true, half);
	EXPECT_EQ(full_stages, 4);
	EXPECT_EQ(half_stages, 4) << "半分辨率下未走四级显式泛光链";
	//核心亮区边界（首个 < 220 的像素，主体约 232）与其后一个像素
	auto halo_by = [](const std::vector<uint8_t>& image, uint32_t& edge_out, int& inner_out, int& outer_out)
	{
		edge_out = WINDOW_WIDTH;
		for (uint32_t x = WINDOW_WIDTH / 2; x < WINDOW_WIDTH; ++x)
		{
			if (pixel_get(image, x, WINDOW_HEIGHT / 2)[0] < 220)
			{
				edge_out = x;
				break;
			}
		}
		if (edge_out + 1 >= WINDOW_WIDTH)
		{
			inner_out = 0;
			outer_out = 0;
			return;
		}
		inner_out = static_cast<int>(pixel_get(image, edge_out, WINDOW_HEIGHT / 2)[0]);
		outer_out = static_cast<int>(pixel_get(image, edge_out + 1, WINDOW_HEIGHT / 2)[0]);
	};
	uint32_t full_edge = 0;
	uint32_t half_edge = 0;
	int full_inner = 0;
	int full_outer = 0;
	int half_inner = 0;
	int half_outer = 0;
	halo_by(full, full_edge, full_inner, full_outer);
	halo_by(half, half_edge, half_inner, half_outer);
	int full_center = static_cast<int>(pixel_get(full, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	int half_center = static_cast<int>(pixel_get(half, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	RecordProperty("half_bloom_full_edge", static_cast<int>(full_edge));
	RecordProperty("half_bloom_half_edge", static_cast<int>(half_edge));
	RecordProperty("half_bloom_full_halo", full_inner * 1000 + full_outer);
	RecordProperty("half_bloom_half_halo", half_inner * 1000 + half_outer);
	RecordProperty("half_bloom_center_diff", std::abs(full_center - half_center));
	RecordProperty("half_bloom_changed_pixels", static_cast<int>(byjy_render_test::changed_pixels(full, half, 3)));
	ASSERT_LT(full_edge + 1, WINDOW_WIDTH) << "全分辨率下未找到核心亮区边界";
	ASSERT_LT(half_edge + 1, WINDOW_WIDTH) << "半分辨率下未找到核心亮区边界";
	//半分辨率下泛光仍生效
	EXPECT_GE(half_inner, 150) << "半分辨率边界外侧无泛光：" << half_inner;
	EXPECT_GE(half_outer, 120) << "半分辨率泛光扩散不足：" << half_outer;
	//开关生效：两帧存在可观测差异
	EXPECT_GT(byjy_render_test::changed_pixels(full, half, 3), 0u) << "半分辨率开关未改变画面";
	//主体亮度不被改写（泛光只作用于边缘扩散）
	EXPECT_LE(std::abs(full_center - half_center), 12) << "半分辨率改变了主体亮度："
		<< full_center << " / " << half_center;
}
