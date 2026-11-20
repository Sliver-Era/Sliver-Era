/*
渲染粒子容量与统计测试（2026-11-21 自「渲染粒子测试.cpp」拆出）
覆盖：超大容量与重建、模拟统计、降级矩阵、性能基线。
*/

#include "渲染粒子测试辅助.h"

/*
超大容量与重建（第三百二十轮）
容量上限 1 048 576（双缓冲 64 MB ＋ 力场 320 B）：资源可用、零步自检通过、观测字节正确；
同值重复设置不重建；收缩到 64 后按新容量重建且可用。
*/
TEST(Renderer_Particle_Test, 超大容量与重建)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	//未创建资源时为 0
	EXPECT_EQ(renderer.particle().particle_gpu_bytes(), 0u);
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
	ASSERT_TRUE(renderer.particle().particle_gpu_available());
	const uint64_t expected_maximum = static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX)
		* engine::PARTICLE_STRIDE * 2u + static_cast<uint64_t>(engine::PARTICLE_FORCE_MAX) * engine::PARTICLE_FORCE_STRIDE
		+ (8u + (static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) + 31u) / 32u * 4u)
		+ static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) * 4u + 4u
		//间接命令缓冲：在途帧数 2 × 20 B（第三百五十二轮）
		+ 2u * 20u;
	EXPECT_EQ(renderer.particle().particle_gpu_bytes(), expected_maximum);
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
	//同值重复设置：不重建、字节不变
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(renderer.particle().particle_gpu_bytes(), expected_maximum);
	//超上限钳到上限
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX + 1u));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
	//收缩重建
	ASSERT_TRUE(粒子.capacity_set(64));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), 64u);
	ASSERT_TRUE(renderer.particle().particle_gpu_available());
	const uint64_t expected_small = static_cast<uint64_t>(64) * engine::PARTICLE_STRIDE * 2u
		+ static_cast<uint64_t>(engine::PARTICLE_FORCE_MAX) * engine::PARTICLE_FORCE_STRIDE
		+ (8u + (64u + 31u) / 32u * 4u) + static_cast<uint64_t>(64) * 4u + 4u
		//间接命令缓冲：在途帧数 2 × 20 B（第三百五十二轮）
		+ 2u * 20u;
	EXPECT_EQ(renderer.particle().particle_gpu_bytes(), expected_small);
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
}

/*
粒子模拟统计（第三百二十二轮）
CPU 参考路径：出生累计 16、寿命到期死亡 16、活跃 0（出生 − 死亡 = 活跃）；主机侧模拟耗时 ≥ 0。
GPU 路径：十步后统计自洽；设备支持时间戳时 particle_gpu_ms ≥ 0，否则为 −1。
*/
TEST(Renderer_Particle_Test, 粒子模拟统计)
{
	engine::Particle_Emitter emitter = point_emitter();
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 0.6f;
	emitter.count = 16;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	//CPU 参考路径
	engine::Renderer cpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	engine::Particle_Host cpu粒子(cpu);
	ASSERT_TRUE(cpu.particle().particle_enable_set(true));
	ASSERT_TRUE(cpu粒子.capacity_set(64));
	ASSERT_TRUE(cpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle().particle_params_set(params));
	engine::Frame_Stats stats = cpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 0u);
	EXPECT_EQ(stats.particle_born_total, 0u);
	EXPECT_EQ(stats.particle_died_total, 0u);
	ASSERT_EQ(cpu粒子.emit(16), 16u);
	stats = cpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 16u);
	EXPECT_EQ(stats.particle_born_total, 16u);
	//十二步 × 0.05 s = 0.6 s，寿命 ≤ 0.6 s 全部回收
	for (uint32_t step = 0; step < 12; ++step)
		cpu粒子.step(0.05f);
	stats = cpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 0u);
	EXPECT_EQ(stats.particle_born_total, 16u);
	EXPECT_EQ(stats.particle_died_total, 16u);
	EXPECT_EQ(stats.particle_born_total - stats.particle_died_total, stats.particle_active);
	EXPECT_GE(stats.particle_sim_ms, 0.0);
	//GPU 路径
	engine::Renderer gpu;
	ASSERT_TRUE(gpu.init(hidden_config()));
	engine::Particle_Host gpu粒子(gpu);
	ASSERT_TRUE(gpu.particle().particle_enable_set(true));
	ASSERT_TRUE(gpu粒子.capacity_set(64));
	ASSERT_TRUE(gpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle().particle_params_set(params));
	ASSERT_EQ(gpu粒子.emit(16), 16u);
	ASSERT_TRUE(gpu.particle().particle_gpu_available());
	for (uint32_t step = 0; step < 12; ++step)
		ASSERT_TRUE(gpu.particle().particle_gpu_step(0.05f));
	stats = gpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 0u);
	EXPECT_EQ(stats.particle_born_total, 16u);
	EXPECT_EQ(stats.particle_died_total, 16u);
	EXPECT_EQ(stats.particle_born_total - stats.particle_died_total, stats.particle_active);
	EXPECT_GE(stats.particle_sim_ms, 0.0);
	if (gpu.compute().compute_timestamp_supported())
		EXPECT_GE(stats.particle_gpu_ms, 0.0);
	else
		EXPECT_EQ(stats.particle_gpu_ms, -1.0);
}

/*
粒子降级矩阵（第三百二十三轮）
四组环境（计算关闭 / 计算开启 × 粒子关闭 / 粒子开启）：
(i) 粒子关闭与计算开关均不改变既有画面（逐字节一致）；
(ii) 计算关闭时 CPU 镜像路径仍可绘制（与计算开启时的 CPU 绘制逐字节一致）；
(iii) 开启粒子的绘制与关闭态画面不同（确实生效）。
*/
TEST(Renderer_Particle_Test, 粒子降级矩阵)
{
	//离屏渲染到 64×64 目标并读回（BGRA）
	auto render_once = [](engine::Renderer& renderer, engine::Mesh_Handle plane, uint64_t target,
		bool with_particle, std::vector<uint8_t>& out)
	{
		renderer.device().render_target_set(target);
		float transform[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
		renderer.resource().mesh_draw(plane, transform);
		//可选的粒子绘制
		if (with_particle)
			renderer.particle().particle_draw(0.4f);
		EXPECT_TRUE(renderer.frame_render());
		out.assign(64 * 64 * 4, 0);
		uint32_t width = 0;
		uint32_t height = 0;
		EXPECT_TRUE(renderer.resource().render_target_readback(target, out.data(), static_cast<uint32_t>(out.size()),
			&width, &height));
	};
	//初始化配置（可切换计算路径）
	auto make_config = [](bool compute_enable)
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 64;
		config.hidden = true;
		config.compute_enable = compute_enable;
		return config;
	};
	engine::Renderer_Config config_off = make_config(false);
	engine::Renderer_Config config_on = make_config(true);
	std::vector<uint8_t> plain_off;
	std::vector<uint8_t> plain_off_particles_enabled;
	std::vector<uint8_t> plain_on;
	//(1) 计算关闭 ＋ 粒子关闭
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_off));
		engine::Particle_Host 粒子(renderer);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(plane, 1.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
		render_once(renderer, plane, target, false, plain_off);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(plane);
	}
	//(2) 计算关闭 ＋ 粒子开启（不绘制）
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_off));
		engine::Particle_Host 粒子(renderer);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(plane, 1.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
		render_once(renderer, plane, target, false, plain_off_particles_enabled);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(plane);
	}
	EXPECT_EQ(plain_off, plain_off_particles_enabled);
	//(3) 计算开启 ＋ 粒子关闭
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_on));
		engine::Particle_Host 粒子(renderer);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(plane, 1.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
		render_once(renderer, plane, target, false, plain_on);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(plane);
	}
	EXPECT_EQ(plain_off, plain_on);
	//参数：单粒子（原点、速度 0、寿命 1）
	engine::Particle_Emitter emitter{};
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 1;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	std::vector<uint8_t> particle_off;
	std::vector<uint8_t> particle_on;
	//(4) 计算关闭 ＋ 粒子开启 ＋ 绘制
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_off));
		engine::Particle_Host 粒子(renderer);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(8));
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_EQ(粒子.emit(1), 1u);
		//计算不可用：GPU 路径关闭，CPU 镜像仍可用
		EXPECT_FALSE(renderer.particle().particle_gpu_available());
		EXPECT_FALSE(renderer.particle().particle_gpu_step(0.1f));
		EXPECT_FALSE(renderer.particle().particle_supported());
		renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(plane, 4.0f, 4.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
		render_once(renderer, plane, target, true, particle_off);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(plane);
	}
	//(5) 计算开启 ＋ 粒子开启 ＋ 绘制（同一 CPU 状态）
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_on));
		engine::Particle_Host 粒子(renderer);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(8));
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_EQ(粒子.emit(1), 1u);
		EXPECT_TRUE(renderer.particle().particle_supported());
		renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(plane, 4.0f, 4.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
		render_once(renderer, plane, target, true, particle_on);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(plane);
	}
	//(6) 计算开关不影响 CPU 绘制结果；粒子确实改变画面
	EXPECT_EQ(particle_off, particle_on);
	uint32_t changed = 0;
	for (std::size_t index = 0; index < plain_off.size(); ++index)
		if (plain_off[index] != particle_off[index])
			++changed;
	EXPECT_GT(changed, 20u);
}


/*
粒子性能基线（第三百二十五轮）
口径：1920×1080（MSAA 4×，设备不支持时自动降级）、1 万活跃粒子、容量 65 536；
记录 CPU 参考单步、GPU 同步链单步（上传 / 调度 / 回读）与整帧绘制的主机 / GPU 耗时；
另测 1M 容量资源分配与零步自检。
*/
TEST(Renderer_Particle_Test, 粒子性能基线)
{
	engine::Renderer_Config config;
	config.width = 1920;
	config.height = 1080;
	config.hidden = true;
	config.msaa_samples = 4;
	//1 万粒子一次实例化提交：实例容量需覆盖
	config.instance_capacity = 16384;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(65536));
	engine::Particle_Emitter emitter = point_emitter();
	emitter.count = 10000;
	emitter.speed_min = 0.6f;
	emitter.speed_max = 1.4f;
	emitter.lifetime_min = 4.0f;
	emitter.lifetime_max = 6.0f;
	engine::Particle_Params params{};
	params.gravity[1] = -0.8f;
	params.damping = 0.99f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(10000), 10000u);
	//CPU 参考单步（十步平均）
	const auto cpu_begin = std::chrono::high_resolution_clock::now();
	for (uint32_t step = 0; step < 10; ++step)
		粒子.step(1.0f / 60.0f);
	const auto cpu_end = std::chrono::high_resolution_clock::now();
	double cpu_step_ms = std::chrono::duration<double, std::milli>(cpu_end - cpu_begin).count() / 10.0;
	//GPU 同步链单步（十步平均）
	ASSERT_TRUE(renderer.particle().particle_gpu_available());
	const auto gpu_begin = std::chrono::high_resolution_clock::now();
	for (uint32_t step = 0; step < 10; ++step)
		ASSERT_TRUE(renderer.particle().particle_gpu_step(1.0f / 60.0f));
	const auto gpu_end = std::chrono::high_resolution_clock::now();
	double gpu_step_ms = std::chrono::duration<double, std::milli>(gpu_end - gpu_begin).count() / 10.0;
	//整帧绘制（1 万 billboard）
	ASSERT_TRUE(renderer.particle().particle_draw(0.06f));
	ASSERT_TRUE(renderer.frame_render());
	const engine::Frame_Stats draw_stats = renderer.frame_stats_get();
	//1M 容量资源分配与零步自检
	const auto alloc_begin = std::chrono::high_resolution_clock::now();
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	ASSERT_TRUE(renderer.particle().particle_gpu_available());
	const auto alloc_end = std::chrono::high_resolution_clock::now();
	double alloc_ms = std::chrono::duration<double, std::milli>(alloc_end - alloc_begin).count();
	EXPECT_EQ(renderer.particle().particle_gpu_bytes(),
		static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) * engine::PARTICLE_STRIDE * 2u
			+ static_cast<uint64_t>(engine::PARTICLE_FORCE_MAX) * engine::PARTICLE_FORCE_STRIDE
			+ (8u + (static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) + 31u) / 32u * 4u)
			+ static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) * 4u + 4u
			//间接命令缓冲：在途帧数 2 × 20 B（第三百五十二轮）
			+ 2u * 20u);
	const auto probe_begin = std::chrono::high_resolution_clock::now();
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
	const auto probe_end = std::chrono::high_resolution_clock::now();
	double probe_ms = std::chrono::duration<double, std::milli>(probe_end - probe_begin).count();
	std::cout << "[粒子基线] CPU步进=" << cpu_step_ms << " ms  GPU同步步进=" << gpu_step_ms
		<< " ms  录制=" << draw_stats.host_record_ms << " ms  GPU帧=" << draw_stats.gpu_frame_ms
		<< " ms  1M分配=" << alloc_ms << " ms  1M零步自检=" << probe_ms << " ms" << std::endl;
	//合理上限（防止性能退化）
	EXPECT_LT(cpu_step_ms, 10.0);
	EXPECT_LT(gpu_step_ms, 200.0);
	EXPECT_LT(draw_stats.host_record_ms, 10.0);
	EXPECT_LT(alloc_ms + probe_ms, 2000.0);
}
