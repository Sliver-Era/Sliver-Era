/*
渲染动画契约测试（方向十 · 阶段 A）
覆盖：蒙皮顶点 Skinned_Vertex 的步长、字段偏移、默认权重，与静态 Vertex 前缀一致性，
      以及 Skinned_Mesh_Data 的容器可用性。
本套件只验证数据契约，不涉及 Vulkan；旧 Vertex / Mesh_Data 行为不变由既有用例守护。
*/
#include <cstddef>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

//获取网格数据类型（Vertex / Skinned_Vertex / Skinned_Mesh_Data）
#include "Engine/EngineFoundation/src/loader/assets/Mesh_Loader/网格加载器.h"

//蒙皮顶点契约测试夹具
class Renderer_Animation_Contract_Test : public ::testing::Test
{
};

//步长须为 48 字节（蒙皮管线布局按此登记）
TEST_F(Renderer_Animation_Contract_Test, 蒙皮顶点步长48)
{
	EXPECT_EQ(sizeof(engine::Skinned_Vertex), 48u);
}

//字段偏移固定（与顶点属性登记一一对应）
TEST_F(Renderer_Animation_Contract_Test, 蒙皮顶点字段偏移)
{
	EXPECT_EQ(offsetof(engine::Skinned_Vertex, position), 0u);
	EXPECT_EQ(offsetof(engine::Skinned_Vertex, uv), 12u);
	EXPECT_EQ(offsetof(engine::Skinned_Vertex, normal), 20u);
	EXPECT_EQ(offsetof(engine::Skinned_Vertex, joint), 32u);
	EXPECT_EQ(offsetof(engine::Skinned_Vertex, weight), 40u);
}

//默认权重全绑关节 0（权重 1.0），未知蒙皮时等价于单位变换
TEST_F(Renderer_Animation_Contract_Test, 默认权重绑定关节0)
{
	engine::Skinned_Vertex vertex{};
	EXPECT_EQ(vertex.joint[0], 0u);
	EXPECT_EQ(vertex.joint[1], 0u);
	EXPECT_EQ(vertex.joint[2], 0u);
	EXPECT_EQ(vertex.joint[3], 0u);
	EXPECT_EQ(vertex.weight[0], 65535u);
	EXPECT_EQ(vertex.weight[1], 0u);
	EXPECT_EQ(vertex.weight[2], 0u);
	EXPECT_EQ(vertex.weight[3], 0u);
}

//前 32 字节与静态 Vertex 完全一致（同一几何语义，仅追加蒙皮字段）
TEST_F(Renderer_Animation_Contract_Test, 前32字节与静态顶点一致)
{
	engine::Vertex plain{};
	plain.position[0] = 1.5f; plain.position[1] = -2.0f; plain.position[2] = 0.25f;
	plain.uv[0] = 0.125f; plain.uv[1] = 0.875f;
	plain.normal[0] = 0.0f; plain.normal[1] = 1.0f; plain.normal[2] = 0.0f;

	engine::Skinned_Vertex skinned{};
	skinned.position[0] = plain.position[0];
	skinned.position[1] = plain.position[1];
	skinned.position[2] = plain.position[2];
	skinned.uv[0] = plain.uv[0];
	skinned.uv[1] = plain.uv[1];
	skinned.normal[0] = plain.normal[0];
	skinned.normal[1] = plain.normal[1];
	skinned.normal[2] = plain.normal[2];

	EXPECT_EQ(std::memcmp(&plain, &skinned, sizeof(engine::Vertex)), 0);
}

//蒙皮网格数据可承载顶点与索引
TEST_F(Renderer_Animation_Contract_Test, 蒙皮网格容器可用)
{
	engine::Skinned_Mesh_Data data;
	data.vertices.resize(4);
	data.indices = { 0, 1, 2, 0, 2, 3 };
	EXPECT_EQ(data.vertices.size(), 4u);
	EXPECT_EQ(data.indices.size(), 6u);
	//逐顶点默认权重保持契约
	for (const engine::Skinned_Vertex& vertex : data.vertices)
		EXPECT_EQ(vertex.weight[0], 65535u);
}
