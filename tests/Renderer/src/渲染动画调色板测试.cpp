/*
渲染动画调色板测试（方向十 · 阶段 A 第 3 轮）
覆盖：调色板容量与矩阵步长契约、单位矩阵填充、越界写入整体拒绝，
      以及渲染器初始化后调色板描述符通路可用（容量查询 ＋ 正常绘制一帧）。
数据用例不接触 Vulkan；集成用例验证新 binding 与描述符池不破坏既有绘制路径。
*/
#include <vector>

#include "测试辅助/渲染测试辅助.h"
//获取调色板数据契约
#include "Engine/EngineRenderer/src/render/Animation/动画数据.h"

using namespace byjy_render_test;

//渲染动画调色板测试夹具
class Renderer_Animation_Palette_Test : public ::testing::Test
{
};

//容量与单条矩阵步长固定（GPU 缓冲按此分配）
TEST_F(Renderer_Animation_Palette_Test, 调色板容量与步长)
{
	EXPECT_EQ(engine::ANIMATION_PALETTE_CAPACITY, 1024u);
	EXPECT_EQ(engine::ANIMATION_PALETTE_MATRIX_FLOATS, 16u);
	EXPECT_EQ(engine::ANIMATION_PALETTE_MATRIX_BYTES, 64u);
	//单演员骨骼上限须落在每帧槽容量之内
	EXPECT_LE(engine::ANIMATION_ACTOR_BONES_MAX, engine::ANIMATION_PALETTE_CAPACITY);
}

//单位矩阵填充：连续三条均为单位阵，且不越过填充区间
TEST_F(Renderer_Animation_Palette_Test, 单位矩阵填充)
{
	//三条矩阵外加一个哨兵浮点
	std::vector<float> data(static_cast<std::size_t>(engine::ANIMATION_PALETTE_MATRIX_FLOATS) * 3 + 1, -1.0f);
	engine::palette_identity_fill(data.data(), 3);
	//首条：对角线为 1，其余为 0
	EXPECT_FLOAT_EQ(data[0], 1.0f);
	EXPECT_FLOAT_EQ(data[5], 1.0f);
	EXPECT_FLOAT_EQ(data[10], 1.0f);
	EXPECT_FLOAT_EQ(data[15], 1.0f);
	EXPECT_FLOAT_EQ(data[1], 0.0f);
	//第二条与第三条起点同样为单位阵对角线
	EXPECT_FLOAT_EQ(data[16], 1.0f);
	EXPECT_FLOAT_EQ(data[21], 1.0f);
	EXPECT_FLOAT_EQ(data[32], 1.0f);
	EXPECT_FLOAT_EQ(data[42], 1.0f);
	//哨兵未被触碰
	EXPECT_FLOAT_EQ(data.back(), -1.0f);
}

//越界或空指针写入整体拒绝，合法区间按条写入
TEST_F(Renderer_Animation_Palette_Test, 越界写入被拒绝)
{
	//两条容量的目标区
	std::vector<float> data(static_cast<std::size_t>(engine::ANIMATION_PALETTE_MATRIX_FLOATS) * 2, 0.0f);
	//单条源矩阵（首元素标记）
	std::vector<float> source(engine::ANIMATION_PALETTE_MATRIX_FLOATS, 0.0f);
	source[0] = 2.0f;
	//起点越界
	EXPECT_FALSE(engine::palette_matrix_write(data.data(), 2, 2, source.data(), 1));
	//末尾越界
	EXPECT_FALSE(engine::palette_matrix_write(data.data(), 2, 1, source.data(), 2));
	//空目标
	EXPECT_FALSE(engine::palette_matrix_write(nullptr, 2, 0, source.data(), 1));
	//越界请求不得产生部分写
	EXPECT_FLOAT_EQ(data[0], 0.0f);
	//合法写入第 2 条
	ASSERT_TRUE(engine::palette_matrix_write(data.data(), 2, 1, source.data(), 1));
	EXPECT_FLOAT_EQ(data[16], 2.0f);
	//空写入视为成功空操作
	EXPECT_TRUE(engine::palette_matrix_write(data.data(), 2, 0, nullptr, 0));
}

//渲染器初始化后调色板通路可用，且绘制一帧不受新增描述符影响
TEST_F(Renderer_Animation_Palette_Test, 初始化后容量可查询并可绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//容量查询与契约一致
	EXPECT_EQ(renderer.resource().animation_palette_capacity(), engine::ANIMATION_PALETTE_CAPACITY);

	//加载并登记一个四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	ASSERT_NE(handle, 0u);
	EXPECT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	//推进一帧（新描述符若未完整绑定，校验层会在本帧报错）
	EXPECT_TRUE(renderer.frame_render());
	renderer.resource().mesh_unload(handle);
}
