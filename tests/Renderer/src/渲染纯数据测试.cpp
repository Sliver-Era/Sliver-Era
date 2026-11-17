/*
渲染纯数据测试（2026-11-17 测试层改造 C）
不建窗口、不建 GPU：直接覆盖渲染核心的 Vulkan-free 纯数据逻辑。
覆盖：簇网格簇数与钳制、前缀和 / 排他偏移、有效光源数、簇覆盖计数（方向光）、
      世界包围体构造与旋转保守扩张、相机投影往返与 AABB 可见性。
目的：让数据 / 数学逻辑的回归不再依赖整机 E2E。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

//簇划分与光源记录（Vulkan-free）
#include "Engine/EngineRenderer/src/render/Cluster/簇划分.h"
#include "Engine/EngineRenderer/src/render/Scene/光源数据.h"
//相机与世界包围体（Vulkan-free）
#include "Engine/EngineRenderer/src/render/Scene/相机.h"
#include "Engine/EngineRenderer/src/render/GPU驱动/包围体.h"
//结构化像素指标（公共测试辅助，不建渲染器）
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//纯数据测试夹具
	class Renderer_Pure_Data_Test : public ::testing::Test
	{
	};
}

//默认网格为 16 × 9 × 24 = 3456 簇；任一轴为 0 时簇数为 0
TEST_F(Renderer_Pure_Data_Test, 簇网格簇数)
{
	EXPECT_EQ(engine::cluster_grid_clusters(engine::Cluster_Grid{}), 3456u);
	engine::Cluster_Grid zero_z;
	zero_z.z = 0;
	EXPECT_EQ(engine::cluster_grid_clusters(zero_z), 0u);
}

//逐轴钳到 1..64；总簇数超 4096 时收缩 z 轴
TEST_F(Renderer_Pure_Data_Test, 簇网格钳制)
{
	engine::Cluster_Grid input{ 0, 100, 0 };
	engine::Cluster_Grid clamped = engine::cluster_grid_clamp(input);
	EXPECT_EQ(clamped.x, 1u);
	EXPECT_EQ(clamped.y, 64u);
	EXPECT_EQ(clamped.z, 1u);
	//64 × 64 × 64 超限 → z 收缩到 1，总簇数恰为 4096
	engine::Cluster_Grid large{ 64, 64, 64 };
	engine::Cluster_Grid large_clamped = engine::cluster_grid_clamp(large);
	EXPECT_LE(engine::cluster_grid_clusters(large_clamped), 4096u);
	EXPECT_EQ(large_clamped.z, 1u);
}

//包含式全量前缀和与排他偏移
TEST_F(Renderer_Pure_Data_Test, 前缀和与排他偏移)
{
	const uint32_t counts[5] = { 1, 0, 3, 2, 4 };
	uint32_t prefix[5] = {};
	ASSERT_EQ(engine::cluster_prefix_scan(prefix, counts, 5), 5u);
	EXPECT_EQ(prefix[0], 1u);
	EXPECT_EQ(prefix[1], 1u);
	EXPECT_EQ(prefix[2], 4u);
	EXPECT_EQ(prefix[3], 6u);
	EXPECT_EQ(prefix[4], 10u);
	//排他偏移：每簇写入起点，返回总条目数
	uint32_t offsets[5] = {};
	ASSERT_EQ(engine::cluster_list_offsets(counts, 5, offsets), 10u);
	EXPECT_EQ(offsets[0], 0u);
	EXPECT_EQ(offsets[1], 1u);
	EXPECT_EQ(offsets[2], 1u);
	EXPECT_EQ(offsets[3], 4u);
	EXPECT_EQ(offsets[4], 6u);
}

//块内包含式前缀和：跨块边界重新起算，块总和单独写出
TEST_F(Renderer_Pure_Data_Test, 块内前缀和)
{
	//256 + 44 两个块
	std::vector<uint32_t> counts(300, 1u);
	std::vector<uint32_t> prefix(300, 0u);
	std::vector<uint32_t> totals(2, 0u);
	ASSERT_EQ(engine::cluster_prefix_block_scan(prefix.data(), totals.data(), 2, counts.data(), 300), 2u);
	EXPECT_EQ(prefix[0], 1u);
	EXPECT_EQ(prefix[255], 256u);
	EXPECT_EQ(prefix[256], 1u);
	EXPECT_EQ(prefix[299], 44u);
	EXPECT_EQ(totals[0], 256u);
	EXPECT_EQ(totals[1], 44u);
}

//有效光源数：Cluster 关闭截断到 32；开启按记录容量 1024 截断
TEST_F(Renderer_Pure_Data_Test, 有效光源数)
{
	EXPECT_EQ(engine::light_effective_count(40, false), 32u);
	EXPECT_EQ(engine::light_effective_count(40, true), 40u);
	EXPECT_EQ(engine::light_effective_count(2000, true), 1024u);
}

//方向光覆盖全部簇（CPU 参考）
TEST_F(Renderer_Pure_Data_Test, 方向光覆盖全部簇)
{
	engine::Cluster_Grid grid{ 2, 2, 2 };
	std::vector<uint32_t> counts(engine::cluster_grid_clusters(grid), 0u);
	engine::Light_Record light{};
	light.type = static_cast<uint32_t>(engine::Light_Type::Directional);
	light.flags = 1u;
	engine::Cluster_Input input{};
	//单位 view_proj 足以让方向光路径走通
	input.view_proj[0] = 1.0f;
	input.view_proj[5] = 1.0f;
	input.view_proj[10] = 1.0f;
	input.view_proj[15] = 1.0f;
	uint32_t written = engine::cluster_counts_reference(counts.data(), static_cast<uint32_t>(counts.size()),
		grid, &light, 1, input);
	ASSERT_EQ(written, 8u);
	for (uint32_t value : counts)
		EXPECT_EQ(value, 1u);
}

//单位变换下的世界包围体：AABB 逐轴外扩、球半径覆盖盒角
TEST_F(Renderer_Pure_Data_Test, 世界包围体单位变换)
{
	const float local_min[3] = { -1.0f, -1.0f, -1.0f };
	const float local_max[3] = { 1.0f, 1.0f, 1.0f };
	const float identity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	engine::World_Bounds bounds{};
	ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, identity));
	//中心在原点
	EXPECT_NEAR(bounds.center[0], 0.0f, 1e-6f);
	EXPECT_NEAR(bounds.center[1], 0.0f, 1e-6f);
	EXPECT_NEAR(bounds.center[2], 0.0f, 1e-6f);
	//AABB 为 ±(1 + 外扩量)
	EXPECT_NEAR(bounds.aabb_max[0], 1.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-6f);
	EXPECT_NEAR(bounds.aabb_min[2], -1.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-6f);
	//球半径不小于局部半对角长
	EXPECT_GT(bounds.radius, 1.7320508f);
	EXPECT_LT(bounds.radius, 1.8f);
}

//绕 Z 轴 45°：世界 AABB 必须按 8 角点重求（保守扩张到 √2）
TEST_F(Renderer_Pure_Data_Test, 世界包围体旋转保守扩张)
{
	const float local_min[3] = { -1.0f, -1.0f, -1.0f };
	const float local_max[3] = { 1.0f, 1.0f, 1.0f };
	const float c = 0.70710678f;
	const float s = 0.70710678f;
	//列主序：绕 +Z 旋转 45°
	const float model[16] = {
		c, s, 0.0f, 0.0f,
		-s, c, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	engine::World_Bounds bounds{};
	ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, model));
	//旋转后 x 向半宽为 √2（而非 1）
	EXPECT_NEAR(bounds.aabb_max[0], 1.41421356f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-4f);
	EXPECT_NEAR(bounds.aabb_min[0], -1.41421356f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-4f);
	//球半径覆盖扩展后的盒角
	EXPECT_GT(bounds.radius, 1.41421356f);
}

//非法入参：空指针、下界大于上界、矩阵非有限值
TEST_F(Renderer_Pure_Data_Test, 世界包围体非法入参)
{
	engine::World_Bounds bounds{};
	const float identity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	const float local_min[3] = { 1.0f, 0.0f, 0.0f };
	const float local_max[3] = { -1.0f, 0.0f, 0.0f };
	EXPECT_FALSE(engine::world_bounds_build(bounds, local_min, local_max, identity));
	EXPECT_FALSE(engine::world_bounds_build(bounds, nullptr, local_max, identity));
	float invalid[16] = {};
	invalid[0] = std::nanf("");
	EXPECT_FALSE(engine::world_bounds_build(bounds, local_min, local_max, invalid));
}

//相机投影 → 反投影往返：中心点应回到世界原点
TEST_F(Renderer_Pure_Data_Test, 相机投影反投影往返)
{
	engine::Camera camera;
	camera.position_set(0.0f, 0.0f, 3.0f);
	camera.target_set(0.0f, 0.0f, 0.0f);
	camera.aspect_set(1.0f);
	camera.fov_set(1.0472f);
	camera.near_far_set(0.1f, 100.0f);
	float u = 0.0f;
	float v = 0.0f;
	float depth = 0.0f;
	const float center[3] = { 0.0f, 0.0f, 0.0f };
	ASSERT_TRUE(camera.project_to_screen(center, &u, &v, &depth));
	EXPECT_NEAR(u, 0.5f, 1e-3f);
	EXPECT_NEAR(v, 0.5f, 1e-3f);
	EXPECT_GT(depth, 0.0f);
	EXPECT_LT(depth, 1.0f);
	float back[3] = {};
	ASSERT_TRUE(camera.unproject_from_screen(u, v, depth, back));
	EXPECT_NEAR(back[0], 0.0f, 1e-3f);
	EXPECT_NEAR(back[1], 0.0f, 1e-3f);
	EXPECT_NEAR(back[2], 0.0f, 1e-3f);
}

//相机 AABB 可见性：机位前方可见、背后不可见
TEST_F(Renderer_Pure_Data_Test, 相机AABB可见性)
{
	engine::Camera camera;
	camera.position_set(0.0f, 0.0f, 3.0f);
	camera.target_set(0.0f, 0.0f, 0.0f);
	camera.aspect_set(1.0f);
	camera.fov_set(1.0472f);
	camera.near_far_set(0.1f, 100.0f);
	//机位前方（原点附近）
	const float front_min[3] = { -0.5f, -0.5f, -0.5f };
	const float front_max[3] = { 0.5f, 0.5f, 0.5f };
	EXPECT_TRUE(camera.aabb_visible(front_min, front_max));
	//机位背后（+Z 方向，相机朝 -Z）
	const float behind_min[3] = { -0.5f, -0.5f, 5.5f };
	const float behind_max[3] = { 0.5f, 0.5f, 6.5f };
	EXPECT_FALSE(camera.aabb_visible(behind_min, behind_max));
}

//结构化像素指标：亮度、最大通道差、差异 / 压暗 / 提亮像素数
TEST_F(Renderer_Pure_Data_Test, 结构化像素指标)
{
	//全黑 / 全白两帧
	std::vector<uint8_t> black(byjy_render_test::pixel_bytes(), 0u);
	std::vector<uint8_t> white(byjy_render_test::pixel_bytes(), 255u);
	const uint32_t total = byjy_render_test::WINDOW_WIDTH * byjy_render_test::WINDOW_HEIGHT;
	//中灰亮度按 Rec.709 应约等于 128
	std::array<uint8_t, 4> mid{ 128u, 128u, 128u, 255u };
	EXPECT_NEAR(byjy_render_test::luma_of(mid), 128.0f, 0.5f);
	//通道差与差异像素
	EXPECT_EQ(byjy_render_test::max_channel_diff(black, white), 255);
	EXPECT_EQ(byjy_render_test::changed_pixels(black, white, 6), total);
	//压暗 / 提亮方向不能颠倒
	EXPECT_EQ(byjy_render_test::darkened_count(black, white, 40.0f), total);
	EXPECT_EQ(byjy_render_test::brightened_count(black, white, 40.0f), 0u);
	EXPECT_EQ(byjy_render_test::brightened_count(white, black, 40.0f), total);
}
