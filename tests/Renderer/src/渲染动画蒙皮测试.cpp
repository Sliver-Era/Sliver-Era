/*
渲染动画蒙皮测试（阶段 B 第 9 轮 / 收敛轮 858）
覆盖：蒙皮矩阵（world × inverse_bind）求值与参数拒绝、渲染器调色板初始单位矩阵、
      上传与读回一致、越界与空指针拒绝。蒙皮矩阵为纯数据用例，上传 / 读回为 GPU 缓冲用例。
*/
#include <gtest/gtest.h>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画姿态.h"

using namespace byjy_render_test;

//渲染动画蒙皮测试夹具
class Renderer_Animation_Skin_Test : public ::testing::Test
{
};

//构造两根骨骼与对应逆绑定：根绑定 T(1,2,3) / 逆 T(-1,-2,-3)，子绑定 T(0,1,0) / 逆 T(0,-1,0)
static std::shared_ptr<engine::Animation_Skeleton> make_skeleton(void)
{
	std::vector<engine::Animation_Bone> bones;
	engine::Animation_Bone root;
	root.name = "root";
	root.parent = -1;
	root.bind_translation[0] = 1.0f;
	root.bind_translation[1] = 2.0f;
	root.bind_translation[2] = 3.0f;
	root.inverse_bind[12] = -1.0f;
	root.inverse_bind[13] = -2.0f;
	root.inverse_bind[14] = -3.0f;
	engine::Animation_Bone child;
	child.name = "child";
	child.parent = 0;
	child.bind_translation[1] = 1.0f;
	child.inverse_bind[13] = -1.0f;
	bones.push_back(root);
	bones.push_back(child);
	auto skeleton = std::make_shared<engine::Animation_Skeleton>();
	engine::Animation_Skeleton::build(bones, *skeleton);
	return skeleton;
}

//绑定姿态下：根调色板为单位阵，子调色板等于根绑定平移
TEST_F(Renderer_Animation_Skin_Test, 绑定姿态蒙皮矩阵)
{
	auto skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	ASSERT_TRUE(engine::skeleton_pose_world_update(pose));
	ASSERT_TRUE(engine::skeleton_pose_skin_build(pose));
	const float* palette = pose.skin_matrix.data();
	//根：world × 逆绑定 = 单位阵
	EXPECT_FLOAT_EQ(palette[0], 1.0f);
	EXPECT_FLOAT_EQ(palette[5], 1.0f);
	EXPECT_FLOAT_EQ(palette[10], 1.0f);
	EXPECT_FLOAT_EQ(palette[15], 1.0f);
	EXPECT_FLOAT_EQ(palette[12], 0.0f);
	EXPECT_FLOAT_EQ(palette[13], 0.0f);
	EXPECT_FLOAT_EQ(palette[14], 0.0f);
	//子：world 平移 (1,3,3) × 逆绑定平移 (0,-1,0) = (1,2,3)
	EXPECT_FLOAT_EQ(palette[16 + 12], 1.0f);
	EXPECT_FLOAT_EQ(palette[16 + 13], 2.0f);
	EXPECT_FLOAT_EQ(palette[16 + 14], 3.0f);
}

//参数拒绝：未绑定资产、蒙皮矩阵尺寸不足
TEST_F(Renderer_Animation_Skin_Test, 蒙皮矩阵参数拒绝)
{
	auto skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	//未绑定资产的姿态无法求值
	engine::Animation_Pose unbound;
	EXPECT_FALSE(engine::skeleton_pose_world_update(unbound));
	EXPECT_FALSE(engine::skeleton_pose_skin_build(unbound));
	//蒙皮矩阵尺寸不足
	pose.skin_matrix.pop_back();
	EXPECT_FALSE(engine::skeleton_pose_skin_build(pose));
}

//渲染器调色板初始全为单位矩阵（首条与末条抽查）
TEST_F(Renderer_Animation_Skin_Test, 初始调色板为单位矩阵)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const uint32_t capacity = renderer.resource().animation_palette_capacity();
	ASSERT_GT(capacity, 1u);
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, 0));
	EXPECT_FLOAT_EQ(matrix[0], 1.0f);
	EXPECT_FLOAT_EQ(matrix[5], 1.0f);
	EXPECT_FLOAT_EQ(matrix[10], 1.0f);
	EXPECT_FLOAT_EQ(matrix[15], 1.0f);
	//末条同样为单位矩阵
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, capacity - 1));
	EXPECT_FLOAT_EQ(matrix[0], 1.0f);
	EXPECT_FLOAT_EQ(matrix[12], 0.0f);
}

//写入与读回一致，且不改动相邻条目
TEST_F(Renderer_Animation_Skin_Test, 调色板上传读回一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//平移矩阵（列主序）
	float uploaded[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		5.0f, 6.0f, 7.0f, 1.0f };
	ASSERT_TRUE(renderer.resource().animation_palette_upload(uploaded, 1, 4));
	//目标条目一致
	float readback[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(readback, 1, 4));
	for (uint32_t component = 0; component < 16; ++component)
		EXPECT_FLOAT_EQ(readback[component], uploaded[component]);
	//相邻条目保持单位矩阵
	ASSERT_TRUE(renderer.resource().animation_palette_readback(readback, 1, 3));
	EXPECT_FLOAT_EQ(readback[0], 1.0f);
	EXPECT_FLOAT_EQ(readback[12], 0.0f);
	ASSERT_TRUE(renderer.resource().animation_palette_readback(readback, 1, 5));
	EXPECT_FLOAT_EQ(readback[0], 1.0f);
	EXPECT_FLOAT_EQ(readback[13], 0.0f);
}

//越界与空指针整体拒绝；零条视为成功空操作
TEST_F(Renderer_Animation_Skin_Test, 调色板边界拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const uint32_t capacity = renderer.resource().animation_palette_capacity();
	float matrix[16] = {};
	//起点越界
	EXPECT_FALSE(renderer.resource().animation_palette_upload(matrix, 1, capacity));
	EXPECT_FALSE(renderer.resource().animation_palette_readback(matrix, 1, capacity));
	//末尾越界
	EXPECT_FALSE(renderer.resource().animation_palette_upload(matrix, 2, capacity - 1));
	EXPECT_FALSE(renderer.resource().animation_palette_readback(matrix, 2, capacity - 1));
	//空指针
	EXPECT_FALSE(renderer.resource().animation_palette_upload(nullptr, 1, 0));
	EXPECT_FALSE(renderer.resource().animation_palette_readback(nullptr, 1, 0));
	//零条为空操作
	EXPECT_TRUE(renderer.resource().animation_palette_upload(nullptr, 0, 0));
	EXPECT_TRUE(renderer.resource().animation_palette_readback(nullptr, 0, 0));
}
