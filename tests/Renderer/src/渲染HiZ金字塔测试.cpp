/*
渲染HiZ金字塔测试（2026-11-21 自「渲染HiZ测试.cpp」拆出）
覆盖：默认关闭且资源惰性、金字塔级数与逐级尺寸、计算关闭时不可用、逐帧生成、深度历史采样句柄、
      关闭时不生成、墙后实例剔除、部分遮挡场景不误剔。
*/

#include "渲染HiZ测试辅助.h"

/*
默认关闭且资源惰性
口径：初始化后开关为假、状态全 0；开启后才建立资源；关闭只置标志、资源保留供再次开启复用。
*/
TEST_F(Renderer_HiZ_Test, 默认关闭且资源惰性)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//能力不足则跳过（本机应具备计算与 R32 浮点存储图像）
	if (!renderer.compute().hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力（计算 ＋ R32 浮点存储图像）";
	EXPECT_FALSE(renderer.compute().hiz_enable_get());
	//未开启时不存在资源
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t levels = 1;
	EXPECT_FALSE(renderer.compute().hiz_state_get(width, height, levels));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
	EXPECT_EQ(levels, 0u);
	//开启后资源建立
	ASSERT_TRUE(renderer.compute().hiz_enable_set(true));
	EXPECT_TRUE(renderer.compute().hiz_enable_get());
	ASSERT_TRUE(renderer.compute().hiz_state_get(width, height, levels));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(levels, EXPECTED_LEVELS);
	//关闭只置标志，资源保留
	ASSERT_TRUE(renderer.compute().hiz_enable_set(false));
	EXPECT_FALSE(renderer.compute().hiz_enable_get());
	EXPECT_TRUE(renderer.compute().hiz_state_get(width, height, levels));
	EXPECT_EQ(levels, EXPECTED_LEVELS);
}

/*
金字塔级数与逐级尺寸
口径：级数 = 自 max(w,h) 逐级折半至 1×1 的级数；第 i 级尺寸 = (max(1, w >> i), max(1, h >> i))。
*/
TEST_F(Renderer_HiZ_Test, 金字塔级数与逐级尺寸)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	EXPECT_TRUE(renderer.compute().hiz_enable_get());
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t levels = 0;
	ASSERT_TRUE(renderer.compute().hiz_state_get(width, height, levels));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(levels, EXPECTED_LEVELS);
	//逐级尺寸（64×36 折半链）
	const uint32_t expected[EXPECTED_LEVELS][2] = {
		{ 64, 36 }, { 32, 18 }, { 16, 9 }, { 8, 4 }, { 4, 2 }, { 2, 1 }, { 1, 1 },
	};
	for (uint32_t level = 0; level < EXPECTED_LEVELS; ++level)
	{
		ASSERT_TRUE(renderer.compute().hiz_level_extent(level, width, height)) << "级 " << level;
		EXPECT_EQ(width, expected[level][0]) << "级 " << level;
		EXPECT_EQ(height, expected[level][1]) << "级 " << level;
	}
	//越界级返回假且出参归零
	EXPECT_FALSE(renderer.compute().hiz_level_extent(EXPECTED_LEVELS, width, height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
}

/*
计算路径强制关闭时的降级
口径：`compute_enable = false` 时 Hi-Z 能力为假，开启请求被拒绝且开关保持关闭、无资源。
*/
TEST_F(Renderer_HiZ_Test, 计算关闭时不可用)
{
	engine::Renderer_Config config = hidden_config();
	config.compute_enable = false;
	config.hiz_enable = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	EXPECT_FALSE(renderer.compute().hiz_supported());
	EXPECT_FALSE(renderer.compute().hiz_enable_get());
	//开启被拒绝
	EXPECT_FALSE(renderer.compute().hiz_enable_set(true));
	EXPECT_FALSE(renderer.compute().hiz_enable_get());
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t levels = 1;
	EXPECT_FALSE(renderer.compute().hiz_state_get(width, height, levels));
	EXPECT_EQ(levels, 0u);
}

/*
金字塔逐帧生成（第二百五十四轮）
口径：单采样路径开启 Hi-Z 后，每帧场景退场时录制「深度拷贝 ＋ 逐级降采样」；
生成次数随帧数增长，且在途帧槽均在两帧内各自生成过一次。
*/
TEST_F(Renderer_HiZ_Test, 金字塔逐帧生成)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	//三个实例（同一网格三批）
	std::array<float, 16> identity = identity_matrix();
	std::vector<std::array<float, 16>> matrices = { identity, identity, identity };
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, matrices[0].data(), static_cast<uint32_t>(matrices.size())));
		ASSERT_TRUE(renderer.frame_render());
	}
	//三帧均完成生成
	EXPECT_GE(renderer.compute().hiz_generation_count(), 3u);
	//两个在途帧槽均已有有效历史
	for (uint32_t slot = 0; slot < 2; ++slot)
		EXPECT_TRUE(renderer.compute().hiz_history_ready(slot)) << "帧槽 " << slot;
	renderer.resource().mesh_unload(mesh);
}

/*
深度历史采样句柄（第三百六十五轮）
口径：开启 Hi-Z 后，历史在逐帧生成结束时转为采样布局；句柄查询仅在「启用 ＋ 资源就绪 ＋ 该槽有历史」时为真。
未生成前 / 关闭开关后 / 越界槽位均返回假且出参归零。
*/
TEST_F(Renderer_HiZ_Test, 深度历史采样句柄)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	//未生成前：句柄为假且出参归零
	uint64_t view = 0;
	uint64_t sampler = 0;
	EXPECT_FALSE(renderer.compute().hiz_history_sample_get(0, view, sampler));
	EXPECT_EQ(view, 0u);
	EXPECT_EQ(sampler, 0u);
	//三帧后两个在途帧槽均可取句柄
	std::array<float, 16> identity = identity_matrix();
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, identity.data(), 1));
		ASSERT_TRUE(renderer.frame_render());
	}
	for (uint32_t slot = 0; slot < 2; ++slot)
	{
		uint64_t slot_view = 0;
		uint64_t slot_sampler = 0;
		EXPECT_TRUE(renderer.compute().hiz_history_sample_get(slot, slot_view, slot_sampler)) << "帧槽 " << slot;
		EXPECT_NE(slot_view, 0u);
		EXPECT_NE(slot_sampler, 0u);
	}
	//越界槽位与关闭开关后均不可取
	uint64_t out_view = 0;
	uint64_t out_sampler = 0;
	EXPECT_FALSE(renderer.compute().hiz_history_sample_get(8, out_view, out_sampler));
	EXPECT_TRUE(renderer.compute().hiz_enable_set(false));
	EXPECT_FALSE(renderer.compute().hiz_history_sample_get(0, out_view, out_sampler));
	renderer.resource().mesh_unload(mesh);
}

/*
关闭时不生成
口径：默认关闭时全部帧均不录制生成命令，历史保持无效、生成计数为 0。
*/
TEST_F(Renderer_HiZ_Test, 关闭时不生成)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	std::array<float, 16> identity = identity_matrix();
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, identity.data(), 1));
		ASSERT_TRUE(renderer.frame_render());
	}
	EXPECT_EQ(renderer.compute().hiz_generation_count(), 0u);
	EXPECT_FALSE(renderer.compute().hiz_history_ready(0));
	EXPECT_FALSE(renderer.compute().hiz_history_ready(1));
	renderer.resource().mesh_unload(mesh);
}

/*
遮挡剔除：墙后实例被剔除（第二百五十六轮）
口径：先渲染一面覆盖屏幕的近墙（两帧），再对墙后 / 墙前两批实例调用 GPU 压缩：
墙后应全部剔除、墙前应全部可见；关闭 Hi-Z 时墙后实例不因遮挡被剔除。
*/
TEST_F(Renderer_HiZ_Test, 遮挡剔除墙后实例)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	//相机与朝向（沿 −Z 观察；墙在 z = 3，实例在 z = 0 / 4.5）
	renderer.scene().camera_position_set(0.0f, 0.0f, 5.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	//近墙：放大 30 倍并平移到 z = 3
	std::array<float, 16> wall = identity_matrix();
	wall[0] = 30.0f;
	wall[5] = 30.0f;
	wall[14] = 3.0f;
	//两帧：第 0 帧写深度并生成金字塔，第 1 帧再次生成（保证在途槽就绪）
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, wall.data(), 1));
		ASSERT_TRUE(renderer.frame_render());
	}
	//实例批次（墙后 z = 0 / 墙前 z = 4.5）
	constexpr uint32_t COUNT = 200;
	std::vector<std::array<float, 16>> behind(COUNT);
	std::vector<std::array<float, 16>> front(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[14] = 0.0f;
		behind[index] = matrix;
		matrix[14] = 4.5f;
		front[index] = matrix;
	}
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	const float camera[3] = { 0.0f, 0.0f, 5.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	std::vector<std::array<float, 16>> compacted(COUNT);
	uint32_t behind_visible = COUNT;
	uint32_t front_visible = 0;
	ASSERT_TRUE(renderer.compute().gpu_instance_compact_ex(behind[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &behind_visible));
	ASSERT_TRUE(renderer.compute().gpu_instance_compact_ex(front[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &front_visible));
	//屏幕两侧边缘（NDC x ≈ ±0.45）的墙后实例也应被剔除（覆盖边缘的有效性检查）
	std::vector<std::array<float, 16>> corner(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = (index % 2 == 0) ? 1.1f : -1.1f;
		matrix[14] = 0.0f;
		corner[index] = matrix;
	}
	uint32_t corner_visible = COUNT;
	ASSERT_TRUE(renderer.compute().gpu_instance_compact_ex(corner[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &corner_visible));
	//墙后全部剔除（含屏幕两侧边缘）、墙前全部可见
	EXPECT_EQ(corner_visible, 0u);
	//关闭 Hi-Z 的对照：墙后实例不应因遮挡被剔除
	engine::Renderer without;
	ASSERT_TRUE(without.init(hidden_config()));
	without.scene().camera_position_set(0.0f, 0.0f, 5.0f);
	without.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	uint32_t without_visible = 0;
	ASSERT_TRUE(without.compute().gpu_instance_compact_ex(behind[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &without_visible));
	EXPECT_EQ(behind_visible, 0u);
	EXPECT_EQ(front_visible, COUNT);
	//生成确实发生
	EXPECT_GE(renderer.compute().hiz_generation_count(), 2u);
	EXPECT_EQ(without_visible, COUNT);
	renderer.resource().mesh_unload(mesh);
}

/*
部分遮挡场景：缝隙内实例不误剔（第二百五十七轮）
口径：左右两堵墙在 z = 3（覆盖屏幕两侧），中央留缝；缝内实例（z = 0，墙后）应全部可见（缝隙深度为清屏 1.0），
墙正后方实例应全部剔除。用于验证保守口径在「部分遮挡」场景下不产生误剔。
*/
TEST_F(Renderer_HiZ_Test, 部分遮挡场景不误剔)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	renderer.scene().camera_position_set(0.0f, 0.0f, 5.0f);
	renderer.scene().camera_target_set(0.0f, 0.0f, 0.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	//左右两堵墙：半宽 0.5、半高 1.5（世界），平移 x = ±0.9、z = 3
	std::array<float, 16> left_wall = identity_matrix();
	left_wall[0] = 2.0f;
	left_wall[5] = 6.0f;
	left_wall[12] = -0.9f;
	left_wall[14] = 3.0f;
	std::array<float, 16> right_wall = left_wall;
	right_wall[12] = 0.9f;
	std::vector<std::array<float, 16>> walls = { left_wall, right_wall };
	//两帧：写深度并生成金字塔
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.resource().mesh_draw_instanced(mesh, walls[0].data(), static_cast<uint32_t>(walls.size())));
		ASSERT_TRUE(renderer.frame_render());
	}
	//缝隙内实例（x = 0，墙后 z = 0）
	constexpr uint32_t COUNT = 200;
	std::vector<std::array<float, 16>> gap(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[14] = 0.0f;
		gap[index] = matrix;
	}
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	const float camera[3] = { 0.0f, 0.0f, 5.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	std::vector<std::array<float, 16>> compacted(COUNT);
	uint32_t gap_visible = 0;
	ASSERT_TRUE(renderer.compute().gpu_instance_compact_ex(gap[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &gap_visible));
	//缝隙内全部可见（部分遮挡场景零误剔）
	EXPECT_EQ(gap_visible, COUNT);
	renderer.resource().mesh_unload(mesh);
}
