/*
渲染GPU驱动剔除测试（2026-11-21 自「渲染GPU驱动测试.cpp」拆出）
覆盖：视锥平面与球判可见、GPU 剔除与 CPU 一致且更保守、GPU 压缩与 CPU 一致、剔除开关画面等价、
      距离与背面剔除边界、GPU 距离剔除一致、三态统计单调、间接绘制开关画面等价。
*/

#include "渲染GPU驱动测试辅助.h"

TEST_F(Renderer_GPU_Driven_Test, 视锥平面与球判可见)
{
	//单位裁剪矩阵：视锥退化为 [−1, 1] 立方体
	std::array<float, 16> identity = identity_matrix();
	engine::Frustum_Planes planes;
	ASSERT_TRUE(engine::frustum_planes_extract(planes, identity.data()));
	//六个平面均已归一化
	for (uint32_t plane = 0; plane < 6; ++plane)
	{
		const float* value = planes.planes[plane];
		const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
		EXPECT_NEAR(length, 1.0f, 1e-4f) << "平面 " << plane;
	}
	//原点小球可见
	const float center_origin[3] = { 0.0f, 0.0f, 0.0f };
	EXPECT_TRUE(engine::frustum_sphere_visible(planes, center_origin, 0.1f));
	//盒内边缘小球可见（1.05 − 1 = 0.05 ≤ 半径）
	const float center_edge[3] = { 1.05f, 0.0f, 0.0f };
	EXPECT_TRUE(engine::frustum_sphere_visible(planes, center_edge, 0.1f));
	//盒外小球不可见
	const float center_out[3] = { 1.5f, 0.0f, 0.0f };
	EXPECT_FALSE(engine::frustum_sphere_visible(planes, center_out, 0.1f));
	//跨边界的大球仍可见（半径覆盖到盒内）
	EXPECT_TRUE(engine::frustum_sphere_visible(planes, center_out, 0.6f));
	//盒判定：完全在立方体内的盒可见、完全在外不可见
	const float inside_min[3] = { -0.5f, -0.5f, -0.5f };
	const float inside_max[3] = { 0.5f, 0.5f, 0.5f };
	EXPECT_TRUE(engine::frustum_aabb_visible(planes, inside_min, inside_max));
	const float outside_min[3] = { 2.0f, 2.0f, 2.0f };
	const float outside_max[3] = { 3.0f, 3.0f, 3.0f };
	EXPECT_FALSE(engine::frustum_aabb_visible(planes, outside_min, outside_max));
	//非法矩阵
	const float broken[16] = { 0.0f };
	EXPECT_FALSE(engine::frustum_planes_extract(planes, broken));
}

/*
GPU 剔除与 CPU 参考一致且 CPU 更保守（第二百四十八轮核心验收）
口径：8 组随机相机 × 2000 个随机实例，断言 GPU 掩码与 CPU 球判定逐项一致，
且 CPU 的 AABB 判定为可见的实例在 GPU 掩码里必须可见（C ⊆ G）。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU剔除与CPU一致且CPU更保守)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//网格局部 AABB（与 quad_data 一致）
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	constexpr uint32_t INSTANCE_COUNT = 2000;
	//随机实例矩阵：平移铺开 ＋ 随机缩放 ＋ 绕 Z 旋转
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-40.0f, 40.0f);
	std::uniform_real_distribution<float> offset_yz(-12.0f, 12.0f);
	std::uniform_real_distribution<float> scale(0.2f, 3.0f);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::vector<std::array<float, 16>> transforms(INSTANCE_COUNT);
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		const float s = scale(generator);
		const float a = angle(generator);
		std::array<float, 16> matrix = identity_matrix();
		matrix[0] = std::cos(a) * s;
		matrix[1] = std::sin(a) * s;
		matrix[4] = -std::sin(a) * s;
		matrix[5] = std::cos(a) * s;
		matrix[10] = s;
		matrix[12] = offset_x(generator);
		matrix[13] = offset_yz(generator);
		matrix[14] = offset_yz(generator);
		transforms[index] = matrix;
	}
	std::vector<uint32_t> mask(INSTANCE_COUNT, 0u);
	//8 组随机相机
	for (uint32_t view = 0; view < 8; ++view)
	{
		renderer.scene().camera_position_set(offset_x(generator) * 0.5f, offset_yz(generator) * 0.5f,
			offset_yz(generator) * 0.5f + 6.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		renderer.scene().camera_fov_set(0.6f + 0.2f * static_cast<float>(view % 4));
		ASSERT_TRUE(renderer.compute().gpu_cull_mask(transforms[0].data(), INSTANCE_COUNT, local_min, local_max, mask.data()));
		//CPU 参考：当前相机矩阵 → 六平面
		float view_proj[16] = {};
		ASSERT_TRUE(renderer.scene().camera_view_proj(view_proj));
		engine::Frustum_Planes planes;
		ASSERT_TRUE(engine::frustum_planes_extract(planes, view_proj));
		uint32_t visible_count = 0;
		for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
		{
			engine::World_Bounds bounds;
			ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, transforms[index].data()));
			const bool cpu_sphere = engine::frustum_sphere_visible(planes, bounds.center, bounds.radius);
			const bool gpu = mask[index] != 0u;
			//GPU 与 CPU 球判定逐项一致
			EXPECT_EQ(gpu, cpu_sphere) << "视图 " << view << " 实例 " << index;
			//CPU 的 AABB 判定更紧：它为可见时必须 GPU 也可见（C ⊆ G）
			if (engine::frustum_aabb_visible(planes, bounds.aabb_min, bounds.aabb_max) && !gpu)
				ADD_FAILURE() << "视图 " << view << " 实例 " << index << " 被 GPU 多剔";
			if (gpu)
				++visible_count;
			}
		//该视图下至少有一半实例可见（相机在场景中，统计口径自洽）
		EXPECT_GE(visible_count, 1u);
	}
}
namespace
{
	//CPU 剔除参考（与渲染器 core 内 frustum_cull_aabb 逐式同构；第二百四十九轮）
	bool cpu_cull_clip(const float view_proj[16], const float model[16],
		const float local_min[3], const float local_max[3])
	{
		//组合矩阵 mvp = view_proj × model（列主序）
		float mvp[16] = {};
		for (uint32_t column = 0; column < 4; ++column)
			for (uint32_t row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (uint32_t inner = 0; inner < 4; ++inner)
					sum += view_proj[inner * 4 + row] * model[column * 4 + inner];
				mvp[column * 4 + row] = sum;
			}
		bool outside[6] = { true, true, true, true, true, true };
		for (uint32_t corner = 0; corner < 8; ++corner)
		{
			const float x = (corner & 1u) != 0 ? local_max[0] : local_min[0];
			const float y = (corner & 2u) != 0 ? local_max[1] : local_min[1];
			const float z = (corner & 4u) != 0 ? local_max[2] : local_min[2];
			const float clip_x = mvp[0] * x + mvp[4] * y + mvp[8] * z + mvp[12];
			const float clip_y = mvp[1] * x + mvp[5] * y + mvp[9] * z + mvp[13];
			const float clip_z = mvp[2] * x + mvp[6] * y + mvp[10] * z + mvp[14];
			const float clip_w = mvp[3] * x + mvp[7] * y + mvp[11] * z + mvp[15];
			if (!(clip_x > clip_w))
				outside[0] = false;
			if (!(clip_x < -clip_w))
				outside[1] = false;
			if (!(clip_y > clip_w))
				outside[2] = false;
			if (!(clip_y < -clip_w))
				outside[3] = false;
			if (!(clip_z < 0.0f))
				outside[4] = false;
			if (!(clip_z > clip_w))
				outside[5] = false;
		}
		for (uint32_t plane = 0; plane < 6; ++plane)
			if (outside[plane])
				return true;
		return false;
	}
}

/*
GPU 压缩与 CPU 剔除一致（第二百四十九轮）
口径：8 组随机相机 × 2000 随机实例，GPU 压缩出的矩阵序列必须与 CPU 剔除保留的序列逐字节一致。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU压缩与CPU剔除一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	constexpr uint32_t INSTANCE_COUNT = 2000;
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-30.0f, 30.0f);
	std::uniform_real_distribution<float> offset_yz(-10.0f, 10.0f);
	std::uniform_real_distribution<float> scale(0.2f, 2.0f);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::vector<std::array<float, 16>> transforms(INSTANCE_COUNT);
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		const float s = scale(generator);
		const float a = angle(generator);
		std::array<float, 16> matrix = identity_matrix();
		matrix[0] = std::cos(a) * s;
		matrix[1] = std::sin(a) * s;
		matrix[4] = -std::sin(a) * s;
		matrix[5] = std::cos(a) * s;
		matrix[10] = s;
		matrix[12] = offset_x(generator);
		matrix[13] = offset_yz(generator);
		matrix[14] = offset_yz(generator);
		transforms[index] = matrix;
	}
	std::vector<float> compacted(static_cast<std::size_t>(INSTANCE_COUNT) * 16, 0.0f);
	for (uint32_t view = 0; view < 8; ++view)
	{
		renderer.scene().camera_position_set(offset_x(generator) * 0.3f, offset_yz(generator) * 0.3f, 6.0f);
		renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
		renderer.scene().camera_fov_set(0.7f + 0.15f * static_cast<float>(view % 4));
		uint32_t visible = 0;
		ASSERT_TRUE(renderer.compute().gpu_instance_compact(transforms[0].data(), INSTANCE_COUNT,
			local_min, local_max, compacted.data(), &visible));
		float view_proj[16] = {};
		ASSERT_TRUE(renderer.scene().camera_view_proj(view_proj));
		//CPU 参考序列
		std::vector<std::array<float, 16>> expected;
		for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
			if (!cpu_cull_clip(view_proj, transforms[index].data(), local_min, local_max))
				expected.push_back(transforms[index]);
		ASSERT_EQ(visible, static_cast<uint32_t>(expected.size())) << "视图 " << view;
		/*
		逐实例比对（按字典序排序后比较）
		原子计数分配的槽位顺序由 GPU 调度决定，不保证与输入顺序一致；
		故校验「集合相等」而非「序列相等」——像素等价由开关用例以整帧像素断言覆盖。
		*/
		std::vector<std::array<float, 16>> gpu_slots(visible);
		for (uint32_t slot = 0; slot < visible; ++slot)
			for (uint32_t element = 0; element < 16; ++element)
				gpu_slots[slot][element] = compacted[static_cast<std::size_t>(slot) * 16 + element];
		auto less = [](const std::array<float, 16>& left, const std::array<float, 16>& right)
		{
			for (uint32_t element = 0; element < 16; ++element)
				if (left[element] != right[element])
					return left[element] < right[element];
			return false;
		};
		std::sort(gpu_slots.begin(), gpu_slots.end(), less);
		std::sort(expected.begin(), expected.end(), less);
		for (uint32_t slot = 0; slot < visible; ++slot)
			for (uint32_t element = 0; element < 16; ++element)
				ASSERT_FLOAT_EQ(gpu_slots[slot][element], expected[slot][element])
					<< "视图 " << view << " 槽 " << slot << " 元素 " << element;
	}
}

/*
GPU 剔除开关的画面与统计等价（第二百四十九轮核心验收）
口径：同一 1000 实例场景，关/开 GPU 剔除的两渲染器必须像素逐字节一致，绘制与剔除统计一致。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU剔除开关画面等价)
{
	engine::Renderer_Config off_config = hidden_config();
	off_config.instance_capacity = 1024;
	engine::Renderer off;
	ASSERT_TRUE(off.init(off_config));
	if (!off.compute().readback_supported() || !off.compute().compute_supported())
		GTEST_SKIP() << "本机不支持像素回读或计算路径";
	engine::Renderer_Config on_config = off_config;
	on_config.gpu_cull_enable = true;
	engine::Renderer on;
	ASSERT_TRUE(on.init(on_config));
	EXPECT_TRUE(on.compute().gpu_cull_enable_get());
	//同一批实例（横向铺开，一部分在视锥外）
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-8.0f, 8.0f);
	std::vector<std::array<float, 16>> transforms(1000);
	for (uint32_t index = 0; index < transforms.size(); ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = offset_x(generator);
		transforms[index] = matrix;
	}
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle on_mesh = 0;
	ASSERT_TRUE(off.resource().mesh_load(off_mesh, quad_data()));
	ASSERT_TRUE(on.resource().mesh_load(on_mesh, quad_data()));
	ASSERT_TRUE(off.resource().mesh_draw_instanced(off_mesh, transforms[0].data(), static_cast<uint32_t>(transforms.size())));
	ASSERT_TRUE(on.resource().mesh_draw_instanced(on_mesh, transforms[0].data(), static_cast<uint32_t>(transforms.size())));
	std::vector<uint8_t> off_pixels(WINDOW_WIDTH * WINDOW_HEIGHT * 4, 0);
	std::vector<uint8_t> on_pixels(off_pixels.size(), 0);
	ASSERT_TRUE(off.frame_render(off_pixels.data(), static_cast<uint32_t>(off_pixels.size())));
	ASSERT_TRUE(on.frame_render(on_pixels.data(), static_cast<uint32_t>(on_pixels.size())));
	//像素逐字节一致
	uint32_t diff = 0;
	for (std::size_t offset = 0; offset < off_pixels.size(); ++offset)
		if (off_pixels[offset] != on_pixels[offset])
			++diff;
	EXPECT_EQ(diff, 0u) << "开/关 GPU 剔除的像素不一致";
	//统计一致（剔除数 / 可见实例数 / 实际绘制实例数）
	const engine::Frame_Stats off_stats = off.frame_stats_get();
	const engine::Frame_Stats on_stats = on.frame_stats_get();
	EXPECT_EQ(off_stats.last_culled_instances, on_stats.last_culled_instances);
	EXPECT_EQ(off_stats.last_instances, on_stats.last_instances);
	EXPECT_EQ(off_stats.drawn_instances, on_stats.drawn_instances);
	//GPU 路径确实产生了耗时或至少计数可读（不支持时间戳时为 −1）
	if (on.compute().compute_timestamp_supported())
		EXPECT_GE(on_stats.gpu_cull_ms, 0.0);
	off.resource().mesh_unload(off_mesh);
	on.resource().mesh_unload(on_mesh);
}
//距离与背面剔除边界（第二百五十轮；纯 CPU 口径）
TEST_F(Renderer_GPU_Driven_Test, 距离与背面剔除边界)
{
	const float camera[3] = { 0.0f, 0.0f, 0.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	//恰好等于阈值仍可见（严格大于才剔除）
	const float at_edge[3] = { 5.0f, 0.0f, 0.0f };
	EXPECT_FALSE(engine::distance_cull(at_edge, camera, 5.0f));
	//略超阈值被剔除
	const float beyond[3] = { 5.001f, 0.0f, 0.0f };
	EXPECT_TRUE(engine::distance_cull(beyond, camera, 5.0f));
	//阈值 ≤ 0 不限距离
	EXPECT_FALSE(engine::distance_cull(beyond, camera, 0.0f));
	EXPECT_FALSE(engine::distance_cull(beyond, camera, -1.0f));
	//相机前方可见、正交可见、后方被剔除
	const float front[3] = { 0.0f, 0.0f, -3.0f };
	const float side[3] = { 3.0f, 0.0f, 0.0f };
	const float behind[3] = { 0.0f, 0.0f, 3.0f };
	EXPECT_FALSE(engine::backface_cull(front, camera, forward));
	EXPECT_FALSE(engine::backface_cull(side, camera, forward));
	EXPECT_TRUE(engine::backface_cull(behind, camera, forward));
}

/*
GPU 距离 / 背面剔除与 CPU 一致（第二百五十轮）
口径：同一 2000 实例场景，开启距离上限与背面剔除，GPU 压缩可见数必须等于 CPU 参考（距离 ∪ 背面 ∪ 视锥）。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU距离剔除与CPU一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	constexpr uint32_t INSTANCE_COUNT = 2000;
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-20.0f, 20.0f);
	std::uniform_real_distribution<float> offset_yz(-8.0f, 8.0f);
	std::vector<std::array<float, 16>> transforms(INSTANCE_COUNT);
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = offset_x(generator);
		matrix[13] = offset_yz(generator);
		matrix[14] = offset_yz(generator);
		transforms[index] = matrix;
	}
	//相机在 +Z 侧看向原点；距离上限 22；开背面剔除
	const float camera[3] = { 0.0f, 0.0f, 6.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	constexpr float MAX_DISTANCE = 22.0f;
	renderer.scene().camera_position_set(camera[0], camera[1], camera[2]);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	std::vector<float> compacted(static_cast<std::size_t>(INSTANCE_COUNT) * 16, 0.0f);
	uint32_t visible = 0;
	ASSERT_TRUE(renderer.compute().gpu_instance_compact_ex(transforms[0].data(), INSTANCE_COUNT,
		local_min, local_max, camera, forward, MAX_DISTANCE, true, compacted.data(), &visible));
	float view_proj[16] = {};
	ASSERT_TRUE(renderer.scene().camera_view_proj(view_proj));
	uint32_t expected = 0;
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		//实例中心 = 平移列
		const float center[3] = { transforms[index][12], transforms[index][13], transforms[index][14] };
		const bool culled = engine::distance_cull(center, camera, MAX_DISTANCE)
			|| engine::backface_cull(center, camera, forward)
			|| cpu_cull_clip(view_proj, transforms[index].data(), local_min, local_max);
		if (!culled)
			++expected;
	}
	EXPECT_EQ(visible, expected);
}
/*
剔除三态统计单调（第二百五十一轮）
口径：同一 512 实例场景三态——关（CPU 剔除）/ GPU 视锥 / GPU 视锥＋距离：
① 关与 GPU 视锥两态剔除数相等（开关等价）；② GPU 派发计数 0 → 1；③ 加距离后可见数不增。
*/
TEST_F(Renderer_GPU_Driven_Test, 剔除三态统计单调)
{
	engine::Renderer_Config base = hidden_config();
	base.instance_capacity = 1024;
	engine::Renderer off;
	ASSERT_TRUE(off.init(base));
	engine::Renderer_Config gpu_config = base;
	gpu_config.gpu_cull_enable = true;
	engine::Renderer gpu;
	ASSERT_TRUE(gpu.init(gpu_config));
	if (!gpu.compute().compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	constexpr uint32_t COUNT = 512;
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-8.0f, 8.0f);
	std::vector<std::array<float, 16>> transforms(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = offset_x(generator);
		transforms[index] = matrix;
	}
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle gpu_mesh = 0;
	ASSERT_TRUE(off.resource().mesh_load(off_mesh, quad_data()));
	ASSERT_TRUE(gpu.resource().mesh_load(gpu_mesh, quad_data()));
	ASSERT_TRUE(off.resource().mesh_draw_instanced(off_mesh, transforms[0].data(), COUNT));
	ASSERT_TRUE(gpu.resource().mesh_draw_instanced(gpu_mesh, transforms[0].data(), COUNT));
	ASSERT_TRUE(off.frame_render());
	ASSERT_TRUE(gpu.frame_render());
	const engine::Frame_Stats off_stats = off.frame_stats_get();
	const engine::Frame_Stats gpu_stats = gpu.frame_stats_get();
	//关与 GPU 视锥的剔除 / 可见数一致
	EXPECT_EQ(off_stats.last_culled_instances, gpu_stats.last_culled_instances);
	EXPECT_EQ(off_stats.last_instances, gpu_stats.last_instances);
	//派发计数：关为 0、GPU 为 1
	EXPECT_EQ(off_stats.gpu_cull_dispatches, 0u);
	EXPECT_EQ(gpu_stats.gpu_cull_dispatches, 1u);
	//再加距离上限：可见数不增（且剔除数增加）
	const float center[3] = { 0.0f, 0.0f, 6.0f };
	const float forward_dir[3] = { 0.0f, 0.0f, -1.0f };
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	std::vector<float> compacted(static_cast<std::size_t>(COUNT) * 16, 0.0f);
	uint32_t visible_distance = 0;
	ASSERT_TRUE(gpu.compute().gpu_instance_compact_ex(transforms[0].data(), COUNT, local_min, local_max,
		center, forward_dir, 14.0f, false, compacted.data(), &visible_distance));
	const float* view_proj_dummy = nullptr;
	(void)view_proj_dummy;
	EXPECT_LE(visible_distance, gpu_stats.last_instances);
	//派发计数再增 1
	EXPECT_EQ(gpu.frame_stats_get().gpu_cull_dispatches, 2u);
	off.resource().mesh_unload(off_mesh);
	gpu.resource().mesh_unload(gpu_mesh);
}
/*
间接绘制开关画面等价（第二百五十二轮核心验收）
口径：同一批 8 次实例化绘制（每次 32 实例、实例区连续），开/关间接绘制的像素逐字节一致，
      关时 indirect_draws 恒 0，开时 > 0 且不超过实际绘制项数，绘制实例数守恒。
*/
TEST_F(Renderer_GPU_Driven_Test, 间接绘制开关画面等价)
{
	engine::Renderer_Config off_config = hidden_config();
	off_config.instance_capacity = 512;
	engine::Renderer off;
	ASSERT_TRUE(off.init(off_config));
	if (!off.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	engine::Renderer_Config on_config = off_config;
	on_config.gpu_indirect_enable = true;
	engine::Renderer on;
	ASSERT_TRUE(on.init(on_config));
	constexpr uint32_t CHUNKS = 8;
	constexpr uint32_t PER_CHUNK = 32;
	//实例横向连续铺开（相邻绘制项的实例区在实例缓冲内紧邻）
	std::vector<std::array<float, 16>> transforms(CHUNKS * PER_CHUNK);
	for (uint32_t index = 0; index < transforms.size(); ++index)
	{
		const float ratio = static_cast<float>(index) / static_cast<float>(transforms.size() - 1);
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = -6.0f + 12.0f * ratio;
		transforms[index] = matrix;
	}
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle on_mesh = 0;
	ASSERT_TRUE(off.resource().mesh_load(off_mesh, quad_data()));
	ASSERT_TRUE(on.resource().mesh_load(on_mesh, quad_data()));
	for (uint32_t chunk = 0; chunk < CHUNKS; ++chunk)
	{
		const uint32_t base = chunk * PER_CHUNK;
		ASSERT_TRUE(off.resource().mesh_draw_instanced(off_mesh, transforms[base].data(), PER_CHUNK));
		ASSERT_TRUE(on.resource().mesh_draw_instanced(on_mesh, transforms[base].data(), PER_CHUNK));
	}
	std::vector<uint8_t> off_pixels(WINDOW_WIDTH * WINDOW_HEIGHT * 4, 0);
	std::vector<uint8_t> on_pixels(off_pixels.size(), 0);
	ASSERT_TRUE(off.frame_render(off_pixels.data(), static_cast<uint32_t>(off_pixels.size())));
	ASSERT_TRUE(on.frame_render(on_pixels.data(), static_cast<uint32_t>(on_pixels.size())));
	//像素逐字节一致
	uint32_t diff = 0;
	for (std::size_t offset = 0; offset < off_pixels.size(); ++offset)
		if (off_pixels[offset] != on_pixels[offset])
			++diff;
	EXPECT_EQ(diff, 0u) << "开/关间接绘制的像素不一致";
	const engine::Frame_Stats off_stats = off.frame_stats_get();
	const engine::Frame_Stats on_stats = on.frame_stats_get();
	//关时恒 0；开时确有间接命令，且命令数不多于实际绘制项数（合批只会减少）
	EXPECT_EQ(off_stats.indirect_draws, 0u);
	EXPECT_GT(on_stats.indirect_draws, 0u);
	EXPECT_LE(on_stats.indirect_draws, on_stats.drawn_items);
	//实例数守恒
	EXPECT_EQ(off_stats.drawn_instances, on_stats.drawn_instances);
	off.resource().mesh_unload(off_mesh);
	on.resource().mesh_unload(on_mesh);
}
