/*
渲染动画绑定测试（方向十 · 收敛轮 867）
覆盖：蒙皮网格骨骼数声明与关节下标校验（合法 / 越界 / 未声明 / 超上限）、
      绘制区间不足时的拒绝、权重全零顶点仍可创建（着色器回落原样）。
口径：只验证创建与登记行为，不做像素断言（像素路径已由蒙皮绘制套件覆盖）。
*/
#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画数据.h"

using namespace byjy_render_test;

//渲染动画绑定测试夹具
class Renderer_Animation_Binding_Test : public ::testing::Test
{
};

//构造单个蒙皮三角形（关节与权重可调）
static engine::Skinned_Mesh_Data skinned_triangle(uint16_t joint0, uint16_t weight0)
{
	engine::Skinned_Mesh_Data data;
	data.vertices = {
		{ { -0.5f, -0.4f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { joint0, 0, 0, 0 }, { weight0, 0, 0, 0 } },
		{ { -0.1f, -0.4f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { joint0, 0, 0, 0 }, { weight0, 0, 0, 0 } },
		{ { -0.3f, 0.2f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { joint0, 0, 0, 0 }, { weight0, 0, 0, 0 } },
	};
	data.indices = { 0, 1, 2 };
	return data;
}

//合法骨骼声明：关节均小于声明值
TEST_F(Renderer_Animation_Binding_Test, 合法骨骼声明通过)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(handle, skinned_triangle(1, 65535), 2));
	EXPECT_NE(handle, 0u);
	renderer.resource().mesh_unload(handle);
}

//关节越界：声明 2 根骨骼但关节为 2，整次拒绝
TEST_F(Renderer_Animation_Binding_Test, 关节越界拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	EXPECT_FALSE(renderer.resource().mesh_load_skinned(handle, skinned_triangle(2, 65535), 2));
	EXPECT_EQ(handle, 0u);
}

//未声明骨骼数：跳过关节校验（兼容旧数据）
TEST_F(Renderer_Animation_Binding_Test, 未声明骨骼跳过校验)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(handle, skinned_triangle(7, 65535)));
	EXPECT_NE(handle, 0u);
	renderer.resource().mesh_unload(handle);
}

//骨骼数超单演员上限：拒绝
TEST_F(Renderer_Animation_Binding_Test, 骨骼数超上限拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	EXPECT_FALSE(renderer.resource().mesh_load_skinned(handle, skinned_triangle(0, 65535),
		engine::ANIMATION_ACTOR_BONES_MAX + 1));
	EXPECT_EQ(handle, 0u);
}

//权重全零：仍可创建（着色器按权重和为零回落原样）
TEST_F(Renderer_Animation_Binding_Test, 权重全零仍可创建)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(handle, skinned_triangle(0, 0), 1));
	EXPECT_NE(handle, 0u);
	renderer.resource().mesh_unload(handle);
}

//绘制区间不足：偏移 + 骨骼数超出调色板容量时拒绝
TEST_F(Renderer_Animation_Binding_Test, 绘制区间不足拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(handle, skinned_triangle(0, 65535), 2));
	//末条为起点时区间 1 条容纳不下 2 根骨骼
	const uint32_t last = renderer.resource().animation_palette_capacity() - 1;
	EXPECT_FALSE(renderer.resource().mesh_draw_skinned(handle, nullptr, last));
	//起点 0 正常登记
	EXPECT_TRUE(renderer.resource().mesh_draw_skinned(handle, nullptr, 0));
	renderer.resource().mesh_unload(handle);
}
