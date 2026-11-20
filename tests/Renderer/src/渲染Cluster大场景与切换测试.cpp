/*
渲染Cluster大场景与切换测试（2026-11-21 自「渲染Cluster测试.cpp」拆出）
覆盖：五百一十二盏光源 / 聚光灯、簇开关反复切换、簇网格切换与缓冲重建、大光源整帧耗时对照、
      线性衰减提前退出。
*/

#include "渲染Cluster测试辅助.h"

/*
大光源场景：512 盏（第二百九十一轮）
口径：Cluster 开启登记 512 盏点光（记录 514 条），场景正常出帧且簇条目 / 上传量非零；
CPU 与 GPU 整备结果为同一份计数，并输出两路径耗时（每路径 2 次取最小）。
*/
TEST(Renderer_Cluster_Test, 大光源场景_五百一十二盏)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，大光源场景不可用";
	}
	ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
	constexpr uint32_t LIGHT_COUNT = 512;
	const float color[3] = { 0.6f, 0.7f, 0.8f };
	//登记前的记录数基线（全局 / 环境等非逐盏记录）
	const uint32_t base_records = renderer.scene().light_record_count();
	//32 × 16 网格分布（相机前方，半径 2 覆盖局部簇）
	for (uint32_t index = 0; index < LIGHT_COUNT; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.4f - 6.4f,
			static_cast<float>(index / 32) * 0.3f - 2.4f, 1.0f };
		ASSERT_TRUE(renderer.scene().light_add_point(position, color, 2.0f, 0.05f, 0.0f));
	}
	EXPECT_EQ(renderer.scene().point_light_count(), LIGHT_COUNT);
	//记录数应随登记点数在基线上同步增长
	EXPECT_EQ(renderer.scene().light_record_count(), base_records + LIGHT_COUNT);

	//出帧（逐帧回读口径）
	uint64_t texture = 0;
	std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<uint8_t> pixels(pixel_count, 0u);
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
	}
	//簇条目与上传量非零
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_GT(stats.cluster_entries, 0u);
	EXPECT_GT(stats.cluster_gpu_bytes, 0u);

	//CPU / GPU 整备对照（列表容量上界 = 簇数 × 记录数）
	constexpr uint32_t CLUSTERS = 3456;
	constexpr uint32_t LIST_CAPACITY = CLUSTERS * (LIGHT_COUNT + 2u);
	std::vector<uint32_t> counts_cpu(CLUSTERS);
	std::vector<uint32_t> offsets_cpu(CLUSTERS);
	std::vector<uint32_t> list_cpu(LIST_CAPACITY);
	std::vector<uint32_t> counts_gpu(CLUSTERS);
	std::vector<uint32_t> offsets_gpu(CLUSTERS);
	std::vector<uint32_t> list_gpu(LIST_CAPACITY);
	uint32_t cpu_entries = 0;
	uint32_t gpu_entries = 0;
	double cpu_best_ms = 1e9;
	double gpu_best_ms = 1e9;
	for (uint32_t run = 0; run < 2; ++run)
	{
		ASSERT_TRUE(renderer.compute().cluster_data_build(counts_cpu.data(), offsets_cpu.data(), list_cpu.data(),
			CLUSTERS, LIST_CAPACITY, &cpu_entries, false));
		cpu_best_ms = (std::min)(cpu_best_ms, renderer.compute().cluster_build_ms_get());
		ASSERT_TRUE(renderer.compute().cluster_data_build(counts_gpu.data(), offsets_gpu.data(), list_gpu.data(),
			CLUSTERS, LIST_CAPACITY, &gpu_entries, true));
		gpu_best_ms = (std::min)(gpu_best_ms, renderer.compute().cluster_build_ms_get());
	}
	//两路径同一份计数
	EXPECT_EQ(cpu_entries, gpu_entries);
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
		EXPECT_EQ(counts_cpu[cluster], counts_gpu[cluster]);
	std::cout << "[大光源512] CPU " << cpu_best_ms << " ms / GPU " << gpu_best_ms << " ms | 条目 "
		<< cpu_entries << std::endl;

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
大光源场景：512 盏聚光灯（第二百九十四轮）
口径：512 盏聚光（32 × 16 网格、朝 −Z、窄锥）登记后记录数 514，出帧正常、条目 / 上传量非零；
配合着色器「锥外提前退出」，验证聚光分支在大规模下可用且不影响结果。
*/
TEST(Renderer_Cluster_Test, 大光源场景_五百一十二盏聚光灯)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，大光源聚光场景不可用";
	}
	ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
	constexpr uint32_t LIGHT_COUNT = 512;
	const float color[3] = { 0.5f, 0.7f, 0.9f };
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	//登记前的记录数基线（全局 / 环境等非逐盏记录）
	const uint32_t base_records = renderer.scene().light_record_count();
	//32 × 16 网格；内 20° / 外 30° 窄锥，半径 2
	for (uint32_t index = 0; index < LIGHT_COUNT; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.4f - 6.4f,
			static_cast<float>(index / 32) * 0.3f - 2.4f, 1.5f };
		ASSERT_TRUE(renderer.scene().light_add_spot(position, direction, color, 2.0f, 0.05f, 20.0f, 30.0f, 0.0f));
	}
	EXPECT_EQ(renderer.scene().point_light_count(), LIGHT_COUNT);
	//记录数应随登记点数在基线上同步增长
	EXPECT_EQ(renderer.scene().light_record_count(), base_records + LIGHT_COUNT);

	//出帧（逐帧回读）
	uint64_t texture = 0;
	std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<uint8_t> pixels(pixel_count, 0u);
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
	}
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_GT(stats.cluster_entries, 0u);
	EXPECT_GT(stats.cluster_gpu_bytes, 0u);

	//CPU 整备基线（一次，避免用例过长）
	constexpr uint32_t CLUSTERS = 3456;
	std::vector<uint32_t> counts(CLUSTERS);
	std::vector<uint32_t> offsets(CLUSTERS);
	std::vector<uint32_t> list(CLUSTERS * (LIGHT_COUNT + 2u));
	uint32_t entries = 0;
	ASSERT_TRUE(renderer.compute().cluster_data_build(counts.data(), offsets.data(), list.data(),
		CLUSTERS, static_cast<uint32_t>(list.size()), &entries, false));
	EXPECT_GT(entries, 0u);
	std::cout << "[大光源512聚光] CPU " << renderer.compute().cluster_build_ms_get() << " ms | 条目 " << entries << std::endl;

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
/*
簇开关反复切换（第二百九十八轮）
口径：开启 → 关闭 → 再开启，各出一帧均正常；开启帧条目 / 上传量 > 0，关闭帧两者为 0。
覆盖「真实簇缓冲 ↔ 占位缓冲」的绑定切换与资源保留路径。
*/
TEST(Renderer_Cluster_Test, 簇开关反复切换)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，簇开关切换不可用";
	}
	uint64_t texture = 0;
	std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<uint8_t> pixels(pixel_count, 0u);
	//三帧：开启 / 关闭 / 开启
	const bool states[3] = { true, false, true };
	for (uint32_t cycle = 0; cycle < 3; ++cycle)
	{
		ASSERT_TRUE(renderer.compute().cluster_enable_set(states[cycle]));
		ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		const engine::Frame_Stats stats = renderer.frame_stats_get();
		if (states[cycle])
		{
			EXPECT_GT(stats.cluster_entries, 0u);
			EXPECT_GT(stats.cluster_gpu_bytes, 0u);
		}
		else
		{
			EXPECT_EQ(stats.cluster_entries, 0u);
			EXPECT_EQ(stats.cluster_gpu_bytes, 0u);
		}
	}
	//收尾：关闭并释放
	ASSERT_TRUE(renderer.compute().cluster_enable_set(false));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
簇网格切换与缓冲重建（第二百九十二轮）
口径：运行期切换网格（3456 → 512 → 4032 簇），图形 set0 簇缓冲按容量增长重建并重写绑定；
每档出帧后 `cluster_gpu_bytes = 簇数 × 8 ＋ 条目 × 4`，且条目 > 0。
*/
TEST(Renderer_Cluster_Test, 簇网格切换与缓冲重建)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，簇缓冲重建不可用";
	}
	ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
	//一盏点光（半径 10 保守覆盖全簇）
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(renderer.scene().light_add_point(position, color, 10.0f, 0.5f, 0.1f));
	uint64_t texture = 0;
	std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
	//三档网格：默认 3456 → 缩小 512 → 超出总簇上限被收缩到 4032（32 × 18 × 7）
	const uint32_t grid_sets[3][3] = {
		{ 16, 9, 24 },
		{ 8, 8, 8 },
		{ 32, 18, 24 },
	};
	const uint32_t expected_total[3] = { 3456u, 512u, 4032u };
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<uint8_t> pixels(pixel_count, 0u);
	for (uint32_t stage = 0; stage < 3; ++stage)
	{
		ASSERT_TRUE(renderer.compute().cluster_grid_set(grid_sets[stage][0], grid_sets[stage][1], grid_sets[stage][2]));
		EXPECT_EQ(renderer.compute().cluster_grid_cluster_count(), expected_total[stage]);
		ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		const engine::Frame_Stats stats = renderer.frame_stats_get();
		//每档：条目 > 0 且上传量 = 簇数 × 8 ＋ 条目 × 4
		EXPECT_GT(stats.cluster_entries, 0u);
		EXPECT_EQ(stats.cluster_gpu_bytes, expected_total[stage] * 8u + stats.cluster_entries * 4u);
	}
	//恢复默认网格
	ASSERT_TRUE(renderer.compute().cluster_grid_set(16, 9, 24));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
512 盏整帧耗时对照（第三百零一轮）
口径：1920×1080 ＋ MSAA 1 ＋ 全屏四边形（模型缩放 8×），每态渲染 5 帧取平均；
三态对照：Cluster 关闭 8 盏（第 9 盏被拒）、开启 8 盏、开启 512 盏。
目的：量化采样接入后的整帧 GPU / 录制成本，不设机器相关阈值，结果写入性能基线。
*/
TEST(Renderer_Cluster_Test, 大光源整帧耗时对照)
{
	constexpr uint32_t PERF_WIDTH = 1920;
	constexpr uint32_t PERF_HEIGHT = 1080;
	constexpr uint32_t PERF_FRAMES = 5;
	constexpr uint32_t LIGHT_COUNT = 512;
	//三态共用配置（1920 × 1080、隐藏窗口、MSAA 关闭）
	engine::Renderer_Config config;
	config.width = PERF_WIDTH;
	config.height = PERF_HEIGHT;
	config.hidden = true;
	config.msaa_samples = 1;
	engine::Renderer off;
	engine::Renderer on_eight;
	engine::Renderer on_many;
	ASSERT_TRUE(off.init(config));
	ASSERT_TRUE(on_eight.init(config));
	ASSERT_TRUE(on_many.init(config));
	//开启态需要计算路径
	if (!on_many.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，整帧对照不可用";
	}
	ASSERT_TRUE(on_eight.compute().cluster_enable_set(true));
	ASSERT_TRUE(on_many.compute().cluster_enable_set(true));
	//全屏四边形（缩放 8×，法线朝 +Z）
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle on_eight_mesh = 0;
	engine::Mesh_Handle on_many_mesh = 0;
	ASSERT_TRUE(off.resource().mesh_load(off_mesh, quad_data_lit()));
	ASSERT_TRUE(on_eight.resource().mesh_load(on_eight_mesh, quad_data_lit()));
	ASSERT_TRUE(on_many.resource().mesh_load(on_many_mesh, quad_data_lit()));
	std::array<float, 16> model = identity_matrix();
	model[0] = 8.0f;
	model[5] = 8.0f;
	//关闭主方向光（避免满照饱和，同时让片元成本由点光主导）
	const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
	const float no_color[3] = { 1.0f, 1.0f, 1.0f };
	off.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
	on_eight.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
	on_many.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
	//三态光源：32 × 16 网格、半径 1.5（覆盖屏幕局部，列表长度接近真实场景）
	const float light_color[3] = { 0.6f, 0.7f, 0.8f };
	for (uint32_t index = 0; index < 32; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.4f - 6.4f,
			static_cast<float>(index / 32) * 0.3f - 2.4f, 1.5f };
		ASSERT_TRUE(off.scene().light_add_point(position, light_color, 1.5f, 0.05f, 0.0f));
		ASSERT_TRUE(on_eight.scene().light_add_point(position, light_color, 1.5f, 0.05f, 0.0f));
	}
	//关闭态第 33 盏被拒（上限 32）
	const float ninth_position[3] = { 0.0f, 0.0f, 1.5f };
	EXPECT_FALSE(off.scene().light_add_point(ninth_position, light_color, 1.5f, 0.05f, 0.0f));
	for (uint32_t index = 0; index < LIGHT_COUNT; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.4f - 6.4f,
			static_cast<float>(index / 32) * 0.3f - 2.4f, 1.5f };
		ASSERT_TRUE(on_many.scene().light_add_point(position, light_color, 1.5f, 0.05f, 0.0f));
	}
	EXPECT_EQ(on_many.scene().point_light_count(), LIGHT_COUNT);
	//三态各渲染 5 帧并累加 GPU 帧与录制耗时
	double off_gpu = 0.0;
	double off_record = 0.0;
	double eight_gpu = 0.0;
	double eight_record = 0.0;
	double many_gpu = 0.0;
	double many_record = 0.0;
	for (uint32_t frame = 0; frame < PERF_FRAMES; ++frame)
	{
		ASSERT_TRUE(off.resource().mesh_draw(off_mesh, model.data()));
		ASSERT_TRUE(on_eight.resource().mesh_draw(on_eight_mesh, model.data()));
		ASSERT_TRUE(on_many.resource().mesh_draw(on_many_mesh, model.data()));
		ASSERT_TRUE(off.frame_render());
		ASSERT_TRUE(on_eight.frame_render());
		ASSERT_TRUE(on_many.frame_render());
		off_gpu += off.frame_stats_get().gpu_frame_ms;
		off_record += off.frame_stats_get().host_record_ms;
		eight_gpu += on_eight.frame_stats_get().gpu_frame_ms;
		eight_record += on_eight.frame_stats_get().host_record_ms;
		many_gpu += on_many.frame_stats_get().gpu_frame_ms;
		many_record += on_many.frame_stats_get().host_record_ms;
	}
	const double frames = static_cast<double>(PERF_FRAMES);
	const double off_gpu_avg = off_gpu / frames;
	const double eight_gpu_avg = eight_gpu / frames;
	const double many_gpu_avg = many_gpu / frames;
	const double off_record_avg = off_record / frames;
	const double eight_record_avg = eight_record / frames;
	const double many_record_avg = many_record / frames;
	std::cout << "[大光源整帧301] 1920x1080 MSAA1 | GPU 关32盏 " << off_gpu_avg
		<< " ms / 开32盏 " << eight_gpu_avg << " ms / 开512盏 " << many_gpu_avg
		<< " ms | 录制 关32盏 " << off_record_avg << " ms / 开32盏 " << eight_record_avg
		<< " ms / 开512盏 " << many_record_avg << " ms" << std::endl;
	//有效性：三态均产生 GPU 时间戳样本；关闭态记录截断
	EXPECT_GT(off_gpu_avg, 0.0);
	EXPECT_GT(eight_gpu_avg, 0.0);
	EXPECT_GT(many_gpu_avg, 0.0);
	EXPECT_EQ(off.frame_stats_get().light_truncated, 1u);
	//关闭态不做簇整备
	EXPECT_EQ(off.frame_stats_get().cluster_entries, 0u);
	off.resource().mesh_unload(off_mesh);
	on_eight.resource().mesh_unload(on_eight_mesh);
	on_many.resource().mesh_unload(on_many_mesh);
}

/*
片元簇采样：线性衰减提前退出（第三百零四轮）
口径：八盏小半径（2）点光按线性衰减分布在画面各处，多数片元落在部分光源半径外；
Cluster 开启（逐簇列表）与关闭（原 8 盏循环）两态仍须逐字节一致——锥外 / 半径外贡献恰为 0。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_线性衰减提前退出)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		if (cluster_index != 0)
		{
			if (!renderer.compute().cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
		}
		//关闭主方向光，让画面由点光主导
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
		//八盏小半径点光（线性衰减；多数片元只在其中少数光源半径内）
		const float color[3] = { 0.9f, 0.85f, 0.75f };
		for (uint32_t index = 0; index < 8; ++index)
		{
			const float position[3] = { static_cast<float>(index % 4) * 3.0f - 4.5f,
				static_cast<float>(index / 4) * 2.0f - 1.0f, 1.5f };
			ASSERT_TRUE(renderer.scene().light_add_point(position, color, 2.0f, 0.6f, 0.0f, 1u));
		}
		//逐帧回读
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		results.push_back(std::move(pixels));
		renderer.resource().mesh_unload(mesh);
	}
	//两态画面逐字节一致（半径外贡献 0，提前退出不改变结果）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "线性衰减提前退出改变了画面（应逐字节一致）";
}
