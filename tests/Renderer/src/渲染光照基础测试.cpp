/*
渲染光照基础测试（2026-11-21 自「渲染光照测试.cpp」拆出）
覆盖：朝向光源更亮、无法线网格满照、PBR 掠射高光与漫反射能量修正、非法光照参数忽略、
      立方体正反明暗、旋转立方体两侧面明暗、环境光抬升背光面、点光近远亮度、镜面高光提升亮度。
*/
#include "渲染场景测试辅助.h"
#include "测试辅助/渲染测试辅助.h"




TEST_F(Renderer_Scene_Test, 朝向光源更亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	//正面朝相机（受光）与背面（背光）两种网格
	engine::Mesh_Handle lit_mesh = 0;
	engine::Mesh_Handle dark_mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(lit_mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	ASSERT_TRUE(renderer.resource().mesh_load(dark_mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//受光帧：默认光照（方向 -Z、白、强度 1、环境光 0）→ 照度 1
	uint32_t lit = render_brightness(lit_mesh);
	//背光帧：同一光照，法线与光传播方向同向 → 照度 0
	uint32_t dark = render_brightness(dark_mesh);
	RecordProperty("light_lit", lit);
	RecordProperty("light_dark", dark);
	EXPECT_GE(lit, 200u) << "受光面未达到满照（中心亮度 " << lit << "）";
	EXPECT_LE(dark, 40u) << "背光面未被压暗（中心亮度 " << dark << "）";
	EXPECT_GT(static_cast<int>(lit), static_cast<int>(dark) + 100) << "光照未区分正反面";

	renderer.resource().mesh_unload(lit_mesh);
	renderer.resource().mesh_unload(dark_mesh);
	renderer.resource().texture_unload(texture);
}

/*
未提供法线保持满照（第二十九轮回归口径）：只写位置 / 纹理坐标的旧网格在默认光照下画面不变
*/
TEST_F(Renderer_Scene_Test, 无法线网格保持满照)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data()));
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.resource().mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
	//无法线 → 按满照处理，中心仍为纯白
	EXPECT_GE(center_brightness(pixels), 200u) << "无法线网格被光照压暗，破坏既有画面";
	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
PBR 掠射高光能量修正（第六百七十二轮；D1 第一步）
口径：PBR 开启 ＋ 斜射主光。能量修正关 → 开：镜面乘入射余弦，画面出现差异；
关闭态为默认路径（逐字节不变），门面查询与实际开关一致。
*/
TEST_F(Renderer_Scene_Test, PBR掠射高光能量修正生效)
{
	//单次运行的产物（画面 ＋ 开关状态）
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		bool energy = false;
		bool ok = false;
	};
	auto run = [](bool energy)
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

		//PBR 开启、主光斜射（入射余弦约 0.6）、纯金属放大镜面，便于观察能量修正
		engine::Light_Data light{};
		light.direction[0] = -0.8f;
		light.direction[2] = -0.6f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.pbr_enabled = 1;
		light.pbr_energy = energy ? 1 : 0;
		light.pbr_metallic = 1.0f;
		//提升镜面强度（默认 0 时整段镜面为 0，无法体现能量修正）
		light.specular_intensity = 2.0f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene().scene_apply(camera, light, fog);

		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, full_screen_matrix().data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.energy = renderer.scene().pbr_energy();
		outcome.ok = true;
		return outcome;
	};

	const Outcome plain = run(false);
	const Outcome energy = run(true);
	ASSERT_TRUE(plain.ok) << "默认 PBR 配置初始化失败";
	ASSERT_TRUE(energy.ok) << "能量修正配置初始化失败";
	//门面状态与实际开关一致
	EXPECT_FALSE(plain.energy) << "默认态不应开启能量修正";
	EXPECT_TRUE(energy.energy) << "能量修正未生效";
	//统计画面差异像素
	uint32_t difference = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
		{
			std::array<uint8_t, 4> first = pixel_get(plain.pixels, x, y);
			std::array<uint8_t, 4> second = pixel_get(energy.pixels, x, y);
			if (first != second)
				++difference;
		}
	RecordProperty("energy_difference_pixels", difference);
	EXPECT_GT(difference, 100u) << "能量修正未改变掠射镜面（差异像素 " << difference << "）";
}

/*
PBR 漫反射菲涅尔能量分摊（第六百七十四轮；D1 第二步）
口径：介电材质（金属度 0）＋ 正面视角。能量模式关 → 开：漫反射按 (1 − 0.04) 略降，
整面亮度小幅下降（差异像素覆盖整个受光面）。
*/
TEST_F(Renderer_Scene_Test, PBR漫反射能量分摊生效)
{
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		uint32_t brightness = 0;
		bool ok = false;
	};
	auto run = [](bool energy)
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

		//PBR 介电材质，仅漫反射参与（镜面强度 0）
		engine::Light_Data light{};
		light.direction[0] = -0.4f;
		light.direction[2] = -0.9f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.pbr_enabled = 1;
		light.pbr_energy = energy ? 1 : 0;
		light.pbr_metallic = 0.0f;
		light.specular_intensity = 0.0f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene().scene_apply(camera, light, fog);

		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, full_screen_matrix().data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.brightness = center_brightness(outcome.pixels);
		outcome.ok = true;
		return outcome;
	};

	const Outcome plain = run(false);
	const Outcome energy = run(true);
	ASSERT_TRUE(plain.ok) << "默认 PBR 配置初始化失败";
	ASSERT_TRUE(energy.ok) << "能量分摊配置初始化失败";
	RecordProperty("diffuse_energy_plain", plain.brightness);
	RecordProperty("diffuse_energy_on", energy.brightness);
	//统计差异像素（逐通道差 ≥ 1）
	uint32_t difference = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			if (pixel_get(plain.pixels, x, y) != pixel_get(energy.pixels, x, y))
				++difference;
	RecordProperty("diffuse_energy_difference_pixels", difference);
	//正面视角：漫反射略降（中心亮度不升）
	EXPECT_LE(energy.brightness, plain.brightness) << "能量分摊反而提亮了漫反射";
	EXPECT_GT(difference, 1000u) << "能量分摊未覆盖受光面（差异像素 " << difference << "）";
}

/*
非法光照参数整次忽略（第二十九轮）：零向量方向 / 非数 / 越界颜色均不改变画面
*/
TEST_F(Renderer_Scene_Test, 非法光照参数整次忽略)
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

	//渲染一帧（默认光照）作为基线（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};
	std::vector<uint8_t> baseline = render_frame();
	//零长度方向（无法归一化）→ 整次拒绝
	const float zero_direction[3] = { 0.0f, 0.0f, 0.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(zero_direction, white, 1.0f, 0.0f);
	//非数方向 → 整次拒绝
	const float nan_direction[3] = { std::nanf(""), 0.0f, -1.0f };
	renderer.scene().light_set(nan_direction, white, 1.0f, 0.0f);
	//越界颜色（1.5）→ 整次拒绝
	const float over_color[3] = { 1.5f, 1.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	renderer.scene().light_set(forward, over_color, 1.0f, 0.0f);
	//三次非法调用后画面应与基线逐字节一致
	EXPECT_EQ(baseline, render_frame()) << "非法光照参数改变了画面（未整次拒绝）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
正交投影（第二十轮）：同一四边形在相机远近不同时屏幕占比不变；透视下则明显变小
口径：纯红纹理 ＋ 覆盖视口中央的四边形（单位矩阵）；统计「红饱和（外通道 ≥ 200）」的像素数。
      透视：z = 5 的红色像素数应显著少于 z = 3；
      正交（ortho_height = 2）：z = 3 与 z = 5 的红色像素数应基本一致（差 ≤ 2%）。
*/

TEST_F(Renderer_Scene_Test, 立方体正面随光照方向明暗反转)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(0.5f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//光沿 -Z（默认）：+Z 面正对光源 → 满照
	uint32_t lit = render_brightness();
	//光沿 +Z：+Z 面背光 → 照度 0
	const float toward_camera[3] = { 0.0f, 0.0f, 1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(toward_camera, white_light, 1.0f, 0.0f);
	uint32_t dark = render_brightness();
	RecordProperty("cube_lit", lit);
	RecordProperty("cube_dark", dark);
	EXPECT_GE(lit, 200u) << "立方体正面未被照亮（中心亮度 " << lit << "）";
	EXPECT_LE(dark, 40u) << "立方体正面未被压暗（中心亮度 " << dark << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
旋转立方体的两个可见面亮度不同（第三十轮）
口径：立方体绕 Y 轴 -35°（+X 面转向右侧、+Z 面转向左侧），光沿 -X 传播（光源在 +X）：
      +X 面受光（约 0.82）、+Z 面背光（0）；比较左右两侧区域的最大亮度，避开像素级对齐。
*/
TEST_F(Renderer_Scene_Test, 旋转立方体两侧面明暗不同)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(0.6f)));
	renderer.resource().texture_bind_set(texture);
	//光沿 -X 传播（光源在 +X 一侧）
	const float from_positive_x[3] = { -1.0f, 0.0f, 0.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(from_positive_x, white_light, 1.0f, 0.0f);
	//旋转 -35°（+X 面转向相机右侧）
	std::array<float, 16> model = rotation_y_matrix(-0.6109f);
	ASSERT_TRUE(renderer.resource().mesh_draw(mesh, model.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	//左右两侧区域的最大亮度（立方体投影约占中心 40% 宽，故取中心两侧各 1/8 宽、纵向中部）
	uint32_t left_max = 0;
	uint32_t right_max = 0;
	for (uint32_t y = WINDOW_HEIGHT / 4; y < WINDOW_HEIGHT * 3 / 4; ++y)
	{
		for (uint32_t x = WINDOW_WIDTH * 3 / 8; x < WINDOW_WIDTH * 7 / 16; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			uint32_t brightness = (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
			if (brightness > left_max)
				left_max = brightness;
		}
		for (uint32_t x = WINDOW_WIDTH * 9 / 16; x < WINDOW_WIDTH * 5 / 8; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			uint32_t brightness = (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
			if (brightness > right_max)
				right_max = brightness;
		}
	}
	RecordProperty("cube_left_max", left_max);
	RecordProperty("cube_right_max", right_max);
	EXPECT_GE(right_max, 120u) << "受光面亮度不足（右侧 " << right_max << "）";
	//左侧取样区含背景（清屏色亮度约 71），故以「不高于背景」为压暗判据
	EXPECT_LE(left_max, 90u) << "背光面未被压暗（左侧 " << left_max << "）";
	EXPECT_GT(static_cast<int>(right_max), static_cast<int>(left_max) + 60) << "两侧面明暗未区分";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}

/*
环境光抬升背光面（第三十轮）
口径：法线 (0,0,-1) 的全屏四边形在默认光下照度 0；`light_set` 把环境光抬到 0.5 后中心亮度约 127。
*/
TEST_F(Renderer_Scene_Test, 环境光抬升背光面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.resource().texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](const float* model)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, model));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};
	//默认光照（环境光 0）：背光面全黑
	uint32_t without_ambient = render_brightness(nullptr);
	//环境光 0.5：背光面被抬亮
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(forward, white_light, 1.0f, 0.5f);
	uint32_t with_ambient = render_brightness(nullptr);
	RecordProperty("ambient_off", without_ambient);
	RecordProperty("ambient_half", with_ambient);
	EXPECT_LE(without_ambient, 40u) << "背光面未全黑（亮度 " << without_ambient << "）";
	EXPECT_GE(with_ambient, 90u) << "环境光未抬亮背光面（亮度 " << with_ambient << "）";
	EXPECT_LE(with_ambient, 210u) << "环境光亮度超出预期（亮度 " << with_ambient << "）";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
/*
点光源距离衰减（第三十一轮）
口径：法线 (0,0,1) 的全屏四边形，点光源放在 +Z 轴上不同距离、半径 3：
      距离 1 → 衰减 0.67（亮），距离 2.9 → 衰减 0.03（暗）。
*/
TEST_F(Renderer_Scene_Test, 点光源近远亮度不同)
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

	//近距点光源（距离 1，半径 3）
	const float near_position[3] = { 0.0f, 0.0f, 1.0f };
	renderer.scene().light_point_set(near_position, white_light, 3.0f, 1.0f, 0.0f);
	uint32_t near_brightness = render_brightness();
	//远距点光源（距离 2.9，半径 3 → 衰减约 0.03）
	const float far_position[3] = { 0.0f, 0.0f, 2.9f };
	renderer.scene().light_point_set(far_position, white_light, 3.0f, 1.0f, 0.0f);
	uint32_t far_brightness = render_brightness();
	RecordProperty("point_near", near_brightness);
	RecordProperty("point_far", far_brightness);
	EXPECT_GE(near_brightness, 170u) << "近距点光源未照亮（亮度 " << near_brightness << "）";
	EXPECT_LE(far_brightness, 80u) << "远距点光源未衰减（亮度 " << far_brightness << "）";
	EXPECT_GT(static_cast<int>(near_brightness), static_cast<int>(far_brightness) + 80) << "距离衰减未生效";

	renderer.resource().mesh_unload(mesh);
	renderer.resource().texture_unload(texture);
}
