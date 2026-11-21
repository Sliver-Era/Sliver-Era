/*
渲染动画调色板池测试（方向十 · 收敛轮 860）
覆盖：顺序分配与空洞复用、容量上限与非法参数、渲染器分配 / 读回 / 释放、
      满容量拒绝与释放后复用。分配器为纯数据用例，渲染器用例验证区间内容与缓冲一致。
*/
#include <gtest/gtest.h>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画调色板池.h"

using namespace byjy_render_test;

//渲染动画调色板池测试夹具
class Renderer_Animation_PalettePool_Test : public ::testing::Test
{
};

//顺序分配、释放与空洞复用
TEST_F(Renderer_Animation_PalettePool_Test, 顺序分配与空洞复用)
{
	engine::Animation_Palette_Pool pool;
	pool.reset(16);
	//前两段紧邻追加
	EXPECT_EQ(pool.allocate(4), 0u);
	EXPECT_EQ(pool.allocate(6), 4u);
	EXPECT_EQ(pool.used(), 10u);
	EXPECT_EQ(pool.region_count(), 2u);
	//释放首段并写出条数
	uint32_t released = 0;
	EXPECT_TRUE(pool.release(0, &released));
	EXPECT_EQ(released, 4u);
	EXPECT_EQ(pool.used(), 6u);
	//空洞足够，分配回 0
	EXPECT_EQ(pool.allocate(3), 0u);
	EXPECT_EQ(pool.used(), 9u);
	//释放中间段
	EXPECT_TRUE(pool.release(4));
	EXPECT_EQ(pool.used(), 3u);
	EXPECT_EQ(pool.region_count(), 1u);
}

//容量上限、零条与未知区间
TEST_F(Renderer_Animation_PalettePool_Test, 容量与非法参数)
{
	engine::Animation_Palette_Pool pool;
	pool.reset(10);
	//零条与超容量
	EXPECT_EQ(pool.allocate(0), UINT32_MAX);
	EXPECT_EQ(pool.allocate(11), UINT32_MAX);
	//恰好占满
	EXPECT_EQ(pool.allocate(10), 0u);
	EXPECT_EQ(pool.allocate(1), UINT32_MAX);
	EXPECT_EQ(pool.used(), 10u);
	//未知起始偏移
	EXPECT_FALSE(pool.release(1));
	//释放后可重新占满
	EXPECT_TRUE(pool.release(0));
	EXPECT_EQ(pool.allocate(10), 0u);
}

//渲染器区间分配：新区间为单位矩阵，释放后按新演员复用
TEST_F(Renderer_Animation_PalettePool_Test, 渲染器区间分配与读回)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//两段紧邻分配
	uint32_t first = UINT32_MAX;
	uint32_t second = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(4, first));
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(6, second));
	EXPECT_EQ(first, 0u);
	EXPECT_EQ(second, 4u);
	//新区间首条为单位矩阵
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, first));
	EXPECT_FLOAT_EQ(matrix[0], 1.0f);
	EXPECT_FLOAT_EQ(matrix[5], 1.0f);
	//释放首段后，等长区间复用起点
	ASSERT_TRUE(renderer.resource().animation_palette_release(first));
	uint32_t reused = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(4, reused));
	EXPECT_EQ(reused, 0u);
	//空洞装不下时追加到末尾（当前已用 6 条，起点 10）
	uint32_t appended = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(5, appended));
	EXPECT_EQ(appended, 10u);
	//未知偏移与超单演员上限
	EXPECT_FALSE(renderer.resource().animation_palette_release(3));
	uint32_t ignored = 0;
	EXPECT_FALSE(renderer.resource().animation_palette_allocate(engine::ANIMATION_ACTOR_BONES_MAX + 1, ignored));
}

//满容量后拒绝新演员，释放一段后可复用
TEST_F(Renderer_Animation_PalettePool_Test, 满容量拒绝与释放复用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const uint32_t capacity = renderer.resource().animation_palette_capacity();
	const uint32_t block = engine::ANIMATION_ACTOR_BONES_MAX;
	//按单演员上限分块占满
	uint32_t allocated = 0;
	uint32_t first_offset = UINT32_MAX;
	while (allocated + block <= capacity)
	{
		uint32_t offset = UINT32_MAX;
		ASSERT_TRUE(renderer.resource().animation_palette_allocate(block, offset));
		if (first_offset == UINT32_MAX)
			first_offset = offset;
		allocated += block;
	}
	EXPECT_EQ(allocated, capacity);
	//再分配一条失败
	uint32_t overflow = 0;
	EXPECT_FALSE(renderer.resource().animation_palette_allocate(1, overflow));
	//释放首段后可再分配
	ASSERT_TRUE(renderer.resource().animation_palette_release(first_offset));
	uint32_t reused = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(block, reused));
	EXPECT_EQ(reused, first_offset);
}
