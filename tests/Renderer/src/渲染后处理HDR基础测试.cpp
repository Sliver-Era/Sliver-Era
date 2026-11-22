/*
渲染后处理 HDR 基础测试（2026-11-21 自「渲染后处理链测试.cpp」拆出）
覆盖：HDR 简化算子、泛光提取与闭环、HDR 目标开关、默认路径出帧 / 色调算子 / AgX 去饱和。
*/
#include "渲染后处理链测试辅助.h"
//Vulkan 图像格式枚举（16F / 8 位 / 深度格式常量）
#include <vulkan/vulkan.h>
#include "测试辅助/渲染测试辅助.h"

/*
HDR 下简化算子同样保留高光（方向九第 748 轮）
口径：第 185 轮的简化 Reinhard / 指数两个预设，输入改为只钳下界；
16F 场景目标下指数曲线中心应接近 253，而 8 位路径因提前截断只有约 210。
*/

/*
HDR 下简化算子同样保留高光（方向九第 748 轮）
口径：第 185 轮的简化 Reinhard / 指数两个预设，输入改为只钳下界；
16F 场景目标下指数曲线中心应接近 253，而 8 位路径因提前截断只有约 210。
*/
TEST_F(Renderer_Post_Chain_Test, HDR简化算子保留高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	uint64_t exponent = post_pipeline_register(renderer, "指数色调映射.frag.spv");
	ASSERT_NE(exponent, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&exponent, 1));
	//基线：8 位场景目标 ＋ 默认场景管线
	uint64_t ldr_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(ldr_target, renderer.device().extent_width(), renderer.device().extent_height()));
	renderer.device().render_target_present_set(ldr_target);
	std::vector<uint8_t> ldr_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(ldr_pixels.data(), static_cast<uint32_t>(ldr_pixels.size())));
	//16F 场景目标 ＋ 16F 场景管线变体
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	uint64_t hdr_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(hdr_target,
		renderer.device().extent_width(), renderer.device().extent_height(), hdr_format, depth_format, 1));
	uint64_t hdr_pipeline = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(hdr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
	renderer.device().render_target_present_set(hdr_target);
	renderer.resource().pipeline_bind_set(hdr_pipeline);
	std::vector<uint8_t> hdr_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(hdr_pixels.data(), static_cast<uint32_t>(hdr_pixels.size())));
	int ldr_center = static_cast<int>(pixel_get(ldr_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	int hdr_center = static_cast<int>(pixel_get(hdr_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	EXPECT_GE(ldr_center, 195) << "8 位指数口径异常：" << ldr_center;
	EXPECT_LE(ldr_center, 225) << "8 位指数口径异常：" << ldr_center;
	EXPECT_GE(hdr_center, 245) << "16F 下指数算子未保留高光（8 位 " << ldr_center
		<< " / 16F " << hdr_center << "）";
	EXPECT_GT(hdr_center, ldr_center + 20) << "16F 与 8 位差异不足";
	RecordProperty("exponent_ldr_center", ldr_center);
	RecordProperty("exponent_16f_center", hdr_center);
	renderer.resource().pipeline_bind_set(0);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(hdr_pipeline);
	renderer.resource().pipeline_unload(exponent);
	renderer.resource().render_target_destroy(hdr_target);
	renderer.resource().render_target_destroy(ldr_target);
}

/*
HDR 泛光提取保留高光（方向九第 753 轮）
口径：亮部提取只做 source × 软膝权重，本身不钳制；但输出若落在 8 位中间目标，>1 会被写入处截断。
本用例同一条「亮部提取 → 色调映射」链，仅中间目标格式不同：
16F 中间目标可读到 >1 的提取强度，8 位中间目标恒 ≤1。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光提取保留高光)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//曝光 +2（×4）：白四边形输出 4.0
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t ldr_format = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	//场景目标：16F 与 8 位各一
	uint64_t scene_hdr = 0;
	uint64_t scene_ldr = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_hdr, width, height, hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create(scene_ldr, width, height));
	//场景管线变体（16F / 8 位）
	uint64_t scene_hdr_pipeline = 0;
	uint64_t scene_ldr_pipeline = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_hdr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_ldr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, ldr_format));
	//中间目标与亮部提取管线变体
	uint64_t mid_hdr = 0;
	uint64_t mid_ldr = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(mid_hdr, width, height, hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(mid_ldr, width, height, ldr_format, depth_format, 1));
	uint64_t extract_hdr = 0;
	uint64_t extract_ldr = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(extract_hdr, "后处理.vert.spv", "亮部提取.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, hdr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(extract_ldr, "后处理.vert.spv", "亮部提取.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, ldr_format));
	//末级：色调映射（写交换链）
	uint64_t tone = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	//按「亮部提取 ＋ 中间目标 ＋ 色调映射」出一帧
	std::vector<uint8_t> pixels(pixel_bytes());
	auto frame_by = [&](uint64_t scene_target, uint64_t scene_pipeline, uint64_t extract_stage, uint64_t intermediate)
	{
		renderer.device().render_target_present_set(scene_target);
		renderer.resource().pipeline_bind_set(scene_pipeline);
		uint64_t stages[2] = { extract_stage, tone };
		EXPECT_TRUE(renderer.device().post_chain_set(stages, 2));
		uint64_t chain_targets[2] = { scene_target, intermediate };
		EXPECT_TRUE(renderer.device().render_target_present_chain_set(chain_targets, 2));
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	//浮点读回中间目标中心像素
	auto mid_center = [&](uint64_t intermediate)
	{
		std::vector<float> values(static_cast<std::size_t>(width) * height * 4);
		uint32_t read_width = 0;
		uint32_t read_height = 0;
		EXPECT_TRUE(renderer.resource().render_target_readback_float(intermediate, values.data(),
			static_cast<uint32_t>(values.size()), &read_width, &read_height));
		const std::size_t center = (static_cast<std::size_t>(read_height / 2) * read_width + read_width / 2) * 4;
		return values[center];
	};
	frame_by(scene_hdr, scene_hdr_pipeline, extract_hdr, mid_hdr);
	float hdr_center = mid_center(mid_hdr);
	frame_by(scene_ldr, scene_ldr_pipeline, extract_ldr, mid_ldr);
	float ldr_center = mid_center(mid_ldr);
	EXPECT_GT(hdr_center, 1.5f) << "16F 中间目标未保留泛光提取强度（实测 " << hdr_center << "）";
	EXPECT_NEAR(hdr_center, 4.0f, 0.6f) << "泛光提取权重口径异常（应 ≈ source × 1.0）";
	EXPECT_LE(ldr_center, 1.0f) << "8 位中间目标出现 >1（实测 " << ldr_center << "）";
	EXPECT_GE(ldr_center, 0.9f) << "8 位中间目标基线异常（实测 " << ldr_center << "）";
	RecordProperty("bloom_hdr_center", hdr_center);
	RecordProperty("bloom_ldr_center", ldr_center);
	/*
	第二阶段：阈值抬到 1.5（高于 8 位可表示范围）
	8 位场景最亮只有 1.0 < 阈值 − 软膝 → 提取权重为 0（泛光完全消失）；
	16F 场景为 4.0 ≥ 阈值 ＋ 软膝 → 权重仍为 1，泛光保留。
	这是「HDR 下可做真正的高光阈值」的直接证据。
	*/
	renderer.device().bloom_param_set(1.5f, 0.1f, 1.0f);
	frame_by(scene_hdr, scene_hdr_pipeline, extract_hdr, mid_hdr);
	float hdr_high = mid_center(mid_hdr);
	frame_by(scene_ldr, scene_ldr_pipeline, extract_ldr, mid_ldr);
	float ldr_high = mid_center(mid_ldr);
	EXPECT_GT(hdr_high, 1.5f) << "阈值 1.5 时 16F 高光应仍被提取（实测 " << hdr_high << "）";
	EXPECT_LT(ldr_high, 0.05f) << "阈值 1.5 时 8 位场景不应有可提取亮部（实测 " << ldr_high << "）";
	RecordProperty("bloom_hdr_high_threshold", hdr_high);
	RecordProperty("bloom_ldr_high_threshold", ldr_high);
	renderer.resource().pipeline_bind_set(0);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(extract_ldr);
	renderer.resource().pipeline_unload(extract_hdr);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().pipeline_unload(scene_ldr_pipeline);
	renderer.resource().pipeline_unload(scene_hdr_pipeline);
	renderer.resource().render_target_destroy(mid_ldr);
	renderer.resource().render_target_destroy(mid_hdr);
	renderer.resource().render_target_destroy(scene_ldr);
	renderer.resource().render_target_destroy(scene_hdr);
}

/*
HDR 泛光闭环强于 8 位（方向九第 755 轮）
口径：完整「亮部提取 → 模糊 → 泛光叠加」三级链，仅场景目标与两级中间目标的格式不同（16F / RGBA8）。
曝光 +2 使白四边形在 16F 链路输出线性 4.0、8 位链路在写入处被截断为 1.0；
亮部经半径 4 的模糊扩散到四边形边界外的暗区，该处 16F 链路的泛光应明显强于 8 位链路。
测点不硬编码几何：先用「直通参考帧」（无泛光）从中心沿 +X 扫描出第一个暗像素作为边界，
再测边界外 2 像素（仍在模糊半径内）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR泛光闭环强于八位)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.vsync = false;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//曝光 +2（×4）：16F 链路的白四边形输出 4.0，8 位链路截断为 1.0
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	const uint32_t width = renderer.device().extent_width();
	const uint32_t height = renderer.device().extent_height();
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t ldr_format = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	//场景目标（16F / 8 位）
	uint64_t scene_hdr = 0;
	uint64_t scene_ldr = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_hdr, width, height, hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_ldr, width, height, ldr_format, depth_format, 1));
	//泛光中间目标：提取输出与模糊输出（两链路各一套）
	uint64_t extract_out_hdr = 0;
	uint64_t blur_out_hdr = 0;
	uint64_t extract_out_ldr = 0;
	uint64_t blur_out_ldr = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(extract_out_hdr, width, height, hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(blur_out_hdr, width, height, hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(extract_out_ldr, width, height, ldr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(blur_out_ldr, width, height, ldr_format, depth_format, 1));
	//场景管线变体（16F / 8 位）
	uint64_t scene_hdr_pipeline = 0;
	uint64_t scene_ldr_pipeline = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_hdr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_ldr_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, ldr_format));
	//泛光各级管线变体（亮部提取 / 模糊按链路格式，叠加写交换链故跟随上下文）
	uint64_t extract_hdr = 0;
	uint64_t extract_ldr = 0;
	uint64_t blur_hdr = 0;
	uint64_t blur_ldr = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(extract_hdr, "后处理.vert.spv", "亮部提取.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, hdr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(extract_ldr, "后处理.vert.spv", "亮部提取.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, ldr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(blur_hdr, "后处理.vert.spv", "模糊.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, hdr_format));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(blur_ldr, "后处理.vert.spv", "模糊.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, ldr_format));
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(overlay, 0u);
	//参考帧：8 位场景 ＋ 单级直通（无泛光），用于确定亮暗边界
	std::vector<uint8_t> pixels(pixel_bytes());
	{
		uint64_t passthrough = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(passthrough, 0u);
		renderer.device().render_target_present_set(scene_ldr);
		renderer.resource().pipeline_bind_set(scene_ldr_pipeline);
		uint64_t stage = passthrough;
		ASSERT_TRUE(renderer.device().post_chain_set(&stage, 1));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.resource().pipeline_unload(passthrough);
	}
	std::vector<uint8_t> reference = pixels;
	//从中心沿 +X 扫描第一个「暗」像素（参考帧无泛光，边界确定）
	uint32_t edge_x = width;
	for (uint32_t x = width / 2; x < width; ++x)
	{
		//亮块约 255，清屏色约 62（线性 0.05 经 SRGB 编码）
		if (pixel_get(reference, x, height / 2)[0] < 100)
		{
			edge_x = x;
			break;
		}
	}
	ASSERT_LT(edge_x, width) << "参考帧中未找到亮暗边界（四边形可能铺满视口）";
	//测点：边界外 2 像素（模糊半径 4 的扩散范围内）
	uint32_t probe_x = edge_x + 2;
	if (probe_x >= width)
		probe_x = width - 1;
	RecordProperty("bloom_probe_edge_x", static_cast<int>(edge_x));
	RecordProperty("bloom_probe_x", static_cast<int>(probe_x));
	//按链路格式出「提取 → 模糊 → 叠加」一帧
	auto frame_by = [&](uint64_t scene_target, uint64_t scene_pipeline,
		uint64_t extract_stage, uint64_t blur_stage, uint64_t extract_out, uint64_t blur_out)
	{
		renderer.device().render_target_present_set(scene_target);
		renderer.resource().pipeline_bind_set(scene_pipeline);
		uint64_t stages[3] = { extract_stage, blur_stage, overlay };
		EXPECT_TRUE(renderer.device().post_chain_set(stages, 3));
		//模糊半径取 4（须在链长 3 之后设置：级序 1 即模糊级，链外下标会被忽略）
		renderer.device().post_stage_param_set(1, 0.6f, 0.1f, 4.0f);
		uint64_t chain_targets[3] = { scene_target, extract_out, blur_out };
		EXPECT_TRUE(renderer.device().render_target_present_chain_set(chain_targets, 3));
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	//16F 链路
	frame_by(scene_hdr, scene_hdr_pipeline, extract_hdr, blur_hdr, extract_out_hdr, blur_out_hdr);
	int hdr_probe = static_cast<int>(pixel_get(pixels, probe_x, height / 2)[0]);
	//8 位链路
	frame_by(scene_ldr, scene_ldr_pipeline, extract_ldr, blur_ldr, extract_out_ldr, blur_out_ldr);
	int ldr_probe = static_cast<int>(pixel_get(pixels, probe_x, height / 2)[0]);
	int reference_probe = static_cast<int>(pixel_get(reference, probe_x, height / 2)[0]);
	RecordProperty("bloom_reference_probe", reference_probe);
	RecordProperty("bloom_16f_probe", hdr_probe);
	RecordProperty("bloom_8bit_probe", ldr_probe);
	//参考帧测点应仍是清屏色（无泛光；清屏色本机约 88）
	EXPECT_LE(reference_probe, 95) << "参考帧测点不是暗区（用例前提不成立）：" << reference_probe;
	//16F 链路的泛光应扩散到边界外并明显提亮
	EXPECT_GT(hdr_probe, reference_probe + 10) << "16F 泛光未扩散到边界外暗区：参考 " << reference_probe
		<< " / 16F " << hdr_probe;
	//同一条链下 16F 的泛光强度应显著高于 8 位（写入截断导致 4.0 → 1.0）
	EXPECT_GE(hdr_probe, ldr_probe + 15) << "16F 泛光闭环未强于 8 位：16F " << hdr_probe
		<< " / 8 位 " << ldr_probe;
	renderer.resource().pipeline_bind_set(0);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(blur_ldr);
	renderer.resource().pipeline_unload(blur_hdr);
	renderer.resource().pipeline_unload(extract_ldr);
	renderer.resource().pipeline_unload(extract_hdr);
	renderer.resource().pipeline_unload(overlay);
	renderer.resource().pipeline_unload(scene_ldr_pipeline);
	renderer.resource().pipeline_unload(scene_hdr_pipeline);
	renderer.resource().render_target_destroy(blur_out_ldr);
	renderer.resource().render_target_destroy(extract_out_ldr);
	renderer.resource().render_target_destroy(blur_out_hdr);
	renderer.resource().render_target_destroy(extract_out_hdr);
	renderer.resource().render_target_destroy(scene_ldr);
	renderer.resource().render_target_destroy(scene_hdr);
}

/*
HDR 目标开关（方向九第 756 轮）
口径：Renderer_Config.hdr_targets 默认关闭 —— resource().render_target_create 跟随交换链格式（8 位）；
开启后该接口改用 16F，使离屏目标可保存 >1 的线性值。
用例以曝光 +2 的白四边形为输入，同一段读回逻辑区分两种目标：8 位中心 ≈1.0、16F 中心 ≈4.0。
16F 目标的渲染通道需要格式匹配的场景管线变体，故该分支经 pipeline_load_ex 登记。
*/
TEST_F(Renderer_Post_Chain_Test, HDR目标开关生效)
{
	const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
	//读回目标中心 R（16F 原始线性值 / 8 位按 255 归一）
	auto center_of = [](engine::Renderer& renderer, uint64_t target)
	{
		const uint32_t width = renderer.device().extent_width();
		const uint32_t height = renderer.device().extent_height();
		std::vector<float> values(static_cast<std::size_t>(width) * height * 4);
		uint32_t read_width = 0;
		uint32_t read_height = 0;
		EXPECT_TRUE(renderer.resource().render_target_readback_float(target, values.data(),
			static_cast<uint32_t>(values.size()), &read_width, &read_height));
		if (read_width == 0 || read_height == 0)
			return 0.0f;
		return values[(static_cast<std::size_t>(read_height / 2) * read_width + read_width / 2) * 4];
	};
	//关闭（默认）：目标为 8 位，中心被写入处截断
	float ldr_center = 0.0f;
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		EXPECT_FALSE(renderer.device().hdr_targets()) << "默认配置不应开启 16F 目标";
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target,
			renderer.device().extent_width(), renderer.device().extent_height()));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		renderer.device().render_target_present_set(target);
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		ldr_center = center_of(renderer, target);
		renderer.resource().mesh_unload(handle);
		renderer.resource().render_target_destroy(target);
	}
	//开启：目标为 16F，中心保留 >1
	float hdr_center = 0.0f;
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		EXPECT_TRUE(renderer.device().hdr_targets()) << "配置开启后查询应为真";
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		//16F 目标需要格式匹配的场景管线变体
		uint64_t scene_pipeline = 0;
		ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_pipeline, "场景.vert.spv", "场景.frag.spv",
			engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target,
			renderer.device().extent_width(), renderer.device().extent_height()));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		renderer.resource().pipeline_bind_set(scene_pipeline);
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		renderer.device().render_target_present_set(target);
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		hdr_center = center_of(renderer, target);
		renderer.resource().pipeline_bind_set(0);
		renderer.resource().mesh_unload(handle);
		renderer.resource().pipeline_unload(scene_pipeline);
		renderer.resource().render_target_destroy(target);
	}
	RecordProperty("hdr_switch_ldr_center", ldr_center);
	RecordProperty("hdr_switch_16f_center", hdr_center);
	//默认关闭 → 8 位目标（1.0 上限）
	EXPECT_LE(ldr_center, 1.0f) << "关闭开关时目标出现 >1（实测 " << ldr_center << "）";
	EXPECT_GE(ldr_center, 0.9f) << "关闭开关时目标基线异常（实测 " << ldr_center << "）";
	//开启 → 16F 目标（保留曝光后的 4.0）
	EXPECT_GT(hdr_center, 1.5f) << "开启开关后目标未保留 >1（实测 " << hdr_center << "）";
	EXPECT_NEAR(hdr_center, 4.0f, 0.6f) << "16F 目标口径异常（应 ≈ 曝光后原值 4.0）";
}

/*
HDR 默认路径（方向九第 757 轮）
口径：hdr_targets 开启且未显式指定呈现 / 渲染目标时，场景写入内部 16F 场景目标，
再由内置色调映射级呈现到交换链；关闭时保持既有直通路径。
曝光 +2 使白四边形在 16F 下为线性 4.0，经色调映射压缩后中心低于直通的饱和值。
*/
TEST_F(Renderer_Post_Chain_Test, HDR默认路径出帧)
{
	//中心 R 采样（默认路径直接回读交换链）
	auto center_of = [](engine::Renderer& renderer, const std::vector<uint8_t>& pixels)
	{
		(void)renderer;
		return static_cast<int>(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
	};
	//关闭（默认）：既有直通路径
	int ldr_center = 0;
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		EXPECT_FALSE(renderer.device().hdr_targets());
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		EXPECT_EQ(renderer.frame_count(), 1u);
		ldr_center = center_of(renderer, pixels);
		renderer.resource().mesh_unload(handle);
	}
	//开启：内部 16F 场景目标 ＋ 内置色调映射
	int hdr_center = 0;
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		EXPECT_TRUE(renderer.device().hdr_targets());
		renderer.resource().texture_bind_set(0);
		ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		//未显式设置任何目标：默认路径自行接管
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		EXPECT_EQ(renderer.frame_count(), 1u);
		hdr_center = center_of(renderer, pixels);
		renderer.resource().mesh_unload(handle);
	}
	RecordProperty("hdr_default_ldr_center", ldr_center);
	RecordProperty("hdr_default_16f_center", hdr_center);
	//关闭时直通饱和（白 1.0 → 255）
	EXPECT_GE(ldr_center, 250) << "关闭开关时默认路径应为直通饱和白：" << ldr_center;
	//开启时经 16F 与色调映射：中心被压缩但仍为亮部（且严格低于直通饱和值）
	EXPECT_LE(hdr_center, ldr_center - 2) << "开启开关后未经过色调映射（中心仍为直通饱和）："
		<< hdr_center << " / " << ldr_center;
	EXPECT_GE(hdr_center, 200) << "开启开关后画面异常（中心过暗）：" << hdr_center;
}

/*
HDR 默认路径 · 色调映射算子可切换（方向九第 759 轮）
口径：Renderer_Config.hdr_tone_operator 选择默认路径末级算子（0 = ACES、1 = 胶片、2 = 指数、3 = 扩展 Reinhard）。
曝光 +2 的白四边形在 16F 下为线性 4.0，四种算子应给出不同且均未饱和（< 255）的中心值；
越界值按 0 处理（与 ACES 完全一致）。
*/
TEST_F(Renderer_Post_Chain_Test, HDR默认路径色调算子)
{
	//按算子出一帧默认路径画面，返回中心 R
	auto center_by = [&](uint32_t tone_operator)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		config.hdr_tone_operator = tone_operator;
		EXPECT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			return -1;
		renderer.resource().texture_bind_set(0);
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		int center = static_cast<int>(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[0]);
		renderer.resource().mesh_unload(handle);
		return center;
	};
	int aces = center_by(0);
	int film = center_by(1);
	int exponent = center_by(2);
	int reinhard = center_by(3);
	int fallback = center_by(9);
	RecordProperty("tone_aces", aces);
	RecordProperty("tone_film", film);
	RecordProperty("tone_exponent", exponent);
	RecordProperty("tone_reinhard", reinhard);
	//本机不支持回读（-1）则跳过
	if (aces < 0 || film < 0 || exponent < 0 || reinhard < 0 || fallback < 0)
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//ACES / 胶片 / 指数应未饱和；扩展 Reinhard 白点为 2.0，4.0 处趋于白点，允许饱和
	EXPECT_LT(aces, 255);
	EXPECT_LT(film, 255);
	EXPECT_LT(exponent, 255);
	/*
	各算子取曲线不同：至少两两之间有一半以上组合可区分（≥3 级差），
	此处以「胶片与 ACES 不同」为主断言，其余记录属性供后续校准。
	*/
	EXPECT_GE(std::abs(film - aces), 3) << "胶片与 ACES 不可区分：" << film << " / " << aces;
	//越界算子回落 ACES（与 0 逐像素一致）
	EXPECT_EQ(fallback, aces) << "越界算子未回落 ACES：" << fallback << " / " << aces;
}

/*
HDR 默认路径 · AgX 彩色高光去饱和（方向九第 760 轮）
口径：AgX 与逐通道算子的关键差别是彩色高光经输入矩阵去饱和。
场景为纯红四边形 + 曝光 +2（线性 4.0 红）：ACES 逐通道仍保持纯红（G ≈ 0），
AgX 的 G / B 会出现明显抬升（去饱和），同时 R 仍为亮部。
*/
TEST_F(Renderer_Post_Chain_Test, HDR默认路径AgX去饱和)
{
	//纯红纹理（RGBA8，16×16）
	std::vector<uint8_t> red_pixels(16 * 16 * 4, 0);
	for (std::size_t index = 0; index < red_pixels.size(); index += 4)
	{
		red_pixels[index] = 255;
		red_pixels[index + 3] = 255;
	}
	//按算子出一帧，返回中心像素
	auto center_by = [&](uint32_t tone_operator)
	{
		std::array<uint8_t, 4> center{};
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		config.hdr_targets = true;
		config.hdr_tone_operator = tone_operator;
		EXPECT_TRUE(renderer.init(config));
		renderer.resource().texture_bind_set(0);
		uint64_t red_texture = 0;
		EXPECT_TRUE(renderer.resource().texture_load(red_texture, red_pixels.data(), 16, 16));
		EXPECT_TRUE(renderer.scene().exposure_set(2.0f));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		renderer.resource().texture_bind_set(red_texture);
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		renderer.resource().mesh_unload(handle);
		renderer.resource().texture_unload(red_texture);
		return center;
	};
	std::array<uint8_t, 4> aces = center_by(0);
	std::array<uint8_t, 4> agx = center_by(4);
	RecordProperty("agx_aces_center", pixel_text(aces));
	RecordProperty("agx_agx_center", pixel_text(agx));
	//交换链为 B8G8R8A8：红色落在 index 2，非红通道为 index 0 / 1
	//ACES：纯红高光（非红通道近零）
	EXPECT_GE(aces[2], 200) << "ACES 红色高光异常：" << pixel_text(aces);
	EXPECT_LE(aces[0], 5) << "ACES 出现去饱和（通道 0 过高）：" << pixel_text(aces);
	EXPECT_LE(aces[1], 5) << "ACES 出现去饱和（通道 1 过高）：" << pixel_text(aces);
	//AgX：去饱和（非红通道明显抬升），红通道仍为亮部
	EXPECT_GE(agx[0], 60) << "AgX 未对彩色高光去饱和（通道 0 过低）：" << pixel_text(agx);
	EXPECT_GE(agx[1], 60) << "AgX 未对彩色高光去饱和（通道 1 过低）：" << pixel_text(agx);
	EXPECT_GE(agx[2], 200) << "AgX 红色高光过暗：" << pixel_text(agx);
	EXPECT_GT(agx[1], aces[1] + 30) << "AgX 与 ACES 的通道差异不足：" << agx[1] << " / " << aces[1];
}
