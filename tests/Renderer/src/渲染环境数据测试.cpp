/*
环境光照数据契约（2026-11-17 · 方向九阶段 B 第 713 轮）
覆盖：未初始化读取为零、设置与读取、非法值整次忽略、场景描述字段生效。
本文件只验证数据与门面，不验证着色效果（着色在 714 轮接入）。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	using byjy_render_test::hidden_config;

	//环境数据测试夹具
	class Renderer_Environment_Test : public ::testing::Test
	{
	};
}

//未初始化读取：颜色与强度均为 0
TEST_F(Renderer_Environment_Test, 未初始化读取为零)
{
	engine::Renderer renderer;
	float color[3] = { 1.0f, 1.0f, 1.0f };
	float intensity = 9.0f;
	renderer.scene().environment_get(color, &intensity);
	EXPECT_FLOAT_EQ(color[0], 0.0f);
	EXPECT_FLOAT_EQ(color[1], 0.0f);
	EXPECT_FLOAT_EQ(color[2], 0.0f);
	EXPECT_FLOAT_EQ(intensity, 0.0f);
}

//设置合法值后可原样读回
TEST_F(Renderer_Environment_Test, 设置与读取)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const float color[3] = { 0.2f, 0.4f, 0.6f };
	renderer.scene().environment_set(color, 1.5f);
	float out[3] = {};
	float intensity = 0.0f;
	renderer.scene().environment_get(out, &intensity);
	EXPECT_FLOAT_EQ(out[0], 0.2f);
	EXPECT_FLOAT_EQ(out[1], 0.4f);
	EXPECT_FLOAT_EQ(out[2], 0.6f);
	EXPECT_FLOAT_EQ(intensity, 1.5f);
}

//非法颜色或强度整次忽略，保留上一次合法值
TEST_F(Renderer_Environment_Test, 非法值整次忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const float valid[3] = { 0.1f, 0.2f, 0.3f };
	renderer.scene().environment_set(valid, 0.5f);
	//颜色分量越界
	const float bad_color[3] = { 0.1f, 2.0f, 0.3f };
	renderer.scene().environment_set(bad_color, 1.0f);
	//负强度
	renderer.scene().environment_set(valid, -1.0f);
	float out[3] = {};
	float intensity = 0.0f;
	renderer.scene().environment_get(out, &intensity);
	EXPECT_FLOAT_EQ(out[0], 0.1f);
	EXPECT_FLOAT_EQ(out[1], 0.2f);
	EXPECT_FLOAT_EQ(out[2], 0.3f);
	EXPECT_FLOAT_EQ(intensity, 0.5f);
}

//场景描述的 environment 字段经 scene_apply 生效
TEST_F(Renderer_Environment_Test, 场景描述字段生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Scene_Data scene{};
	scene.environment_color[0] = 0.25f;
	scene.environment_color[1] = 0.50f;
	scene.environment_color[2] = 0.75f;
	scene.environment_intensity = 0.8f;
	renderer.scene().scene_apply(scene);
	float out[3] = {};
	float intensity = 0.0f;
	renderer.scene().environment_get(out, &intensity);
	EXPECT_FLOAT_EQ(out[0], 0.25f);
	EXPECT_FLOAT_EQ(out[1], 0.50f);
	EXPECT_FLOAT_EQ(out[2], 0.75f);
	EXPECT_FLOAT_EQ(intensity, 0.8f);
}
