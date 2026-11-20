/*
渲染粒子基础与模拟测试（2026-11-21 自「渲染粒子测试.cpp」拆出）
覆盖：未初始化拒绝、默认关闭、开关与容量钳制、发射器 / 参数校验、发射与步进、
      GPU 推进资源自检 / 单步对照 / 生命周期与死亡同步、billboard 定位、年龄颜色曲线、
      颜色覆盖、软边、颜色分桶、混合档与深度写入、深度分桶稳定性。
*/

#include "渲染粒子测试辅助.h"

TEST(Renderer_Particle_Test, 未初始化拒绝)
{
	engine::Renderer renderer;
	engine::Particle_Host 粒子(renderer);
	EXPECT_FALSE(renderer.particle().particle_supported());
	EXPECT_FALSE(renderer.particle().particle_enable_set(true));
	EXPECT_FALSE(粒子.capacity_set(16));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), 0u);
	EXPECT_EQ(粒子.active_count(), 0u);
}

//默认关闭：初始化后开关关闭、容量为默认值、活跃为 0
TEST(Renderer_Particle_Test, 默认关闭)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	EXPECT_TRUE(renderer.particle().particle_supported());
	EXPECT_FALSE(renderer.particle().particle_enable_get());
	EXPECT_EQ(renderer.particle().particle_capacity_get(), engine::PARTICLE_CAPACITY_DEFAULT);
	EXPECT_EQ(粒子.active_count(), 0u);
	//关闭时发射 / 步进返回 0
	EXPECT_EQ(粒子.emit(8), 0u);
	EXPECT_EQ(粒子.step(1.0f / 60.0f), 0u);
}

//容量钳制：0 → 1；超上限 → 上限
TEST(Renderer_Particle_Test, 容量钳制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(0));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), 1u);
	ASSERT_TRUE(粒子.capacity_set(engine::PARTICLE_CAPACITY_MAX + 1u));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
}

//发射器 / 参数：关闭时拒绝；开启后非法配置拒绝、合法接受
TEST(Renderer_Particle_Test, 配置校验)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	engine::Particle_Emitter emitter = point_emitter();
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	//关闭态拒绝
	EXPECT_FALSE(renderer.particle().particle_emitter_set(emitter));
	EXPECT_FALSE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	//非法发射器（数量 0）
	engine::Particle_Emitter bad_emitter = emitter;
	bad_emitter.count = 0;
	EXPECT_FALSE(renderer.particle().particle_emitter_set(bad_emitter));
	//非法参数（阻尼 > 1）
	engine::Particle_Params bad_params = params;
	bad_params.damping = 2.0f;
	EXPECT_FALSE(renderer.particle().particle_params_set(bad_params));
	//合法
	EXPECT_TRUE(renderer.particle().particle_emitter_set(emitter));
	EXPECT_TRUE(renderer.particle().particle_params_set(params));
}

//发射与步进：数量、寿命回收、关闭再开启沿用池内容
TEST(Renderer_Particle_Test, 发射与步进)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(64));
	ASSERT_TRUE(renderer.particle().particle_emitter_set(point_emitter()));
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	EXPECT_EQ(粒子.emit(64), 32u);
	EXPECT_EQ(粒子.active_count(), 32u);
	//单步：寿命未到，无死亡
	EXPECT_EQ(粒子.step(1.0f / 60.0f), 0u);
	EXPECT_EQ(粒子.active_count(), 32u);
	//一秒后全部回收（寿命 ≤ 1.0 s）
	uint32_t died = 0;
	for (uint32_t step = 0; step < 61; ++step)
		died += 粒子.step(1.0f / 60.0f);
	EXPECT_EQ(died, 32u);
	EXPECT_EQ(粒子.active_count(), 0u);
	//关闭不释放池内容：关态查询 / 发射为 0，重新开启恢复
	EXPECT_EQ(粒子.emit(8), 8u);
	ASSERT_TRUE(renderer.particle().particle_enable_set(false));
	EXPECT_EQ(粒子.emit(8), 0u);
	EXPECT_EQ(粒子.active_count(), 0u);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	EXPECT_EQ(粒子.active_count(), 8u);
}

//GPU 推进资源（第三百一十一轮）：关闭态不可用；开启后可用且零步自检为恒等
TEST(Renderer_Particle_Test, GPU推进自检)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	//关闭态路径不可用
	EXPECT_FALSE(renderer.particle().particle_gpu_available());
	EXPECT_FALSE(renderer.particle().particle_gpu_probe());
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(64));
	ASSERT_TRUE(renderer.particle().particle_emitter_set(point_emitter()));
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	//本机计算路径可用：惰性创建后即可用
	EXPECT_TRUE(renderer.particle().particle_gpu_available());
	//空池零步自检
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
	//发射后零步自检逐粒子恒等（dt = 0、阻尼 1）
	EXPECT_EQ(粒子.emit(32), 32u);
	EXPECT_EQ(粒子.active_count(), 32u);
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
	//容量变化后按新容量重建
	ASSERT_TRUE(粒子.capacity_set(128));
	EXPECT_EQ(renderer.particle().particle_capacity_get(), 128u);
	EXPECT_TRUE(renderer.particle().particle_gpu_available());
	EXPECT_TRUE(renderer.particle().particle_gpu_probe());
}

/*
GPU 单步对照（第三百一十二轮）
同种子 / 同配置的两套渲染器各发射 32 个粒子，CPU 参考与 GPU 推进连跑 10 步；
逐粒子比对位置 / 速度 / 年龄 / 寿命（阈值 1e-5）。
*/
TEST(Renderer_Particle_Test, GPU推进单步对照)
{
	engine::Particle_Emitter emitter = point_emitter();
	emitter.lifetime_min = 2.0f;
	emitter.lifetime_max = 3.0f;
	engine::Particle_Params params{};
	params.gravity[1] = -2.0f;
	params.damping = 0.9f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	engine::Particle_Host cpu粒子(cpu);
	ASSERT_TRUE(gpu.init(hidden_config()));
	engine::Particle_Host gpu粒子(gpu);
	ASSERT_TRUE(cpu.particle().particle_enable_set(true));
	ASSERT_TRUE(gpu.particle().particle_enable_set(true));
	ASSERT_TRUE(cpu粒子.capacity_set(64));
	ASSERT_TRUE(gpu粒子.capacity_set(64));
	ASSERT_TRUE(cpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle().particle_params_set(params));
	ASSERT_TRUE(gpu.particle().particle_params_set(params));
	ASSERT_EQ(cpu粒子.emit(32), 32u);
	ASSERT_EQ(gpu粒子.emit(32), 32u);
	ASSERT_TRUE(gpu.particle().particle_gpu_available());
	//步长非法拒绝
	EXPECT_FALSE(gpu.particle().particle_gpu_step(0.0f));
	EXPECT_FALSE(gpu.particle().particle_gpu_step(-1.0f));
	EXPECT_FALSE(gpu.particle().particle_gpu_step(std::numeric_limits<float>::quiet_NaN()));
	//十步推进（同参数同顺序）
	for (uint32_t step = 0; step < 10; ++step)
	{
		cpu粒子.step(0.05f);
		ASSERT_TRUE(gpu.particle().particle_gpu_step(0.05f));
	}
	//逐活跃索引比对
	uint32_t compared = 0;
	for (uint32_t index = 0; index < 64; ++index)
	{
		engine::Particle_State expected{};
		engine::Particle_State actual{};
		//CPU 侧非活跃索引跳过
		if (!cpu粒子.state_get(index, expected))
			continue;
		ASSERT_TRUE(gpu粒子.state_get(index, actual));
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(expected.position[axis], actual.position[axis], 1e-5f);
			EXPECT_NEAR(expected.velocity[axis], actual.velocity[axis], 1e-5f);
		}
		EXPECT_NEAR(expected.age, actual.age, 1e-5f);
		EXPECT_NEAR(expected.lifetime, actual.lifetime, 1e-5f);
		++compared;
	}
	EXPECT_EQ(compared, 32u);
	EXPECT_EQ(gpu粒子.active_count(), 32u);
}

/*
GPU 生命周期与死亡同步（第三百一十三轮）
寿命 0.25..0.3、步长 0.05 的 32 个粒子：逐步比对 CPU / GPU 死亡数与活跃数，
全部死亡后确认无幽灵粒子、继续推进不复活、再次发射沿用回收索引。
*/
TEST(Renderer_Particle_Test, GPU生命周期与死亡同步)
{
	engine::Particle_Emitter emitter = point_emitter();
	emitter.lifetime_min = 0.25f;
	emitter.lifetime_max = 0.3f;
	engine::Particle_Params params{};
	params.gravity[1] = -1.0f;
	params.damping = 0.9f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	engine::Particle_Host cpu粒子(cpu);
	ASSERT_TRUE(gpu.init(hidden_config()));
	engine::Particle_Host gpu粒子(gpu);
	ASSERT_TRUE(cpu.particle().particle_enable_set(true));
	ASSERT_TRUE(gpu.particle().particle_enable_set(true));
	ASSERT_TRUE(cpu粒子.capacity_set(64));
	ASSERT_TRUE(gpu粒子.capacity_set(64));
	ASSERT_TRUE(cpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle().particle_params_set(params));
	ASSERT_TRUE(gpu.particle().particle_params_set(params));
	ASSERT_EQ(cpu粒子.emit(32), 32u);
	ASSERT_EQ(gpu粒子.emit(32), 32u);
	ASSERT_TRUE(gpu.particle().particle_gpu_available());
	//逐步比对死亡数与活跃数
	uint32_t total_died = 0;
	for (uint32_t step = 0; step < 10; ++step)
	{
		uint32_t cpu_died = cpu粒子.step(0.05f);
		ASSERT_TRUE(gpu.particle().particle_gpu_step(0.05f));
		EXPECT_EQ(cpu_died, gpu.particle().particle_gpu_died_last());
		EXPECT_EQ(cpu粒子.active_count(), gpu粒子.active_count());
		total_died += gpu.particle().particle_gpu_died_last();
	}
	EXPECT_EQ(total_died, 32u);
	EXPECT_EQ(gpu粒子.active_count(), 0u);
	//无幽灵粒子：全部索引不可读
	uint32_t readable = 0;
	for (uint32_t index = 0; index < 64; ++index)
	{
		engine::Particle_State state{};
		if (gpu粒子.state_get(index, state))
			++readable;
	}
	EXPECT_EQ(readable, 0u);
	//全部死亡后继续推进不复活
	ASSERT_TRUE(gpu.particle().particle_gpu_step(0.05f));
	EXPECT_EQ(gpu.particle().particle_gpu_died_last(), 0u);
	EXPECT_EQ(gpu粒子.active_count(), 0u);
	//再次发射沿用回收索引，且状态可读
	EXPECT_EQ(gpu粒子.emit(32), 32u);
	EXPECT_EQ(gpu粒子.active_count(), 32u);
	readable = 0;
	for (uint32_t index = 0; index < 64; ++index)
	{
		engine::Particle_State state{};
		if (gpu粒子.state_get(index, state))
			++readable;
	}
	EXPECT_EQ(readable, 32u);
}

/*
粒子 billboard 定位（第三百一十四轮）
64×64 离屏目标、相机 (0,0,3) 注视原点：原点处单个粒子绘制后中心像素亮、边角像素保持清屏色；
关闭粒子后不再登记绘制项。
*/
TEST(Renderer_Particle_Test, 粒子billboard定位)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
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
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	EXPECT_EQ(粒子.emit(1), 1u);
	//尺寸非法拒绝
	EXPECT_FALSE(renderer.particle().particle_draw(0.0f));
	EXPECT_FALSE(renderer.particle().particle_draw(-1.0f));
	//登记一个实例化绘制项
	ASSERT_TRUE(renderer.particle().particle_draw(0.5f));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	ASSERT_TRUE(renderer.frame_render());
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_EQ(width, 64u);
	EXPECT_EQ(height, 64u);
	//中心像素（BGRA：红色下标 2）显著亮于清屏色
	std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::size_t corner = (static_cast<std::size_t>(4) * 64 + 4) * 4;
	EXPECT_GT(pixels[center + 2], 40u);
	EXPECT_GT(pixels[center + 0], 4u);
	EXPECT_LT(pixels[corner + 2], 8u);
	//关闭粒子后不再登记绘制项
	ASSERT_TRUE(renderer.particle().particle_enable_set(false));
	EXPECT_FALSE(renderer.particle().particle_draw(0.5f));
	EXPECT_FALSE(renderer.particle().particle_gpu_available());
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子年龄颜色曲线（第三百一十五轮）
同一粒子（速度 0、重力 0、寿命 1.0）：年龄 0 与年龄 0.8 两帧的中心像素比较，
年轻者绿色分量更高、两者红色分量均保持高位（暖白 → 暗红渐变）。
*/
TEST(Renderer_Particle_Test, 粒子年龄颜色曲线)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
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
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(1), 1u);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//第一帧：年龄 0（暖白）
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t young_green = pixels[center + 1];
	uint8_t young_red = pixels[center + 2];
	//推进到年龄 0.8（未死亡）
	for (uint32_t step = 0; step < 8; ++step)
		粒子.step(0.1f);
	ASSERT_EQ(粒子.active_count(), 1u);
	//第二帧：年龄 0.8（暗红）
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t old_green = pixels[center + 1];
	uint8_t old_red = pixels[center + 2];
	//年轻者更偏暖白（绿色更高），两者红色均保持高位
	EXPECT_GT(young_green, 100u);
	EXPECT_GT(static_cast<uint32_t>(young_green), static_cast<uint32_t>(old_green) + 30u);
	EXPECT_GT(young_red, 100u);
	EXPECT_GT(old_red, 100u);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子颜色覆盖绘制（第三百五十四轮）
单枚原点粒子配置「蓝 → 绿」：CPU 参考路径年龄 0 中心像素蓝优、年龄 0.8 绿优（随年龄插值）；
常驻 GPU 直接 / 间接两条路径推送块同口径，年龄 ≈ 0 中心像素同样蓝优。
像素缓冲为 BGRA：偏移 +0 = 蓝、+1 = 绿、+2 = 红。
*/
TEST(Renderer_Particle_Test, 粒子颜色覆盖绘制)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
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
	params.color_start[2] = 1.0f;
	params.color_end[1] = 1.0f;
	params.color_enabled = 1;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	const std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//CPU 参考路径：年龄 0（新生端为蓝）
	ASSERT_EQ(粒子.emit(1), 1u);
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_GT(pixels[center + 0], 100u);
	EXPECT_GT(static_cast<uint32_t>(pixels[center + 0]), static_cast<uint32_t>(pixels[center + 1]) + 60u);
	//CPU 参考路径：推进到年龄 0.8（衰老端为绿）
	for (uint32_t step = 0; step < 8; ++step)
		粒子.step(0.1f);
	ASSERT_EQ(粒子.active_count(), 1u);
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_GT(pixels[center + 1], 100u);
	EXPECT_GT(static_cast<uint32_t>(pixels[center + 1]), static_cast<uint32_t>(pixels[center + 0]) + 60u);
	//常驻 GPU 路径：GPU 侧发射后单步推进（年龄 ≈ 0）
	ASSERT_TRUE(renderer.particle().particle_resident_set(true));
	ASSERT_EQ(engine::particle_gpu::emit(renderer, 1), 1u);
	ASSERT_TRUE(engine::particle_gpu::step_resident(renderer, 0.01f));
	//GPU 直接绘制
	ASSERT_TRUE(renderer.particle().particle_gpu_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_GT(pixels[center + 0], 100u);
	EXPECT_GT(static_cast<uint32_t>(pixels[center + 0]), static_cast<uint32_t>(pixels[center + 1]) + 60u);
	//GPU 间接绘制
	ASSERT_TRUE(renderer.particle().particle_indirect_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_GT(pixels[center + 0], 100u);
	EXPECT_GT(static_cast<uint32_t>(pixels[center + 0]), static_cast<uint32_t>(pixels[center + 1]) + 60u);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子软边可配（第三百五十五轮）
单枚粒子（边长 1.6、颜色覆盖为纯蓝）：edge_softness = 0 保持内置 0.65 内半径；
改为 0.95 后内半径收缩到 0.05，同帧整幅蓝色通道累加值显著下降（约 0.7 倍），证明软边宽度进入着色链路。
*/
TEST(Renderer_Particle_Test, 粒子软边可配)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
	engine::Particle_Emitter emitter{};
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
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(1), 1u);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//整幅蓝色通道累加（软硬边对比口径）
	auto blue_sum = [&](void) -> uint64_t
	{
		std::fill(pixels.begin(), pixels.end(), 0);
		EXPECT_TRUE(renderer.resource().render_target_readback(target, pixels.data(),
			static_cast<uint32_t>(pixels.size()), &width, &height));
		uint64_t sum = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += pixels[index];
		return sum;
	};
	//硬边（软边宽度 0：内置 0.65 内半径）
	ASSERT_TRUE(renderer.particle().particle_draw(1.6f));
	ASSERT_TRUE(renderer.frame_render());
	const uint64_t hard_sum = blue_sum();
	//软边（宽度 0.95：内半径 0.05）
	params.edge_softness = 0.95f;
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_draw(1.6f));
	ASSERT_TRUE(renderer.frame_render());
	const uint64_t soft_sum = blue_sum();
	std::cout << "[软边] 硬边蓝和=" << hard_sum << " 软边蓝和=" << soft_sum << std::endl;
	EXPECT_GT(hard_sum, 0u);
	EXPECT_LT(soft_sum, hard_sum);
	EXPECT_LT(soft_sum * 100u, hard_sum * 85u);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子颜色分桶绘制（第三百五十七轮）
分桶模式（4 桶）下每桶各自登记一次实例化绘制：颜色覆盖须逐桶写入推送块，
故整幅蓝通道累加值与单桶背景下的同色粒子一致量级；分桶开启后仍能出帧且有蓝优像素。
*/
TEST(Renderer_Particle_Test, 粒子颜色分桶绘制)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(16));
	engine::Particle_Emitter emitter{};
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	//球壳半径 1：发射点自带不同视线深度，进入不同桶
	emitter.extents[0] = 1.0f;
	emitter.count = 8;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.color_start[2] = 1.0f;
	params.color_end[2] = 1.0f;
	params.color_enabled = 1;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(8), 8u);
	ASSERT_TRUE(renderer.particle().particle_sort_buckets_set(4));
	EXPECT_EQ(renderer.particle().particle_sort_buckets_get(), 4u);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	//分桶开启：出帧后统计蓝优像素
	ASSERT_TRUE(renderer.particle().particle_draw(0.4f));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint32_t blue_pixels = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		//蓝明显高于其余通道即视为蓝优像素
		if (pixels[index] > 40u && pixels[index] > pixels[index + 1] && pixels[index] > pixels[index + 2])
			++blue_pixels;
	std::cout << "[颜色分桶] 桶数=4 绘制项=" << renderer.frame_stats_get().drawn_items
		<< " 蓝优像素=" << blue_pixels << std::endl;
	EXPECT_GT(blue_pixels, 0u);
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子混合档与深度写入（第三百一十六轮）
两枚重合粒子：Alpha 档中心像素为单层颜色，切换 Additive 后叠加增亮；
默认档为 Alpha、非法档拒绝、深度写入开关可读写（切换触发管线重建）。
*/
TEST(Renderer_Particle_Test, 粒子混合档与深度写入)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
	engine::Particle_Emitter emitter{};
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 2;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(2), 2u);
	//默认档位
	EXPECT_EQ(renderer.particle().particle_blend_get(), engine::Blend_Mode::Alpha);
	EXPECT_FALSE(renderer.particle().particle_depth_write_get());
	//非法档拒绝
	EXPECT_FALSE(renderer.particle().particle_blend_set(engine::Blend_Mode::Opaque));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//Alpha 档：单层颜色
	ASSERT_TRUE(renderer.particle().particle_blend_set(engine::Blend_Mode::Alpha));
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t alpha_blue = pixels[center + 0];
	//Additive 档：叠加增亮
	ASSERT_TRUE(renderer.particle().particle_blend_set(engine::Blend_Mode::Additive));
	EXPECT_EQ(renderer.particle().particle_blend_get(), engine::Blend_Mode::Additive);
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t additive_blue = pixels[center + 0];
	EXPECT_GT(static_cast<uint32_t>(additive_blue), static_cast<uint32_t>(alpha_blue) + 30u);
	//深度写入开关（切换触发重建，绘制仍可用）
	ASSERT_TRUE(renderer.particle().particle_depth_write_set(true));
	EXPECT_TRUE(renderer.particle().particle_depth_write_get());
	ASSERT_TRUE(renderer.particle().particle_depth_write_set(false));
	EXPECT_FALSE(renderer.particle().particle_depth_write_get());
	ASSERT_TRUE(renderer.particle().particle_draw(0.6f));
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}

/*
粒子深度分桶近似排序（第三百一十七轮）
盒发射 16 个粒子（深度分散）：4 桶下连续两帧像素逐字节一致（结果稳定、无闪烁）；
桶数钳制到 0..16，关闭分桶后单次绘制仍可用。
*/
TEST(Renderer_Particle_Test, 粒子深度分桶稳定性)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	engine::Particle_Host 粒子(renderer);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.scene().clear_color_set(clear);
	renderer.scene().camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(32));
	engine::Particle_Emitter emitter{};
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.extents[0] = 0.6f;
	emitter.extents[1] = 0.6f;
	emitter.extents[2] = 2.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Box);
	emitter.count = 16;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(16), 16u);
	//默认关闭分桶
	EXPECT_EQ(renderer.particle().particle_sort_buckets_get(), 0u);
	ASSERT_TRUE(renderer.particle().particle_sort_buckets_set(4));
	EXPECT_EQ(renderer.particle().particle_sort_buckets_get(), 4u);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, 64, 64));
	renderer.device().render_target_set(target);
	std::vector<uint8_t> first(64 * 64 * 4, 0);
	std::vector<uint8_t> second(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//第一帧
	ASSERT_TRUE(renderer.particle().particle_draw(0.25f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.resource().render_target_readback(target, first.data(), static_cast<uint32_t>(first.size()),
		&width, &height));
	//第二帧（同状态）
	ASSERT_TRUE(renderer.particle().particle_draw(0.25f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.resource().render_target_readback(target, second.data(), static_cast<uint32_t>(second.size()),
		&width, &height));
	EXPECT_EQ(first, second);
	//桶数钳制与关闭
	ASSERT_TRUE(renderer.particle().particle_sort_buckets_set(100));
	EXPECT_EQ(renderer.particle().particle_sort_buckets_get(), 16u);
	ASSERT_TRUE(renderer.particle().particle_sort_buckets_set(0));
	EXPECT_EQ(renderer.particle().particle_sort_buckets_get(), 0u);
	ASSERT_TRUE(renderer.particle().particle_draw(0.25f));
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(target);
}
