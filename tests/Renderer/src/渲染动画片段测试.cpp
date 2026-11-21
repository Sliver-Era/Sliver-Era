/*
渲染动画片段测试（方向十 · 阶段 B 第 4 轮）
覆盖：空片段 / 空轨道的合法性、关键帧时间严格递增、片段时长覆盖关键帧、
      轨道按骨骼名与通道查找、插值模式枚举取值。本套件只验证纯数据规则，不涉及采样与播放。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Animation/动画片段.h"

//渲染动画片段测试夹具
class Renderer_Animation_Clip_Test : public ::testing::Test
{
};

//构造一条合法平移轨道（骨骼 root，两帧）
static engine::Animation_Track translation_track(void)
{
	engine::Animation_Track track;
	track.bone = "root";
	track.channel = engine::Animation_Channel::Translation;
	track.interpolation = engine::Animation_Interpolation::Linear;
	track.keys = {
		{ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
		{ 1.0f, { 1.0f, 2.0f, 3.0f, 0.0f } },
	};
	return track;
}

//空片段不合法，关键帧最大时间为零
TEST_F(Renderer_Animation_Clip_Test, 空片段不合法)
{
	engine::Animation_Clip clip;
	EXPECT_FALSE(engine::animation_clip_valid(clip));
	EXPECT_FLOAT_EQ(engine::animation_clip_key_time_max(clip), 0.0f);
	//只有空轨道同样不合法
	clip.tracks.push_back(engine::Animation_Track{});
	EXPECT_FALSE(engine::animation_clip_valid(clip));
}

//事件合法性：负时间 / 空名 / 时间倒序均拒绝
TEST_F(Renderer_Animation_Clip_Test, 事件合法性)
{
	engine::Animation_Clip clip;
	clip.duration = 1.0f;
	clip.tracks.push_back(translation_track());
	//空事件表合法
	EXPECT_TRUE(engine::animation_events_valid(clip));
	//正常两项
	clip.events = { { 0.25f, "step" }, { 0.25f, "sound" } };
	EXPECT_TRUE(engine::animation_events_valid(clip));
	EXPECT_TRUE(engine::animation_clip_valid(clip));
	//负时间
	clip.events = { { -0.1f, "step" } };
	EXPECT_FALSE(engine::animation_events_valid(clip));
	EXPECT_FALSE(engine::animation_clip_valid(clip));
	//空名称
	clip.events = { { 0.1f, "" } };
	EXPECT_FALSE(engine::animation_events_valid(clip));
	//时间倒序
	clip.events = { { 0.5f, "a" }, { 0.2f, "b" } };
	EXPECT_FALSE(engine::animation_events_valid(clip));
}

//非循环区间收集：左闭右开
TEST_F(Renderer_Animation_Clip_Test, 事件区间收集非循环)
{
	engine::Animation_Clip clip;
	clip.duration = 1.0f;
	clip.tracks.push_back(translation_track());
	clip.events = { { 0.1f, "a" }, { 0.5f, "b" }, { 0.9f, "c" } };
	std::vector<const engine::Animation_Event*> hits;
	ASSERT_TRUE(engine::animation_events_collect(clip, 0.1f, 0.9f, false, hits));
	ASSERT_EQ(hits.size(), 2u);
	EXPECT_EQ(hits[0]->name, "a");
	EXPECT_EQ(hits[1]->name, "b");
	//空区间与逆序区间拒绝
	std::vector<const engine::Animation_Event*> none;
	EXPECT_FALSE(engine::animation_events_collect(clip, 0.5f, 0.5f, false, none));
	EXPECT_FALSE(engine::animation_events_collect(clip, 0.8f, 0.2f, false, none));
	EXPECT_TRUE(none.empty());
}

//循环区间收集：跨末尾按「尾段 → 头段」
TEST_F(Renderer_Animation_Clip_Test, 事件区间收集循环回绕)
{
	engine::Animation_Clip clip;
	clip.duration = 1.0f;
	clip.tracks.push_back(translation_track());
	clip.events = { { 0.05f, "head" }, { 0.5f, "mid" }, { 0.95f, "tail" } };
	std::vector<const engine::Animation_Event*> hits;
	//0.9 → 1.1：先 0.95（tail），再回绕到 0.05（head）
	ASSERT_TRUE(engine::animation_events_collect(clip, 0.9f, 1.1f, true, hits));
	ASSERT_EQ(hits.size(), 2u);
	EXPECT_EQ(hits[0]->name, "tail");
	EXPECT_EQ(hits[1]->name, "head");
	//区间长于整段：全部事件按序命中一次
	std::vector<const engine::Animation_Event*> all;
	ASSERT_TRUE(engine::animation_events_collect(clip, 0.0f, 2.0f, true, all));
	EXPECT_EQ(all.size(), 3u);
}

//轨道按骨骼名与通道查找，错名或错通道返回空
TEST_F(Renderer_Animation_Clip_Test, 轨道按骨骼名与通道查找)
{
	engine::Animation_Clip clip;
	clip.tracks.push_back(translation_track());
	//同骨骼再加一条旋转轨道
	engine::Animation_Track rotation;
	rotation.bone = "root";
	rotation.channel = engine::Animation_Channel::Rotation;
	rotation.keys = { { 0.0f, { 0.0f, 0.0f, 0.0f, 1.0f } } };
	clip.tracks.push_back(rotation);

	//命中平移轨道
	const engine::Animation_Track* found = engine::animation_track_find(
		clip, "root", engine::Animation_Channel::Translation);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->keys.size(), 2u);
	//命中旋转轨道
	found = engine::animation_track_find(clip, "root", engine::Animation_Channel::Rotation);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->keys.size(), 1u);
	//错骨骼名与未出现的通道均不命中
	EXPECT_EQ(engine::animation_track_find(clip, "arm", engine::Animation_Channel::Translation), nullptr);
	EXPECT_EQ(engine::animation_track_find(clip, "root", engine::Animation_Channel::Scale), nullptr);
}

//关键帧时间须严格递增：乱序、重复时间与非有限值均判非法
TEST_F(Renderer_Animation_Clip_Test, 关键帧时间严格递增)
{
	//合法轨道
	EXPECT_TRUE(engine::animation_track_valid(translation_track()));
	//乱序
	engine::Animation_Track track = translation_track();
	track.keys[0].time = 2.0f;
	EXPECT_FALSE(engine::animation_track_valid(track));
	//重复时间
	track = translation_track();
	track.keys[1].time = 0.0f;
	EXPECT_FALSE(engine::animation_track_valid(track));
	//负时间
	track = translation_track();
	track.keys[0].time = -1.0f;
	EXPECT_FALSE(engine::animation_track_valid(track));
	//非有限值
	track = translation_track();
	track.keys[1].value[2] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::animation_track_valid(track));
}

//片段时长须覆盖全部关键帧
TEST_F(Renderer_Animation_Clip_Test, 片段时长覆盖关键帧)
{
	engine::Animation_Clip clip;
	clip.name = "walk";
	clip.tracks.push_back(translation_track());
	//关键帧最大时间 1.0
	EXPECT_FLOAT_EQ(engine::animation_clip_key_time_max(clip), 1.0f);
	//时长不足
	clip.duration = 0.5f;
	EXPECT_FALSE(engine::animation_clip_valid(clip));
	//时长恰好覆盖
	clip.duration = 1.0f;
	EXPECT_TRUE(engine::animation_clip_valid(clip));
	//时长有余
	clip.duration = 2.0f;
	EXPECT_TRUE(engine::animation_clip_valid(clip));
	//非有限时长
	clip.duration = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(engine::animation_clip_valid(clip));
}

//插值模式与通道枚举取值稳定（序列化与着色器分支依赖）
TEST_F(Renderer_Animation_Clip_Test, 插值与通道枚举稳定)
{
	EXPECT_EQ(static_cast<uint32_t>(engine::Animation_Interpolation::Linear), 0u);
	EXPECT_EQ(static_cast<uint32_t>(engine::Animation_Interpolation::Step), 1u);
	EXPECT_EQ(static_cast<uint32_t>(engine::Animation_Channel::Translation), 0u);
	EXPECT_EQ(static_cast<uint32_t>(engine::Animation_Channel::Rotation), 1u);
	EXPECT_EQ(static_cast<uint32_t>(engine::Animation_Channel::Scale), 2u);
}
