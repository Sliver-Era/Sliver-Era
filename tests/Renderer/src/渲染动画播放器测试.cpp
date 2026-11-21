/*
渲染动画播放器测试（方向十 · 阶段 B 第 7 轮）
覆盖：构建与初始状态、播放推进、暂停、速度缩放与非法速度、循环 / 非循环时间归一、
      跳转与轨道采样、空片段与非法片段构建失败。本套件只验证播放状态与采样联动。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Animation/动画播放器.h"

//渲染动画播放器测试夹具
class Renderer_Animation_Player_Test : public ::testing::Test
{
};

//构造合法片段：时长 1 秒，一条平移轨道
static std::shared_ptr<engine::Animation_Clip> valid_clip(void)
{
	auto clip = std::make_shared<engine::Animation_Clip>();
	clip->name = "walk";
	clip->duration = 1.0f;
	engine::Animation_Track track;
	track.bone = "root";
	track.channel = engine::Animation_Channel::Translation;
	track.keys = {
		{ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
		{ 1.0f, { 10.0f, 20.0f, 30.0f, 0.0f } },
	};
	clip->tracks.push_back(track);
	return clip;
}

//构建成功后的初始状态
TEST_F(Renderer_Animation_Player_Test, 构建与初始状态)
{
	engine::Animation_Player player;
	ASSERT_TRUE(player.build(valid_clip()));
	EXPECT_FLOAT_EQ(player.duration_get(), 1.0f);
	EXPECT_FLOAT_EQ(player.time_get(), 0.0f);
	EXPECT_FLOAT_EQ(player.speed_get(), 1.0f);
	EXPECT_TRUE(player.loop_get());
	EXPECT_FALSE(player.playing());
	ASSERT_NE(player.clip_get(), nullptr);
}

//播放态按 delta 推进
TEST_F(Renderer_Animation_Player_Test, 播放推进)
{
	engine::Animation_Player player;
	ASSERT_TRUE(player.build(valid_clip()));
	player.play();
	EXPECT_TRUE(player.playing());
	player.time_advance(0.25f);
	EXPECT_FLOAT_EQ(player.time_get(), 0.25f);
	//暂停态推进无效
	player.pause();
	player.time_advance(0.5f);
	EXPECT_FLOAT_EQ(player.time_get(), 0.25f);
}

//速度缩放推进量，非法速度被忽略
TEST_F(Renderer_Animation_Player_Test, 速度缩放)
{
	engine::Animation_Player player;
	ASSERT_TRUE(player.build(valid_clip()));
	player.play();
	player.speed_set(2.0f);
	EXPECT_FLOAT_EQ(player.speed_get(), 2.0f);
	player.time_advance(0.25f);
	EXPECT_FLOAT_EQ(player.time_get(), 0.5f);
	//负速度与非有限速度不生效
	player.speed_set(-1.0f);
	EXPECT_FLOAT_EQ(player.speed_get(), 2.0f);
	player.speed_set(std::numeric_limits<float>::quiet_NaN());
	EXPECT_FLOAT_EQ(player.speed_get(), 2.0f);
}

//循环回绕与非循环夹紧
TEST_F(Renderer_Animation_Player_Test, 循环与非循环归一)
{
	engine::Animation_Player player;
	ASSERT_TRUE(player.build(valid_clip()));
	player.play();
	//循环：1.5 秒回绕到 0.5
	player.time_advance(1.5f);
	EXPECT_FLOAT_EQ(player.time_get(), 0.5f);
	//关闭循环：越界夹紧到时长
	player.loop_set(false);
	EXPECT_FALSE(player.loop_get());
	player.time_advance(5.0f);
	EXPECT_FLOAT_EQ(player.time_get(), 1.0f);
	//负时间夹紧到 0
	player.time_set(-3.0f);
	EXPECT_FLOAT_EQ(player.time_get(), 0.0f);
}

//跳转后按当前时间采样轨道
TEST_F(Renderer_Animation_Player_Test, 跳转与轨道采样)
{
	engine::Animation_Player player;
	ASSERT_TRUE(player.build(valid_clip()));
	player.time_set(0.5f);
	float value[4] = {};
	ASSERT_TRUE(player.track_sample("root", engine::Animation_Channel::Translation, value));
	EXPECT_FLOAT_EQ(value[0], 5.0f);
	EXPECT_FLOAT_EQ(value[1], 10.0f);
	EXPECT_FLOAT_EQ(value[2], 15.0f);
	//不存在的骨骼或通道
	EXPECT_FALSE(player.track_sample("arm", engine::Animation_Channel::Translation, value));
	EXPECT_FALSE(player.track_sample("root", engine::Animation_Channel::Scale, value));
}

//空片段与非法片段构建失败并清空状态
TEST_F(Renderer_Animation_Player_Test, 无效片段构建失败)
{
	engine::Animation_Player player;
	//空指针
	EXPECT_FALSE(player.build(nullptr));
	EXPECT_FLOAT_EQ(player.duration_get(), 0.0f);
	EXPECT_FALSE(player.playing());
	//非法片段（无轨道）
	auto invalid = std::make_shared<engine::Animation_Clip>();
	invalid->duration = 1.0f;
	EXPECT_FALSE(player.build(invalid));
	EXPECT_EQ(player.clip_get(), nullptr);
	//旧状态被清空后不可播放
	player.play();
	EXPECT_FALSE(player.playing());
}
