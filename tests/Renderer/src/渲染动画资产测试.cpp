/*
渲染动画资产测试（方向十 · 阶段 B 第 5 轮）
覆盖：.anim JSON 构建合法片段、字段类型与通道 / 插值白名单、关键帧时间与时长规则、
      文件加载成功路径与失败清空。本套件只验证资产层，不涉及采样与播放。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Animation/动画片段.h"

//渲染动画资产测试夹具
class Renderer_Animation_Asset_Test : public ::testing::Test
{
};

//合法配置：名 / 时长 / 两条轨道
static nlohmann::json valid_config(void)
{
	nlohmann::json config;
	config["name"] = "walk";
	config["duration"] = 1.0;
	nlohmann::json track;
	track["bone"] = "root";
	track["channel"] = "translation";
	track["interpolation"] = "linear";
	track["keys"] = nlohmann::json::array({
		{ { "time", 0.0 }, { "value", { 0.0, 0.0, 0.0 } } },
		{ { "time", 1.0 }, { "value", { 1.0, 2.0, 3.0 } } },
	});
	nlohmann::json rotation;
	rotation["bone"] = "root";
	rotation["channel"] = "rotation";
	rotation["keys"] = nlohmann::json::array({
		{ { "time", 0.0 }, { "value", { 0.0, 0.0, 0.0, 1.0 } } },
	});
	config["tracks"] = nlohmann::json::array({ track, rotation });
	return config;
}

//合法配置构建成功，字段与轨道查找一致
TEST_F(Renderer_Animation_Asset_Test, 合法配置构建片段)
{
	engine::Animation_Clip clip;
	ASSERT_TRUE(engine::animation_clip_build(valid_config(), clip));
	EXPECT_EQ(clip.name, "walk");
	EXPECT_FLOAT_EQ(clip.duration, 1.0f);
	ASSERT_EQ(clip.tracks.size(), 2u);
	//平移轨道两帧
	const engine::Animation_Track* track = engine::animation_track_find(
		clip, "root", engine::Animation_Channel::Translation);
	ASSERT_NE(track, nullptr);
	EXPECT_EQ(track->interpolation, engine::Animation_Interpolation::Linear);
	ASSERT_EQ(track->keys.size(), 2u);
	EXPECT_FLOAT_EQ(track->keys[1].value[2], 3.0f);
	//旋转轨道缺省插值为线性
	track = engine::animation_track_find(clip, "root", engine::Animation_Channel::Rotation);
	ASSERT_NE(track, nullptr);
	EXPECT_EQ(track->interpolation, engine::Animation_Interpolation::Linear);
	EXPECT_FLOAT_EQ(track->keys[0].value[3], 1.0f);
}

//关键帧时间乱序整体拒绝且输出清空
TEST_F(Renderer_Animation_Asset_Test, 时间乱序被拒绝)
{
	nlohmann::json config = valid_config();
	config["tracks"][0]["keys"][0]["time"] = 2.0;
	engine::Animation_Clip clip;
	//先放入一个旧片段，验证失败时被清空
	clip.name = "stale";
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	EXPECT_TRUE(clip.tracks.empty());
	EXPECT_TRUE(clip.name.empty());
}

//时长不足或非有限整体拒绝
TEST_F(Renderer_Animation_Asset_Test, 时长非法被拒绝)
{
	//时长小于关键帧最大时间
	nlohmann::json config = valid_config();
	config["duration"] = 0.5;
	engine::Animation_Clip clip;
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//非有限时长
	config = valid_config();
	config["duration"] = std::numeric_limits<double>::infinity();
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//缺时长
	config = valid_config();
	config.erase("duration");
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
}

//未知通道、未知插值与空骨骼名被拒绝
TEST_F(Renderer_Animation_Asset_Test, 通道与插值白名单)
{
	nlohmann::json config = valid_config();
	config["tracks"][0]["channel"] = "bend";
	engine::Animation_Clip clip;
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//未知插值
	config = valid_config();
	config["tracks"][0]["interpolation"] = "cubic";
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//空骨骼名
	config = valid_config();
	config["tracks"][0]["bone"] = "";
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//缺轨道数组
	config = valid_config();
	config.erase("tracks");
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
}

//关键帧值分量数量与类型非法被拒绝
TEST_F(Renderer_Animation_Asset_Test, 关键帧值非法被拒绝)
{
	//仅两个分量
	nlohmann::json config = valid_config();
	config["tracks"][0]["keys"][0]["value"] = nlohmann::json::array({ 0.0, 0.0 });
	engine::Animation_Clip clip;
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//分量非数值
	config = valid_config();
	config["tracks"][0]["keys"][0]["value"] = nlohmann::json::array({ 0.0, "x", 0.0 });
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
	//缺关键帧时间
	config = valid_config();
	config["tracks"][0]["keys"][0].erase("time");
	EXPECT_FALSE(engine::animation_clip_build(config, clip));
}

//文件加载：合法 .anim 成功，缺失路径失败且输出清空
TEST_F(Renderer_Animation_Asset_Test, 文件加载成功与失败清空)
{
	//写临时片段文件（相对可执行文件工作目录）
	const std::string file_path = "anim_asset_temp.anim";
	{
		std::ofstream file(file_path, std::ios::binary);
		ASSERT_TRUE(file.is_open());
		file << valid_config().dump();
	}
	engine::Animation_Clip clip;
	ASSERT_TRUE(engine::animation_clip_load_file(file_path, clip));
	EXPECT_EQ(clip.name, "walk");
	EXPECT_NE(engine::animation_track_find(clip, "root", engine::Animation_Channel::Translation), nullptr);
	//清理临时文件
	std::error_code error_info;
	std::filesystem::remove(file_path, error_info);

	//缺失路径：失败并清空旧内容
	clip.name = "stale";
	EXPECT_FALSE(engine::animation_clip_load_file("anim_missing_temp.anim", clip));
	EXPECT_TRUE(clip.tracks.empty());
	EXPECT_TRUE(clip.name.empty());
}
