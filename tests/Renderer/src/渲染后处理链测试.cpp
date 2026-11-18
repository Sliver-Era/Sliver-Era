/*
渲染后处理链测试（链生命周期 / 单级与两级链 / 预设：灰度、描边、色调映射及串联）
（第五十九轮按功能域拆分：泛光相关见「渲染后处理泛光测试.cpp」、调色与可观测性见「渲染后处理调色测试.cpp」；逻辑未改）
*/
#include "渲染后处理链测试辅助.h"
//场景几何与纹理构造（强光照压力场景用例）
#include "渲染场景测试辅助.h"

//UI 通道（后处理链「UI 后置」用例需要向帧链路挂接绘制回调）
#include "Engine/EngineRenderer/src/render/Renderer/渲染器_UI通道.h"
#include "测试辅助/渲染测试辅助.h"

TEST_F(Renderer_Post_Chain_Test, 未初始化设置无副作用)
{
	engine::Renderer renderer;
	//未初始化时调用应静默返回 / 返回假（不崩溃）
	EXPECT_FALSE(renderer.device().render_target_present_chain_set(nullptr, 1));
	EXPECT_FALSE(renderer.device().post_chain_set(nullptr, 0));
	EXPECT_FALSE(renderer.valid());
}

//未登记句柄被整链拒绝，且无效呈现目标被回落
TEST_F(Renderer_Post_Chain_Test, 无效句柄回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//含未登记句柄的链应被整链拒绝
	uint64_t unregistered = 12345;
	EXPECT_FALSE(renderer.device().post_chain_set(&unregistered, 1));
	//设定一个未登记的呈现目标（应关闭该路径）
	renderer.device().render_target_present_set(999);
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	//仍应走默认交换链路径并成功出帧
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(renderer.frame_count(), 1u);
}

//单级链（count = 1，反色）生效：中心（四边形）被反色为黑、角落（清屏色）被反色为亮
TEST_F(Renderer_Post_Chain_Test, 单级链反色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建场景目标
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	//登记反色管线并设为单级链
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&invert, 1));
	//登记场景并设定同帧后处理呈现
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//一帧内完成（帧计数仅推进 1）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//中心为四边形（白 → 反色后为黑）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD) << "中心应为反色后的黑：" << pixel_text(center);
	EXPECT_GE(center[3], SATURATION_THRESHOLD);
	//角落为清屏色（暗 → 反色后为亮）
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	int corner_min = *std::min_element(corner.begin(), corner.begin() + 3);
	EXPECT_GE(corner_min, 200) << "角落应为反色后的亮色：" << pixel_text(corner);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(invert);
	renderer.resource().render_target_destroy(scene_target);
}

//两级链「反色 → 反色」＝ 原画面：与直接走交换链路径的回读画面一致
TEST_F(Renderer_Post_Chain_Test, 两级反色恒等于原画面)
{
	//参考帧：直接走交换链路径
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.resource().mesh_unload(handle);
	}
	//对照帧：两级「反色 → 反色」链
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		//创建 ping-pong 成对目标
		uint64_t scene_target = 0;
		uint64_t second_target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
		ASSERT_TRUE(renderer.resource().render_target_create(second_target, renderer.device().extent_width(), renderer.device().extent_height()));
		//登记两级反色管线（各自独立句柄，共用同一顶点着色器）
		uint64_t stages[2] = { 0, 0 };
		stages[0] = post_pipeline_register(renderer, "反色.frag.spv");
		stages[1] = post_pipeline_register(renderer, "反色.frag.spv");
		ASSERT_NE(stages[0], 0u);
		ASSERT_NE(stages[1], 0u);
		ASSERT_NE(stages[0], stages[1]);
		ASSERT_TRUE(renderer.device().post_chain_set(stages, 2));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.device().render_target_present_set(scene_target);
		uint64_t chain_targets[2] = { scene_target, second_target };
		ASSERT_TRUE(renderer.device().render_target_present_chain_set(chain_targets, 2));
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		EXPECT_EQ(renderer.frame_count(), 1u);
		renderer.resource().mesh_unload(handle);
		renderer.resource().pipeline_unload(stages[0]);
		renderer.resource().pipeline_unload(stages[1]);
		renderer.resource().render_target_destroy(scene_target);
		renderer.resource().render_target_destroy(second_target);
	}
	//两画面逐通道一致（证明确实按序执行了两级并复合为恒等）
	int difference = max_channel_diff(reference, chained);
	EXPECT_LE(difference, CHAIN_TOLERANCE) << "两级反色链画面与原画面不一致（最大通道差 " << difference << "）";
}

//两级链「反色 → 亮度阈值」：中心（四边形）为纯黑、角落（清屏色）为纯白
TEST_F(Renderer_Post_Chain_Test, 两级反色与阈值叠加)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建 ping-pong 成对目标
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	ASSERT_TRUE(renderer.resource().render_target_create(second_target, renderer.device().extent_width(), renderer.device().extent_height()));
	//登记两级管线：一级反色、二级亮度阈值（顶点复用 后处理.vert）
	uint64_t stages[2] = { 0, 0 };
	stages[0] = post_pipeline_register(renderer, "反色.frag.spv");
	stages[1] = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(stages[0], 0u);
	ASSERT_NE(stages[1], 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(stages, 2));
	//登记场景并设定成对目标
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	/*
	预期：四边形为白（线性 1.0）→ 反色 0 → 亮度 0 < 0.5 → 纯黑；
	      清屏色为暗（约 0.05）→ 反色约 0.95 → 亮度 ≥ 0.5 → 纯白。
	*/
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD) << "中心应为纯黑：" << pixel_text(center);
	EXPECT_GE(center[3], SATURATION_THRESHOLD);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), SATURATION_THRESHOLD) << "角落应为纯白：" << pixel_text(corner);
	EXPECT_GE(corner[3], SATURATION_THRESHOLD);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(stages[0]);
	renderer.resource().pipeline_unload(stages[1]);
	renderer.resource().render_target_destroy(scene_target);
	renderer.resource().render_target_destroy(second_target);
}

/*
链长 1 与既有单级默认路径逐字节等价（收口后 `render_to_target_present` 为 `render_chain_present` 的薄封装）
参考帧走「不设链」的既有单级默认路径（内置 后处理.vert / 后处理.frag）；
对照帧走「链长 1 ＋ 经 pipeline_load 登记同一条 后处理 片元着色器」。
两条路径现共用同一套录制实现，故两帧画面应**逐字节相同**（除取整外无任何差异）。
*/
TEST_F(Renderer_Post_Chain_Test, 单级链与既有单级路径等价)
{
	//参考帧：既有单级默认路径（不设链）
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.device().render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.resource().mesh_unload(handle);
		renderer.resource().render_target_destroy(scene_target);
	}
	//对照帧：链长 1（同一片元着色器，经 pipeline_load 登记）
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
		uint64_t stage = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(stage, 0u);
		ASSERT_TRUE(renderer.device().post_chain_set(&stage, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.device().render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		renderer.resource().mesh_unload(handle);
		renderer.resource().pipeline_unload(stage);
		renderer.resource().render_target_destroy(scene_target);
	}
	//两条路径共用同一录制实现 → 画面应逐字节相同
	int difference = max_channel_diff(reference, chained);
	EXPECT_EQ(difference, 0) << "链长 1 与既有单级默认路径画面不一致（最大通道差 " << difference << "）";
}

/*
目标尺寸与交换链不一致**不再回落**（第十七轮放宽；原 R-FACADE-08 / R-FRAME-09 的 1:1 要求已解除）
用比交换链窄 8 像素的目标作为「同帧后处理呈现」目标 ＋ 单级反色链：本帧应**仍在后处理路径上**出帧，
故中心（白四边形）被反色成黑、角落（清屏色）被反色成亮。
*/
TEST_F(Renderer_Post_Chain_Test, 目标尺寸不一致仍走后处理)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//比交换链窄的目标（触发范围不一致）
	uint64_t narrow_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(narrow_target, renderer.device().extent_width() - 8, renderer.device().extent_height()));
	//链长 1 的反色链（若误走后处理路径则中心会被反色成黑）
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&invert, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(narrow_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//本帧应在后处理路径上正常出帧（尺寸不一致不再拒绝）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//走了后处理：中心被反色成黑、角落被反色成亮
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD)
		<< "中心未被反色（尺寸不一致时仍回落了原路径）：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), 200)
		<< "角落未被反色：" << pixel_text(corner);
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(invert);
	renderer.resource().render_target_destroy(narrow_target);
}

/*
链状态只读诊断入口（post_chain_count / post_chain_get）
断言「计数与级序可读」「卸载中间级后级序压缩」「容量截断与空指针」三件事；
纯只读接口，不改变渲染行为。
*/
TEST_F(Renderer_Post_Chain_Test, 链计数与级序可读)
{
	engine::Renderer renderer;
	//未初始化时一律 0
	EXPECT_EQ(renderer.device().post_chain_count(), 0u);
	uint64_t probe[4] = { 0, 0, 0, 0 };
	EXPECT_EQ(renderer.device().post_chain_get(probe, 4), 0u);
	ASSERT_TRUE(renderer.init(hidden_config()));
	//初始化后仍未设链
	EXPECT_EQ(renderer.device().post_chain_count(), 0u);

	//登记三条后处理管线
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	uint64_t invert_again = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	ASSERT_NE(invert_again, 0u);
	ASSERT_NE(invert, threshold);
	ASSERT_NE(threshold, invert_again);

	//设三级链
	uint64_t chain[3] = { invert, threshold, invert_again };
	ASSERT_TRUE(renderer.device().post_chain_set(chain, 3));
	EXPECT_EQ(renderer.device().post_chain_count(), 3u);
	//逐级读回须与设置顺序一致
	uint64_t read_back[4] = { 0, 0, 0, 0 };
	ASSERT_EQ(renderer.device().post_chain_get(read_back, 4), 3u);
	EXPECT_EQ(read_back[0], invert);
	EXPECT_EQ(read_back[1], threshold);
	EXPECT_EQ(read_back[2], invert_again);

	//容量不足：按容量截断，且返回实际写入数
	uint64_t single[2] = { 0, 0 };
	ASSERT_EQ(renderer.device().post_chain_get(single, 1), 1u);
	EXPECT_EQ(single[0], invert);
	//出参为空：返回 0
	EXPECT_EQ(renderer.device().post_chain_get(nullptr, 4), 0u);

	//卸载中间一级 → 该句柄被摘除、后续级序前移（计数 3 → 2）
	renderer.resource().pipeline_unload(threshold);
	EXPECT_EQ(renderer.device().post_chain_count(), 2u);
	uint64_t compressed[4] = { 0, 0, 0, 0 };
	ASSERT_EQ(renderer.device().post_chain_get(compressed, 4), 2u);
	EXPECT_EQ(compressed[0], invert);
	EXPECT_EQ(compressed[1], invert_again);

	//清空链 → 计数归 0
	ASSERT_TRUE(renderer.device().post_chain_set(nullptr, 0));
	EXPECT_EQ(renderer.device().post_chain_count(), 0u);
	EXPECT_EQ(renderer.device().post_chain_get(compressed, 4), 0u);
	renderer.resource().pipeline_unload(invert);
	renderer.resource().pipeline_unload(invert_again);
}

/*
后处理预设 · 灰度：三通道趋同（去色），白四边形与清屏色均被去色
*/
TEST_F(Renderer_Post_Chain_Test, 灰度预设去色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&gray, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心（白四边形）与角落（清屏色）都应三通道趋同
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2) << "中心 r/g 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[1]) - static_cast<int>(center[2])), 2) << "中心 g/b 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(corner[0]) - static_cast<int>(corner[1])), 2) << "角落 r/g 未趋同：" << pixel_text(corner);
	EXPECT_LE(std::abs(static_cast<int>(corner[1]) - static_cast<int>(corner[2])), 2) << "角落 g/b 未趋同：" << pixel_text(corner);
	//白四边形去色后仍应亮
	EXPECT_GE(center[0], 200) << "去色后中心过暗：" << pixel_text(center);
	EXPECT_GT(corner[0], 4) << "角落去色后被压成纯黑（应保留清屏色亮度）：" << pixel_text(corner);

	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(gray);
	renderer.resource().render_target_destroy(scene_target);
}

/*
后处理预设 · 描边：与灰度帧对比，边界处出现明显压暗（内部差异）
口径：以「同一场景、同一目标的灰度帧」为参考，统计「比参考暗至少 40」的像素数 > 0，
      且中心（无边缘区域）不被大幅压暗 —— 避免依赖具体边界坐标。
*/
TEST_F(Renderer_Post_Chain_Test, 描边预设压暗边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	uint64_t edge = post_pipeline_register(renderer, "描边.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_NE(edge, 0u);
	engine::Mesh_Handle handle = 0;
	engine::Mesh_Handle scene_mesh = 0;
	scene_register(renderer, scene_mesh);
	handle = scene_mesh;
	renderer.device().render_target_present_set(scene_target);

	//第一帧：灰度（作为参考）
	ASSERT_TRUE(renderer.device().post_chain_set(&gray, 1));
	std::vector<uint8_t> gray_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(gray_pixels.data(), static_cast<uint32_t>(gray_pixels.size())));

	//第二帧：描边（同一场景重登记绘制项）
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.device().post_chain_set(&edge, 1));
	std::vector<uint8_t> edge_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(edge_pixels.data(), static_cast<uint32_t>(edge_pixels.size())));

	//边界处应出现明显压暗像素
	uint32_t darkened = darkened_count(edge_pixels, gray_pixels, 40.0f);
	RecordProperty("edge_darkened_pixels", darkened);
	EXPECT_GT(darkened, 0u) << "描边未压暗任何像素（与灰度帧完全一致）";
	//中心（远离四边形边界）不应被明显压暗
	std::array<uint8_t, 4> gray_center = pixel_get(gray_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> edge_center = pixel_get(edge_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(luma_of(gray_center) - luma_of(edge_center), 20.0f)
		<< "中心被过度压暗（描边泄漏到内部）：灰 " << pixel_text(gray_center) << " / 边 " << pixel_text(edge_center);

	renderer.resource().mesh_unload(scene_mesh);
	renderer.resource().pipeline_unload(gray);
	renderer.resource().pipeline_unload(edge);
	renderer.resource().render_target_destroy(scene_target);
}

/*
后处理预设 · 色调映射：高光被压缩（白 255 → 约 230），不再顶到最大亮度
*/
TEST_F(Renderer_Post_Chain_Test, 色调映射压缩高亮)
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
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//白四边形（输入线性 1.0）经简化 ACES 后约 0.80 → sRGB 编码约 232
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("tone_mapped_center", static_cast<int>(center[0]));
	EXPECT_GE(center[0], 200) << "高光被压缩过头：" << pixel_text(center);
	EXPECT_LE(center[0], 246) << "高光未被压缩（仍接近 255）：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2);

	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(tone);
	renderer.resource().render_target_destroy(scene_target);
}

/*
后处理预设 · 胶片色调映射（方向九第 739 轮）
Hable filmic 曲线（无色彩矩阵）：白四边形中心压缩到 170–220、三通道中性（差 ≤ 2），
且与 ACES 预设画面可区分（新增算子可切换）。
*/
TEST_F(Renderer_Post_Chain_Test, 胶片色调映射压缩高亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t aces = post_pipeline_register(renderer, "色调映射.frag.spv");
	uint64_t filmic_stage = post_pipeline_register(renderer, "胶片色调映射.frag.spv");
	ASSERT_NE(aces, 0u);
	ASSERT_NE(filmic_stage, 0u);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	//ACES 基线
	ASSERT_TRUE(renderer.device().post_chain_set(&aces, 1));
	std::vector<uint8_t> aces_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(aces_pixels.data(), static_cast<uint32_t>(aces_pixels.size())));
	//胶片曲线
	ASSERT_TRUE(renderer.device().post_chain_set(&filmic_stage, 1));
	std::vector<uint8_t> filmic_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(filmic_pixels.data(), static_cast<uint32_t>(filmic_pixels.size())));
	std::array<uint8_t, 4> aces_center = pixel_get(aces_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> filmic_center = pixel_get(filmic_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(filmic_center[0], 170) << "胶片曲线压暗过头：" << pixel_text(filmic_center);
	EXPECT_LE(filmic_center[0], 220) << "胶片曲线未压缩高光：" << pixel_text(filmic_center);
	EXPECT_LE(std::abs(static_cast<int>(filmic_center[0]) - static_cast<int>(filmic_center[1])), 2)
		<< "胶片曲线出现通道色偏：" << pixel_text(filmic_center);
	EXPECT_LE(std::abs(static_cast<int>(filmic_center[1]) - static_cast<int>(filmic_center[2])), 2)
		<< "胶片曲线出现通道色偏：" << pixel_text(filmic_center);
	EXPECT_GT(changed_pixels(aces_pixels, filmic_pixels, 4), 0u) << "胶片曲线与 ACES 画面无法区分";
	RecordProperty("filmic_center", static_cast<int>(filmic_center[0]));
	RecordProperty("aces_center", static_cast<int>(aces_center[0]));
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(filmic_stage);
	renderer.resource().pipeline_unload(aces);
	renderer.resource().render_target_destroy(scene_target);
}

/*
胶片曲线白点可配（方向九第 741 轮）
口径：未设参（泛光参数槽全 0）与第 739 轮逐字节一致；
白点调到 4.0 后中心亮度上升且通道仍中性；参数复位后画面逐字节回到默认。
*/
TEST_F(Renderer_Post_Chain_Test, 胶片曲线白点可配)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t filmic_stage = post_pipeline_register(renderer, "胶片色调映射.frag.spv");
	ASSERT_NE(filmic_stage, 0u);
	ASSERT_TRUE(renderer.device().post_chain_set(&filmic_stage, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.device().render_target_present_set(scene_target);
	//按当前参数出帧（绘制项按帧提交）
	auto frame_now = [&](std::vector<uint8_t>& pixels)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	//默认口径（参数槽全 0）
	std::vector<uint8_t> default_pixels(pixel_bytes());
	frame_now(default_pixels);
	//白点 4.0、曝光偏置保持默认（逐级额外四参槽）
	renderer.device().post_stage_extra_set(0, 4.0f, 2.0f, 0.0f, 0.0f);
	std::vector<uint8_t> bright_pixels(pixel_bytes());
	frame_now(bright_pixels);
	//参数复位
	renderer.device().post_stage_extra_set(0, 0.0f, 0.0f, 0.0f, 0.0f);
	std::vector<uint8_t> reset_pixels(pixel_bytes());
	frame_now(reset_pixels);
	std::array<uint8_t, 4> default_center = pixel_get(default_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> bright_center = pixel_get(bright_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//默认口径落在第 739 轮的既有区间
	EXPECT_GE(default_center[0], 170) << "默认胶片曲线偏离既有口径：" << pixel_text(default_center);
	EXPECT_LE(default_center[0], 220) << "默认胶片曲线偏离既有口径：" << pixel_text(default_center);
	//白点变小 → 归一化分母变小 → 中心更亮
	EXPECT_GE(static_cast<int>(bright_center[0]), static_cast<int>(default_center[0]) + 8)
		<< "白点参数未生效：" << pixel_text(default_center) << " → " << pixel_text(bright_center);
	EXPECT_LE(std::abs(static_cast<int>(bright_center[0]) - static_cast<int>(bright_center[1])), 2)
		<< "参数化后出现通道色偏：" << pixel_text(bright_center);
	//参数复位后逐字节回到默认
	int difference = max_channel_diff(default_pixels, reset_pixels);
	EXPECT_EQ(difference, 0) << "参数复位后未回到默认画面（最大通道差 " << difference << "）";
	RecordProperty("filmic_default_center", static_cast<int>(default_center[0]));
	RecordProperty("filmic_bright_center", static_cast<int>(bright_center[0]));
	renderer.resource().mesh_unload(handle);
	renderer.resource().pipeline_unload(filmic_stage);
	renderer.resource().render_target_destroy(scene_target);
}

/*
强光照压力场景 × 胶片色调映射（方向九第 743 轮）
32 盏点光 ＋ 1000 实例 ＋ 环境光照 ＋ 全部方向九 PBR 特性：
胶片曲线下亮部不顶满（最大通道 ≤ 210），ACES 基线仍饱和（≥ 220），且整体亮度更低。
*/
TEST_F(Renderer_Post_Chain_Test, 强光照下胶片曲线不顶满)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config(320, 180);
	config.instance_capacity = 128;
	config.vsync = false;
	config.validation = false;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//帧范围（与窗口配置一致）
	const uint32_t frame_width = 320;
	const uint32_t frame_height = 180;
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(scene_target, renderer.device().extent_width(), renderer.device().extent_height()));
	uint64_t filmic_stage = post_pipeline_register(renderer, "胶片色调映射.frag.spv");
	uint64_t aces_stage = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(filmic_stage, 0u);
	ASSERT_NE(aces_stage, 0u);
	//灰纹理与小方块网格
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(0.15f)));
	//100 个实例：10 × 10 网格平铺在 z = 0 平面（多物体场景）
	std::vector<std::array<float, 16>> instances(100, rotation_y_matrix(0.0f));
	for (std::size_t index = 0; index < instances.size(); ++index)
	{
		float column = static_cast<float>(index % 10) - 4.5f;
		float row = static_cast<float>((index / 10) % 10) - 4.5f;
		instances[index][12] = column * 0.2f;
		instances[index][13] = row * 0.2f;
		instances[index][14] = 0.0f;
	}
	//8 盏点光环绕 ＋ 主平行光（强度抬高 ＋ 曝光 +2，确保进入亮部饱和区间）
	const float light_color[3] = { 1.0f, 0.85f, 0.7f };
	for (uint32_t index = 0; index < 8; ++index)
	{
		float angle = 6.2831853f * static_cast<float>(index) / 8.0f;
		const float position[3] = { 3.0f * std::cos(angle), 1.5f, 3.0f * std::sin(angle) };
		renderer.scene().light_point_set(position, light_color, 200.0f, 12.0f, 0.0f);
	}
	const float direction[3] = { 0.0f, -0.5f, -1.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, white, 8.0f, 0.05f);
	//曝光 +2（×4）：把场景推入过曝区间，检验色调映射的亮部收束
	ASSERT_TRUE(renderer.scene().exposure_set(2.0f));
	//PBR ＋ 环境光照 ＋ 方向九全部特性
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
	renderer.scene().pbr_skin_sss_set(true);
	renderer.scene().pbr_sheen_set(true);
	//相机置于原点前方，确保方块进入画面
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.device().render_target_present_set(scene_target);
	//按指定算子出帧（实例绘制按帧重新登记）
	auto frame_with = [&](uint64_t stage, std::vector<uint8_t>& pixels)
	{
		EXPECT_TRUE(renderer.device().post_chain_set(&stage, 1));
		EXPECT_TRUE(renderer.resource().mesh_draw_instanced(mesh, instances[0].data(), static_cast<uint32_t>(instances.size())));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	std::vector<uint8_t> filmic_pixels(static_cast<std::size_t>(frame_width) * frame_height * 4);
	std::vector<uint8_t> aces_pixels(static_cast<std::size_t>(frame_width) * frame_height * 4);
	frame_with(filmic_stage, filmic_pixels);
	frame_with(aces_stage, aces_pixels);
	//整帧 RGB 最大通道
	auto max_channel = [](const std::vector<uint8_t>& pixels)
	{
		int maximum = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			for (std::size_t channel = 0; channel < 3; ++channel)
				maximum = std::max(maximum, static_cast<int>(pixels[index + channel]));
		return maximum;
	};
	//整帧平均亮度（Rec.709 权重）
	auto mean_luma = [](const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		std::size_t count = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		{
			sum += 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
			++count;
		}
		return count == 0 ? 0.0 : sum / static_cast<double>(count);
	};
	int filmic_max = max_channel(filmic_pixels);
	int aces_max = max_channel(aces_pixels);
	double filmic_mean = mean_luma(filmic_pixels);
	double aces_mean = mean_luma(aces_pixels);
	//几何确实进入画面（两算子只在非清屏像素上不同）
	EXPECT_GT(changed_pixels(filmic_pixels, aces_pixels, 4), 0u) << "强光照压力场景未绘制出几何";
	EXPECT_LE(filmic_max, 210) << "胶片曲线下亮部仍顶满（最大通道 " << filmic_max << "）";
	EXPECT_GE(aces_max, 220) << "ACES 基线未出现亮部饱和（最大通道 " << aces_max << "）";
	EXPECT_LT(filmic_mean, aces_mean) << "胶片曲线未整体压低亮度";
	RecordProperty("stress_filmic_max", filmic_max);
	RecordProperty("stress_aces_max", aces_max);
	RecordProperty("stress_filmic_mean", filmic_mean);
	RecordProperty("stress_aces_mean", aces_mean);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
	renderer.resource().pipeline_unload(aces_stage);
	renderer.resource().pipeline_unload(filmic_stage);
	renderer.resource().render_target_destroy(scene_target);
}

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
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
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