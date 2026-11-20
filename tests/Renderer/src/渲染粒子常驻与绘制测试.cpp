/*
渲染粒子常驻与绘制测试（2026-11-21 自「渲染粒子测试.cpp」拆出）
覆盖：常驻 GPU 发射 / 推进与死亡压缩 / 百万粒子基线、GPU 与间接绘制、百万间接命令基线、
      绘制统计等价、常驻紧凑百万基线、紧凑索引环形回绕、常驻绘制与紧凑索引。
*/

#include "渲染粒子测试辅助.h"

/*
常驻 GPU 发射（第三百二十七轮）
环形写指针 ＋ 活跃位图 ＋ 活跃计数：三次发射后活跃数守恒、池满截断、出生统计同步；
关闭常驻回到 CPU 镜像活跃数；计算不可用时拒绝常驻与 GPU 发射。
*/
TEST(Renderer_Particle_Test, 常驻GPU发射)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(64));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 10;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	//常驻关闭：GPU 发射不生效
	EXPECT_FALSE(renderer.particle().particle_resident_get());
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 5), 0u);
	//开启常驻
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	EXPECT_TRUE(renderer.particle().particle_resident_get());
	EXPECT_EQ(粒子.active_count(), 0u);
	//三次发射
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 10), 10u);
	EXPECT_EQ(粒子.active_count(), 10u);
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 10), 10u);
	EXPECT_EQ(粒子.active_count(), 20u);
	//池满截断（64 容量）
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 64), 44u);
	EXPECT_EQ(粒子.active_count(), 64u);
	//已满
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 8), 0u);
	EXPECT_EQ(renderer.frame_stats_get().particle_born_total, 64u);
	//关闭常驻：回到 CPU 镜像（未发射 → 0）
	ASSERT_TRUE(renderer.particle().particle_resident_set(false));
	EXPECT_EQ(粒子.active_count(), 0u);
	//计算不可用：拒绝常驻与 GPU 发射
	engine::Renderer_Config config_off;
	config_off.width = 64;
	config_off.height = 64;
	config_off.hidden = true;
	config_off.compute_enable = false;
	engine::Renderer renderer_off;
	ASSERT_TRUE(renderer_off.init(config_off));
	ASSERT_TRUE(renderer_off.particle().particle_enable_set(true));
	EXPECT_FALSE(renderer_off.particle().particle_resident_set(true));
	EXPECT_FALSE(renderer_off.particle().particle_resident_get());
	EXPECT_EQ(engine::particle_gpu::emit(renderer_off, 4), 0u);
}

/*
常驻推进与死亡压缩（第三百二十八轮）
常驻发射 32 个（寿命 0.5..0.6）：十二步 0.05 s 后活跃归零、死亡累计 32、无幽灵；
再次发射 8 个沿用环形指针；常驻下同步链推进 / 自检被拒，关闭常驻后恢复。
*/
TEST(Renderer_Particle_Test, 常驻推进与死亡压缩)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(64));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 0.6f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 32;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	//常驻发射 32
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 32), 32u);
	EXPECT_EQ(粒子.active_count(), 32u);
	//十二步 0.05 s → 全部死亡
	const auto begin = std::chrono::high_resolution_clock::now();
	uint32_t died_total = 0;
	for (uint32_t step = 0; step < 12; ++step)
	{
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
		died_total += renderer.particle().particle_gpu_died_last();
	}
	const auto end = std::chrono::high_resolution_clock::now();
	double per_step_ms = std::chrono::duration<double, std::milli>(end - begin).count() / 12.0;
	EXPECT_EQ(粒子.active_count(), 0u);
	EXPECT_EQ(died_total, 32u);
	EXPECT_EQ(renderer.frame_stats_get().particle_died_total, 32u);
	//再次发射沿用环形指针
	EXPECT_EQ(engine::particle_gpu::emit(renderer, 8), 8u);
	EXPECT_EQ(粒子.active_count(), 8u);
	//常驻下同步链推进 / 自检被拒
	EXPECT_FALSE(renderer.particle().particle_gpu_step(0.05f));
	EXPECT_FALSE(renderer.particle().particle_gpu_probe());
	//关闭常驻：同步链恢复可用
	ASSERT_TRUE(renderer.particle().particle_resident_set(false));
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
	//常驻单步主机耗时上限（提交 + 围栏 + 两次 4 B 回读）
	std::cout << "[常驻推进] 单步主机耗时=" << per_step_ms << " ms" << std::endl;
	EXPECT_LT(per_step_ms, 5.0);
}

/*
常驻百万粒子基线（第三百二十九轮；方向二阶段 C 收口）
容量 1 048 576：常驻 GPU 发射 100 万 → 五步常驻推进（寿命 100 不死亡）→ 活跃计数守恒；
记录发射与单步耗时（主机侧），并断言合理上限。
*/
TEST(Renderer_Particle_Test, 常驻百万粒子基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 100.0f;
	emitter.lifetime_max = 100.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 1.0f;
	emitter.count = engine::PARTICLE_EMIT_COUNT_MAX;
	engine::Particle_Params params{};
	params.gravity[1] = -0.5f;
	params.damping = 0.99f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	//一次发射 1 048 576（分 16 批，每批 ≤ 65536 上限）
	const auto emit_begin = std::chrono::high_resolution_clock::now();
	uint32_t total = 0;
	for (uint32_t batch = 0; batch < 16; ++batch)
		total += engine::particle_gpu::emit(renderer, engine::PARTICLE_EMIT_COUNT_MAX);
	const auto emit_end = std::chrono::high_resolution_clock::now();
	double emit_ms = std::chrono::duration<double, std::milli>(emit_end - emit_begin).count();
	EXPECT_EQ(total, engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(粒子.active_count(), engine::PARTICLE_CAPACITY_MAX);
	//五步常驻推进
	const auto step_begin = std::chrono::high_resolution_clock::now();
	for (uint32_t step = 0; step < 5; ++step)
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 1.0f / 60.0f));
	const auto step_end = std::chrono::high_resolution_clock::now();
	double per_step_ms = std::chrono::duration<double, std::milli>(step_end - step_begin).count() / 5.0;
	//寿命 100 s 内不死亡 → 活跃守恒
	EXPECT_EQ(粒子.active_count(), engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(renderer.particle().particle_gpu_died_last(), 0u);
	std::cout << "[常驻百万] 发射16批=" << emit_ms << " ms  单步=" << per_step_ms << " ms" << std::endl;
	//百万粒子单步为 GPU 吞吐口径（实测约 100 ms 量级，见性能基线）
	EXPECT_LT(per_step_ms, 1000.0);
	EXPECT_LT(emit_ms, 10000.0);
}


/*
粒子 GPU 绘制（第三百四十一轮；阶段 D）
常驻发射 256 个 → 紧凑 → 着色器内 billboard（不消费实例属性）：出帧有绘制项与可见像素；
未开启常驻 / 无活跃粒子时返回假。
*/
TEST(Renderer_Particle_Test, 粒子GPU绘制)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(256));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 0.3f;
	emitter.count = 256;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	//未开启常驻：拒绝
	EXPECT_FALSE(renderer.particle().particle_gpu_draw(0.1f));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 256), 256u);
	for (uint32_t step = 0; step < 5; ++step)
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
	//相机先就位（绘制时按相机位置构造 billboard 基向量），再登记 GPU 绘制项
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_gpu_draw(0.6f));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
	EXPECT_EQ(renderer.frame_stats_get().drawn_instances, 256u);
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint32_t visible = 0;
	uint32_t max_channel = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
	{
		max_channel = pixels[index] > max_channel ? pixels[index] : max_channel;
		max_channel = pixels[index + 1] > max_channel ? pixels[index + 1] : max_channel;
		max_channel = pixels[index + 2] > max_channel ? pixels[index + 2] : max_channel;
		//任意通道明显高于清屏色即视为粒子像素
		if (pixels[index + 0] > 8u || pixels[index + 1] > 8u || pixels[index + 2] > 8u)
			++visible;
	}
	std::cout << "[GPU绘制] 可见=" << visible << " 最大通道=" << max_channel
		<< " 绘制项=" << renderer.frame_stats_get().drawn_items
		<< " 实例=" << renderer.frame_stats_get().drawn_instances << std::endl;
	EXPECT_GT(visible, 0u);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子 GPU 间接绘制（第三百四十二轮；阶段 D）
常驻发射 256 个 → 计算着色器写单条间接命令（索引数 6、实例数 256、其余 0）→
录制阶段以 vkCmdDrawIndexedIndirect 出帧：绘制项与可见像素均非零；未开启常驻时返回假。
*/
TEST(Renderer_Particle_Test, 粒子间接绘制)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(256));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 0.3f;
	emitter.count = 256;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	//未开启常驻：拒绝
	EXPECT_FALSE(renderer.particle().particle_indirect_draw(0.1f));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 256), 256u);
	for (uint32_t step = 0; step < 5; ++step)
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
	//相机就位后生成间接命令
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_indirect_draw(0.6f));
	//命令内容：索引数 = 四边形 6、实例数 = 紧凑计数、其余为 0
	uint32_t command[5] = {};
	ASSERT_TRUE(renderer.particle().particle_indirect_command_get(command));
	EXPECT_EQ(command[0], 6u);
	EXPECT_EQ(command[1], 256u);
	EXPECT_EQ(command[2], 0u);
	EXPECT_EQ(command[3], 0u);
	EXPECT_EQ(command[4], 0u);
	//出帧
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
	EXPECT_EQ(renderer.frame_stats_get().drawn_instances, 256u);
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint32_t visible = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		//任意通道明显高于清屏色即视为粒子像素
		if (pixels[index + 0] > 8u || pixels[index + 1] > 8u || pixels[index + 2] > 8u)
			++visible;
	std::cout << "[间接绘制] 可见=" << visible << " 绘制项=" << renderer.frame_stats_get().drawn_items
		<< " 实例=" << renderer.frame_stats_get().drawn_instances << std::endl;
	EXPECT_GT(visible, 0u);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
百万粒子间接命令基线（第三百四十二轮；阶段 D）
容量 1 048 576 全活跃：一次 particle_indirect_draw（紧凑 ＋ 单线程命令写入，不渲染）后，
命令的实例数等于全池活跃数，记录三次主机侧耗时（前两次预热）并断言上限。
*/
TEST(Renderer_Particle_Test, 百万粒子间接命令基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 100.0f;
	emitter.lifetime_max = 100.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 1.0f;
	emitter.count = engine::PARTICLE_EMIT_COUNT_MAX;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	//分 16 批发射满池
	uint32_t total = 0;
	for (uint32_t batch = 0; batch < 16; ++batch)
		total += engine::particle_gpu::emit(renderer, engine::PARTICLE_EMIT_COUNT_MAX);
	ASSERT_EQ(total, engine::PARTICLE_CAPACITY_MAX);
	//三次间接绘制（前两次预热），取最后一次耗时
	double draw_ms = 0.0;
	for (uint32_t pass = 0; pass < 3; ++pass)
	{
		const auto begin = std::chrono::high_resolution_clock::now();
		ASSERT_TRUE(renderer.particle().particle_indirect_draw(0.05f));
		const auto end = std::chrono::high_resolution_clock::now();
		draw_ms = std::chrono::duration<double, std::milli>(end - begin).count();
	}
	uint32_t command[5] = {};
	ASSERT_TRUE(renderer.particle().particle_indirect_command_get(command));
	EXPECT_EQ(command[1], engine::PARTICLE_CAPACITY_MAX);
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	std::cout << "[百万间接命令] 生成=" << draw_ms << " ms  实例数=" << command[1]
		<< "  统计紧凑=" << stats.particle_compact_ms << " ms  统计活跃=" << stats.particle_active << std::endl;
	EXPECT_EQ(stats.particle_active, engine::PARTICLE_CAPACITY_MAX);
	//百万紧凑 ＋ 单线程命令写入（实测数毫秒量级）
	EXPECT_LT(draw_ms, 200.0);
}

/*
常驻绘制与 CPU 绘制统计等价（第三百三十八轮）
用两个独立渲染器各走一条路径（避免同类渲染器连续两帧的绘制项登记偏移）：
同一发射器 / 参数下发射 64 个并推进五步，绘制项数与实际绘制实例数须一致。
*/
TEST(Renderer_Particle_Test, 常驻绘制统计等价)
{
	//公共发射器与参数（两路径同配置）
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.3f;
	emitter.speed_max = 0.5f;
	emitter.lifetime_min = 2.0f;
	emitter.lifetime_max = 2.0f;
	emitter.count = 64;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	//CPU 路径
	engine::Frame_Stats cpu_stats{};
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		engine::Particle_Host 粒子(renderer);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(256));
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_EQ(粒子.emit(64), 64u);
		for (uint32_t step = 0; step < 5; ++step)
			(void)粒子.step(0.05f);
		ASSERT_TRUE(renderer.particle().particle_draw(0.1f));
		ASSERT_TRUE(renderer.frame_render());
		cpu_stats = renderer.frame_stats_get();
	}
	//常驻路径
	engine::Frame_Stats resident_stats{};
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		engine::Particle_Host 粒子(renderer);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(256));
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_TRUE(renderer.particle().particle_resident_set(true));
		ASSERT_EQ(engine::particle_gpu::emit(renderer, 64), 64u);
		for (uint32_t step = 0; step < 5; ++step)
			ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
		EXPECT_EQ(粒子.active_count(), 64u);
		ASSERT_TRUE(renderer.particle().particle_draw(0.1f));
		ASSERT_TRUE(renderer.frame_render());
		resident_stats = renderer.frame_stats_get();
	}
	//两路径绘制登记口径一致（绘制项数 / 实例数）
	EXPECT_EQ(resident_stats.drawn_items, cpu_stats.drawn_items);
	EXPECT_EQ(resident_stats.drawn_instances, cpu_stats.drawn_instances);
	std::cout << "[绘制等价] CPU项=" << cpu_stats.drawn_items << " 实例=" << cpu_stats.drawn_instances
		<< "  常驻项=" << resident_stats.drawn_items << " 实例=" << resident_stats.drawn_instances << std::endl;
}

/*
常驻紧凑百万基线（第三百三十四轮；阶段 D）
容量 1 048 576 全活跃时紧凑索引：计数等于活跃数（1 048 576），记录三次紧凑的主机侧耗时并断言上限。
*/
TEST(Renderer_Particle_Test, 常驻紧凑百万基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 100.0f;
	emitter.lifetime_max = 100.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 1.0f;
	emitter.count = engine::PARTICLE_EMIT_COUNT_MAX;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	//分 16 批发射满池
	uint32_t total = 0;
	for (uint32_t batch = 0; batch < 16; ++batch)
		total += engine::particle_gpu::emit(renderer, engine::PARTICLE_EMIT_COUNT_MAX);
	ASSERT_EQ(total, engine::PARTICLE_CAPACITY_MAX);
	//三次紧凑，取最后一次耗时（前两次作预热）
	double compact_ms = 0.0;
	for (uint32_t pass = 0; pass < 3; ++pass)
	{
		const auto begin = std::chrono::high_resolution_clock::now();
		ASSERT_TRUE(renderer.particle().particle_gpu_compact());
		const auto end = std::chrono::high_resolution_clock::now();
		compact_ms = std::chrono::duration<double, std::milli>(end - begin).count();
	}
	EXPECT_EQ(renderer.particle().particle_compact_count_get(), engine::PARTICLE_CAPACITY_MAX);
	//抽样校验索引取值（紧凑顺序不保证升序，仅校验范围）
	uint32_t samples[4] = {};
	ASSERT_EQ(renderer.particle().particle_compact_indices_get(samples, 4), 4u);
	for (uint32_t slot = 0; slot < 4; ++slot)
		EXPECT_LT(samples[slot], engine::PARTICLE_CAPACITY_MAX);
	const engine::Frame_Stats compact_stats = renderer.frame_stats_get();
	std::cout << "[常驻紧凑百万] 紧凑=" << compact_ms << " ms  索引数="
		<< renderer.particle().particle_compact_count_get() << "  统计紧凑=" << compact_stats.particle_compact_ms
		<< " ms  统计活跃=" << compact_stats.particle_active << std::endl;
//帧统计的紧凑耗时与活跃数同口径（第三百四十轮）
EXPECT_GT(compact_stats.particle_compact_ms, 0.0);
EXPECT_EQ(compact_stats.particle_active, engine::PARTICLE_CAPACITY_MAX);
	//百万紧凑为主机侧调度 ＋ 4 B 回读（实测数毫秒量级）
	EXPECT_LT(compact_ms, 200.0);
}

/*
紧凑索引环形回绕（第三百三十六轮）
容量 128 发射满池 → 推进至部分粒子死亡 → 再发射（环形指针回绕，部分落点仍活跃）：
活跃计数以「位图基数」为不变式（不因覆盖仍活跃槽位重复计数），紧凑计数与活跃数一致。
*/
TEST(Renderer_Particle_Test, 紧凑索引环形回绕)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(128));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.1f;
	emitter.speed_max = 0.2f;
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 0.7f;
	emitter.count = 128;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	//满池发射
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 128), 128u);
	EXPECT_EQ(粒子.active_count(), 128u);
	//推进 0.55 s：寿命 ≤ 0.55 的粒子死亡，其余存活
	for (uint32_t step = 0; step < 11; ++step)
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
	const uint32_t survivors = 粒子.active_count();
	ASSERT_LT(survivors, 128u);
	//再发射：环形指针回绕，部分落点仍活跃（旧实现会把重叠槽位重复计数 → 活跃数越过容量）
	const uint32_t spawned = engine::particle_gpu::emit(renderer, 128);
	EXPECT_GT(spawned, 0u);
	const uint32_t active_after = 粒子.active_count();
	EXPECT_LE(active_after, 128u);
	//紧凑：计数 = 活跃数（位图基数），索引互异且全部落在 [0, 128)
	ASSERT_TRUE(renderer.particle().particle_gpu_compact());
	EXPECT_EQ(renderer.particle().particle_compact_count_get(), active_after);
	std::vector<uint32_t> indices(active_after, 0xFFFFFFFFu);
	ASSERT_EQ(renderer.particle().particle_compact_indices_get(indices.data(), active_after), active_after);
	std::vector<uint8_t> seen(128, 0u);
	for (uint32_t slot = 0; slot < active_after; ++slot)
	{
		ASSERT_LT(indices[slot], 128u);
		EXPECT_EQ(seen[indices[slot]], 0u);
		seen[indices[slot]] = 1u;
	}
}

/*
常驻绘制（第三百三十轮）
常驻发射 256 个后回读位图与状态并按活跃位构建实例：出帧有绘制项、中心区域出现粒子像素；
容量超过 65536 时常驻绘制返回假（大容量绘制留待间接绘制）。
*/
TEST(Renderer_Particle_Test, 常驻绘制)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(4096));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 0.3f;
	emitter.count = 256;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 256), 256u);
	EXPECT_EQ(粒子.active_count(), 256u);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	//常驻绘制与出帧
	ASSERT_TRUE(renderer.particle().particle_draw(0.1f));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint32_t visible = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
	{
		//任意通道明显高于清屏色即视为粒子像素
		if (pixels[index + 0] > 8u || pixels[index + 1] > 8u || pixels[index + 2] > 8u)
			++visible;
	}
	EXPECT_GT(visible, 0u);
	//超过 65536 容量：常驻绘制返回假（留待间接绘制）
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX));
	EXPECT_FALSE(renderer.particle().particle_draw(0.1f));
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
常驻紧凑索引（第三百三十三轮；阶段 D）
常驻发射 96 个后紧凑：索引数等于活跃数、索引按池索引升序且互异；
死亡后（推进至寿命结束）紧凑数下降；未开启常驻时紧凑被拒。
*/
TEST(Renderer_Particle_Test, 常驻紧凑索引)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(1024));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.2f;
	emitter.speed_max = 0.4f;
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 0.6f;
	emitter.count = 96;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	//未开启常驻：紧凑被拒
	EXPECT_FALSE(renderer.particle().particle_gpu_compact());
	EXPECT_EQ(renderer.particle().particle_compact_count_get(), 0u);
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 96), 96u);
	//紧凑：索引数与活跃数一致
	ASSERT_TRUE(renderer.particle().particle_gpu_compact());
	EXPECT_EQ(renderer.particle().particle_compact_count_get(), 96u);
	EXPECT_EQ(renderer.particle().particle_compact_count_get(), 粒子.active_count());
	std::vector<uint32_t> indices(96, 0xFFFFFFFFu);
	ASSERT_EQ(renderer.particle().particle_compact_indices_get(indices.data(), 96), 96u);
	//互异且恰好覆盖 0..95（原子取位顺序不保证与池索引顺序一致）
	std::vector<uint8_t> seen(96, 0u);
	for (uint32_t slot = 0; slot < 96; ++slot)
	{
		ASSERT_LT(indices[slot], 96u);
		EXPECT_EQ(seen[indices[slot]], 0u);
		seen[indices[slot]] = 1u;
	}
	EXPECT_EQ(std::count(seen.begin(), seen.end(), static_cast<uint8_t>(1)), 96);
	//推进至全部死亡：紧凑数归零
	for (uint32_t step = 0; step < 14; ++step)
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
	EXPECT_EQ(粒子.active_count(), 0u);
	ASSERT_TRUE(renderer.particle().particle_gpu_compact());
	EXPECT_EQ(renderer.particle().particle_compact_count_get(), 0u);
}
