/*
渲染粒子路径与软粒子测试（2026-11-21 自「渲染粒子测试.cpp」拆出）
覆盖：绘制路径观测、三路径绘制对照、软粒子深度淡化、离屏软粒子深度淡化、
      软粒子档位开销与扫掠、粒子参数快照读取。
*/

#include "渲染粒子测试辅助.h"

/*
粒子绘制路径观测（第三百五十轮）
同一渲染器依次走 CPU / GPU 直接 / GPU 间接三条登记路径，`Frame_Stats.particle_draw_path` 依次为 1 / 2 / 3；
未开启粒子的新渲染器为 0（未登记）。
*/
TEST(Renderer_Particle_Test, 粒子绘制路径观测)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	//未开启粒子：未登记
	EXPECT_EQ(renderer.frame_stats_get().particle_draw_path, 0u);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(256));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 0.3f;
	emitter.count = 64;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(64), 64u);
	(void)粒子.step(0.05f);
	//CPU 参考路径
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_draw(0.2f));
	EXPECT_EQ(renderer.frame_stats_get().particle_draw_path, 1u);
	//GPU 直接路径（常驻）
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 64), 64u);
	for (uint32_t step = 0; step < 3; ++step)
		ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.05f));
	ASSERT_TRUE(renderer.particle().particle_gpu_draw(0.2f));
	EXPECT_EQ(renderer.frame_stats_get().particle_draw_path, 2u);
	//GPU 间接路径
	ASSERT_TRUE(renderer.particle().particle_indirect_draw(0.2f));
	EXPECT_EQ(renderer.frame_stats_get().particle_draw_path, 3u);
	ASSERT_TRUE(renderer.frame_render());
}

/*
粒子三路径绘制对照（第三百四十五轮；阶段 D 后基线）
同一 65536 常驻粒子分别走 CPU 绘制 / GPU 直接绘制 / GPU 间接绘制三帧（每帧前清零统计）：
三路径实际绘制实例数一致、均有可见像素、可见像素数差异 ≤ 5%，并记录主机录制耗时。
*/
TEST(Renderer_Particle_Test, 粒子三路径绘制对照)
{
	engine::Renderer_Config config;
	config.width = 320;
	config.height = 180;
	config.hidden = true;
	//实例容量与粒子容量同规模（CPU 路径逐实例上传模型矩阵，默认 256 不够）
	config.instance_capacity = engine::PARTICLE_EMIT_COUNT_MAX;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_EMIT_COUNT_MAX));
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
	ASSERT_EQ(engine::particle_gpu::emit(renderer, engine::PARTICLE_EMIT_COUNT_MAX), engine::PARTICLE_EMIT_COUNT_MAX);
	//相机与清屏
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 320, 180));
	renderer.device().render_target_set(target);
	//单路径测量结果
	struct 路径结果
	{
		//绘制与出帧是否成功
		bool ok = false;
		//主机侧录制耗时（毫秒）
		double record_ms = 0.0;
		//GPU 帧耗时（毫秒；不可用时 0）
		double gpu_ms = 0.0;
		//实际绘制项数
		uint64_t items = 0;
		//实际绘制实例数
		uint64_t instances = 0;
		//可见像素数
		uint32_t visible = 0;
		//最近一次绘制路径（0 未登记 / 1 CPU / 2 GPU 直接 / 3 GPU 间接）
		uint32_t draw_path = 0;
	};
	auto 测量 = [&](const std::function<bool(engine::Renderer&)>& 绘制) -> 路径结果
	{
		路径结果 结果{};
		//每路径从零统计（累计量含上一路径的数据）
		renderer.frame_stats_reset();
		结果.ok = 绘制(renderer) && renderer.frame_render();
		if (!结果.ok)
			return 结果;
		const engine::Frame_Stats 统计 = renderer.frame_stats_get();
		结果.record_ms = 统计.host_record_ms;
		结果.gpu_ms = 统计.gpu_frame_ms;
		结果.items = 统计.drawn_items;
		结果.instances = 统计.drawn_instances;
		结果.draw_path = 统计.particle_draw_path;
		std::vector<uint8_t> 像素(320u * 180u * 4u, 0u);
		uint32_t 宽 = 0;
		uint32_t 高 = 0;
		if (renderer.resource().render_target_readback(target, 像素.data(), static_cast<uint32_t>(像素.size()), &宽, &高))
			for (std::size_t 偏移 = 0; 偏移 + 3 < 像素.size(); 偏移 += 4)
				//任意通道明显高于清屏色即视为粒子像素
				if (像素[偏移] > 8u || 像素[偏移 + 1] > 8u || 像素[偏移 + 2] > 8u)
					++结果.visible;
		return 结果;
	};
	//三路径（同一帧内容：常驻状态未推进，绘制输入一致；边长 0.2 保证在 320×180 下可见）
	路径结果 CPU路径 = 测量([](engine::Renderer& 渲染器) { return 渲染器.particle().particle_draw(0.2f); });
	路径结果 GPU路径 = 测量([](engine::Renderer& 渲染器) { return 渲染器.particle().particle_gpu_draw(0.2f); });
	路径结果 间接路径 = 测量([](engine::Renderer& 渲染器) { return 渲染器.particle().particle_indirect_draw(0.2f); });
	EXPECT_TRUE(CPU路径.ok);
	EXPECT_TRUE(GPU路径.ok);
	EXPECT_TRUE(间接路径.ok);
	//绘制量一致（每路径 1 个绘制项、65536 实例）
	EXPECT_EQ(CPU路径.instances, engine::PARTICLE_EMIT_COUNT_MAX);
	EXPECT_EQ(GPU路径.instances, engine::PARTICLE_EMIT_COUNT_MAX);
	EXPECT_EQ(间接路径.instances, engine::PARTICLE_EMIT_COUNT_MAX);
	EXPECT_EQ(CPU路径.items, 1u);
	EXPECT_EQ(GPU路径.items, 1u);
	EXPECT_EQ(间接路径.items, 1u);
	//绘制路径观测（第三百四十八轮）：1 = CPU / 2 = GPU 直接 / 3 = GPU 间接
	EXPECT_EQ(CPU路径.draw_path, 1u);
	EXPECT_EQ(GPU路径.draw_path, 2u);
	EXPECT_EQ(间接路径.draw_path, 3u);
	//可见像素非零且三路径接近（同公式、仅构造位置不同：主机 vs 顶点着色器）
	EXPECT_GT(CPU路径.visible, 0u);
	EXPECT_GT(GPU路径.visible, 0u);
	EXPECT_GT(间接路径.visible, 0u);
	auto 差异比例 = [](uint32_t 甲, uint32_t 乙) -> double
	{
		const uint32_t 大 = 甲 > 乙 ? 甲 : 乙;
		if (大 == 0u)
			return 0.0;
		const uint32_t 差 = 甲 > 乙 ? 甲 - 乙 : 乙 - 甲;
		return static_cast<double>(差) / static_cast<double>(大);
	};
	const double GPU差异 = 差异比例(CPU路径.visible, GPU路径.visible);
	const double 间接差异 = 差异比例(CPU路径.visible, 间接路径.visible);
	std::cout << "[三路径绘制] CPU 录制=" << CPU路径.record_ms << " ms 可见=" << CPU路径.visible
		<< "  GPU 录制=" << GPU路径.record_ms << " ms GPU帧=" << GPU路径.gpu_ms << " ms 可见=" << GPU路径.visible
		<< "  间接 录制=" << 间接路径.record_ms << " ms GPU帧=" << 间接路径.gpu_ms << " ms 可见=" << 间接路径.visible
		<< "  GPU差异=" << GPU差异 << " 间接差异=" << 间接差异 << std::endl;
	EXPECT_LT(GPU差异, 0.05);
	EXPECT_LT(间接差异, 0.05);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子软粒子深度淡化（第三百六十六轮；阶段 E）
同一 64×36 目标、同一枚纯蓝粒子（z=0.6）与同一面黑墙（z=0.58，粒子后方 0.02）：
墙面染黑后蓝通道累加近似只反映粒子；开档位（0.005）后粒子贴近墙面处的 Alpha 被按深度差淡出，
蓝和明显低于关闭档位的同场景。每场景先画三帧让该帧槽历史就绪，再取最后一帧像素累加。
*/
TEST(Renderer_Particle_Test, 粒子软粒子深度淡化)
{
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	bool hiz_ok = true;
	//单场景测量（soft_range 为 0 表示关闭软粒子）
	auto run_scene = [&](float soft_range, uint64_t& blue_sum_out) -> void
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		config.hiz_enable = true;
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		engine::Particle_Host 粒子(renderer);
		//能力不足：交由测试体统一跳过
		if (!renderer.compute().hiz_supported())
		{
			hiz_ok = false;
			return;
		}
		renderer.scene().clear_color_set(clear);
		renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(8));
		engine::Particle_Emitter emitter{};
		emitter.position[2] = 0.6f;
		emitter.direction[2] = -1.0f;
		emitter.speed_min = 0.0f;
		emitter.speed_max = 0.0f;
		emitter.lifetime_min = 10.0f;
		emitter.lifetime_max = 10.0f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
		emitter.count = 1;
		engine::Particle_Params params{};
		params.damping = 1.0f;
		params.color_start[2] = 1.0f;
		params.color_end[2] = 1.0f;
		params.color_enabled = 1;
		params.soft_depth_range = soft_range;
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_EQ(粒子.emit(1), 1u);
		//墙面（4×4，向相机方向平移到 z=0.58；染黑避免其蓝通道遮挡比较）
		engine::Mesh_Handle wall = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(wall, 4.0f, 4.0f));
		ASSERT_TRUE(renderer.resource().material_tint_set(0.0f, 0.0f, 0.0f, 1.0f));
		/*
		走交换链路径（Hi-Z 生成只挂在交换链场景退场上；离屏目标不生成历史）；
		最后一帧顺带回读像素（隐藏窗口回读，BGRA）。
		*/
		std::vector<uint8_t> pixels(64 * 36 * 4, 0);
		uint32_t width = 0;
		uint32_t height = 0;
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			float transform[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.58f, 1.0f };
			ASSERT_TRUE(renderer.resource().mesh_draw(wall, transform));
			ASSERT_TRUE(renderer.particle().particle_draw(1.0f));
			if (frame + 1 == 3)
				ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()),
					&width, &height));
			else
				ASSERT_TRUE(renderer.frame_render());
		}
		//诊断口径：生成次数与两个在途帧槽的历史就绪状态
		uint64_t debug_view = 0;
		uint64_t debug_sampler = 0;
		EXPECT_GT(renderer.compute().hiz_generation_count(), 0u);
		EXPECT_TRUE(renderer.compute().hiz_history_ready(0));
		EXPECT_TRUE(renderer.compute().hiz_history_sample_get(0, debug_view, debug_sampler));
		uint64_t sum = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += pixels[index];
		blue_sum_out = sum;
		renderer.resource().mesh_unload(wall);
	};
	uint64_t off_sum = 0;
	uint64_t on_sum = 0;
	run_scene(0.0f, off_sum);
	run_scene(0.005f, on_sum);
	if (!hiz_ok)
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	std::cout << "[软粒子] 关闭档位蓝和=" << off_sum << " 开启档位蓝和=" << on_sum << std::endl;
	EXPECT_GT(off_sum, 0u);
	EXPECT_LT(on_sum * 100u, off_sum * 85u);
}

/*
离屏目标软粒子深度淡化（第三百六十八轮；阶段 E）
离屏渲染目标同样挂接 Hi-Z 场景退场回调（深度历史 ＋ 金字塔）；
GPU 直接与间接两条绘制路径各自比较软粒子档位关闭 / 开启的蓝和，
口径与交换链用例一致（被墙面遮挡的粒子按深度差淡出）。
*/
TEST(Renderer_Particle_Test, 离屏软粒子深度淡化)
{
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	bool hiz_ok = true;
	//单场景测量（indirect 为真走间接绘制；soft_range 为 0 表示关闭软粒子）
	auto run_scene = [&](bool indirect, float soft_range, uint64_t& blue_sum_out) -> void
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		config.hiz_enable = true;
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		engine::Particle_Host 粒子(renderer);
		//能力不足：交由测试体统一跳过
		if (!renderer.compute().hiz_supported())
		{
			hiz_ok = false;
			return;
		}
		renderer.scene().clear_color_set(clear);
		renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(8));
		engine::Particle_Emitter emitter{};
		emitter.position[2] = 0.6f;
		emitter.direction[2] = -1.0f;
		emitter.speed_min = 0.0f;
		emitter.speed_max = 0.0f;
		emitter.lifetime_min = 10.0f;
		emitter.lifetime_max = 10.0f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
		emitter.count = 1;
		engine::Particle_Params params{};
		params.damping = 1.0f;
		params.color_start[2] = 1.0f;
		params.color_end[2] = 1.0f;
		params.color_enabled = 1;
		params.soft_depth_range = soft_range;
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		//GPU 路径须常驻；发射一枚后不推进（粒子停在发射点 z=0.6）
		ASSERT_TRUE(renderer.particle().particle_resident_set(true));
		ASSERT_EQ(engine::particle_gpu::emit(renderer, 1), 1u);
		//墙面（4×4，向相机方向平移到 z=0.58；染黑避免其蓝通道遮挡比较）
		engine::Mesh_Handle wall = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(wall, 4.0f, 4.0f));
		ASSERT_TRUE(renderer.resource().material_tint_set(0.0f, 0.0f, 0.0f, 1.0f));
		//离屏目标与交换链同尺寸（Hi-Z 按交换链范围构建，尺寸不一致时回调跳过）
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 36));
		renderer.device().render_target_set(target);
		//三帧：前两帧建历史，末帧回读
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			float transform[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.58f, 1.0f };
			ASSERT_TRUE(renderer.resource().mesh_draw(wall, transform));
			ASSERT_TRUE(indirect ? renderer.particle().particle_indirect_draw(1.0f) : renderer.particle().particle_gpu_draw(1.0f));
			ASSERT_TRUE(renderer.frame_render());
		}
		//离屏路径同样生成深度历史（第三百六十八轮）
		EXPECT_GT(renderer.compute().hiz_generation_count(), 0u);
		EXPECT_TRUE(renderer.compute().hiz_history_ready(0));
		std::vector<uint8_t> pixels(64 * 36 * 4, 0);
		uint32_t width = 0;
		uint32_t height = 0;
		ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
			&width, &height));
		uint64_t sum = 0;
		//蓝通道（BGR 与 RGB 口径不确定，取红 / 蓝两通道较大者）
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += pixels[index] > pixels[index + 2] ? pixels[index] : pixels[index + 2];
		blue_sum_out = sum;
		renderer.device().render_target_set(0);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(wall);
	};
	uint64_t direct_off = 0;
	uint64_t direct_on = 0;
	uint64_t indirect_off = 0;
	uint64_t indirect_on = 0;
	run_scene(false, 0.0f, direct_off);
	run_scene(false, 0.005f, direct_on);
	run_scene(true, 0.0f, indirect_off);
	run_scene(true, 0.005f, indirect_on);
	if (!hiz_ok)
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	std::cout << "[离屏软粒子] 直接 关闭=" << direct_off << " 开启=" << direct_on
		<< "  间接 关闭=" << indirect_off << " 开启=" << indirect_on << std::endl;
	EXPECT_GT(direct_off, 0u);
	EXPECT_GT(indirect_off, 0u);
	EXPECT_LT(direct_on * 100u, direct_off * 85u);
	EXPECT_LT(indirect_on * 100u, indirect_off * 85u);
}

/*
软粒子档位开销（第三百六十九轮；阶段 E）
同一 320×180 常驻 GPU 直接绘制场景（Hi-Z 开启、1 万粒子），档位关闭 / 开启各跑八帧，
取稳定段（第 3..8 帧）的主机录制与 GPU 帧均值；两档差异应在噪声范围内，开启不得明显退化。
*/
TEST(Renderer_Particle_Test, 软粒子档位开销)
{
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	bool hiz_ok = true;
	//单档位测量（soft_range 为 0 表示关闭软粒子）
	auto measure = [&](float soft_range, double& record_ms_out, double& gpu_ms_out) -> void
	{
		record_ms_out = 0.0;
		gpu_ms_out = 0.0;
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		config.hiz_enable = true;
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		engine::Particle_Host 粒子(renderer);
		//能力不足：交由测试体统一跳过
		if (!renderer.compute().hiz_supported())
		{
			hiz_ok = false;
			return;
		}
		renderer.scene().clear_color_set(clear);
		renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(16384));
		engine::Particle_Emitter emitter{};
		emitter.direction[1] = 1.0f;
		emitter.speed_min = 0.5f;
		emitter.speed_max = 1.0f;
		emitter.lifetime_min = 100.0f;
		emitter.lifetime_max = 100.0f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
		emitter.extents[0] = 1.0f;
		emitter.count = 10000;
		engine::Particle_Params params{};
		params.gravity[1] = -0.5f;
		params.damping = 0.99f;
		params.soft_depth_range = soft_range;
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_TRUE(renderer.particle().particle_resident_set(true));
		ASSERT_EQ(engine::particle_gpu::emit(renderer, 10000), 10000u);
		//八帧：前两帧建历史（第 1 帧无历史、档位自动清零），取第 3..8 帧稳定段
		double record_sum = 0.0;
		double gpu_sum = 0.0;
		uint32_t samples = 0;
		for (uint32_t frame = 0; frame < 8; ++frame)
		{
			ASSERT_TRUE(renderer.particle().particle_gpu_draw(0.04f));
			ASSERT_TRUE(renderer.frame_render());
			//稳定段采样（此时 GPU 时间戳与历史均已就绪）
			if (frame >= 2)
			{
				const engine::Frame_Stats stats = renderer.frame_stats_get();
				record_sum += stats.host_record_ms;
				gpu_sum += stats.gpu_frame_ms;
				++samples;
			}
		}
		record_ms_out = record_sum / samples;
		gpu_ms_out = gpu_sum / samples;
	};
	double off_record = 0.0;
	double off_gpu = 0.0;
	double on_record = 0.0;
	double on_gpu = 0.0;
	measure(0.0f, off_record, off_gpu);
	measure(0.01f, on_record, on_gpu);
	if (!hiz_ok)
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	std::cout << "[软粒子开销] 关闭 录制=" << off_record << " ms GPU帧=" << off_gpu
		<< " ms  开启 录制=" << on_record << " ms GPU帧=" << on_gpu << " ms" << std::endl;
	//开启档位不得明显退化（噪声容差放宽：录制 ≤ 4×＋0.5 ms、GPU ≤ 4×＋0.5 ms；采集环境抖动多次触发 2× 阈值）
	EXPECT_GT(off_record, 0.0);
	EXPECT_LT(on_record, off_record * 4.0 + 0.5);
	EXPECT_LT(on_gpu, off_gpu * 4.0 + 0.5);
}

TEST(Renderer_Particle_Test, 粒子参数快照读取)
{
	//未初始化：全零默认
	engine::Renderer renderer;
	const engine::Particle_Params empty = renderer.particle().particle_params_get();
	EXPECT_FLOAT_EQ(empty.soft_depth_range, 0.0f);
	EXPECT_EQ(empty.color_enabled, 0u);
	//初始化并开启粒子
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	engine::Particle_Params params{};
	params.soft_depth_range = 0.01f;
	params.color_start[2] = 1.0f;
	params.color_enabled = 1;
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	//快照与设置值一致
	const engine::Particle_Params applied = renderer.particle().particle_params_get();
	EXPECT_FLOAT_EQ(applied.soft_depth_range, 0.01f);
	EXPECT_EQ(applied.color_enabled, 1u);
	//非法档位整次拒绝：快照保持既有值
	engine::Particle_Params invalid = params;
	invalid.soft_depth_range = 2.0f;
	EXPECT_FALSE(renderer.particle().particle_params_set(invalid));
	EXPECT_FLOAT_EQ(renderer.particle().particle_params_get().soft_depth_range, 0.01f);
}

/*
软粒子档位扫掠（第三百七十一轮；阶段 E）
同一离屏场景（黑墙在纯蓝粒子后方 0.02、GPU 直接绘制），档位 0 / 0.002 / 0.005 / 0.02 逐档测量：
淡出强度随档位单调不增（片元 alpha = clamp(深度差 / 档位, 0, 1)），蓝和应为非增序列。
*/
TEST(Renderer_Particle_Test, 软粒子档位扫掠)
{
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	bool hiz_ok = true;
	//单档位测量（离屏 64×36、GPU 直接绘制一枚粒子）
	auto measure = [&](float soft_range, uint64_t& sum_out) -> void
	{
		sum_out = 0;
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		config.hiz_enable = true;
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		engine::Particle_Host 粒子(renderer);
		//能力不足：交由测试体统一跳过
		if (!renderer.compute().hiz_supported())
		{
			hiz_ok = false;
			return;
		}
		renderer.scene().clear_color_set(clear);
		renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		ASSERT_TRUE(renderer.particle().particle_enable_set(true));
		ASSERT_TRUE(粒子.capacity_set(8));
		engine::Particle_Emitter emitter{};
		emitter.position[2] = 0.6f;
		emitter.direction[2] = -1.0f;
		emitter.speed_min = 0.0f;
		emitter.speed_max = 0.0f;
		emitter.lifetime_min = 10.0f;
		emitter.lifetime_max = 10.0f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
		emitter.count = 1;
		engine::Particle_Params params{};
		params.damping = 1.0f;
		params.color_start[2] = 1.0f;
		params.color_end[2] = 1.0f;
		params.color_enabled = 1;
		params.soft_depth_range = soft_range;
		ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle().particle_params_set(params));
		ASSERT_TRUE(renderer.particle().particle_resident_set(true));
		ASSERT_EQ(engine::particle_gpu::emit(renderer, 1), 1u);
		//墙面（4×4，向相机方向平移到 z=0.58；染黑避免其蓝通道遮挡比较）
		engine::Mesh_Handle wall = 0;
		ASSERT_TRUE(renderer.resource().mesh_plane_load(wall, 4.0f, 4.0f));
		ASSERT_TRUE(renderer.resource().material_tint_set(0.0f, 0.0f, 0.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 36));
		renderer.device().render_target_set(target);
		//三帧：前两帧建历史，末帧回读
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			float transform[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.58f, 1.0f };
			ASSERT_TRUE(renderer.resource().mesh_draw(wall, transform));
			ASSERT_TRUE(renderer.particle().particle_gpu_draw(1.0f));
			ASSERT_TRUE(renderer.frame_render());
		}
		std::vector<uint8_t> pixels(64 * 36 * 4, 0);
		uint32_t width = 0;
		uint32_t height = 0;
		ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
			&width, &height));
		uint64_t sum = 0;
		//蓝通道（BGR 与 RGB 口径不确定，取红 / 蓝两通道较大者）
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += pixels[index] > pixels[index + 2] ? pixels[index] : pixels[index + 2];
		renderer.device().render_target_set(0);
		renderer.resource().render_target_destroy(target);
		renderer.resource().mesh_unload(wall);
		sum_out = sum;
	};
	uint64_t off = 0;
	uint64_t tiny = 0;
	uint64_t mid = 0;
	uint64_t wide = 0;
	measure(0.0f, off);
	measure(0.002f, tiny);
	measure(0.005f, mid);
	measure(0.02f, wide);
	if (!hiz_ok)
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	std::cout << "[软粒子扫掠] 0=" << off << " 0.002=" << tiny << " 0.005=" << mid
		<< " 0.02=" << wide << std::endl;
	EXPECT_GT(off, 0u);
	//淡出强度随档位单调不增
	EXPECT_LE(tiny, off);
	EXPECT_LE(mid, tiny);
	EXPECT_LE(wide, mid);
	//最大档位相对关闭档显著淡出
	EXPECT_LT(wide * 100u, off * 40u);
}
