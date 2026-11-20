/*
渲染场景环境光测试（2026-11-21 自「渲染场景效果测试.cpp」拆出）
覆盖：环境光颜色着色、非法环境光颜色忽略、场景描述携带 / 非法环境光颜色、半球环境光上下异色。
*/
#include "渲染场景测试辅助.h"
#include "测试辅助/渲染测试辅助.h"



TEST_F(Renderer_Scene_Test, 环境光颜色着色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.resource().texture_bind_set(texture);
	//方向光朝 -Z + 环境光 0.5：背光面只吃环境光
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(forward, white_light, 1.0f, 0.5f);

	//渲染一帧并返回中心像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_center = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	};

	std::array<uint8_t, 4> plain = render_center();
	//纯红环境光
	const float red[3] = { 1.0f, 0.0f, 0.0f };
	renderer.scene().ambient_color_set(red);
	std::array<uint8_t, 4> tinted = render_center();
	RecordProperty("ambient_plain", pixel_text(plain));
	RecordProperty("ambient_red", pixel_text(tinted));
	//默认白：三通道接近相等
	EXPECT_LE(std::abs(static_cast<int>(plain[0]) - static_cast<int>(plain[1])), 3) << "默认环境光非中性";
	//纯红：外通道（R 与 B 的较大者）保留、另一外通道与绿通道接近 0（通道序无关）
	uint8_t outer = outer_max(tinted);
	uint8_t inner = tinted[0] < tinted[2] ? tinted[0] : tinted[2];
	EXPECT_GE(outer, 50u) << "红色环境光未保留外通道（" << pixel_text(tinted) << "）";
	EXPECT_LE(inner, 30u) << "另一外通道未被压暗（" << pixel_text(tinted) << "）";
	EXPECT_LE(tinted[1], 30u) << "绿色通道未被压暗（" << pixel_text(tinted) << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
非法环境光颜色被忽略（第四十轮）
口径：分量越界（1.5）整次忽略；画面与调用前逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 非法环境光颜色被忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.resource().texture_bind_set(texture);
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(forward, white_light, 1.0f, 0.5f);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	std::vector<uint8_t> baseline = render_frame();
	const float invalid[3] = { 1.5f, 0.0f, 0.0f };
	renderer.scene().ambient_color_set(invalid);
	EXPECT_EQ(render_frame(), baseline) << "非法环境光颜色改变了画面";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
场景描述携带环境光颜色（第四十一轮）
口径：中灰纹理 ＋ 背光面 ＋ 环境光 0.5；同一 Scene_Data 把 ambient_color 由白改红：
      外通道保留、另一外通道与绿通道接近 0（通道序无关）。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带环境光颜色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回中心像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_center = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	};

	//场景：方向光朝 -Z ＋ 环境光 0.5（背光面只吃环境光）
	engine::Scene_Data scene{};
	scene.light_direction[0] = 0.0f;
	scene.light_direction[1] = 0.0f;
	scene.light_direction[2] = -1.0f;
	scene.light_intensity = 1.0f;
	scene.light_ambient = 0.5f;
	renderer.scene().scene_apply(scene);
	std::array<uint8_t, 4> plain = render_center();
	//同一场景改红色环境光
	scene.ambient_color[0] = 1.0f;
	scene.ambient_color[1] = 0.0f;
	scene.ambient_color[2] = 0.0f;
	renderer.scene().scene_apply(scene);
	std::array<uint8_t, 4> tinted = render_center();
	RecordProperty("scene_ambient_plain", pixel_text(plain));
	RecordProperty("scene_ambient_red", pixel_text(tinted));
	//默认白：三通道接近相等
	EXPECT_LE(std::abs(static_cast<int>(plain[0]) - static_cast<int>(plain[1])), 3) << "默认环境光非中性";
	//红色：外通道保留、另一外通道与绿通道接近 0
	uint8_t outer = outer_max(tinted);
	uint8_t inner = tinted[0] < tinted[2] ? tinted[0] : tinted[2];
	EXPECT_GE(outer, 50u) << "场景环境光颜色未保留外通道（" << pixel_text(tinted) << "）";
	EXPECT_LE(inner, 30u) << "另一外通道未被压暗（" << pixel_text(tinted) << "）";
	EXPECT_LE(tinted[1], 30u) << "绿色通道未被压暗（" << pixel_text(tinted) << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
场景描述中的非法环境光颜色被忽略（第四十一轮）
口径：分量越界（2.0）整次忽略；画面与调用前逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 场景描述非法环境光颜色被忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	engine::Scene_Data baseline{};
	baseline.light_ambient = 0.5f;
	renderer.scene().scene_apply(baseline);
	std::vector<uint8_t> plain = render_frame();
	//非法环境光颜色（2.0）应被整次忽略
	engine::Scene_Data invalid = baseline;
	invalid.ambient_color[0] = 2.0f;
	renderer.scene().scene_apply(invalid);
	EXPECT_EQ(render_frame(), plain) << "场景描述中的非法环境光颜色改变了画面";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
平方反比下聚光灯仍只照锥内（第四十二轮）
口径：白纹理 ＋ 法线 (0,0,1) 四边形、方向光关闭；聚光灯 (0,0,1) 朝 -Z、窄锥（10°/20°）、半径 10、
      衰减模式切平方反比：中心（锥内）亮、角部 (41,9)（锥外）仍被压暗。
*/


TEST_F(Renderer_Scene_Test, 半球环境光上下异色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	//朝上法线与朝下法线的两块四边形
	engine::Mesh_Handle up_mesh = 0;
	engine::Mesh_Handle down_mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(up_mesh, quad_data_with_normal(0.0f, 1.0f, 0.0f)));
	ASSERT_TRUE(renderer.resource().mesh_load(down_mesh, quad_data_with_normal(0.0f, -1.0f, 0.0f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//关闭方向光，仅保留环境光（强度 0.5），天顶白、地面黑
	const float light_direction[3] = { 0.0f, 0.0f, -1.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	const float black[3] = { 0.0f, 0.0f, 0.0f };
	renderer.scene().light_set(light_direction, white, 0.0f, 0.5f);
	renderer.scene().ambient_color_set(white);
	//启用满强度半球
	renderer.scene().ambient_ground_set(black, 1.0f);
	uint32_t up = render_brightness(up_mesh);
	uint32_t down = render_brightness(down_mesh);
	//关闭半球（退化为单一环境光颜色）
	renderer.scene().ambient_ground_set(black, 0.0f);
	uint32_t up_plain = render_brightness(up_mesh);
	uint32_t down_plain = render_brightness(down_mesh);

	RecordProperty("hemisphere_up", up);
	RecordProperty("hemisphere_down", down);
	RecordProperty("hemisphere_up_plain", up_plain);
	RecordProperty("hemisphere_down_plain", down_plain);

	EXPECT_GT(up, down + 80u) << "朝上法线未取天顶色（" << up << " vs " << down << "）";
	EXPECT_LT(down, 40u) << "朝下法线未取地面色（" << down << "）";
	EXPECT_GE(up, 100u) << "朝上法线亮度偏低（" << up << "）";
	EXPECT_NEAR(up_plain, down_plain, 5u) << "半球关闭后上下法线不应有差异（"
		<< up_plain << " vs " << down_plain << "）";

	renderer.resource().mesh_unload(up_mesh);
	renderer.resource().mesh_unload(down_mesh);
	renderer.resource().texture_unload(texture);
}
