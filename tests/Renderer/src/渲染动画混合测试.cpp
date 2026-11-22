/*
渲染动画混合测试（方向十 · 收敛轮 864）
覆盖：双片段姿态混合（权重 0 / 1 退化、中间值线性、旋转点积取反与归一化）、
      非法权重 / 非法片段拒绝、门面按混合姿态写调色板。
口径：纯数据层断言姿态与世界矩阵；门面用例走资源域调色板读回。
*/
#include <gtest/gtest.h>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画应用.h"

using namespace byjy_render_test;

namespace
{
	//单骨骼骨架（根绑定为零、逆绑定单位）
	std::shared_ptr<engine::Animation_Skeleton> single_bone_skeleton(void)
	{
		std::vector<engine::Animation_Bone> bones;
		engine::Animation_Bone root;
		root.name = "root";
		root.parent = -1;
		bones.push_back(root);
		auto skeleton = std::make_shared<engine::Animation_Skeleton>();
		engine::Animation_Skeleton::build(bones, *skeleton);
		return skeleton;
	}

	//返回片段：根平移 from → to（时长 1 秒）
	engine::Animation_Clip translation_clip(float from, float to)
	{
		engine::Animation_Clip clip;
		clip.name = "move";
		clip.duration = 1.0f;
		engine::Animation_Track track;
		track.bone = "root";
		track.channel = engine::Animation_Channel::Translation;
		track.keys = {
			{ 0.0f, { from, 0.0f, 0.0f, 0.0f } },
			{ 1.0f, { to, 0.0f, 0.0f, 0.0f } },
		};
		clip.tracks.push_back(track);
		return clip;
	}

	//返回片段：根绕 Z 旋转 0 → 90°（时长 1 秒）
	engine::Animation_Clip rotation_clip(void)
	{
		engine::Animation_Clip clip;
		clip.name = "turn";
		clip.duration = 1.0f;
		engine::Animation_Track track;
		track.bone = "root";
		track.channel = engine::Animation_Channel::Rotation;
		const float half = 0.70710678f;
		track.keys = {
			{ 0.0f, { 0.0f, 0.0f, 0.0f, 1.0f } },
			{ 1.0f, { 0.0f, 0.0f, half, half } },
		};
		clip.tracks.push_back(track);
		return clip;
	}
}

//渲染动画混合测试夹具
class Renderer_Animation_Blend_Test : public ::testing::Test
{
};

//权重 0 / 1 分别退化为两份源姿态
TEST_F(Renderer_Animation_Blend_Test, 权重端点退化为源姿态)
{
	auto skeleton = single_bone_skeleton();
	const engine::Animation_Clip first = translation_clip(0.0f, 10.0f);
	const engine::Animation_Clip second = translation_clip(20.0f, 30.0f);
	engine::Animation_Pose pose;
	//权重 0：0.5 秒处根平移 5
	ASSERT_TRUE(engine::animation_clip_pose_blend(first, second, skeleton, 0.5f, 0.5f, 0.0f, false, false, pose));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(0, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 5.0f);
	//权重 1：根平移 25
	ASSERT_TRUE(engine::animation_clip_pose_blend(first, second, skeleton, 0.5f, 0.5f, 1.0f, false, false, pose));
	ASSERT_TRUE(pose.world_get(0, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 25.0f);
}

//权重 0.5：平移线性取中，越界权重夹紧
TEST_F(Renderer_Animation_Blend_Test, 中间权重线性混合与夹紧)
{
	auto skeleton = single_bone_skeleton();
	const engine::Animation_Clip first = translation_clip(0.0f, 10.0f);
	const engine::Animation_Clip second = translation_clip(20.0f, 30.0f);
	engine::Animation_Pose pose;
	float matrix[16] = {};
	//中点：5 与 25 之间取 15
	ASSERT_TRUE(engine::animation_clip_pose_blend(first, second, skeleton, 0.5f, 0.5f, 0.5f, false, false, pose));
	ASSERT_TRUE(pose.world_get(0, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 15.0f);
	//负权重夹紧到 0
	ASSERT_TRUE(engine::animation_clip_pose_blend(first, second, skeleton, 0.5f, 0.5f, -2.0f, false, false, pose));
	ASSERT_TRUE(pose.world_get(0, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 5.0f);
	//大于 1 的权重夹紧到 1
	ASSERT_TRUE(engine::animation_clip_pose_blend(first, second, skeleton, 0.5f, 0.5f, 3.0f, false, false, pose));
	ASSERT_TRUE(pose.world_get(0, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 25.0f);
}

//旋转混合：单位旋转与 90° 之间取 45°
TEST_F(Renderer_Animation_Blend_Test, 旋转混合取短弧中点)
{
	auto skeleton = single_bone_skeleton();
	const engine::Animation_Clip still = translation_clip(0.0f, 0.0f);
	const engine::Animation_Clip turn = rotation_clip();
	engine::Animation_Pose pose;
	ASSERT_TRUE(engine::animation_clip_pose_blend(still, turn, skeleton, 1.0f, 1.0f, 0.5f, false, false, pose));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(0, matrix));
	//45° 绕 Z：列主序第一列为 (cos45, sin45, 0)
	EXPECT_NEAR(matrix[0], 0.70710678f, 1e-4f);
	EXPECT_NEAR(matrix[1], 0.70710678f, 1e-4f);
	EXPECT_NEAR(matrix[4], -0.70710678f, 1e-4f);
	EXPECT_NEAR(matrix[5], 0.70710678f, 1e-4f);
}

//非法输入：非有限权重、非法片段、非法骨架
TEST_F(Renderer_Animation_Blend_Test, 非法输入拒绝)
{
	auto skeleton = single_bone_skeleton();
	const engine::Animation_Clip valid = translation_clip(0.0f, 10.0f);
	engine::Animation_Clip invalid;
	engine::Animation_Pose pose;
	//非有限权重
	EXPECT_FALSE(engine::animation_clip_pose_blend(valid, valid, skeleton, 0.0f, 0.0f,
		std::numeric_limits<float>::quiet_NaN(), false, false, pose));
	//非法源片段
	EXPECT_FALSE(engine::animation_clip_pose_blend(invalid, valid, skeleton, 0.0f, 0.0f, 0.5f, false, false, pose));
	//空骨架
	auto empty = std::make_shared<engine::Animation_Skeleton>();
	EXPECT_FALSE(engine::animation_clip_pose_blend(valid, valid, empty, 0.0f, 0.0f, 0.5f, false, false, pose));
}

//门面：混合姿态写入演员调色板并可读回
TEST_F(Renderer_Animation_Blend_Test, 门面混合写入调色板)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(1, offset));

	auto skeleton = single_bone_skeleton();
	const engine::Animation_Clip first = translation_clip(0.0f, 10.0f);
	const engine::Animation_Clip second = translation_clip(20.0f, 30.0f);
	//权重 0.5：根世界平移 15，逆绑定单位 → 调色板平移 15
	ASSERT_TRUE(renderer.animation().actor_blend_write(offset, first, second, skeleton, 0.5f, 0.5f, 0.5f, false));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, offset));
	EXPECT_FLOAT_EQ(matrix[12], 15.0f);
	//非法片段拒绝
	engine::Animation_Clip invalid;
	EXPECT_FALSE(renderer.animation().actor_blend_write(offset, invalid, second, skeleton, 0.0f, 0.0f, 0.5f, false));
	renderer.animation().actor_destroy(offset);
}
