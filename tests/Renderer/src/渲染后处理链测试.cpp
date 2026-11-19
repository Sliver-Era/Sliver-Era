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

namespace
{
	//显式泛光链装配件（第 778 轮）：16F 场景目标 ＋ 场景管线 ＋ 泛光预设 ＋ 末级色调映射管线
	struct Explicit_Bloom_Chain
	{
		uint64_t scene_target = 0;
		uint64_t scene_pipeline = 0;
		uint64_t tone_pipeline = 0;
		engine::Bloom_Preset preset;
	};

	//按描述建立一套显式泛光链（场景目标与场景管线恒为 16F；调用方负责最后释放）
	//samples = 场景目标采样数；tone_operator 决定末级色调映射片元（0 ACES / 1 胶片 / 2 指数 / 3 扩展 Reinhard / 4 AgX）
	bool explicit_bloom_chain_build(engine::Renderer& renderer, Explicit_Bloom_Chain& chain,
		const engine::Bloom_Preset_Desc& desc, uint32_t samples = 1, uint32_t tone_operator = 0)
	{
		const uint32_t width = renderer.device().extent_width();
		const uint32_t height = renderer.device().extent_height();
		const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
		const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
		if (!renderer.resource().render_target_create_format(chain.scene_target, width, height,
			hdr_format, depth_format, samples))
			return false;
		if (!renderer.resource().pipeline_load_ex(chain.scene_pipeline, "场景.vert.spv", "场景.frag.spv",
			engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, samples, hdr_format))
			return false;
		//末级色调映射片元（与默认路径的算子选择口径一致）
		const char* tone_fragments[5] = { "色调映射.frag.spv", "胶片色调映射.frag.spv", "指数色调映射.frag.spv",
			"Reinhard扩展色调映射.frag.spv", "AgX色调映射.frag.spv" };
		const char* tone_fragment = tone_operator < 5 ? tone_fragments[tone_operator] : tone_fragments[0];
		if (!renderer.resource().pipeline_load(chain.tone_pipeline, "后处理.vert.spv", tone_fragment))
			return false;
		if (!renderer.device().post_bloom_preset_build(chain.preset, desc))
			return false;
		//订阅四级链（提取 → 模糊 → 叠加 → 色调映射）与「场景 ＋ 三个中间目标」
		uint64_t stages[4] = { chain.preset.pipelines[0], chain.preset.pipelines[1], chain.preset.pipelines[2],
			chain.tone_pipeline };
		if (!renderer.device().post_chain_set(stages, 4))
			return false;
		uint64_t targets[4] = { chain.scene_target, chain.preset.targets[0], chain.preset.targets[1],
			chain.preset.targets[2] };
		if (!renderer.device().render_target_present_chain_set(targets, 4))
			return false;
		return renderer.device().post_bloom_preset_params(chain.preset, desc);
	}

	//释放显式泛光链（先摘链、再释放资源）
	void explicit_bloom_chain_destroy(engine::Renderer& renderer, Explicit_Bloom_Chain& chain)
	{
		renderer.device().post_chain_set(nullptr, 0);
		renderer.device().render_target_present_chain_set(nullptr, 0);
		renderer.device().post_bloom_preset_release(chain.preset);
		if (chain.tone_pipeline != 0)
			renderer.resource().pipeline_unload(chain.tone_pipeline);
		if (chain.scene_pipeline != 0)
			renderer.resource().pipeline_unload(chain.scene_pipeline);
		if (chain.scene_target != 0)
			renderer.resource().render_target_destroy(chain.scene_target);
		chain.tone_pipeline = 0;
		chain.scene_pipeline = 0;
		chain.scene_target = 0;
	}
}

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
后处理中间级保留逐材质色调（第 789 轮）
口径：分级调色此前恒写 alpha 1，放在泛光 / 场景与末级色调之间会冲掉逐材质编号；
修复后透传 source.a。用例：两级链「分级调色（恒等参数）→ 通用色调」，左 AgX / 右 ACES 纯蓝仍分裂。
*/
TEST_F(Renderer_Post_Chain_Test, HDR中间级保留逐材质色调)
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
	uint64_t mid_target = 0;
	uint64_t scene_pipeline = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(scene_target, width, height,
		hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().render_target_create_format(mid_target, width, height,
		hdr_format, depth_format, 1));
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
	uint64_t grade_pipeline = post_pipeline_register(renderer, "分级调色.frag.spv");
	uint64_t tone_pipeline = post_pipeline_register(renderer, "材质色调映射.frag.spv");
	ASSERT_NE(grade_pipeline, 0u);
	ASSERT_NE(tone_pipeline, 0u);
	uint64_t stages[2] = { grade_pipeline, tone_pipeline };
	ASSERT_TRUE(renderer.device().post_chain_set(stages, 2));
	uint64_t targets[2] = { scene_target, mid_target };
	ASSERT_TRUE(renderer.device().render_target_present_chain_set(targets, 2));
	//分级调色用恒等参数（lift 0 / gamma 1 / gain 1 / saturation 1），末级回落 ACES
	renderer.device().post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
	renderer.device().post_stage_tone_operator_set(1, 0);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Material_Data base;
	base.texture = texture;
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
	ASSERT_EQ(renderer.frame_stats_get().post_stages, 2u) << "两级链未生效";
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
	RecordProperty("grade_tone_left_span", left_span);
	RecordProperty("grade_tone_right_span", right_span);
	EXPECT_GE(right_span, 120) << "右侧 ACES 未保持纯色：" << right_span;
	EXPECT_LT(left_span, right_span - 30) << "中间级丢失逐材质色调编号：AgX " << left_span
		<< " / ACES " << right_span;
	renderer.device().post_chain_set(nullptr, 0);
	renderer.device().render_target_present_chain_set(nullptr, 0);
	renderer.resource().material_unload(aces_handle);
	renderer.resource().material_unload(agx_handle);
	renderer.resource().texture_unload(texture);
	renderer.resource().pipeline_unload(tone_pipeline);
	renderer.resource().pipeline_unload(grade_pipeline);
	renderer.resource().pipeline_unload(scene_pipeline);
	renderer.resource().render_target_destroy(mid_target);
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
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
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
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
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
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(scene_pipeline, "场景.vert.spv", "场景.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, 1, hdr_format));
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