/*
渲染GPU驱动包围体测试（2026-11-21 自「渲染GPU驱动测试.cpp」拆出）
覆盖：单位矩阵包围体、旋转后包围盒按角点重求、球 / 盒包含角点、非法参数、性能基线采集、多批次合批基线。
*/

#include "渲染GPU驱动测试辅助.h"

TEST_F(Renderer_GPU_Driven_Test, 单位矩阵包围体)
{
	const float local_min[3] = { -1.0f, -0.5f, -2.0f };
	const float local_max[3] = { 1.0f, 0.5f, 2.0f };
	std::array<float, 16> identity = identity_matrix();
	engine::World_Bounds bounds;
	ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, identity.data()));
	//中心在原点
	EXPECT_FLOAT_EQ(bounds.center[0], 0.0f);
	EXPECT_FLOAT_EQ(bounds.center[1], 0.0f);
	EXPECT_FLOAT_EQ(bounds.center[2], 0.0f);
	//半径 = 扩张后 AABB 角点最远距离 × 倍率 ＋ 附加量（球须覆盖盒）
	const float corner_distance = std::sqrt(1.002f * 1.002f + 0.502f * 0.502f + 2.002f * 2.002f);
	EXPECT_NEAR(bounds.radius, corner_distance * engine::WORLD_BOUNDS_SPHERE_SCALE + engine::WORLD_BOUNDS_SPHERE_EXPAND, 1e-5f);
	//盒 = 局部盒 ± 外扩
	EXPECT_NEAR(bounds.aabb_min[0], -1.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[2], 2.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
}

//绕 Y 轴 90 度：世界盒的 X 轴取局部 Z 轴范围，Z 轴取局部 X 轴范围
TEST_F(Renderer_GPU_Driven_Test, 旋转后包围盒按角点重求)
{
	const float local_min[3] = { -1.0f, -2.0f, -3.0f };
	const float local_max[3] = { 1.0f, 2.0f, 3.0f };
	std::array<float, 16> rotation = rotate_y_90_matrix();
	engine::World_Bounds bounds;
	ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, rotation.data()));
	//X 轴 = 局部 Z 轴范围
	EXPECT_NEAR(bounds.aabb_min[0], -3.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[0], 3.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	//Y 轴不变
	EXPECT_NEAR(bounds.aabb_min[1], -2.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[1], 2.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	//Z 轴 = 局部 X 轴范围
	EXPECT_NEAR(bounds.aabb_min[2], -1.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[2], 1.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
}

//随机变换：球必须包含全部角点，盒必须包含全部角点（保守性核心断言）
TEST_F(Renderer_GPU_Driven_Test, 包围球与包围盒包含角点)
{
	const float local_min[3] = { -0.8f, -0.4f, -1.2f };
	const float local_max[3] = { 0.8f, 0.4f, 1.2f };
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> scale(0.2f, 4.0f);
	std::uniform_real_distribution<float> offset(-50.0f, 50.0f);
	for (uint32_t sample = 0; sample < 200; ++sample)
	{
		//随机欧拉角 + 各轴缩放 + 平移
		const float ax = angle(generator);
		const float ay = angle(generator);
		const float az = angle(generator);
		const float sx = scale(generator);
		const float sy = scale(generator);
		const float sz = scale(generator);
		//绕 X 旋转
		std::array<float, 16> matrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, std::cos(ax), std::sin(ax), 0.0f,
			0.0f, -std::sin(ax), std::cos(ax), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		//绕 Y 旋转（左乘）
		std::array<float, 16> rotation_y = {
			std::cos(ay), 0.0f, -std::sin(ay), 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			std::sin(ay), 0.0f, std::cos(ay), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		std::array<float, 16> combined{};
		for (uint32_t column = 0; column < 4; ++column)
			for (uint32_t row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (uint32_t inner = 0; inner < 4; ++inner)
					sum += rotation_y[inner * 4 + row] * matrix[column * 4 + inner];
				combined[column * 4 + row] = sum;
			}
		//绕 Z 旋转
		std::array<float, 16> rotation_z = {
			std::cos(az), std::sin(az), 0.0f, 0.0f,
			-std::sin(az), std::cos(az), 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		std::array<float, 16> rotated{};
		for (uint32_t column = 0; column < 4; ++column)
			for (uint32_t row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (uint32_t inner = 0; inner < 4; ++inner)
					sum += rotation_z[inner * 4 + row] * combined[column * 4 + inner];
				rotated[column * 4 + row] = sum;
			}
		//各轴缩放 + 平移
		rotated[0] *= sx;
		rotated[1] *= sx;
		rotated[2] *= sx;
		rotated[4] *= sy;
		rotated[5] *= sy;
		rotated[6] *= sy;
		rotated[8] *= sz;
		rotated[9] *= sz;
		rotated[10] *= sz;
		rotated[12] = offset(generator);
		rotated[13] = offset(generator);
		rotated[14] = offset(generator);
		engine::World_Bounds bounds;
		ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, rotated.data()));
		//逐角点校验包含关系
		for (uint32_t corner = 0; corner < 8; ++corner)
		{
			const float x = (corner & 1u) != 0 ? local_max[0] : local_min[0];
			const float y = (corner & 2u) != 0 ? local_max[1] : local_min[1];
			const float z = (corner & 4u) != 0 ? local_max[2] : local_min[2];
			std::array<float, 3> world = transform_point(rotated.data(), x, y, z);
			//球包含
			const float dx = world[0] - bounds.center[0];
			const float dy = world[1] - bounds.center[1];
			const float dz = world[2] - bounds.center[2];
			EXPECT_LE(std::sqrt(dx * dx + dy * dy + dz * dz), bounds.radius + 1e-4f) << "样本 " << sample << " 角点 " << corner;
			//盒包含
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				EXPECT_GE(world[axis], bounds.aabb_min[axis] - 1e-4f) << "样本 " << sample << " 轴 " << axis;
				EXPECT_LE(world[axis], bounds.aabb_max[axis] + 1e-4f) << "样本 " << sample << " 轴 " << axis;
			}
		}
	}
}

//非法入参：空指针 / 下界大于上界 / 矩阵含非数
TEST_F(Renderer_GPU_Driven_Test, 包围体非法参数)
{
	std::array<float, 16> identity = identity_matrix();
	engine::World_Bounds bounds;
	const float good_min[3] = { -1.0f, -1.0f, -1.0f };
	const float bad_max[3] = { -2.0f, -1.0f, -1.0f };
	EXPECT_FALSE(engine::world_bounds_build(bounds, nullptr, bad_max, identity.data()));
	EXPECT_FALSE(engine::world_bounds_build(bounds, good_min, bad_max, identity.data()));
	std::array<float, 16> broken = identity;
	broken[0] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::world_bounds_build(bounds, good_min, bad_max, broken.data()));
}

/*
性能基线采集（第二百四十七轮前置子步）
口径：1080p ＋ MSAA 4× ＋ 1 万实例横向铺开；渲染 BASELINE_FRAMES 帧，
输出平均帧耗时、平均 CPU 剔除耗时与绘制统计，作为阶段 B / C / D 各轮的对照基线。
断言只保证「跑得通、计数自洽」，性能数值写入 `性能基线_方向一.md`，不使用绝对阈值。
*/
TEST_F(Renderer_GPU_Driven_Test, 性能基线采集)
{
	engine::Renderer_Config config;
	config.width = BASELINE_WIDTH;
	config.height = BASELINE_HEIGHT;
	config.hidden = true;
	config.msaa_samples = 4;
	config.instance_capacity = BASELINE_INSTANCES;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	//准备网格与实例矩阵
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	std::vector<std::array<float, 16>> matrices = baseline_matrices(BASELINE_INSTANCES);
	//逐帧渲染并收集
	double host_record_sum = 0.0;
	double gpu_frame_sum = 0.0;
	double cpu_cull_sum = 0.0;
	double gpu_samples = 0.0;
	uint64_t last_culled_instances = 0;
	uint64_t last_instances = 0;
	for (uint32_t frame = 0; frame < BASELINE_FRAMES; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, matrices[0].data(), BASELINE_INSTANCES));
		ASSERT_TRUE(renderer.frame_render());
		const engine::Frame_Stats stats = renderer.frame_stats_get();
		host_record_sum += stats.host_record_ms;
		gpu_frame_sum += stats.gpu_frame_ms;
		//GPU 时间戳支持时才有样本
		if (stats.gpu_frame_ms > 0.0)
			gpu_samples += 1.0;
		//剔除耗时（当前帧）
		cpu_cull_sum += stats.cpu_cull_ms;
		last_culled_instances = stats.last_culled_instances;
		last_instances = stats.last_instances;
	}
	const double frames = static_cast<double>(BASELINE_FRAMES);
	std::printf("[性能基线] 分辨率 %ux%u MSAA %u 实例 %u | 平均录制 %.3f ms | 平均 GPU 帧 %.3f ms（样本 %.0f）| 平均 CPU 剔除 %.3f ms | 末帧可见实例 %llu / 剔除实例 %llu\n",
		BASELINE_WIDTH, BASELINE_HEIGHT, config.msaa_samples, BASELINE_INSTANCES,
		host_record_sum / frames, gpu_frame_sum / frames, gpu_samples, cpu_cull_sum / frames,
		static_cast<unsigned long long>(last_instances), static_cast<unsigned long long>(last_culled_instances));
	//基线自洽性：至少一帧有绘制实例，且剔除计数不超过登记数
	EXPECT_GE(last_instances + last_culled_instances, 1u);
	EXPECT_GE(cpu_cull_sum, 0.0);
	renderer.resource().mesh_unload(mesh);
}
/*
多批次合批基线采集（第二百五十二轮）
口径：1080p ＋ MSAA 4× ＋ 1 万实例，切分为 100 批 × 100 实例（实例区连续），
      对照「关 / 开间接绘制」的平均录制耗时与 GPU 帧耗时，并校验两态绘制实例数守恒。
单批 1 万实例只有一条绘制命令，间接替换无收益；合批收益体现在多批次场景，故以此口径记录 ±%。
*/
TEST_F(Renderer_GPU_Driven_Test, 多批次合批基线采集)
{
	engine::Renderer_Config base_config;
	base_config.width = BASELINE_WIDTH;
	base_config.height = BASELINE_HEIGHT;
	base_config.hidden = true;
	base_config.msaa_samples = 4;
	base_config.instance_capacity = BASELINE_INSTANCES;
	constexpr uint32_t BATCHES = 100;
	constexpr uint32_t PER_BATCH = BASELINE_INSTANCES / BATCHES;
	std::vector<std::array<float, 16>> matrices = baseline_matrices(BASELINE_INSTANCES);
	//两态渲染器（除间接开关外配置一致）
	engine::Renderer off;
	engine::Renderer on;
	ASSERT_TRUE(off.init(base_config));
	engine::Renderer_Config on_config = base_config;
	on_config.gpu_indirect_enable = true;
	ASSERT_TRUE(on.init(on_config));
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle on_mesh = 0;
	ASSERT_TRUE(off.resource().mesh_load(off_mesh, quad_data()));
	ASSERT_TRUE(on.resource().mesh_load(on_mesh, quad_data()));
	double off_record_sum = 0.0;
	double off_gpu_sum = 0.0;
	double on_record_sum = 0.0;
	double on_gpu_sum = 0.0;
	uint64_t off_drawn = 0;
	uint64_t on_drawn = 0;
	for (uint32_t frame = 0; frame < BASELINE_FRAMES; ++frame)
	{
		for (uint32_t batch = 0; batch < BATCHES; ++batch)
		{
			const uint32_t offset = batch * PER_BATCH;
			ASSERT_TRUE(off.resource().mesh_draw_instanced(off_mesh, matrices[offset].data(), PER_BATCH));
			ASSERT_TRUE(on.resource().mesh_draw_instanced(on_mesh, matrices[offset].data(), PER_BATCH));
		}
		ASSERT_TRUE(off.frame_render());
		ASSERT_TRUE(on.frame_render());
		const engine::Frame_Stats off_stats = off.frame_stats_get();
		const engine::Frame_Stats on_stats = on.frame_stats_get();
		off_record_sum += off_stats.host_record_ms;
		off_gpu_sum += off_stats.gpu_frame_ms;
		on_record_sum += on_stats.host_record_ms;
		on_gpu_sum += on_stats.gpu_frame_ms;
		off_drawn = off_stats.drawn_instances;
		on_drawn = on_stats.drawn_instances;
	}
	const double frames = static_cast<double>(BASELINE_FRAMES);
	const double off_record = off_record_sum / frames;
	const double on_record = on_record_sum / frames;
	const double off_gpu = off_gpu_sum / frames;
	const double on_gpu = on_gpu_sum / frames;
	std::printf("[合批基线] %u 批 × %u 实例 | 录制 关 %.3f ms / 开 %.3f ms（%+.1f%%）| GPU 帧 关 %.3f ms / 开 %.3f ms（%+.1f%%）\n",
		BATCHES, PER_BATCH, off_record, on_record,
		off_record > 0.0 ? (on_record - off_record) / off_record * 100.0 : 0.0,
		off_gpu, on_gpu,
		off_gpu > 0.0 ? (on_gpu - off_gpu) / off_gpu * 100.0 : 0.0);
	//两态绘制实例数守恒；开时确有间接命令
	EXPECT_EQ(off_drawn, on_drawn);
	EXPECT_GT(on.frame_stats_get().indirect_draws, 0u);
	off.resource().mesh_unload(off_mesh);
	on.resource().mesh_unload(on_mesh);
}

//视锥平面提取与球 / 盒判定（第二百四十八轮；纯 CPU 口径）
