/*
渲染动画采样测试（方向十 · 阶段 B 第 6 轮）
覆盖：线性中点、阶跃保持、越界夹紧、循环回绕、单帧片段与空轨道边界、旋转四元数归一化。
本套件只验证采样器纯数据行为，不涉及播放器时间推进。
*/
#include <cmath>

#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Animation/动画采样.h"

//渲染动画采样测试夹具
class Renderer_Animation_Sample_Test : public ::testing::Test
{
};

//构造线性平移轨道：两帧 0 / 1 秒
static engine::Animation_Track linear_track(void)
{
	engine::Animation_Track track;
	track.bone = "root";
	track.channel = engine::Animation_Channel::Translation;
	track.interpolation = engine::Animation_Interpolation::Linear;
	track.keys = {
		{ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
		{ 1.0f, { 10.0f, 20.0f, 30.0f, 0.0f } },
	};
	return track;
}

//线性插值：中点取两帧均值
TEST_F(Renderer_Animation_Sample_Test, 线性中点插值)
{
	float value[4] = {};
	ASSERT_TRUE(engine::animation_track_sample(linear_track(), 0.5f, 1.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 5.0f);
	EXPECT_FLOAT_EQ(value[1], 10.0f);
	EXPECT_FLOAT_EQ(value[2], 15.0f);
	//时间恰为首帧
	ASSERT_TRUE(engine::animation_track_sample(linear_track(), 0.0f, 1.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 0.0f);
}

//阶跃插值：关键帧之间保持左帧值
TEST_F(Renderer_Animation_Sample_Test, 阶跃插值保持左帧)
{
	engine::Animation_Track track = linear_track();
	track.interpolation = engine::Animation_Interpolation::Step;
	float value[4] = {};
	ASSERT_TRUE(engine::animation_track_sample(track, 0.99f, 1.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 0.0f);
	//到达下一帧时间取新值
	ASSERT_TRUE(engine::animation_track_sample(track, 1.0f, 1.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 10.0f);
}

//越界时间按非循环规则夹紧
TEST_F(Renderer_Animation_Sample_Test, 越界时间夹紧)
{
	float value[4] = {};
	//早于首帧
	ASSERT_TRUE(engine::animation_track_sample(linear_track(), -5.0f, 1.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 0.0f);
	EXPECT_FLOAT_EQ(value[2], 0.0f);
	//晚于末帧
	ASSERT_TRUE(engine::animation_track_sample(linear_track(), 10.0f, 1.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 10.0f);
	EXPECT_FLOAT_EQ(value[2], 30.0f);
}

//循环时间按片段时长回绕
TEST_F(Renderer_Animation_Sample_Test, 循环回绕)
{
	//时长 2 秒，2.5 秒回绕到 0.5 秒
	EXPECT_FLOAT_EQ(engine::animation_time_normalize(2.5f, 2.0f, true), 0.5f);
	//负时间回绕到正区间
	EXPECT_FLOAT_EQ(engine::animation_time_normalize(-0.5f, 2.0f, true), 1.5f);
	//回绕后采样等于 0.5 秒处的线性中点
	float value[4] = {};
	ASSERT_TRUE(engine::animation_track_sample(linear_track(), 2.5f, 2.0f, true, value));
	EXPECT_FLOAT_EQ(value[1], 10.0f);
}

//单帧片段恒定取该帧；空轨道返回失败
TEST_F(Renderer_Animation_Sample_Test, 单帧与空轨道边界)
{
	engine::Animation_Track track;
	track.bone = "root";
	track.keys = { { 0.0f, { 0.7f, 1.2f, 1.6f, 0.0f } } };
	float value[4] = {};
	ASSERT_TRUE(engine::animation_track_sample(track, 3.0f, 5.0f, false, value));
	EXPECT_FLOAT_EQ(value[0], 0.7f);
	EXPECT_FLOAT_EQ(value[1], 1.2f);
	EXPECT_FLOAT_EQ(value[2], 1.6f);
	//空轨道
	track.keys.clear();
	EXPECT_FALSE(engine::animation_track_sample(track, 0.0f, 1.0f, false, value));
	//非法时长
	EXPECT_FALSE(engine::animation_track_sample(linear_track(), 0.0f, 0.0f, false, value));
}

//旋转通道线性插值后归一化（nlerp）
TEST_F(Renderer_Animation_Sample_Test, 旋转插值归一化)
{
	engine::Animation_Track track;
	track.bone = "root";
	track.channel = engine::Animation_Channel::Rotation;
	track.interpolation = engine::Animation_Interpolation::Linear;
	track.keys = {
		{ 0.0f, { 0.0f, 0.0f, 0.0f, 1.0f } },
		{ 1.0f, { 0.0f, 0.0f, 1.0f, 0.0f } },
	};
	float value[4] = {};
	ASSERT_TRUE(engine::animation_track_sample(track, 0.5f, 1.0f, false, value));
	//插值后长度须为 1
	const float length = std::sqrt(value[0] * value[0] + value[1] * value[1]
		+ value[2] * value[2] + value[3] * value[3]);
	EXPECT_NEAR(length, 1.0f, 1e-5f);
	//两分量近似相等
	EXPECT_NEAR(value[2], value[3], 1e-5f);
}
