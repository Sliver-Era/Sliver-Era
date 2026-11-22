/*
渲染动画门面测试（方向十 · 收敛轮 861）
覆盖：片段应用（采样 → 姿态）、演员区间分配 / 回收、按姿态与按片段写调色板、
      非法输入拒绝、.anim 文件加载。像素无关，聚焦门面对外行为。
*/
#include <gtest/gtest.h>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画数据.h"
#include "Engine/EngineRenderer/src/render/Animation/动画应用.h"

using namespace byjy_render_test;

//渲染动画门面测试夹具
class Renderer_Animation_Facade_Test : public ::testing::Test
{
};

//两根骨骼：根绑定 T(1,2,3) / 逆 T(-1,-2,-3)，子绑定 T(0,1,0) / 逆 T(0,-1,0)
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

//构建片段：根平移 0 → 10，时长 1 秒
static engine::Animation_Clip make_clip(void)
{
	engine::Animation_Clip clip;
	clip.name = "walk";
	clip.duration = 1.0f;
	engine::Animation_Track track;
	track.bone = "root";
	track.channel = engine::Animation_Channel::Translation;
	track.interpolation = engine::Animation_Interpolation::Linear;
	track.keys = {
		{ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
		{ 1.0f, { 10.0f, 0.0f, 0.0f, 0.0f } },
	};
	clip.tracks.push_back(track);
	return clip;
}

//片段应用：中点采样覆盖平移，缺失通道保留绑定值
TEST_F(Renderer_Animation_Facade_Test, 片段应用生成姿态)
{
	auto skeleton = make_skeleton();
	engine::Animation_Clip clip = make_clip();
	engine::Animation_Pose pose;
	ASSERT_TRUE(engine::animation_clip_pose_build(clip, skeleton, 0.5f, false, pose));
	float matrix[16] = {};
	ASSERT_TRUE(pose.world_get(0, matrix));
	//根平移被片段覆盖为 5
	EXPECT_FLOAT_EQ(matrix[12], 5.0f);
	//根旋转保持绑定（单位）
	EXPECT_FLOAT_EQ(matrix[0], 1.0f);
	EXPECT_FLOAT_EQ(matrix[5], 1.0f);
	//子骨骼世界平移 = 根 5 ＋ 子 1
	ASSERT_TRUE(pose.world_get(1, matrix));
	EXPECT_FLOAT_EQ(matrix[12], 5.0f);
	EXPECT_FLOAT_EQ(matrix[13], 1.0f);
}

//门面演员分配与按姿态写调色板
TEST_F(Renderer_Animation_Facade_Test, 演员分配与姿态写入)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(2, offset));
	EXPECT_EQ(offset, 0u);

	auto skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	ASSERT_TRUE(renderer.animation().actor_palette_write(offset, skeleton, pose));

	//绑定姿态下根调色板为单位阵
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, offset));
	EXPECT_FLOAT_EQ(matrix[0], 1.0f);
	EXPECT_FLOAT_EQ(matrix[12], 0.0f);
	//回收后不可重复回收
	ASSERT_TRUE(renderer.animation().actor_destroy(offset));
	EXPECT_FALSE(renderer.animation().actor_destroy(offset));
}

//门面按片段时间写入：调色板为 world × inverse_bind
TEST_F(Renderer_Animation_Facade_Test, 按片段写入调色板)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(2, offset));

	auto skeleton = make_skeleton();
	engine::Animation_Clip clip = make_clip();
	//0.5 秒：根世界平移 5，逆绑定 -1 后调色板平移 4 / -2 / -3
	ASSERT_TRUE(renderer.animation().actor_clip_write(offset, clip, skeleton, 0.5f, false));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, offset));
	EXPECT_FLOAT_EQ(matrix[12], 4.0f);
	EXPECT_FLOAT_EQ(matrix[13], -2.0f);
	EXPECT_FLOAT_EQ(matrix[14], -3.0f);
	renderer.animation().actor_destroy(offset);
}

//门面非法输入：骨骼数、未知偏移、骨骼数不符、非法片段
TEST_F(Renderer_Animation_Facade_Test, 门面非法输入拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint32_t offset = 0;
	//零骨骼与超上限
	EXPECT_FALSE(renderer.animation().actor_create(0, offset));
	EXPECT_FALSE(renderer.animation().actor_create(engine::ANIMATION_ACTOR_BONES_MAX + 1, offset));
	//未知偏移写入 / 回收
	ASSERT_TRUE(renderer.animation().actor_create(2, offset));
	EXPECT_FALSE(renderer.animation().actor_destroy(offset + 1));
	auto skeleton = make_skeleton();
	engine::Animation_Pose pose;
	ASSERT_TRUE(pose.build(skeleton));
	//骨骼数与登记不符（用另一骨架 1 根）
	std::vector<engine::Animation_Bone> single_bones;
	engine::Animation_Bone only;
	only.name = "root";
	single_bones.push_back(only);
	auto single = std::make_shared<engine::Animation_Skeleton>();
	ASSERT_TRUE(engine::Animation_Skeleton::build(single_bones, *single));
	engine::Animation_Pose single_pose;
	ASSERT_TRUE(single_pose.build(single));
	EXPECT_FALSE(renderer.animation().actor_palette_write(offset, single, single_pose));
	//非法片段
	engine::Animation_Clip invalid;
	EXPECT_FALSE(renderer.animation().actor_clip_write(offset, invalid, skeleton, 0.0f, false));
	renderer.animation().actor_destroy(offset);
}

//片段文件加载：合法文件成功，缺失路径失败并清空
TEST_F(Renderer_Animation_Facade_Test, 片段文件加载)
{
	//写临时 .anim 文件
	const std::string file_path = "anim_facade_temp.anim";
	{
		std::ofstream file(file_path, std::ios::binary);
		ASSERT_TRUE(file.is_open());
		file << R"({"name":"walk","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[0,0,0]},{"time":1.0,"value":[10,0,0]}]}]})";
	}
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	std::shared_ptr<const engine::Animation_Clip> clip;
	ASSERT_TRUE(renderer.animation().clip_load(file_path, clip));
	ASSERT_NE(clip, nullptr);
	EXPECT_EQ(clip->name, "walk");
	//缺失路径：失败并清空
	std::shared_ptr<const engine::Animation_Clip> missing = clip;
	EXPECT_FALSE(renderer.animation().clip_load("anim_facade_missing.anim", missing));
	EXPECT_EQ(missing, nullptr);
	//清理临时文件
	std::error_code error_info;
	std::filesystem::remove(file_path, error_info);
}
