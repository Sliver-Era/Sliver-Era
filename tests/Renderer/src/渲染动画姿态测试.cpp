/*
渲染动画姿态测试（方向十 · 阶段 B 第 8 轮）
覆盖：骨架合法性、绑定姿态求值、父层级平移 / 缩放 / 旋转传播、局部写入与越界拒绝、
      TRS 合成顺序（先缩放、再旋转、最后平移）。本套件只验证姿态纯数据行为。
*/
#include <cmath>

#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Animation/动画姿态.h"

//渲染动画姿态测试夹具
class Renderer_Animation_Pose_Test : public ::testing::Test
{
};

//构造两根骨骼：根平移 (1,0,0)，子平移 (0,2,0)
static engine::Animation_Skeleton make_skeleton(void)
{
	engine::Animation_Skeleton skeleton;
	engine::Animation_Bone root;
	root.name = "root";
	root.parent = -1;
	root.bind_translation[0] = 1.0f;
	engine::Animation_Bone child;
	child.name = "child";
	child.parent = 0;
	child.bind_translation[1] = 2.0f;
	skeleton.bones = { root, child };
	return skeleton;
}

//骨架合法性：空骨架、父下标越位与正常骨架
TEST_F(Renderer_Animation_Pose_Test, 骨架合法性)
{
	engine::Animation_Skeleton skeleton;
	EXPECT_FALSE(skeleton.valid());
	//父下标不得大于等于自身下标
	skeleton = make_skeleton();
	skeleton.bones[1].parent = 1;
	EXPECT_FALSE(skeleton.valid());
	//正常骨架与按名查找
	skeleton = make_skeleton();
	EXPECT_TRUE(skeleton.valid());
	EXPECT_EQ(skeleton.bone_count(), 2u);
	EXPECT_EQ(skeleton.bone_seek("child"), 1);
	EXPECT_EQ(skeleton.bone_seek("missing"), -1);
	EXPECT_EQ(skeleton.bone_get(2), nullptr);
}

//绑定姿态：根与子的世界平移按层级累加
TEST_F(Renderer_Animation_Pose_Test, 绑定姿态层级平移)
{
	engine::Animation_Skeleton skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(0, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 1.0f);
	EXPECT_FLOAT_EQ(matrix[13], 0.0f);
	//子骨骼继承根平移并叠加自身平移
	ASSERT_TRUE(pose.world_get(1, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 1.0f);
	EXPECT_FLOAT_EQ(matrix[13], 2.0f);
}

//父缩放作用于子骨骼的局部平移
TEST_F(Renderer_Animation_Pose_Test, 父缩放传播)
{
	engine::Animation_Skeleton skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	//根缩放 2 倍，子平移 (0,2,0) 变为世界 (0,4,0)（根自身无平移外的偏移）
	float root_scale[3] = { 2.0f, 2.0f, 2.0f };
	float identity_translation[3] = { 0.0f, 0.0f, 0.0f };
	float identity_rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(pose.local_set(0, identity_translation, identity_rotation, root_scale));
	ASSERT_TRUE(pose.world_update(skeleton));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(1, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 0.0f);
	EXPECT_FLOAT_EQ(matrix[13], 4.0f);
}

//父旋转 90° 绕 Z 后，子的局部 (1,0,0) 落到世界 (0,1,0)
TEST_F(Renderer_Animation_Pose_Test, 父旋转传播)
{
	engine::Animation_Skeleton skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	float identity_translation[3] = { 0.0f, 0.0f, 0.0f };
	float identity_scale[3] = { 1.0f, 1.0f, 1.0f };
	float identity_rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float rotate_z90[4] = { 0.0f, 0.0f, 0.70710678f, 0.70710678f };
	ASSERT_TRUE(pose.local_set(0, identity_translation, rotate_z90, identity_scale));
	float child_translation[3] = { 1.0f, 0.0f, 0.0f };
	ASSERT_TRUE(pose.local_set(1, child_translation, identity_rotation, identity_scale));
	ASSERT_TRUE(pose.world_update(skeleton));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(1, matrix));
	EXPECT_NEAR(matrix[12], 0.0f, 1e-5f);
	EXPECT_NEAR(matrix[13], 1.0f, 1e-5f);
}

//局部写入：合法写入生效，越界 / 非有限值 / 尺寸不符被拒绝
TEST_F(Renderer_Animation_Pose_Test, 局部写入与越界拒绝)
{
	engine::Animation_Skeleton skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	float translation[3] = { 0.0f, 5.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(pose.local_set(1, translation, rotation, scale));
	ASSERT_TRUE(pose.world_update(skeleton));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(1, matrix));
	//局部写入替换绑定平移（根的 Y 平移为 0）
	EXPECT_FLOAT_EQ(matrix[13], 5.0f);
	//越界骨骼
	EXPECT_FALSE(pose.local_set(2, translation, rotation, scale));
	//非有限值
	translation[0] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(pose.local_set(0, translation, rotation, scale));
	//姿态尺寸与骨架不符
	pose.translation.pop_back();
	EXPECT_FALSE(pose.world_update(skeleton));
}

//TRS 顺序：先缩放、再旋转、最后平移（平移不被旋转带偏）
TEST_F(Renderer_Animation_Pose_Test, TRS合成顺序)
{
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotate_z90[4] = { 0.0f, 0.0f, 0.70710678f, 0.70710678f };
	float scale[3] = { 2.0f, 2.0f, 2.0f };
	float matrix[16] = {};
	engine::animation_trs_compose(translation, rotate_z90, scale, matrix);
	//平移分量保持原值
	EXPECT_NEAR(matrix[12], 1.0f, 1e-5f);
	EXPECT_NEAR(matrix[13], 0.0f, 1e-5f);
	//90° 绕 Z 的旋转基向量按 2 倍缩放
	EXPECT_NEAR(matrix[0], 0.0f, 1e-5f);
	EXPECT_NEAR(matrix[1], 2.0f, 1e-5f);
}
