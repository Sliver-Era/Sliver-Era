/*
渲染Cluster缓冲与采样测试（2026-11-21 自「渲染Cluster测试.cpp」拆出）
覆盖：光源登记上限与截断 / 容量边界、图形 set0 簇缓冲上传、片元簇采样（八盏 / 三十三盏 / MSAA 四倍）等价。
*/

#include "渲染Cluster测试辅助.h"

TEST(Renderer_Cluster_Test, 光源登记上限与截断)
{
	//关闭 Cluster：登记上限 32 盏，第 33 盏被拒并置截断标志
	engine::Renderer off;
	ASSERT_TRUE(off.init(hidden_config()));
	for (uint32_t index = 0; index < 32; ++index)
	{
		const float position[3] = { static_cast<float>(index) * 0.1f - 1.5f, 0.0f, 0.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		ASSERT_TRUE(off.scene().light_add_point(position, color, 6.0f, 0.4f, 0.1f));
	}
	const float extra_position[3] = { 2.0f, 0.0f, 0.0f };
	const float extra_color[3] = { 1.0f, 1.0f, 1.0f };
	EXPECT_FALSE(off.scene().light_add_point(extra_position, extra_color, 6.0f, 0.4f, 0.1f));
	ASSERT_TRUE(off.frame_render());
	EXPECT_EQ(off.frame_stats_get().light_truncated, 1u);
	EXPECT_EQ(off.scene().point_light_count(), 32u);
	off.scene().lights_clear();
	EXPECT_EQ(off.scene().point_light_count(), 0u);

	//开启 Cluster：可登记超过 8 盏（本用例登记 32 盏）
	engine::Renderer on;
	ASSERT_TRUE(on.init(hidden_config()));
	if (!on.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，Cluster 开启路径不可用";
	}
	ASSERT_TRUE(on.compute().cluster_enable_set(true));
	for (uint32_t index = 0; index < 32; ++index)
	{
		const float position[3] = { static_cast<float>(index) * 0.15f - 2.0f, 0.0f, 0.0f };
		const float color[3] = { 0.8f, 0.8f, 1.0f };
		ASSERT_TRUE(on.scene().light_add_point(position, color, 6.0f, 0.4f, 0.1f));
	}
	EXPECT_EQ(on.scene().point_light_count(), 32u);
	EXPECT_EQ(on.scene().light_record_count(), 34u);
	ASSERT_TRUE(on.frame_render());
	EXPECT_EQ(on.frame_stats_get().light_truncated, 0u);

	//关闭 Cluster 后再次登记被拒，并置截断标志
	ASSERT_TRUE(on.compute().cluster_enable_set(false));
	EXPECT_FALSE(on.scene().light_add_point(extra_position, extra_color, 6.0f, 0.4f, 0.1f));
	ASSERT_TRUE(on.frame_render());
	EXPECT_EQ(on.frame_stats_get().light_truncated, 1u);
}

//光源容量边界（第二百八十三轮）
TEST(Renderer_Cluster_Test, 光源容量边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//本机不支持计算路径时跳过（Cluster 开启路径不可用）
	if (!renderer.compute().cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，Cluster 开启路径不可用";
	}
	ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
	//登记到容量上限（1024 盏）
	constexpr uint32_t CAPACITY = 1024;
	for (uint32_t index = 0; index < CAPACITY; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.1f, 0.0f, 0.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		ASSERT_TRUE(renderer.scene().light_add_point(position, color, 4.0f, 0.2f, 0.1f));
	}
	EXPECT_EQ(renderer.scene().point_light_count(), CAPACITY);
	//记录数 = 主方向光 ＋ 补光 ＋ 逐盏（容量约束下有效数仍为 1024）
	EXPECT_EQ(renderer.scene().light_record_count(), CAPACITY + 2u);
	EXPECT_EQ(engine::light_effective_count(CAPACITY + 2u, true), CAPACITY);
	//第 1025 盏被拒
	const float extra_position[3] = { 3.0f, 0.0f, 0.0f };
	const float extra_color[3] = { 1.0f, 1.0f, 1.0f };
	EXPECT_FALSE(renderer.scene().light_add_point(extra_position, extra_color, 4.0f, 0.2f, 0.1f));
	EXPECT_EQ(renderer.scene().point_light_count(), CAPACITY);
	//清空后归零
	renderer.scene().lights_clear();
	EXPECT_EQ(renderer.scene().point_light_count(), 0u);
	EXPECT_EQ(renderer.scene().light_record_count(), 2u);
}

/*
图形 set0 簇数据缓冲上传（第二百八十八轮）
口径：本轮着色器尚未读取簇缓冲，开 / 关两态画面逐字节一致；
开启时 cluster_gpu_bytes 反映「计数 ＋ 偏移整块 ＋ 列表条目」的上传量，关闭时恒 0。
*/
TEST(Renderer_Cluster_Test, 图形set0簇缓冲上传)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	//两态（下标 0 = 关闭、1 = 开启）
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.cluster_enable = cluster_index != 0;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (config.cluster_enable)
		{
			if (!renderer.compute().cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，无法覆盖簇缓冲上传";
			ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
		}
		renderer.scene().camera_position_set(0.0f, 0.0f, 5.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
		//一盏点光（半径 10 保守覆盖全簇）
		const float position[3] = { 0.0f, 0.0f, 0.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		ASSERT_TRUE(renderer.scene().light_add_point(position, color, 10.0f, 0.5f, 0.1f));
		//逐帧绘制并回读（同上：每帧都带回读）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		engine::Frame_Stats stats = renderer.frame_stats_get();
		//关闭：零上传
		if (cluster_index == 0)
			EXPECT_EQ(stats.cluster_gpu_bytes, 0u);
		else
		{
			//开启：条目 > 0，上传量 = 计数 ＋ 偏移（各 3456 × 4）＋ 列表（条目 × 4）
			EXPECT_GT(stats.cluster_entries, 0u);
			EXPECT_EQ(stats.cluster_gpu_bytes, 3456u * 8u + stats.cluster_entries * 4u);
		}
		results.push_back(std::move(pixels));
		renderer.resource().mesh_unload(mesh);
	}
	//两态画面逐字节一致（着色器本轮不读取簇缓冲）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "簇缓冲接入后开 / 关两态画面不一致";
}


/*
片元簇采样：八盏光源等价（第二百八十九轮）
口径：六盏点光 ＋ 两盏聚光（半径均大到保守覆盖全屏全深度），
Cluster 开启时片元经逐簇列表累加，关闭时走原 8 盏 UBO 前向循环，两态画面须逐字节一致。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_八盏等价)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (cluster_index != 0)
		{
			if (!renderer.compute().cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
		}
		uint64_t texture = 0;
		std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		//关闭主方向光（满照会饱和画面，掩盖点光差异）
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
		//六盏点光 ＋ 两盏聚光（半径 20，均保守覆盖全部簇）
		const float color[3] = { 0.3f, 0.5f, 0.7f };
		for (uint32_t index = 0; index < 6; ++index)
		{
			const float position[3] = { static_cast<float>(index) - 2.5f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.scene().light_add_point(position, color, 20.0f, 0.05f, 0.0f));
		}
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		for (uint32_t index = 0; index < 2; ++index)
		{
			const float position[3] = { index == 0 ? -4.0f : 4.0f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.scene().light_add_spot(position, direction, color, 20.0f, 0.05f, 30.0f, 45.0f, 0.0f));
		}
		//逐帧绘制并回读（本机连续「无回读帧」后再请求回读会得到整幅清屏色，故每帧都带回读）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		//开启态：簇条目已整备并上传
		if (cluster_index != 0)
			EXPECT_GT(renderer.frame_stats_get().cluster_entries, 0u);
		results.push_back(std::move(pixels));
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
	}
	//两态画面逐字节一致（同一 8 盏光源、同一累加顺序）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "八盏光源下簇采样与原前向路径画面不一致";
}

/*
片元簇采样：三十三盏光源扩展（第二百八十九轮；第六百九十九轮 16 槽；第七百零一轮 32 槽）
口径：Cluster 关闭时第 33 盏被拒并置 `light_truncated`；开启时第 33 盏参与照明，
故两态画面必然不同（证明片元确实读取了簇列表与光源记录）。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_三十三盏扩展)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (cluster_index != 0)
		{
			if (!renderer.compute().cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
		}
		uint64_t texture = 0;
		std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		//关闭主方向光（满照会饱和画面，掩盖第 9 盏差异）
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
		//前 32 盏（半径 20 覆盖全部簇；强度压低避免画面饱和后第 33 盏不可见）
		const float color[3] = { 0.4f, 0.5f, 0.6f };
		for (uint32_t index = 0; index < 32; ++index)
		{
			const float position[3] = { static_cast<float>(index) - 15.5f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.scene().light_add_point(position, color, 20.0f, 0.02f, 0.0f));
		}
		//第 33 盏：关闭时被拒，开启时接受（正前方高亮，确保产生可见照度差）
		const float ninth_position[3] = { 0.0f, 0.0f, 3.0f };
		const bool ninth_added = renderer.scene().light_add_point(ninth_position, color, 20.0f, 0.5f, 0.0f);
		if (cluster_index == 0)
			EXPECT_FALSE(ninth_added);
		else
			ASSERT_TRUE(ninth_added);
		//逐帧绘制并回读（本机连续「无回读帧」后再请求回读会得到整幅清屏色，故每帧都带回读）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		engine::Frame_Stats stats = renderer.frame_stats_get();
		//关闭态：截断标志置 1；开启态：无截断
		EXPECT_EQ(stats.light_truncated, cluster_index == 0 ? 1u : 0u);
		results.push_back(std::move(pixels));
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
	}
	//两态画面不同（第 33 盏在开启态参与照明）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_FALSE(same) << "第 33 盏在簇采样开启态未产生可见差异";
}

/*
片元簇采样：MSAA 4× 等价（第二百九十轮）
口径：4× MSAA ＋ 六点光 ＋ 两聚光（覆盖全簇），真实光照下开 / 关两态画面逐字节一致；
确认簇分支与多重采样渲染路径（含解析）组合后不产生像素差异。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_MSAA四倍等价)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 4;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (cluster_index != 0)
		{
			if (!renderer.compute().cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
		}
		uint64_t texture = 0;
		std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		//关闭主方向光（满照会饱和画面）
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_lit()));
		//六盏点光 ＋ 两盏聚光（半径 20，均保守覆盖全部簇）
		const float color[3] = { 0.3f, 0.5f, 0.7f };
		for (uint32_t index = 0; index < 6; ++index)
		{
			const float position[3] = { static_cast<float>(index) - 2.5f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.scene().light_add_point(position, color, 20.0f, 0.05f, 0.0f));
		}
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		for (uint32_t index = 0; index < 2; ++index)
		{
			const float position[3] = { index == 0 ? -4.0f : 4.0f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.scene().light_add_spot(position, direction, color, 20.0f, 0.05f, 30.0f, 45.0f, 0.0f));
		}
		//逐帧绘制并回读
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		results.push_back(std::move(pixels));
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
	}
	//两态画面逐字节一致（4× MSAA 解析后）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "MSAA 4× 下簇采样与原前向路径画面不一致";
}
