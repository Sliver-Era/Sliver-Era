/*
渲染粒子力场与碰撞测试（2026-11-21 自「渲染粒子测试.cpp」拆出）
覆盖：力场校验与解析、力场 GPU 对照、地面 / 球体碰撞、碰撞 GPU 对照。
*/

#include "渲染粒子测试辅助.h"

/*
力场（第三百一十八轮）
校验：力场数与类型 / 方向非法拒绝；风场解析解（vx = 强度 × 时间、位移 = ½ 加速度 × 时间²）；
GPU 对照：风场 ＋ 引力点 ＋ 涡流十步逐粒子 1e-5。
*/
TEST(Renderer_Particle_Test, 力场校验与解析)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
	engine::Particle_Emitter emitter = point_emitter();
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.direction[0] = 1.0f;
	emitter.direction[1] = 0.0f;
	emitter.direction[2] = 0.0f;
	emitter.lifetime_min = 5.0f;
	emitter.lifetime_max = 5.0f;
	emitter.count = 1;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	//力场数越界
	params.force_count = 9;
	EXPECT_FALSE(renderer.particle().particle_params_set(params));
	//涡流方向为零
	params.force_count = 1;
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Vortex);
	EXPECT_FALSE(renderer.particle().particle_params_set(params));
	//风场（+X、强度 2）
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Wind);
	params.forces[0].strength = 2.0f;
	params.forces[0].direction[0] = 1.0f;
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_EQ(粒子.emit(1), 1u);
	//十步 × 0.1 s：vx = 2.0、px = 0.5 × 2.0 × 1.0 = 1.0（离散积分与解析差 ≤ 1e-3）
	for (uint32_t step = 0; step < 10; ++step)
		粒子.step(0.1f);
	engine::Particle_State state{};
	uint32_t readable = 0;
	for (uint32_t index = 0; index < 8; ++index)
		if (粒子.state_get(index, state))
			++readable;
	ASSERT_EQ(readable, 1u);
	EXPECT_NEAR(state.velocity[0], 2.0f, 1e-4f);
	EXPECT_NEAR(state.position[0], 1.1f, 1e-4f);
	EXPECT_NEAR(state.velocity[1], 0.0f, 1e-5f);
	EXPECT_NEAR(state.velocity[2], 0.0f, 1e-5f);
}

/*
力场 GPU 对照（第三百一十八轮）
风场 ＋ 引力点 ＋ 涡流三场同时生效：CPU 参考与 GPU 推进十步，逐活跃粒子 1e-5。
*/
TEST(Renderer_Particle_Test, 力场GPU对照)
{
	engine::Particle_Params params{};
	params.damping = 0.95f;
	params.force_count = 3;
	//风场：+X 方向强度 1
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Wind);
	params.forces[0].strength = 1.0f;
	params.forces[0].direction[0] = 1.0f;
	//引力点：原点、半径 2、强度 1
	params.forces[1].type = static_cast<uint32_t>(engine::Force_Type::Attractor);
	params.forces[1].radius = 2.0f;
	params.forces[1].strength = 1.0f;
	//涡流：绕 +Y 轴、半径 2、强度 1
	params.forces[2].type = static_cast<uint32_t>(engine::Force_Type::Vortex);
	params.forces[2].radius = 2.0f;
	params.forces[2].strength = 1.0f;
	params.forces[2].direction[1] = 1.0f;
	engine::Particle_Emitter emitter = point_emitter();
	emitter.position[0] = 1.0f;
	emitter.position[1] = 0.0f;
	emitter.position[2] = 0.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 5.0f;
	emitter.lifetime_max = 5.0f;
	emitter.count = 4;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	engine::Particle_Host cpu粒子(cpu);
	ASSERT_TRUE(gpu.init(hidden_config()));
	engine::Particle_Host gpu粒子(gpu);
	ASSERT_TRUE(cpu.particle().particle_enable_set(true));
	ASSERT_TRUE(gpu.particle().particle_enable_set(true));
	ASSERT_TRUE(cpu粒子.capacity_set(8));
	ASSERT_TRUE(gpu粒子.capacity_set(8));
	ASSERT_TRUE(cpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle().particle_params_set(params));
	ASSERT_TRUE(gpu.particle().particle_params_set(params));
	ASSERT_EQ(cpu粒子.emit(4), 4u);
	ASSERT_EQ(gpu粒子.emit(4), 4u);
	ASSERT_TRUE(gpu.particle().particle_gpu_available());
	//十步推进
	for (uint32_t step = 0; step < 10; ++step)
	{
		cpu粒子.step(0.05f);
		ASSERT_TRUE(gpu.particle().particle_gpu_step(0.05f));
	}
	//逐活跃索引比对
	uint32_t compared = 0;
	for (uint32_t index = 0; index < 8; ++index)
	{
		engine::Particle_State expected{};
		engine::Particle_State actual{};
		if (!cpu粒子.state_get(index, expected))
			continue;
		ASSERT_TRUE(gpu粒子.state_get(index, actual));
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(expected.position[axis], actual.position[axis], 1e-5f);
			EXPECT_NEAR(expected.velocity[axis], actual.velocity[axis], 1e-5f);
		}
		++compared;
	}
	EXPECT_EQ(compared, 4u);
}

/*
碰撞：地面平面（第三百一十九轮）
重力下落 30 步：粒子始终不穿透地面（最低 y ≥ 地面 − 1e-4），且出现反弹（法向速度由负转正）。
*/
TEST(Renderer_Particle_Test, 碰撞地面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
	engine::Particle_Emitter emitter{};
	emitter.position[1] = 1.0f;
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 1;
	engine::Particle_Params params{};
	params.gravity[1] = -2.0f;
	params.damping = 1.0f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	params.ground_enabled = 1;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(1), 1u);
	//逐步推进并记录最低点与反弹次数
	float lowest = 1e9f;
	uint32_t bounces = 0;
	float previous_velocity_y = 0.0f;
	for (uint32_t step = 0; step < 30; ++step)
	{
		粒子.step(0.1f);
		engine::Particle_State state{};
		uint32_t readable = 0;
		for (uint32_t index = 0; index < 8; ++index)
			if (粒子.state_get(index, state))
				++readable;
		ASSERT_EQ(readable, 1u);
		if (state.position[1] < lowest)
			lowest = state.position[1];
		//法向速度由负转正即记为一次反弹
		if (previous_velocity_y < 0.0f && state.velocity[1] > 0.0f)
			++bounces;
		previous_velocity_y = state.velocity[1];
	}
	EXPECT_GE(lowest, -1e-4f);
	EXPECT_GT(bounces, 0u);
}

/*
碰撞：球体（第三百一十九轮）
风场把粒子沿 −Z 加速撞向原点半径 0.5 的球（速度严格沿轴，避免发射扰动导致掠过球面）：
全程不穿透球面，撞后法向速度反向。
*/
TEST(Renderer_Particle_Test, 碰撞球体)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Host 粒子(renderer);
	ASSERT_TRUE(renderer.particle().particle_enable_set(true));
	ASSERT_TRUE(粒子.capacity_set(8));
	engine::Particle_Emitter emitter{};
	emitter.position[2] = 1.0f;
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 1;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	params.sphere_radius = 0.5f;
	//风场：−Z 方向强度 20（每步 0.1 s 增加 2 m/s）
	params.force_count = 1;
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Wind);
	params.forces[0].strength = 20.0f;
	params.forces[0].direction[2] = -1.0f;
	ASSERT_TRUE(renderer.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle().particle_params_set(params));
	ASSERT_EQ(粒子.emit(1), 1u);
	//逐步推进并记录最小球心距与撞后法向速度
	float minimum_distance = 1e9f;
	bool bounced = false;
	for (uint32_t step = 0; step < 20; ++step)
	{
		粒子.step(0.1f);
		engine::Particle_State state{};
		uint32_t readable = 0;
		for (uint32_t index = 0; index < 8; ++index)
			if (粒子.state_get(index, state))
				++readable;
		ASSERT_EQ(readable, 1u);
		float distance = std::sqrt(state.position[0] * state.position[0]
			+ state.position[1] * state.position[1] + state.position[2] * state.position[2]);
		if (distance < minimum_distance)
			minimum_distance = distance;
		//撞球后 z 方向速度反转为正
		if (state.velocity[2] > 0.0f)
			bounced = true;
	}
	EXPECT_GE(minimum_distance, 0.5f - 1e-4f);
	EXPECT_TRUE(bounced);
}

/*
碰撞 GPU 对照（第三百一十九轮）
地面 ＋ 球体 ＋ 重力同时生效：CPU 参考与 GPU 推进二十步逐粒子位置 / 速度 1e-5。
*/
TEST(Renderer_Particle_Test, 碰撞GPU对照)
{
	engine::Particle_Params params{};
	params.gravity[1] = -2.0f;
	params.damping = 0.98f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	params.sphere_radius = 0.4f;
	params.ground_enabled = 1;
	engine::Particle_Emitter emitter = point_emitter();
	emitter.position[1] = 1.2f;
	emitter.extents[0] = 0.5f;
	emitter.extents[1] = 0.3f;
	emitter.extents[2] = 0.5f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.count = 4;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	engine::Particle_Host cpu粒子(cpu);
	ASSERT_TRUE(gpu.init(hidden_config()));
	engine::Particle_Host gpu粒子(gpu);
	ASSERT_TRUE(cpu.particle().particle_enable_set(true));
	ASSERT_TRUE(gpu.particle().particle_enable_set(true));
	ASSERT_TRUE(cpu粒子.capacity_set(8));
	ASSERT_TRUE(gpu粒子.capacity_set(8));
	ASSERT_TRUE(cpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle().particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle().particle_params_set(params));
	ASSERT_TRUE(gpu.particle().particle_params_set(params));
	ASSERT_EQ(cpu粒子.emit(4), 4u);
	ASSERT_EQ(gpu粒子.emit(4), 4u);
	ASSERT_TRUE(gpu.particle().particle_gpu_available());
	for (uint32_t step = 0; step < 20; ++step)
	{
		cpu粒子.step(0.05f);
		ASSERT_TRUE(gpu.particle().particle_gpu_step(0.05f));
	}
	uint32_t compared = 0;
	for (uint32_t index = 0; index < 8; ++index)
	{
		engine::Particle_State expected{};
		engine::Particle_State actual{};
		if (!cpu粒子.state_get(index, expected))
			continue;
		ASSERT_TRUE(gpu粒子.state_get(index, actual));
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(expected.position[axis], actual.position[axis], 1e-5f);
			EXPECT_NEAR(expected.velocity[axis], actual.velocity[axis], 1e-5f);
		}
		++compared;
	}
	EXPECT_EQ(compared, 4u);
}
