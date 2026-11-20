/*
渲染后处理编号通道测试（2026-11-21 自「渲染后处理链测试.cpp」拆出）
覆盖：编号附件驱动逐材质色调（单采样 / 多重采样）、中间级管线保留逐材质色调、回落算子查询、
      双轨不污染全局回落参数、泛光链保留逐材质色调、显式链逐材质色调。
*/
#include "渲染后处理链测试辅助.h"
#include "渲染后处理链装配辅助.h"
#include "测试辅助/渲染测试辅助.h"

/*
8 位路径段 4 · 编号附件驱动逐材质色调（第 812 轮）
口径：`post_tone_id_target_set(目标)` 后，材质色调映射级从编号附件读 1..5 选曲线；
      撤回（0）时 binding 10 写 1×1 零图，回落全局算子。AgX 材质在两种状态下的画面必须可区分。
*/
TEST_F(Renderer_Post_Chain_Test, 编号附件驱动逐材质色调)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	//带编号附件的 8 位场景目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, width, height,
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	//末级：材质色调映射（全局回落 = 默认算子 0 = ACES）
	uint64_t tone = post_pipeline_register(renderer, "材质色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&tone, 1));
	//纯红纹理（AgX 会明显去饱和，ACES 保持纯色）
	std::vector<uint8_t> red_texels(4 * 4 * 4);
	for (std::size_t index = 0; index < red_texels.size(); index += 4)
	{
		red_texels[index] = 255;
		red_texels[index + 1] = 0;
		red_texels[index + 2] = 0;
		red_texels[index + 3] = 255;
	}
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, red_texels.data(), 4, 4));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//AgX 材质（算子 4）
	engine::Material_Data data;
	data.texture = texture;
	data.pbr_override = true;
	data.pbr_roughness = 0.4f;
	data.pbr_metallic = 0.0f;
	data.tone_operator = 4;
	engine::Material_Handle material = 0;
	ASSERT_TRUE(renderer.resource().material_load(material, data));
	renderer.resource().material_bind_set(material);
	//同帧后处理呈现 ＋ 编号通道 ＋ 高曝光（让红色进入压缩区）
	renderer.device().render_target_present_set(target);
	renderer.device().tone_id_pass_set(true);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	//帧 A：编号来源 = 目标 → 末级读编号选 AgX
	renderer.device().post_tone_id_target_set(target);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> id_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(id_pixels.data(), static_cast<uint32_t>(id_pixels.size())));
	//编号附件实际写入（中心应为 AgX 的 5）
	std::vector<uint8_t> ids(static_cast<std::size_t>(width) * height);
	uint32_t id_width = 0;
	uint32_t id_height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
		static_cast<uint32_t>(ids.size()), &id_width, &id_height));
	EXPECT_EQ(ids[static_cast<std::size_t>(height / 2) * width + width / 2], 5u) << "链路径编号通道未写入";
	//帧 B：撤回编号来源 → binding 10 为零图，回落全局 ACES
	renderer.device().post_tone_id_target_set(0);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> global_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(global_pixels.data(), static_cast<uint32_t>(global_pixels.size())));
	std::array<uint8_t, 4> id_center = pixel_get(id_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> global_center = pixel_get(global_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("tone_id_center", pixel_text(id_center));
	RecordProperty("tone_global_center", pixel_text(global_center));
	//交换链为 B8G8R8A8：红色落在 index 2；AgX 会把非红通道抬起
	EXPECT_GT(static_cast<int>(id_center[0]), static_cast<int>(global_center[0]) + 20)
		<< "编号附件未驱动 AgX 去饱和：" << pixel_text(id_center) << " / " << pixel_text(global_center);
	EXPECT_GT(changed_pixels(id_pixels, global_pixels, 8), 0u) << "编号解码未改变画面";
	//清理
	renderer.device().post_tone_id_target_set(0);
	renderer.device().render_target_present_set(0);
	renderer.resource().material_unload(material);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
}

/*
8 位路径 · 编号附件多重采样驱动逐材质色调（第 825 轮）
口径：链路径（场景目标 ＋ 后处理链）在 4× MSAA 下，编号先写多重采样编号图、按样本 0 解析，
      `材质色调映射` 级仍能按编号选曲线；撤回编号来源后回落全局算子（画面必须变化）。
*/
TEST_F(Renderer_Post_Chain_Test, 编号附件多重采样驱动逐材质色调)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//切到 4× MSAA（不支持则跳过）
	renderer.device().msaa_samples_set(4);
	if (renderer.device().msaa_samples() != 4)
		GTEST_SKIP() << "本机不支持 4× MSAA";
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	//带编号附件的场景目标（采样数跟随设备 = 4）
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, width, height,
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 0));
	//末级：材质色调映射（全局回落 = 默认算子 0 = ACES）
	uint64_t tone = post_pipeline_register(renderer, "材质色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&tone, 1));
	//纯红纹理（AgX 会明显去饱和，ACES 保持纯色）
	std::vector<uint8_t> red_texels(4 * 4 * 4);
	for (std::size_t index = 0; index < red_texels.size(); index += 4)
	{
		red_texels[index] = 255;
		red_texels[index + 1] = 0;
		red_texels[index + 2] = 0;
		red_texels[index + 3] = 255;
	}
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, red_texels.data(), 4, 4));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//AgX 材质（算子 4）
	engine::Material_Data data;
	data.texture = texture;
	data.pbr_override = true;
	data.pbr_roughness = 0.4f;
	data.pbr_metallic = 0.0f;
	data.tone_operator = 4;
	engine::Material_Handle material = 0;
	ASSERT_TRUE(renderer.resource().material_load(material, data));
	renderer.resource().material_bind_set(material);
	//同帧后处理呈现 ＋ 编号通道 ＋ 高曝光（让红色进入压缩区）
	renderer.device().render_target_present_set(target);
	renderer.device().tone_id_pass_set(true);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	//帧 A：编号来源 = 目标 → 末级读编号选 AgX
	renderer.device().post_tone_id_target_set(target);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> id_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(id_pixels.data(), static_cast<uint32_t>(id_pixels.size())));
	//多重采样编号解析后的中心应为 AgX 的 5
	std::vector<uint8_t> ids(static_cast<std::size_t>(width) * height);
	uint32_t id_width = 0;
	uint32_t id_height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
		static_cast<uint32_t>(ids.size()), &id_width, &id_height));
	EXPECT_EQ(ids[static_cast<std::size_t>(height / 2) * width + width / 2], 5u)
		<< "链路径多重采样编号未写入或解析失败";
	//帧 B：撤回编号来源 → binding 10 为零图，回落全局 ACES
	renderer.device().post_tone_id_target_set(0);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> global_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(global_pixels.data(), static_cast<uint32_t>(global_pixels.size())));
	RecordProperty("tone_id_center", pixel_text(pixel_get(id_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	RecordProperty("tone_global_center", pixel_text(pixel_get(global_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	EXPECT_GT(changed_pixels(id_pixels, global_pixels, 8), 0u) << "编号解码未改变画面";
	//清理
	renderer.device().post_tone_id_target_set(0);
	renderer.device().render_target_present_set(0);
	renderer.resource().material_unload(material);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
	renderer.device().msaa_samples_set(1);
}

/*
后处理中间级保留逐材质色调（第 789 轮）
口径：分级调色此前恒写 alpha 1，放在泛光 / 场景与末级色调之间会冲掉逐材质编号；
修复后透传 source.a。用例：两级链「分级调色（恒等参数）→ 通用色调」，左 AgX / 右 ACES 纯蓝仍分裂。
*/
TEST_F(Renderer_Post_Chain_Test, HDR中间级保留逐材质色调)
{
	Intermediate_Tone_Spans spans = run_intermediate_tone_case("分级调色.frag.spv", true);
	ASSERT_EQ(spans.stages, 2u) << "两级链未生效";
	RecordProperty("grade_tone_left_span", spans.left_span);
	RecordProperty("grade_tone_right_span", spans.right_span);
	EXPECT_GE(spans.right_span, 120) << "右侧 ACES 未保持纯色：" << spans.right_span;
	EXPECT_LT(spans.left_span, spans.right_span - 30) << "中间级丢失逐材质色调编号：AgX " << spans.left_span
		<< " / ACES " << spans.right_span;
}

/*
单源调色中间级保留逐材质色调（第 791 轮）
口径：饱和度级同样只改 rgb、须透传场景 alpha；用例结构同 789，中间级换成饱和度（固定 1.5）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR单源调色中间级保留逐材质色调)
{
	Intermediate_Tone_Spans spans = run_intermediate_tone_case("饱和度.frag.spv", false);
	ASSERT_EQ(spans.stages, 2u) << "两级链未生效";
	RecordProperty("saturation_tone_left_span", spans.left_span);
	RecordProperty("saturation_tone_right_span", spans.right_span);
	EXPECT_GE(spans.right_span, 120) << "右侧 ACES 未保持纯色：" << spans.right_span;
	EXPECT_LT(spans.left_span, spans.right_span - 30) << "饱和度中间级丢失逐材质色调编号：AgX "
		<< spans.left_span << " / ACES " << spans.right_span;
}

/*
多采样中间级保留逐材质色调（第 792 轮）
口径：模糊级对 rgb 做加权平均，alpha 不参与加权、按中心像素透传；否则编号被平均后误解码。
*/
TEST_F(Renderer_Post_Chain_Test, HDR多采样中间级保留逐材质色调)
{
	Intermediate_Tone_Spans spans = run_intermediate_tone_case("模糊.frag.spv", false);
	ASSERT_EQ(spans.stages, 2u) << "两级链未生效";
	RecordProperty("blur_tone_left_span", spans.left_span);
	RecordProperty("blur_tone_right_span", spans.right_span);
	EXPECT_GE(spans.right_span, 120) << "右侧 ACES 未保持纯色：" << spans.right_span;
	EXPECT_LT(spans.left_span, spans.right_span - 30) << "模糊中间级丢失逐材质色调编号：AgX "
		<< spans.left_span << " / ACES " << spans.right_span;
}

/*
单源调色中间级 · 伽马校正（第 799 轮）
口径：伽马取全局强度默认 1.0（恒等），只验证 alpha 透传；不改变两个半屏的饱和度结构。
*/
TEST_F(Renderer_Post_Chain_Test, HDR伽马中间级保留逐材质色调)
{
	Intermediate_Tone_Spans spans = run_intermediate_tone_case("伽马校正.frag.spv", false);
	ASSERT_EQ(spans.stages, 2u) << "两级链未生效";
	RecordProperty("gamma_tone_left_span", spans.left_span);
	RecordProperty("gamma_tone_right_span", spans.right_span);
	EXPECT_GE(spans.right_span, 120) << "右侧 ACES 未保持纯色：" << spans.right_span;
	EXPECT_LT(spans.left_span, spans.right_span - 30) << "伽马中间级丢失逐材质色调编号：AgX "
		<< spans.left_span << " / ACES " << spans.right_span;
}

/*
邻域采样中间级 · 锐化（第 799 轮）
口径：锐化只改亮度对比、不改色相，故左右半的 AgX / ACES 分裂应保持；验证 alpha 按中心像素透传。
*/
TEST_F(Renderer_Post_Chain_Test, HDR锐化中间级保留逐材质色调)
{
	Intermediate_Tone_Spans spans = run_intermediate_tone_case("锐化.frag.spv", false);
	ASSERT_EQ(spans.stages, 2u) << "两级链未生效";
	RecordProperty("sharpen_tone_left_span", spans.left_span);
	RecordProperty("sharpen_tone_right_span", spans.right_span);
	EXPECT_GE(spans.right_span, 120) << "右侧 ACES 未保持纯色：" << spans.right_span;
	EXPECT_LT(spans.left_span, spans.right_span - 30) << "锐化中间级丢失逐材质色调编号：AgX "
		<< spans.left_span << " / ACES " << spans.right_span;
}

/*
回落算子查询（第 805 轮）
口径：`post_stage_tone_operator_get` 返回该级生效值 —— 专设优先、未专设回落全局默认、级序越界按 0。
*/
TEST_F(Renderer_Post_Chain_Test, HDR回落算子查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//登记单级链（反色即可，只为让级序有效）
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&invert, 1));
	//未专设 → 回落全局默认（hidden_config 下为 0）
	EXPECT_EQ(renderer.device().post_stage_tone_operator_get(0), 0u);
	//专设 3 后再查
	renderer.device().post_stage_tone_operator_set(0, 3);
	EXPECT_EQ(renderer.device().post_stage_tone_operator_get(0), 3u);
	//级序越界 → 0
	EXPECT_EQ(renderer.device().post_stage_tone_operator_get(3), 0u);
	renderer.device().post_chain_set(nullptr, 0);
	renderer.resource().pipeline_unload(invert);
}
/*
双轨分轨不再污染全局回落槽（第 800 轮）
口径：796 之前，HDR 默认路径在每帧调用 bloom_param_set 写分轨，会把全局回落槽的 x/y 改成 0.5/0.1；
用例：先在默认路径出一帧（算子 5，触发原改写），再切到「显式四级泛光链且不设逐级参数」；
      全局阈值 5.0 是切链前设定的，预期仍然生效（边界外无泛光）；若被改写则边界外被泛光提亮。
*/
TEST_F(Renderer_Post_Chain_Test, HDR双轨不污染全局回落参数)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	config.hdr_targets = true;
	config.hdr_tone_operator = 5;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	//全局回落：阈值 5.0（几乎无泛光）
	renderer.device().bloom_param_set(5.0f, 0.1f, 1.0f);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, nullptr));
	std::vector<uint8_t> warmup(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(warmup.data(), static_cast<uint32_t>(warmup.size())));
	//切到显式四级链（不设逐级参数，靠全局回落）
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	uint64_t scene_target = 0;
	uint64_t scene_pipeline = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_target, width, height,
		hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().scene_pipeline_variant_load(scene_pipeline, hdr_format, 1, depth_format));
	uint64_t tone_pipeline = post_pipeline_register(renderer, "材质色调映射.frag.spv");
	ASSERT_NE(tone_pipeline, 0u);
	engine::Bloom_Preset preset;
	engine::Bloom_Preset_Desc desc;
	ASSERT_TRUE(renderer.device().post_bloom_preset_build(preset, desc));
	uint64_t stages[4] = { preset.pipelines[0], preset.pipelines[1], preset.pipelines[2], tone_pipeline };
	uint64_t targets[4] = { scene_target, preset.targets[0], preset.targets[1], preset.targets[2] };
	ASSERT_TRUE(renderer.device().post_chain_set(stages, 4));
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(targets, 4));
	renderer.resource().pipeline_bind_set(scene_pipeline);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	ASSERT_EQ(renderer.frame_stats_get().post_stages, 4u) << "四级链未生效";
	//从中心沿 +X 找亮暗边界，再取边界外两像素作为测点
	uint32_t edge_x = WINDOW_WIDTH;
	for (uint32_t x = WINDOW_WIDTH / 2; x < WINDOW_WIDTH; ++x)
	{
		if (pixel_get(pixels, x, WINDOW_HEIGHT / 2)[0] < 100)
		{
			edge_x = x;
			break;
		}
	}
	ASSERT_LT(edge_x, WINDOW_WIDTH) << "未找到亮暗边界（四边形可能铺满视口）";
	uint32_t probe_x = edge_x + 2;
	if (probe_x >= WINDOW_WIDTH)
		probe_x = WINDOW_WIDTH - 1;
	int probe = static_cast<int>(pixel_get(pixels, probe_x, WINDOW_HEIGHT / 2)[0]);
	RecordProperty("dual_global_probe", probe);
	//阈值 5.0 仍生效 → 边界外应保持暗（无明显泛光扩散）
	EXPECT_LT(probe, 140) << "全局阈值被双轨分轨覆盖（边界外测点 " << probe << "）";
	renderer.device().post_chain_set(nullptr, 0);
	renderer.device().render_target_present_chain_set(nullptr, 0);
	renderer.device().post_bloom_preset_release(preset);
	renderer.resource().pipeline_unload(tone_pipeline);
	renderer.resource().pipeline_unload(scene_pipeline);
	renderer.resource().render_target_destroy(scene_target);
	renderer.resource().mesh_unload(quad);
}

/*
泛光链保留逐材质色调（第 788 轮）
口径：泛光叠加级此前恒写 alpha 1，会把场景 alpha 携带的逐材质算子编号冲掉，末级只能回落全局；
修复后叠加级透传场景 alpha。用例：显式泛光链 ＋ 通用色调末级，左 AgX / 右 ACES 纯蓝，
断言左右饱和度仍分裂（若编号丢失，两侧都会走全局 ACES，左半不会去饱和）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光链保留逐材质色调)
{
	std::vector<uint8_t> texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < texels.size(); index += 4)
	{
		texels[index + 0] = 255;
		texels[index + 3] = 255;
	}
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.0f);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	uint64_t scene_target = 0;
	uint64_t scene_pipeline = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_target, width, height,
		hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().scene_pipeline_variant_load(scene_pipeline, hdr_format, 1, depth_format));
	uint64_t tone_pipeline = post_pipeline_register(renderer, "材质色调映射.frag.spv");
	ASSERT_NE(tone_pipeline, 0u);
	engine::Bloom_Preset preset;
	engine::Bloom_Preset_Desc desc;
	ASSERT_TRUE(renderer.device().post_bloom_preset_build(preset, desc));
	uint64_t stages[4] = { preset.pipelines[0], preset.pipelines[1], preset.pipelines[2], tone_pipeline };
	ASSERT_TRUE(renderer.device().post_chain_set(stages, 4));
	uint64_t targets[4] = { scene_target, preset.targets[0], preset.targets[1], preset.targets[2] };
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(targets, 4));
	//末级回落算子 ACES（未覆盖像素用）
	renderer.device().post_stage_tone_operator_set(3, 0);
	//两种材质
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Material_Data base;
	base.texture = texture;
	//场景管线变体：材质引用它后仍可携带逐材质色调编号
	base.pipeline = scene_pipeline;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Data agx_data = base;
	agx_data.tone_operator = 4;
	engine::Material_Data aces_data = base;
	aces_data.tone_operator = 0;
	engine::Material_Handle agx_handle = 0;
	engine::Material_Handle aces_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(agx_handle, agx_data));
	ASSERT_TRUE(renderer.resource().material_load(aces_handle, aces_data));
	std::array<float, 16> left_matrix = {};
	left_matrix[0] = 1.0f; left_matrix[5] = 1.0f; left_matrix[10] = 1.0f; left_matrix[12] = -1.1f; left_matrix[15] = 1.0f;
	std::array<float, 16> right_matrix = {};
	right_matrix[0] = 1.0f; right_matrix[5] = 1.0f; right_matrix[10] = 1.0f; right_matrix[12] = 1.1f; right_matrix[15] = 1.0f;
	renderer.resource().pipeline_bind_set(scene_pipeline);
	renderer.resource().material_bind_set(agx_handle);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, left_matrix.data()));
	renderer.resource().material_bind_set(aces_handle);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, right_matrix.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	ASSERT_EQ(renderer.frame_stats_get().post_stages, 4u) << "显式泛光链未走四级";
	auto half_span = [](const std::vector<uint8_t>& image, uint32_t half) -> int
	{
		int span = 0;
		uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
		uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = begin; x < end; ++x)
			{
				std::array<uint8_t, 4> pixel = pixel_get(image, x, y);
				int high = static_cast<int>(*std::max_element(pixel.begin(), pixel.begin() + 3));
				int low = static_cast<int>(*std::min_element(pixel.begin(), pixel.begin() + 3));
				if (high - low > span)
					span = high - low;
			}
		}
		return span;
	};
	int left_span = half_span(pixels, 0);
	int right_span = half_span(pixels, 1);
	RecordProperty("bloom_tone_left_span", left_span);
	RecordProperty("bloom_tone_right_span", right_span);
	EXPECT_GE(right_span, 120) << "右侧 ACES 未保持纯色：" << right_span;
	EXPECT_LT(left_span, right_span - 30) << "泛光链丢失逐材质色调编号：AgX " << left_span
		<< " / ACES " << right_span;
	renderer.resource().material_unload(aces_handle);
	renderer.resource().material_unload(agx_handle);
	renderer.resource().texture_unload(texture);
	renderer.device().post_bloom_preset_release(preset);
	renderer.resource().pipeline_unload(tone_pipeline);
	renderer.resource().pipeline_unload(scene_pipeline);
	renderer.resource().render_target_destroy(scene_target);
	renderer.resource().mesh_unload(quad);
}

/*
显式后处理链 · 逐材质色调（第 781 轮）
口径：调用方自行装配链并使用 `材质色调映射.frag` 时，逐材质覆盖仍优先于该级的回落算子。
左材质 AgX、右材质 ACES，纯蓝：左半去饱和、右半保纯色。
*/
TEST_F(Renderer_Post_Chain_Test, HDR显式链逐材质色调)
{
	//纯蓝纹理（index 0 = 蓝）
	std::vector<uint8_t> texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < texels.size(); index += 4)
	{
		texels[index + 0] = 255;
		texels[index + 3] = 255;
	}
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.0f);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//16F 场景目标 ＋ 16F 场景管线 ＋ 通用色调末级
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	uint64_t scene_target = 0;
	uint64_t scene_pipeline = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_target, width, height,
		hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().scene_pipeline_variant_load(scene_pipeline, hdr_format, 1, depth_format));
	uint64_t tone_pipeline = post_pipeline_register(renderer, "材质色调映射.frag.spv");
	ASSERT_NE(tone_pipeline, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&tone_pipeline, 1));
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(&scene_target, 1));
	//该级回落算子取 ACES（未覆盖像素用）
	renderer.device().post_stage_tone_operator_set(0, 0);
	//两种材质：AgX（4）/ ACES（0）
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Material_Data base;
	base.texture = texture;
	//场景管线变体：材质引用它后仍可携带逐材质色调编号
	base.pipeline = scene_pipeline;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Data agx_data = base;
	agx_data.tone_operator = 4;
	engine::Material_Data aces_data = base;
	aces_data.tone_operator = 0;
	engine::Material_Handle agx_handle = 0;
	engine::Material_Handle aces_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(agx_handle, agx_data));
	ASSERT_TRUE(renderer.resource().material_load(aces_handle, aces_data));
	//左 AgX / 右 ACES
	std::array<float, 16> left_matrix = {};
	left_matrix[0] = 1.0f; left_matrix[5] = 1.0f; left_matrix[10] = 1.0f; left_matrix[12] = -1.1f; left_matrix[15] = 1.0f;
	std::array<float, 16> right_matrix = {};
	right_matrix[0] = 1.0f; right_matrix[5] = 1.0f; right_matrix[10] = 1.0f; right_matrix[12] = 1.1f; right_matrix[15] = 1.0f;
	renderer.resource().pipeline_bind_set(scene_pipeline);
	renderer.resource().material_bind_set(agx_handle);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, left_matrix.data()));
	renderer.resource().material_bind_set(aces_handle);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, right_matrix.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	ASSERT_EQ(renderer.frame_stats_get().post_stages, 1u) << "显式单级色调链未生效";
	//半屏最大通道差（饱和度粗量）
	auto half_span = [](const std::vector<uint8_t>& image, uint32_t half) -> int
	{
		int span = 0;
		uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
		uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = begin; x < end; ++x)
			{
				std::array<uint8_t, 4> pixel = pixel_get(image, x, y);
				int high = static_cast<int>(*std::max_element(pixel.begin(), pixel.begin() + 3));
				int low = static_cast<int>(*std::min_element(pixel.begin(), pixel.begin() + 3));
				if (high - low > span)
					span = high - low;
			}
		}
		return span;
	};
	int left_span = half_span(pixels, 0);
	int right_span = half_span(pixels, 1);
	RecordProperty("chain_tone_left_agx_span", left_span);
	RecordProperty("chain_tone_right_aces_span", right_span);
	EXPECT_GE(right_span, 120) << "显式链右侧 ACES 未保持纯色：" << right_span;
	EXPECT_LT(left_span, right_span - 30) << "显式链左侧 AgX 未去饱和：AgX " << left_span
		<< " / ACES " << right_span;
	renderer.resource().material_unload(aces_handle);
	renderer.resource().material_unload(agx_handle);
	renderer.resource().texture_unload(texture);
	renderer.resource().pipeline_unload(scene_pipeline);
	renderer.resource().pipeline_unload(tone_pipeline);
	renderer.resource().render_target_destroy(scene_target);
	renderer.resource().mesh_unload(quad);
}
