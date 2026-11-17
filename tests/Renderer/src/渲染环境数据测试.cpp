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
	using byjy_render_test::pixel_bytes;
	using byjy_render_test::pixel_get;
	using byjy_render_test::center_brightness;
	using byjy_render_test::changed_pixels;
	using byjy_render_test::solid_gray;
	using byjy_render_test::cube_data;

	//环境数据测试夹具
	class Renderer_Environment_Test : public ::testing::Test
	{
	};

	//环境着色场景：中性灰立方体 ＋ 正对相机的方向光
	void environment_scene_register(engine::Renderer& renderer, engine::Mesh_Handle& mesh, uint64_t& texture)
	{
		std::vector<uint8_t> texels = solid_gray();
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(1.0f)));
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		//光强压低、关闭高光，避免中心饱和而掩盖环境增量
		renderer.scene().light_set(direction, color, 0.3f, 0.0f);
		renderer.scene().specular_set(0.0f, 16.0f);
	}

	//画一帧并回读
	std::vector<uint8_t> environment_frame(engine::Renderer& renderer, engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	}

	//六面同色立方体贴图（1×1 × 6 面）
	std::vector<uint8_t> environment_cube(uint8_t red, uint8_t green, uint8_t blue)
	{
		std::vector<uint8_t> faces(6 * 4);
		for (std::size_t index = 0; index < faces.size(); index += 4)
		{
			faces[index] = red;
			faces[index + 1] = green;
			faces[index + 2] = blue;
			faces[index + 3] = 255;
		}
		return faces;
	}
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

//环境关闭（默认 0）：连续两帧逐字节一致
TEST_F(Renderer_Environment_Test, 环境关闭默认帧稳定)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	environment_scene_register(renderer, mesh, texture);
	std::vector<uint8_t> first = environment_frame(renderer, mesh);
	std::vector<uint8_t> second = environment_frame(renderer, mesh);
	EXPECT_EQ(first, second) << "环境关闭态两帧不一致";
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//环境开启：画面可区分且中心亮度不下降
TEST_F(Renderer_Environment_Test, 环境开启提亮画面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	environment_scene_register(renderer, mesh, texture);
	//环境关闭基线
	renderer.scene().environment_set(nullptr, 0.0f);
	std::vector<uint8_t> baseline = environment_frame(renderer, mesh);
	//环境开启（中性灰环境）
	const float environment[3] = { 0.5f, 0.5f, 0.5f };
	renderer.scene().environment_set(environment, 1.0f);
	std::vector<uint8_t> lit = environment_frame(renderer, mesh);
	EXPECT_GT(changed_pixels(baseline, lit, 6), 0u) << "环境光照未改变画面";
	EXPECT_GE(center_brightness(lit), center_brightness(baseline)) << "环境光照使中心变暗";
	RecordProperty("environment_baseline_center", static_cast<int>(center_brightness(baseline)));
	RecordProperty("environment_lit_center", static_cast<int>(center_brightness(lit)));
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//环境立方图采样：天空盒开启时环境颜色随贴图变化，关闭时退回常量
TEST_F(Renderer_Environment_Test, 环境立方图采样生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	environment_scene_register(renderer, mesh, texture);
	//环境白色 tint、强度 1；天空盒自身 IBL 强度归零，避免混入
	const float tint[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().environment_set(tint, 1.0f);
	renderer.resource().sky_params_set(0.0f, 0.0f);
	renderer.resource().sky_enable_set(true);
	//蓝色立方体
	uint64_t blue_cube = 0;
	std::vector<uint8_t> blue_faces = environment_cube(0, 0, 255);
	ASSERT_TRUE(renderer.resource().texture_cube_load(blue_cube, blue_faces.data(), 1, 1));
	renderer.resource().sky_texture_set(blue_cube);
	std::vector<uint8_t> blue_frame = environment_frame(renderer, mesh);
	std::array<uint8_t, 4> blue_center = pixel_get(blue_frame, 32, 18);
	//红色立方体
	uint64_t red_cube = 0;
	std::vector<uint8_t> red_faces = environment_cube(255, 0, 0);
	ASSERT_TRUE(renderer.resource().texture_cube_load(red_cube, red_faces.data(), 1, 1));
	renderer.resource().sky_texture_set(red_cube);
	std::vector<uint8_t> red_frame = environment_frame(renderer, mesh);
	std::array<uint8_t, 4> red_center = pixel_get(red_frame, 32, 18);
	EXPECT_NE(blue_center, red_center) << "环境立方图未参与着色";
	//关闭天空盒：环境退回常量（白色 tint），中心应与蓝色贴图不同
	renderer.resource().sky_enable_set(false);
	std::vector<uint8_t> constant_frame = environment_frame(renderer, mesh);
	std::array<uint8_t, 4> constant_center = pixel_get(constant_frame, 32, 18);
	EXPECT_NE(blue_center, constant_center) << "关闭天空盒后环境未退回常量";
	RecordProperty("environment_cube_blue", static_cast<int>(center_brightness(blue_frame)));
	RecordProperty("environment_cube_red", static_cast<int>(center_brightness(red_frame)));
	RecordProperty("environment_cube_constant", static_cast<int>(center_brightness(constant_frame)));
	renderer.resource().texture_unload(red_cube);
	renderer.resource().texture_unload(blue_cube);
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
