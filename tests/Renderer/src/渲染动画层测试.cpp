/*
渲染动画层测试（方向十 · 收敛轮 872）
覆盖：叠加层权重 0 等价基础片段、平移叠加、旋转叠加（相对绑定姿态的偏移）、
      非法权重 / 非法片段拒绝、装配层设置与清除叠加层。
口径：门面用例读回调色板数值；装配用例经关卡装载走完整链路。
*/
#include <gtest/gtest.h>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画应用.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"

using namespace byjy_render_test;

namespace
{
	//单骨骼骨架（绑定为零、逆绑定单位）
	std::shared_ptr<engine::Animation_Skeleton> single_bone(void)
	{
		std::vector<engine::Animation_Bone> bones;
		engine::Animation_Bone root;
		root.name = "root";
		bones.push_back(root);
		auto skeleton = std::make_shared<engine::Animation_Skeleton>();
		engine::Animation_Skeleton::build(bones, *skeleton);
		return skeleton;
	}

	//定长平移片段
	engine::Animation_Clip translation_clip(float value)
	{
		engine::Animation_Clip clip;
		clip.duration = 1.0f;
		engine::Animation_Track track;
		track.bone = "root";
		track.channel = engine::Animation_Channel::Translation;
		track.keys = {
			{ 0.0f, { value, 0.0f, 0.0f, 0.0f } },
			{ 1.0f, { value, 0.0f, 0.0f, 0.0f } },
		};
		clip.tracks.push_back(track);
		return clip;
	}

	//绕 Z 0 → 90° 的旋转片段
	engine::Animation_Clip rotation_clip(void)
	{
		engine::Animation_Clip clip;
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

	//写文本文件（目录自动创建）
	void write_text(const std::string& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(std::filesystem::path("out/anim_layer_test/"), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}
}

//渲染动画层测试夹具
class Renderer_Animation_Layer_Test : public ::testing::Test
{
protected:
	void TearDown(void) override
	{
		std::error_code code;
		std::filesystem::remove_all(std::filesystem::path("out/anim_layer_test/"), code);
	}
};

//权重 0：叠加结果与基础片段逐字节一致
TEST_F(Renderer_Animation_Layer_Test, 权重零等价基础片段)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const auto skeleton = single_bone();
	const engine::Animation_Clip base = translation_clip(6.0f);
	const engine::Animation_Clip overlay = translation_clip(20.0f);

	//基础片段基准
	uint32_t single = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(1, single));
	ASSERT_TRUE(renderer.animation().actor_clip_write(single, base, skeleton, 0.0f, false));
	float reference[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(reference, 1, single));
	renderer.animation().actor_destroy(single);

	//叠加权重 0
	uint32_t layered = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(1, layered));
	ASSERT_TRUE(renderer.animation().actor_layer_write(layered, base, overlay, skeleton, 0.0f, 0.0f, 0.0f, false));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, layered));
	EXPECT_EQ(std::memcmp(reference, matrix, sizeof(matrix)), 0);
	renderer.animation().actor_destroy(layered);
}

//平移叠加：base 6 ＋ (overlay 20 - 绑定 0) × 0.5 = 16
TEST_F(Renderer_Animation_Layer_Test, 平移叠加按权重)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const auto skeleton = single_bone();
	const engine::Animation_Clip base = translation_clip(6.0f);
	const engine::Animation_Clip overlay = translation_clip(20.0f);
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(1, offset));
	ASSERT_TRUE(renderer.animation().actor_layer_write(offset, base, overlay, skeleton, 0.0f, 0.0f, 0.5f, false));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, offset));
	EXPECT_FLOAT_EQ(matrix[12], 16.0f);
	//权重 1：完整叠加偏移（base 6 ＋ overlay 相对绑定的 20 = 26）
	ASSERT_TRUE(renderer.animation().actor_layer_write(offset, base, overlay, skeleton, 0.0f, 0.0f, 1.0f, false));
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, offset));
	EXPECT_FLOAT_EQ(matrix[12], 26.0f);
	renderer.animation().actor_destroy(offset);
}

//旋转叠加：base 90° ＋ overlay 90° × 0.5 = 135°
TEST_F(Renderer_Animation_Layer_Test, 旋转叠加按权重)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const auto skeleton = single_bone();
	const engine::Animation_Clip base = rotation_clip();
	const engine::Animation_Clip overlay = rotation_clip();
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(1, offset));
	ASSERT_TRUE(renderer.animation().actor_layer_write(offset, base, overlay, skeleton, 1.0f, 1.0f, 0.5f, false));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, offset));
	//135° 绕 Z：第一列 (cos135, sin135, 0)
	EXPECT_NEAR(matrix[0], -0.70710678f, 1e-4f);
	EXPECT_NEAR(matrix[1], 0.70710678f, 1e-4f);
	renderer.animation().actor_destroy(offset);
}

//非法叠加：非有限权重、非法基础 / 叠加片段
TEST_F(Renderer_Animation_Layer_Test, 非法叠加输入拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const auto skeleton = single_bone();
	const engine::Animation_Clip base = translation_clip(1.0f);
	const engine::Animation_Clip overlay = translation_clip(2.0f);
	engine::Animation_Clip invalid;
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(1, offset));
	//非有限权重
	EXPECT_FALSE(renderer.animation().actor_layer_write(offset, base, overlay, skeleton, 0.0f, 0.0f,
		std::numeric_limits<float>::quiet_NaN(), false));
	//非法叠加片段
	EXPECT_FALSE(renderer.animation().actor_layer_write(offset, base, invalid, skeleton, 0.0f, 0.0f, 0.5f, false));
	renderer.animation().actor_destroy(offset);
}

//装配层：设置后按叠加写入，清除后回到单片段
TEST_F(Renderer_Animation_Layer_Test, 装配层设置与清除)
{
	write_text("out/anim_layer_test/skeleton.json",
		R"({"bones":[{"name":"root","parent":-1}]})");
	write_text("out/anim_layer_test/base.anim",
		R"({"name":"base","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[6,0,0]},{"time":1.0,"value":[6,0,0]}]}]})");
	write_text("out/anim_layer_test/overlay.anim",
		R"({"name":"overlay","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[20,0,0]},{"time":1.0,"value":[20,0,0]}]}]})");
	write_text("out/anim_layer_test/level.txt",
		"level 层\ncamera position 0 1 3\nskeleton 甲 skeleton.json\nanim 甲 base.anim\n");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, "out/anim_layer_test/level.txt"));

	//设置 0.5 权重叠加层：6 ＋ 20 × 0.5 = 16
	std::shared_ptr<const engine::Animation_Clip> overlay;
	ASSERT_TRUE(renderer.animation().clip_load("out/anim_layer_test/overlay.anim", overlay));
	ASSERT_TRUE(engine::level_bridge_animation_layer_set(scene, "甲", overlay, 0.5f));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.0f), 1u);
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, scene.actors[0].palette_offset));
	EXPECT_FLOAT_EQ(matrix[12], 16.0f);
	//清除叠加层：回到基础片段 6
	ASSERT_TRUE(engine::level_bridge_animation_layer_set(scene, "甲", nullptr, 0.0f));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.0f), 1u);
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, scene.actors[0].palette_offset));
	EXPECT_FLOAT_EQ(matrix[12], 6.0f);
	//非法权重与未知演员
	EXPECT_FALSE(engine::level_bridge_animation_layer_set(scene, "甲", overlay, 1.5f));
	EXPECT_FALSE(engine::level_bridge_animation_layer_set(scene, "乙", overlay, 0.5f));
	engine::level_bridge_unload(renderer, scene);
}
