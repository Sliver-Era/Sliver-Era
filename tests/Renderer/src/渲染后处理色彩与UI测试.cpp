/*
渲染后处理色彩与 UI 测试（2026-11-21 自「渲染后处理链测试.cpp」拆出）
覆盖：胶片与 ACES 混合、Reinhard、曝光与色调映射联动、预设串联、链参数批量读回、
      逐级额外四参、UI 后置不被处理 / 不被色调映射。
*/
#include "渲染后处理链测试辅助.h"
#include "测试辅助/渲染测试辅助.h"

//UI 通道（后处理链「UI 后置」用例需要向帧链路挂接绘制回调）
#include "Engine/EngineRenderer/src/render/Renderer/渲染器_UI通道.h"

/*
HDR 场景目标保留高光（方向九第 745 轮）
口径：曝光 +2 时，8 位场景目标在写入处把 >1 截断（ACES 中心约 232）；
16F 场景目标保留 >1 光强，经同一 ACES 后中心更亮（≥ 242），证明高光信息未被丢失。
默认 8 位路径不受影响（本用例不改变默认目标的创建方式）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR场景目标保留高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//内置白纹理与四边形
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//曝光 +2（×4）：白四边形输出 4.0，超过 8 位目标的表示范围
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	//ACES 后处理级（宿为交换链，跟随上下文颜色格式）
	uint64_t tone = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&tone, 1));
	//基线：8 位场景目标 ＋ 默认场景管线
	uint64_t ldr_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(ldr_target, renderer.device().extent_width(), renderer.device().extent_height()));
	renderer.device().render_target_present_set(ldr_target);
	std::vector<uint8_t> ldr_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(ldr_pixels.data(), static_cast<uint32_t>(ldr_pixels.size())));
	//16F 场景目标（深度固定 D32，与下方管线变体一致）
	uint64_t hdr_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(hdr_target,
		renderer.device().extent_width(), renderer.device().extent_height(),
		static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 1));
	//16F 场景管线变体（同一对着色器，仅颜色附件格式不同）
	uint64_t hdr_pipeline = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(hdr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true,
		static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 1, static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT)));
	renderer.device().render_target_present_set(hdr_target);
	renderer.resource().pipeline_bind_set(hdr_pipeline);
	std::vector<uint8_t> hdr_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(hdr_pixels.data(), static_cast<uint32_t>(hdr_pixels.size())));
	//中心（白四边形）亮度对照
	int ldr_center = static_cast<int>(pixel_get(ldr_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	int hdr_center = static_cast<int>(pixel_get(hdr_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	EXPECT_GE(ldr_center, 220) << "8 位基线未进入高光区间：" << ldr_center;
	EXPECT_LE(ldr_center, 240) << "8 位基线口径异常：" << ldr_center;
	EXPECT_GE(hdr_center, ldr_center + 8) << "16F 目标未保留高光（8 位 " << ldr_center
		<< " / 16F " << hdr_center << "）";
	EXPECT_LE(hdr_center, 255) << "16F 目标输出越界：" << hdr_center;
	EXPECT_GT(changed_pixels(ldr_pixels, hdr_pixels, 4), 0u) << "16F 与 8 位画面无差异";
	RecordProperty("hdr_ldr_center", ldr_center);
	RecordProperty("hdr_16f_center", hdr_center);
	renderer.resource().pipeline_bind_set(0);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(hdr_pipeline);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().render_target_destroy(hdr_target);
	renderer.resource().render_target_destroy(ldr_target);
}

/*
胶片与 ACES 的 look 混合（方向九第 742 轮）
口径：混合比例 z 取 0 时与纯胶片画面逐字节相同；取 1 时与 ACES 预设逐字节相同；
中间值 0.5 的中心亮度夹在两者之间（可连续调校强光照亮部收束）。
*/
TEST_F(Renderer_Post_Chain_Test, 胶片与ACES可混合)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t filmic_stage = post_pipeline_register(renderer, "胶片色调映射.frag.spv");
	uint64_t aces_stage = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(filmic_stage, 0u);
	ASSERT_NE(aces_stage, 0u);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	//纯胶片帧（额外四参全 0）
	ASSERT_TRUE(renderer.device().post_chain_set(&filmic_stage, 1));
	renderer.device().post_stage_extra_set(0, 0.0f, 0.0f, 0.0f, 0.0f);
	std::vector<uint8_t> filmic_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(filmic_pixels.data(), static_cast<uint32_t>(filmic_pixels.size())));
	//混合比例 1.0（等价 ACES）
	renderer.device().post_stage_extra_set(0, 0.0f, 0.0f, 1.0f, 0.0f);
	std::vector<uint8_t> mix_full_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(mix_full_pixels.data(), static_cast<uint32_t>(mix_full_pixels.size())));
	//混合比例 0.5
	renderer.device().post_stage_extra_set(0, 0.0f, 0.0f, 0.5f, 0.0f);
	std::vector<uint8_t> mix_half_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(mix_half_pixels.data(), static_cast<uint32_t>(mix_half_pixels.size())));
	//参照：ACES 预设本身
	ASSERT_TRUE(renderer.device().post_chain_set(&aces_stage, 1));
	std::vector<uint8_t> aces_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(aces_pixels.data(), static_cast<uint32_t>(aces_pixels.size())));
	//混合比例 1.0 与 ACES 预设逐字节一致
	int full_diff = max_channel_diff(aces_pixels, mix_full_pixels);
	EXPECT_EQ(full_diff, 0) << "混合比例 1 未等价 ACES（最大通道差 " << full_diff << "）";
	//中心亮度：胶片 < 中间 < ACES
	int filmic_center = static_cast<int>(pixel_get(filmic_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	int half_center = static_cast<int>(pixel_get(mix_half_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	int aces_center = static_cast<int>(pixel_get(aces_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	EXPECT_LT(filmic_center, half_center) << "混合 0.5 未比纯胶片更亮";
	EXPECT_LT(half_center, aces_center) << "混合 0.5 未比 ACES 更暗";
	//混合 0.5 的画面既不等于胶片也不等于 ACES
	EXPECT_GT(changed_pixels(filmic_pixels, mix_half_pixels, 2), 0u);
	EXPECT_GT(changed_pixels(aces_pixels, mix_half_pixels, 2), 0u);
	RecordProperty("mix_filmic_center", filmic_center);
	RecordProperty("mix_half_center", half_center);
	RecordProperty("mix_aces_center", aces_center);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(aces_stage);
	renderer.resource().pipeline_unload(filmic_stage);
	renderer.resource().render_target_destroy(scene_target);
}

/*
后处理预设 · Reinhard 扩展色调映射（方向九第 740 轮）
中性白输入下：中心落在 195–218、通道中性（差 ≤ 2），
且与 ACES（约 232）与胶片（约 186）三算子两两可区分（差异像素 > 0）。
*/
TEST_F(Renderer_Post_Chain_Test, Reinhard色调映射压缩高亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t aces = post_pipeline_register(renderer, "色调映射.frag.spv");
	uint64_t filmic_stage = post_pipeline_register(renderer, "胶片色调映射.frag.spv");
	uint64_t reinhard_stage = post_pipeline_register(renderer, "Reinhard扩展色调映射.frag.spv");
	ASSERT_NE(aces, 0u);
	ASSERT_NE(filmic_stage, 0u);
	ASSERT_NE(reinhard_stage, 0u);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	//按指定算子出帧（绘制项按帧提交，每帧重新登记）
	auto frame_by = [&](uint64_t stage, std::vector<uint8_t>& pixels)
	{
		EXPECT_TRUE(renderer.device().post_chain_set(&stage, 1));
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	std::vector<uint8_t> aces_pixels(pixel_bytes());
	std::vector<uint8_t> filmic_pixels(pixel_bytes());
	std::vector<uint8_t> reinhard_pixels(pixel_bytes());
	frame_by(aces, aces_pixels);
	frame_by(filmic_stage, filmic_pixels);
	frame_by(reinhard_stage, reinhard_pixels);
	std::array<uint8_t, 4> aces_center = pixel_get(aces_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> filmic_center = pixel_get(filmic_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> reinhard_center = pixel_get(reinhard_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(reinhard_center[0], 195) << "Reinhard 压暗过头：" << pixel_text(reinhard_center);
	EXPECT_LE(reinhard_center[0], 218) << "Reinhard 未压缩高光：" << pixel_text(reinhard_center);
	EXPECT_LE(std::abs(static_cast<int>(reinhard_center[0]) - static_cast<int>(reinhard_center[1])), 2)
		<< "Reinhard 出现通道色偏：" << pixel_text(reinhard_center);
	EXPECT_LE(std::abs(static_cast<int>(reinhard_center[1]) - static_cast<int>(reinhard_center[2])), 2)
		<< "Reinhard 出现通道色偏：" << pixel_text(reinhard_center);
	EXPECT_GT(changed_pixels(aces_pixels, reinhard_pixels, 4), 0u) << "Reinhard 与 ACES 画面无法区分";
	EXPECT_GT(changed_pixels(filmic_pixels, reinhard_pixels, 4), 0u) << "Reinhard 与胶片画面无法区分";
	RecordProperty("reinhard_center", static_cast<int>(reinhard_center[0]));
	RecordProperty("filmic_center", static_cast<int>(filmic_center[0]));
	RecordProperty("aces_center", static_cast<int>(aces_center[0]));
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(reinhard_stage);
	renderer.resource().pipeline_unload(filmic_stage);
	renderer.resource().pipeline_unload(aces);
	renderer.resource().render_target_destroy(scene_target);
}

/*
曝光与色调映射联动（方向九第 738 轮）
口径：ACES 链下 EV −2 / 0 / +2 的中心亮度单调不减，且 −2 严格低于 0。
注意：绘制项按帧提交，每帧必须重新登记绘制（737 轮首版用例漏掉此步，后续帧只剩清屏色）。
*/
TEST_F(Renderer_Post_Chain_Test, 曝光与色调映射联动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t tone = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&tone, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	auto frame_at = [&](float ev)
	{
		EXPECT_TRUE(renderer.scene().exposure_set(ev));
		//每帧重新登记绘制
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return static_cast<int>(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	};
	int dark = frame_at(-2.0f);
	int base = frame_at(0.0f);
	int bright = frame_at(2.0f);
	EXPECT_LT(dark, base) << "负曝光未压暗";
	EXPECT_LE(base, bright) << "正曝光使画面变暗";
	RecordProperty("exposure_dark", dark);
	RecordProperty("exposure_base", base);
	RecordProperty("exposure_bright", bright);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().render_target_destroy(scene_target);
}

/*
后处理预设可串联：灰度 → 描边 两级链可正常出帧，且中心仍为去色后的亮色
*/
TEST_F(Renderer_Post_Chain_Test, 预设可串联)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	ASSERT_TRUE(renderer.resource().render_target_create(second_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	uint64_t edge = post_pipeline_register(renderer, "描边.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_NE(edge, 0u);
	uint64_t chain[2] = { gray, edge };
	ASSERT_TRUE(renderer.device().post_chain_set(chain, 2));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//中心仍应为去色后的亮色（两级链未破坏内部）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(luma_of(center), 180.0f) << "两级链后中心过暗：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[2])), 4)
		<< "两级链后中心未保持去色：" << pixel_text(center);

	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(gray);
	renderer.resource().pipeline_unload(edge);
	renderer.resource().render_target_destroy(second_target);
	renderer.resource().render_target_destroy(scene_target);
}

/*
批量读回链各级参数（第八十二轮）
口径：两级链只给第 0 级设专设值 → 批量读回返回 2、第 0 级为设定值、第 1 级全 0；
      容量不足一级 / 出参为空 / 未初始化返回 0。
*/
TEST_F(Renderer_Post_Chain_Test, 链参数可批量读回)
{
	engine::Renderer renderer;
	float empty[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//未初始化：返回 0 且不写内容
	EXPECT_EQ(renderer.device().post_chain_params_get(empty, 4), 0u);
	EXPECT_EQ(renderer.device().post_chain_params_get(nullptr, 8), 0u);
	EXPECT_EQ(empty[0], 1.0f);

	ASSERT_TRUE(renderer.init(hidden_config()));
	//容量不足一级
	EXPECT_EQ(renderer.device().post_chain_params_get(empty, 3), 0u);

	//两级链
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	uint64_t chain[2] = { invert, threshold };
	ASSERT_TRUE(renderer.device().post_chain_set(chain, 2));
	//只给第 0 级设专设值
	renderer.device().post_stage_param_set(0, 0.7f, 0.2f, 2.5f);

	//批量读回两级（8 个浮点）
	float values[8] = {};
	ASSERT_EQ(renderer.device().post_chain_params_get(values, 8), 2u);
	EXPECT_FLOAT_EQ(values[0], 0.7f);
	EXPECT_FLOAT_EQ(values[1], 0.2f);
	EXPECT_FLOAT_EQ(values[2], 2.5f);
	EXPECT_FLOAT_EQ(values[3], 0.0f);
	//第 1 级未专设 → 全 0
	for (uint32_t each = 4; each < 8; ++each)
		EXPECT_FLOAT_EQ(values[each], 0.0f) << "第 1 级未专设却读回非零：下标 " << each;

	//容量只够一级 → 返回 1，只写前 4 个
	float single[4] = {};
	EXPECT_EQ(renderer.device().post_chain_params_get(single, 4), 1u);
	EXPECT_FLOAT_EQ(single[0], 0.7f);

	renderer.resource().pipeline_unload(invert);
	renderer.resource().pipeline_unload(threshold);
}

/*
逐级额外四参（第四百零一轮）
单级「反色」链：写入合法 extra / 非有限值 / 链外级序后，画面都应逐字节不变
（既有片元均未声明 offset 128，故该组只占位、不参与着色）。
*/
TEST_F(Renderer_Post_Chain_Test, 逐级额外四参不改变画面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	engine::Mesh_Handle quad = 0;
	scene_register(renderer, quad);
	//基线（未写 extra）：先出一帧完成目标布局预热，再取基线
	const uint64_t 链[1] = { invert };
	ASSERT_TRUE(renderer.device().post_chain_set(链, 1));
	std::vector<uint8_t> 预热(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(预热.data(), static_cast<uint32_t>(预热.size())));
	std::vector<uint8_t> 基线(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(基线.data(), static_cast<uint32_t>(基线.size())));
	//写入合法 extra 后再渲染：画面应完全一致
	renderer.device().post_stage_extra_set(0, 1.0f, -2.5f, 3.75f, 0.5f);
	std::vector<uint8_t> 写入后(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(写入后.data(), static_cast<uint32_t>(写入后.size())));
	EXPECT_EQ(max_channel_diff(基线, 写入后), 0) << "额外四参影响了画面";
	//非有限值整次忽略（保持原值）
	renderer.device().post_stage_extra_set(0, std::nanf(""), 1.0f, 1.0f, 1.0f);
	std::vector<uint8_t> 非法后(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(非法后.data(), static_cast<uint32_t>(非法后.size())));
	EXPECT_EQ(max_channel_diff(写入后, 非法后), 0);
	//链外级序被拒绝（不崩溃且画面一致）
	renderer.device().post_stage_extra_set(5, 1.0f, 1.0f, 1.0f, 1.0f);
	std::vector<uint8_t> 越界后(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(越界后.data(), static_cast<uint32_t>(越界后.size())));
	EXPECT_EQ(max_channel_diff(非法后, 越界后), 0);
	renderer.resource().pipeline_unload(invert);
	renderer.resource().mesh_unload(quad);
}

/*
后处理链 · UI 后置（第六百三十九轮）
口径：链上挂一级「灰度」。若 UI 仍被后处理，UI 画出的彩色会被去色（三通道相等）；
现在 UI 在末级之后绘制，故 UI 覆盖处保留明显通道差，而场景本身仍按灰度处理。
*/
namespace
{
	//UI 回调用固定色铺满整帧（验证「UI 不被后处理」只需一处可判读的彩色区域）
	struct Ui_Clear_Data
	{
		//清屏色（线性 RGBA）
		float color[4];
	};

	//UI 绘制回调：在帧链路的 UI 通道内以清屏色覆盖整帧
	void ui_clear_frame(VkCommandBuffer command_buffer, void* user_data)
	{
		Ui_Clear_Data* data = static_cast<Ui_Clear_Data*>(user_data);
		//颜色附件清除（aspect = 颜色、附件序号 0）
		VkClearAttachment attachment{};
		attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachment.colorAttachment = 0;
		for (std::size_t index = 0; index < 4; ++index)
			attachment.clearValue.color.float32[index] = data->color[index];
		//整帧清除区域
		VkClearRect rect{};
		rect.rect.offset = { 0, 0 };
		rect.rect.extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
		rect.baseArrayLayer = 0;
		rect.layerCount = 1;
		vkCmdClearAttachments(command_buffer, 1, &attachment, 1, &rect);
	}
}

TEST_F(Renderer_Post_Chain_Test, 后处理链上UI不被处理)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//场景目标与单级灰度链
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&gray, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);

	//参考帧（无 UI）：灰度使白四边形与清屏色都变灰（三通道相等）
	std::vector<uint8_t> without_ui(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(without_ui.data(), static_cast<uint32_t>(without_ui.size())));
	std::array<uint8_t, 4> gray_center = pixel_get(without_ui, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	int gray_span = static_cast<int>(*std::max_element(gray_center.begin(), gray_center.begin() + 3))
		- static_cast<int>(*std::min_element(gray_center.begin(), gray_center.begin() + 3));
	EXPECT_LE(gray_span, CHAIN_TOLERANCE) << "灰度级未生效：" << pixel_text(gray_center);

	//挂接 UI 回调并再出一帧：UI 色绘在末级之后，通道差应被完整保留
	engine::Ui_Channel_Context* channel = renderer.ui().ui_channel();
	ASSERT_NE(channel, nullptr);
	Ui_Clear_Data ui_data{ { 0.55f, 0.30f, 0.12f, 1.0f } };
	channel->draw_callback_set(ui_clear_frame, &ui_data);
	std::vector<uint8_t> with_ui(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(with_ui.data(), static_cast<uint32_t>(with_ui.size())));
	std::array<uint8_t, 4> ui_center = pixel_get(with_ui, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	int ui_span = static_cast<int>(*std::max_element(ui_center.begin(), ui_center.begin() + 3))
		- static_cast<int>(*std::min_element(ui_center.begin(), ui_center.begin() + 3));
	EXPECT_GE(ui_span, 40) << "UI 被后处理去色（三通道差仅 " << ui_span << "）：" << pixel_text(ui_center);

	//收尾：清回调并释放
	channel->draw_callback_clear();
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(gray);
	renderer.resource().render_target_destroy(scene_target);
}

/*
HDR 默认路径 · UI 显示域混合（方向九第 758 轮）
口径：hdr_targets 开启时场景经 16F ＋ 色调映射后才呈现到交换链；UI 必须在末级之后绘制，
故 UI 清屏色不应被色调映射改变 —— 开启与关闭两帧的 UI 像素应逐通道一致。
若 UI 被画进 16F 目标（经 ACES 压缩），三通道会出现明显偏移。
*/
TEST_F(Renderer_Post_Chain_Test, HDR默认路径UI不被色调映射)
{
	//UI 固定色（线性 RGBA）
	Ui_Clear_Data ui_data{ { 0.55f, 0.30f, 0.12f, 1.0f } };
	//按配置出一帧「UI 铺满整帧」，取中心像素
	auto frame_ui = [&](bool hdr, std::array<uint8_t, 4>& out)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = hdr;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		renderer.resource().texture_bind_set(0);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		engine::Ui_Channel_Context* channel = renderer.ui().ui_channel();
		ASSERT_NE(channel, nullptr);
		channel->draw_callback_set(ui_clear_frame, &ui_data);
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		out = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		channel->draw_callback_clear();
		renderer.resource().mesh_unload(handle);
	};
	std::array<uint8_t, 4> ldr_ui{};
	std::array<uint8_t, 4> hdr_ui{};
	frame_ui(false, ldr_ui);
	frame_ui(true, hdr_ui);
	RecordProperty("hdr_ui_ldr", pixel_text(ldr_ui));
	RecordProperty("hdr_ui_16f", pixel_text(hdr_ui));
	//UI 在末级之后绘制：两帧 UI 像素应一致（±2 取整）
	for (std::size_t index = 0; index < 3; ++index)
	{
		int difference = std::abs(static_cast<int>(ldr_ui[index]) - static_cast<int>(hdr_ui[index]));
		EXPECT_LE(difference, 2) << "HDR 路径下 UI 被色调映射改变（通道 " << index << " 差 " << difference
			<< "）：关闭 " << pixel_text(ldr_ui) << " / 开启 " << pixel_text(hdr_ui);
	}
}
