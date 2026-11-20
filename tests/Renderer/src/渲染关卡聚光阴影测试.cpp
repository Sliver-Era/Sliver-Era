/*
渲染关卡聚光阴影测试（2026-11-21 自「渲染关卡测试.cpp」拆出）
覆盖：关卡聚光阴影渲染、强度 / 软度 / 序号与越界、与方向光阴影共存、按需重绘、分辨率切换、Cluster 阴影。
*/

#include "渲染关卡测试辅助.h"

/*
聚光灯阴影（第四百七十四轮；方案 A）
口径：第 1 盏聚光灯从上方照地面，遮挡立方体开启阴影后在地面投下暗区；开启前后差异 > 200 像素。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影)
{
	write_cube_assets();
	//地面（压扁立方体）＋ 遮挡体
	write_text(level_dir() / L"聚光阴影.level",
		"level 聚光阴影\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光阴影.level"), false));

	//关闭阴影：基线
	engine::Light_Data 灯光 = 关卡.light;
	ASSERT_EQ(灯光.spot_count, 1u);
	灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//开启聚光灯阴影
	灯光.spot_shadow_enabled = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(有影.data(), static_cast<uint32_t>(有影.size())));

	const uint32_t 差异 = changed_pixels(无影, 有影);
	std::cout << "[关卡] 聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 200u) << "聚光灯阴影未生效";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影强度独立（第四百七十六轮）
口径：strength = 0 时与未开阴影逐像素等价（≤ 2）；strength = 1 时恢复遮蔽（> 200 像素差异）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影强度)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光强度.level",
		"level 聚光强度\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光强度.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	//基线：未开聚光阴影
	灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//强度 0：应等价于未开阴影
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_strength = 0.0f;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 零强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(零强度.data(), static_cast<uint32_t>(零强度.size())));
	const uint32_t 零差异 = changed_pixels(无影, 零强度);
	std::cout << "[关卡] 聚光阴影零强度差异 " << 零差异 << std::endl;
	EXPECT_LE(零差异, 2u) << "强度 0 未完全关闭阴影";

	//强度 1：恢复遮蔽
	灯光.spot_shadow_strength = 1.0f;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 全强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(全强度.data(), static_cast<uint32_t>(全强度.size())));
	const uint32_t 全差异 = changed_pixels(无影, 全强度);
	std::cout << "[关卡] 聚光阴影全强度差异 " << 全差异 << std::endl;
	EXPECT_GT(全差异, 200u) << "强度 1 未产生遮蔽";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影软度独立（第四百七十六轮）
口径：硬边（软度 0）与软边（软度 0.75 → 5×5 PCF）在阴影边缘产生可观测差异（> 10 像素）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影软度)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光软度.level",
		"level 聚光软度\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光软度.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	//硬边
	灯光.spot_shadow_softness = 0.0f;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 硬边(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(硬边.data(), static_cast<uint32_t>(硬边.size())));

	//软边（0.75 → 5×5 邻域）
	灯光.spot_shadow_softness = 0.75f;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 软边(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(软边.data(), static_cast<uint32_t>(软边.size())));

	const uint32_t 差异 = changed_pixels(硬边, 软边);
	std::cout << "[关卡] 聚光阴影软度差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 10u) << "聚光灯软度未生效";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影序号（第四百七十九轮）
口径：两盏聚光各照一块地面与遮挡体；序号 0 与序号 1 只有一处被遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影序号)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光序号.level",
		"level 聚光序号\n"
		"camera position 0 6 7\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光序号.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 2u);

	//序号 0：只遮蔽左灯
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 4u);
	std::vector<uint8_t> 左影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(左影.data(), static_cast<uint32_t>(左影.size())));

	//序号 1：只遮蔽右灯
	灯光.spot_shadow_index = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 4u);
	std::vector<uint8_t> 右影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(右影.data(), static_cast<uint32_t>(右影.size())));

	const uint32_t 差异 = changed_pixels(左影, 右影);
	std::cout << "[关卡] 聚光阴影序号差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "聚光灯阴影序号未生效";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影序号越界（第四百七十九轮）
口径：只登记 2 盏聚光时序号 5 越界 → 按无聚光阴影处理，与关闭阴影的画面等价（≤ 2 像素）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影序号越界)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光越界.level",
		"level 聚光越界\n"
		"camera position 0 6 7\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光越界.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	//基线：关闭聚光阴影
	灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 4u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//开启但序号越界
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 5;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 4u);
	std::vector<uint8_t> 越界(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(越界.data(), static_cast<uint32_t>(越界.size())));

	const uint32_t 差异 = changed_pixels(无影, 越界);
	std::cout << "[关卡] 聚光阴影越界差异 " << 差异 << std::endl;
	EXPECT_LE(差异, 2u) << "越界序号仍产生阴影";
	level_bridge_unload(renderer, 场景);
}

/*
方向光与聚光阴影共存（第四百八十四轮；方案 B）
口径：方向光阴影开启时再开聚光阴影 → 画面继续变化（> 50 像素），证明两者互不覆盖、同时生效。
*/
TEST(Renderer_Level_Test, 关卡聚光与方向光阴影共存)
{
	write_cube_assets();
	write_text(level_dir() / L"共存阴影.level",
		"level 共存阴影\n"
		"camera position 0 6 8\n"
		"camera target 0 0 0\n"
		"light direction 0.3 -1 0.2\n"
		"light color 1 1 1\n"
		"light intensity 0.5\n"
		"light shadow 1 0.004 0 0.8 1024\n"
		"light spot 2 4 0 0 -1 0 1 1 1 10 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("共存阴影.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	//基线：两套阴影都关
	灯光.shadow_enabled = 0;
	灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//仅方向光阴影
	灯光.shadow_enabled = 1;
	灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 方向影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(方向影.data(), static_cast<uint32_t>(方向影.size())));
	const uint32_t 方向差异 = changed_pixels(无影, 方向影);
	std::cout << "[关卡] 方向光阴影差异 " << 方向差异 << std::endl;
	EXPECT_GT(方向差异, 100u) << "方向光阴影未生效";

	//方向光 ＋ 聚光阴影
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 2u);
	std::vector<uint8_t> 双影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(双影.data(), static_cast<uint32_t>(双影.size())));
	const uint32_t 共存差异 = changed_pixels(方向影, 双影);
	std::cout << "[关卡] 聚光叠加方向光差异 " << 共存差异 << std::endl;
	EXPECT_GT(共存差异, 50u) << "聚光阴影未与方向光阴影共存";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影按需重绘（第四百八十六轮）
口径：静态场景连续渲染时阴影 pass 只重绘首帧；移动聚光灯后再重绘一次（签名含聚光矩阵）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影按需重绘)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光重绘.level",
		"level 聚光重绘\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光重绘.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	ASSERT_TRUE(renderer.frame_render());

	//静态连续渲染：阴影 pass 不应再增长
	const uint64_t 静态后 = renderer.scene().shadow_pass_count();
	for (int frame = 0; frame < 4; ++frame)
	{
		ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
		ASSERT_TRUE(renderer.frame_render());
	}
	EXPECT_EQ(renderer.scene().shadow_pass_count(), 静态后) << "静态场景仍重绘阴影";

	//移动聚光灯位置：签名变化 → 重绘一次
	灯光.spot_position[0][0] = 1.0f;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.scene().shadow_pass_count(), 静态后) << "聚光灯移动后未重绘阴影";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影分辨率切换（第四百八十七轮）
口径：1024 → 512 → 1024 三次渲染均成功；回到 1024 后画面与首次 1024 逐字节一致（≤ 2）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影分辨率切换)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光分辨率.level",
		"level 聚光分辨率\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光分辨率.level"), false));
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	std::vector<uint8_t> 原图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(原图.data(), static_cast<uint32_t>(原图.size())));

	//切到 512 再切回 1024
	renderer.scene().shadow_resolution_set(512);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	std::vector<uint8_t> 低分辨率(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(低分辨率.data(), static_cast<uint32_t>(低分辨率.size())));
	renderer.scene().shadow_resolution_set(1024);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	std::vector<uint8_t> 复原(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(复原.data(), static_cast<uint32_t>(复原.size())));

	const uint32_t 差异 = changed_pixels(原图, 复原);
	std::cout << "[关卡] 分辨率往返差异 " << 差异 << std::endl;
	EXPECT_LE(差异, 2u) << "分辨率往返后画面未复原";
	EXPECT_GE(changed_pixels(原图, 低分辨率), 1u) << "512 分辨率未改变阴影细节";
	level_bridge_unload(renderer, 场景);
}
