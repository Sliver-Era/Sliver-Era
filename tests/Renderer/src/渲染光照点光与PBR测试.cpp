/*
渲染光照点光与PBR测试（2026-11-21 自「渲染光照测试.cpp」拆出）
覆盖：多盏点光累加、点光上限与清空、第八盏 / 第三十二盏点光参与着色、场景描述携带点光、
      光照场景文字可见、高八槽零强度逐字节一致、PBR 粗糙度绝对口径解耦光泽度。
*/
#include "渲染场景测试辅助.h"
#include "测试辅助/渲染测试辅助.h"


/*
镜面高光提升亮度，非法高光参数整次忽略（第三十一轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 全屏四边形；光沿 -Z、相机在 +Z，故 H = N，高光强度直接叠加到照度上。
      高光 0 时亮度约 188（线性 0.5 的 sRGB 编码）；高光 0.6 时被钳制到 1.0（约 255）。
*/
TEST_F(Renderer_Scene_Test, 镜面高光提升亮度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//高光关闭（默认）
	uint32_t plain = render_brightness();
	//高光 0.6、光泽度 16（H = N → pow(1, 16) = 1）
	renderer.scene().specular_set(0.6f, 16.0f);
	uint32_t highlighted = render_brightness();
	RecordProperty("spec_plain", plain);
	RecordProperty("spec_highlight", highlighted);
	EXPECT_GE(highlighted, 230u) << "高光未提升亮度（" << plain << " → " << highlighted << "）";
	EXPECT_GT(static_cast<int>(highlighted), static_cast<int>(plain) + 30) << "高光增量不足";
	//非法高光参数（负强度 / 光泽度小于 1）整次忽略 → 画面与上一帧逐字节一致
	renderer.scene().specular_set(-1.0f, 0.0f);
	EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
	std::vector<uint8_t> after(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(after.data(), static_cast<std::uint32_t>(after.size())));
	EXPECT_EQ(center_brightness(after), highlighted) << "非法高光参数改变了画面";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
/*
多个点光源累加（第三十二轮）
口径：法线 (0,0,1) 全屏四边形、半径 3；单灯（中心 0,0,1）与双灯（±0.5, 0, 1）：
      单灯照度约 0.67；双灯各约 0.56，累加后钳制到 1.0 → 画面更亮。
*/
TEST_F(Renderer_Scene_Test, 多个点光源累加更亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.resource().texture_bind_set(texture);
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//单灯（中心）
	const float single_position[3] = { 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.scene().light_add_point(single_position, white_light, 3.0f, 1.0f, 0.0f));
	uint32_t single = render_brightness();
	//追加第二盏灯（左移 0.5）
	const float second_position[3] = { -0.5f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.scene().light_add_point(second_position, white_light, 3.0f, 1.0f, 0.0f));
	uint32_t doubled = render_brightness();
	RecordProperty("single_light", single);
	RecordProperty("double_light", doubled);
	EXPECT_GE(doubled, single + 15u) << "双灯未比单灯更亮（" << single << " → " << doubled << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
点光源数量上限与清空（第三十二轮；第六十二轮 4 → 8；第六百九十九轮 8 → 16；第七百零一轮 16 → 32）
口径：上限 32 个；第 33 个返回假且画面不变；`lights_clear` 后回到默认方向光。
*/
TEST_F(Renderer_Scene_Test, 点光源上限与清空)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.resource().texture_bind_set(texture);
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//登记 32 个点光源（高度避开中心像素，避免完全饱和而无法比较）
	const float positions[32][3] = {
		{ -1.5f, 0.0f, 1.0f }, { -0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f, 1.0f }, { 1.5f, 0.0f, 1.0f },
		{ -1.5f, 0.5f, 1.0f }, { -0.5f, 0.5f, 1.0f }, { 0.5f, 0.5f, 1.0f }, { 1.5f, 0.5f, 1.0f },
		{ -1.5f, -0.5f, 1.0f }, { -0.5f, -0.5f, 1.0f }, { 0.5f, -0.5f, 1.0f }, { 1.5f, -0.5f, 1.0f },
		{ -1.5f, 1.0f, 1.0f }, { -0.5f, 1.0f, 1.0f }, { 0.5f, 1.0f, 1.0f }, { 1.5f, 1.0f, 1.0f },
		{ -1.5f, 0.0f, -1.0f }, { -0.5f, 0.0f, -1.0f }, { 0.5f, 0.0f, -1.0f }, { 1.5f, 0.0f, -1.0f },
		{ -1.5f, 0.5f, -1.0f }, { -0.5f, 0.5f, -1.0f }, { 0.5f, 0.5f, -1.0f }, { 1.5f, 0.5f, -1.0f },
		{ -1.5f, -0.5f, -1.0f }, { -0.5f, -0.5f, -1.0f }, { 0.5f, -0.5f, -1.0f }, { 1.5f, -0.5f, -1.0f },
		{ -1.5f, 1.0f, -1.0f }, { -0.5f, 1.0f, -1.0f }, { 0.5f, 1.0f, -1.0f }, { 1.5f, 1.0f, -1.0f },
	};
	for (uint32_t index = 0; index < 32; ++index)
		ASSERT_TRUE(renderer.scene().light_add_point(positions[index], white_light, 6.0f, 0.4f, 0.0f));
	uint32_t full = render_brightness();
	//第 33 个被拒（上限 32），画面不变
	ASSERT_FALSE(renderer.scene().light_add_point(positions[0], white_light, 6.0f, 0.4f, 0.0f));
	EXPECT_EQ(render_brightness(), full) << "超上限的追加改变了画面";
	//清空后回到默认方向光（满照，纯白中心）
	renderer.scene().lights_clear();
	EXPECT_GE(render_brightness(), 200u) << "清空点光源后未回到方向光满照";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
第八盏点光源参与着色（第六十二轮）
口径：方向光强度 0、环境光 0；先登记 7 盏强度 0 的点光源 → 中心暗；
     第 8 盏（中心正前方、强度 1）登记后中心被照亮，证明循环上限确为 8。
*/
TEST_F(Renderer_Scene_Test, 第八盏点光源参与着色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.resource().texture_bind_set(texture);

	//方向光关闭、环境光 0
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(forward, white_light, 0.0f, 0.0f);

	//渲染一帧并返回中心亮度
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//前 7 盏强度 0（占位）
	const float center_light[3] = { 0.0f, 0.0f, 1.0f };
	for (uint32_t index = 0; index < 7; ++index)
		ASSERT_TRUE(renderer.scene().light_add_point(center_light, white_light, 6.0f, 0.0f, 0.0f));
	uint32_t before = render_brightness();
	//第 8 盏强度 1 → 中心被照亮
	ASSERT_TRUE(renderer.scene().light_add_point(center_light, white_light, 6.0f, 1.0f, 0.0f));
	uint32_t after = render_brightness();
	RecordProperty("light8_before", before);
	RecordProperty("light8_after", after);
	EXPECT_LE(before, 40u) << "占位光源意外照亮了中心（" << before << "）";
	EXPECT_GE(after, 150u) << "第 8 盏点光源未参与着色（" << after << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
/*
场景描述携带点光源（第三十三轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 四边形；同一 Scene_Data 两次应用（0 盏 vs 2 盏点光源），
      其余参数（方向光强度 0、环境光 0）相同 → 仅点光源造成亮度差。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带点光源更亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//仅方向光（强度 0 → 全黑）
	engine::Scene_Data scene{};
	scene.light_intensity = 0.0f;
	renderer.scene().scene_apply(scene);
	uint32_t without_points = render_brightness();
	//追加两盏点光源（左右各一，半径 3）
	scene.light_count = 2;
	scene.point_light_position[0][0] = -0.5f;
	scene.point_light_position[0][1] = 0.0f;
	scene.point_light_position[0][2] = 1.0f;
	scene.point_light_position[1][0] = 0.5f;
	scene.point_light_position[1][1] = 0.0f;
	scene.point_light_position[1][2] = 1.0f;
	scene.point_light_radius[0] = 3.0f;
	scene.point_light_radius[1] = 3.0f;
	renderer.scene().scene_apply(scene);
	uint32_t with_points = render_brightness();
	RecordProperty("scene_no_points", without_points);
	RecordProperty("scene_two_points", with_points);
	EXPECT_LE(without_points, 40u) << "方向光关闭时画面未变暗（亮度 " << without_points << "）";
	EXPECT_GE(with_points, 120u) << "场景描述中的点光源未生效（亮度 " << with_points << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
光照场景上的文字仍可见（第三十三轮）
口径：纯白纹理立方体 ＋ 方向光渲染；再叠一行文字；断言文字区域存在明显亮像素
      （文字走独立着色器，不受场景光照影响；字体缺失时跳过）。
*/
TEST_F(Renderer_Scene_Test, 光照场景上的文字仍可见)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//系统等宽字体缺失则跳过
	uint64_t font = 0;
	if (!renderer.ui().font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16))
		GTEST_SKIP() << "系统中缺少 consola.ttf";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(0.5f)));
	renderer.resource().texture_bind_set(texture);

	//场景：方向光（默认）+ 白色文字叠在画面上
	engine::Scene_Data scene{};
	renderer.scene().scene_apply(scene);
	const float white_text[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	renderer.ui().text_color_set(white_text);
	ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.ui().text_draw(font, "LIGHT", 4.0f, 4.0f));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	//文字所在区域（左上角）统计接近纯白的像素数
	uint32_t bright_count = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT / 3; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH / 3; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			if (value[0] >= 200 && value[1] >= 200 && value[2] >= 200)
				++bright_count;
		}
	RecordProperty("text_bright_pixels", bright_count);
	EXPECT_GT(bright_count, 10u) << "光照场景上的文字不可见（亮像素 " << bright_count << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

//逐像素差异计数（RGBA 任一通道不同即记一处；第六百九十九轮）
static uint32_t pixel_difference(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
{
	//尺寸不一致按最大差异
	if (left.size() != right.size())
		return 0xFFFFFFFFu;
	uint32_t differences = 0;
	for (std::size_t index = 0; index + 3 < left.size(); index += 4)
	{
		if (left[index] != right[index] || left[index + 1] != right[index + 1]
			|| left[index + 2] != right[index + 2] || left[index + 3] != right[index + 3])
			++differences;
	}
	return differences;
}

/*
前向高 24 槽参与着色（第六百九十九轮 16 槽；第七百零一轮 32 槽）
口径：方向光强度 0、环境光 0；先登记 31 盏强度 0 的占位光源 → 中心暗；
     第 32 盏（中心正前方、强度 1）登记后中心被照亮，证明前向循环确为 32 槽。
*/
TEST_F(Renderer_Scene_Test, 第三十二盏点光源参与着色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.resource().texture_bind_set(texture);

	//方向光关闭、环境光 0
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(forward, white_light, 0.0f, 0.0f);

	//渲染一帧并返回中心亮度
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//前 31 盏强度 0（占位）
	const float center_light[3] = { 0.0f, 0.0f, 1.0f };
	for (uint32_t index = 0; index < 31; ++index)
		ASSERT_TRUE(renderer.scene().light_add_point(center_light, white_light, 6.0f, 0.0f, 0.0f));
	uint32_t before = render_brightness();
	//第 32 盏强度 1 → 中心被照亮
	ASSERT_TRUE(renderer.scene().light_add_point(center_light, white_light, 6.0f, 1.0f, 0.0f));
	uint32_t after = render_brightness();
	RecordProperty("light32_before", before);
	RecordProperty("light32_after", after);
	EXPECT_LE(before, 40u) << "占位光源意外照亮了中心（" << before << "）";
	EXPECT_GE(after, 150u) << "第 32 盏点光源未参与着色（" << after << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
高 8 槽零强度不改变画面（第六百九十九轮）
口径：8 盏相机光 → 画面 A；追加 4 盏强度 0（落高 8 槽）→ 画面 B 与 A 逐字节一致；
     该 4 盏改为强度 0.8 → 画面 C 与 A 出现差异（证明高槽数据确实进入着色器）。
*/
TEST_F(Renderer_Scene_Test, 高八槽零强度逐字节一致)
{
	//指定高 8 槽额外光源数量与强度的一次运行
	auto run = [](uint32_t extra_count, float extra_intensity)
	{
		std::vector<uint8_t> pixels;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.compute().readback_supported())
			return pixels;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.resource().texture_load(texture, texels.data(), 2, 2))
			return pixels;
		engine::Mesh_Handle mesh = 0;
		if (!renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return pixels;
		renderer.resource().texture_bind_set(texture);
		//方向光关闭、环境光 0，仅点光贡献
		const float forward[3] = { 0.0f, 0.0f, -1.0f };
		const float white_light[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(forward, white_light, 0.0f, 0.0f);
		//低 8 槽：四角偏上位置，强度 0.1（保持不饱和，便于观察高槽增量）
		const float low_positions[8][3] = {
			{ -1.5f, 0.0f, 1.0f }, { -0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f, 1.0f }, { 1.5f, 0.0f, 1.0f },
			{ -1.5f, 0.5f, 1.0f }, { -0.5f, 0.5f, 1.0f }, { 0.5f, 0.5f, 1.0f }, { 1.5f, 0.5f, 1.0f },
		};
		for (uint32_t index = 0; index < 8; ++index)
			if (!renderer.scene().light_add_point(low_positions[index], white_light, 6.0f, 0.1f, 0.0f))
				return pixels;
		//高 8 槽前 extra_count 盏：底部位置，强度由参数控制
		const float high_positions[4][3] = {
			{ -1.5f, -0.5f, 1.0f }, { -0.5f, -0.5f, 1.0f }, { 0.5f, -0.5f, 1.0f }, { 1.5f, -0.5f, 1.0f },
		};
		for (uint32_t index = 0; index < extra_count; ++index)
			if (!renderer.scene().light_add_point(high_positions[index], white_light, 6.0f, extra_intensity, 0.0f))
				return pixels;

		if (!renderer.resource().mesh_draw(mesh, nullptr))
			return pixels;
		pixels.resize(pixel_bytes());
		if (!renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())))
			pixels.clear();
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
		return pixels;
	};

	//画面 A：仅低 8 槽；画面 B：低 8 槽 ＋ 高 8 槽 4 盏零强度；画面 C：高槽 4 盏强度 0.8
	std::vector<uint8_t> baseline = run(0u, 0.0f);
	std::vector<uint8_t> high_zero = run(4u, 0.0f);
	std::vector<uint8_t> high_lit = run(4u, 0.8f);
	ASSERT_FALSE(baseline.empty()) << "基线画面为空";
	ASSERT_FALSE(high_zero.empty()) << "零强度画面为空";
	ASSERT_FALSE(high_lit.empty()) << "高槽点亮画面为空";
	//高槽零强度的 4 盏不改变画面
	EXPECT_EQ(pixel_difference(baseline, high_zero), 0u) << "高 8 槽零强度改变了画面";
	//高槽点亮后出现差异
	uint32_t differences = pixel_difference(baseline, high_lit);
	RecordProperty("high_slot_difference", differences);
	EXPECT_GT(differences, 0u) << "高 8 槽强度未进入着色器";
}

/*
PBR 粗糙度绝对口径（第七百轮；D1 收尾）
口径：PBR 开启、金属度 0.8、全局粗糙度 0.2；镜面光泽度 16 与 512 各渲染一次：
      旧口径下粗糙度由光泽度换算 → 两画面不同；绝对口径下粗糙度直接取 0.2 → 两画面逐字节一致。
*/
TEST_F(Renderer_Scene_Test, PBR粗糙度绝对口径解耦光泽度)
{
	//一次运行的产物（画面 ＋ 开关回读）
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		bool absolute = false;
	};

	//指定绝对口径与光泽度的一次运行
	auto run = [](bool absolute, float shininess)
	{
		Outcome outcome;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.compute().readback_supported())
			return outcome;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.resource().texture_load(texture, texels.data(), 2, 2))
			return outcome;
		engine::Mesh_Handle mesh = 0;
		if (!renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return outcome;
		renderer.resource().texture_bind_set(texture);
		//PBR 开启：斜射主光、金属度 0.8、全局粗糙度 0.2、镜面强度 1，光泽度由参数控制
		const float direction[3] = { -0.8f, 0.0f, -0.6f };
		const float white[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, white, 1.0f, 0.0f);
		renderer.scene().pbr_enable_set(true);
		renderer.scene().pbr_roughness_set(0.2f);
		renderer.scene().pbr_metallic_set(0.8f);
		renderer.scene().specular_set(1.0f, shininess);
		//绝对口径开关（门面）
		renderer.scene().pbr_roughness_absolute_set(absolute);
		outcome.absolute = renderer.scene().pbr_roughness_absolute();
		//渲染并回读
		if (!renderer.resource().mesh_draw(mesh, full_screen_matrix().data()))
			return outcome;
		outcome.pixels.resize(pixel_bytes());
		if (!renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())))
			outcome.pixels.clear();
		renderer.resource().mesh_unload(mesh);
		renderer.resource().texture_unload(texture);
		return outcome;
	};

	//旧口径：粗糙度随光泽度变化 → 两个光泽度画面不同
	Outcome legacy_low = run(false, 16.0f);
	Outcome legacy_high = run(false, 512.0f);
	ASSERT_FALSE(legacy_low.pixels.empty()) << "旧口径低光泽画面为空";
	ASSERT_FALSE(legacy_high.pixels.empty()) << "旧口径高光泽画面为空";
	EXPECT_FALSE(legacy_low.absolute) << "默认绝对口径开关未保持关闭";
	uint32_t legacy_difference = pixel_difference(legacy_low.pixels, legacy_high.pixels);
	RecordProperty("legacy_shininess_difference", legacy_difference);
	EXPECT_GT(legacy_difference, 0u) << "旧口径下光泽度未参与粗糙度换算";

	//绝对口径：粗糙度固定 0.2 → 两个光泽度画面逐字节一致
	Outcome absolute_low = run(true, 16.0f);
	Outcome absolute_high = run(true, 512.0f);
	ASSERT_FALSE(absolute_low.pixels.empty()) << "绝对口径低光泽画面为空";
	ASSERT_FALSE(absolute_high.pixels.empty()) << "绝对口径高光泽画面为空";
	EXPECT_TRUE(absolute_low.absolute) << "绝对口径开关回读不一致";
	EXPECT_EQ(pixel_difference(absolute_low.pixels, absolute_high.pixels), 0u)
		<< "绝对口径下光泽度仍影响粗糙度";
}
