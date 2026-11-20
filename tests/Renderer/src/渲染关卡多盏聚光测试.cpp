/*
渲染关卡多盏聚光测试（2026-11-21 自「渲染关卡测试.cpp」拆出）
覆盖：多盏聚光阴影渲染、运行期开关往返与非法输入、质量参数与序号往返、四盏聚光与开销。
*/

#include "渲染关卡测试辅助.h"

/*
Cluster 路径聚光阴影（第四百八十八轮）
口径：开启 Cluster 后聚光阴影仍生效（同一场景，开启/关闭阴影的画面差异 > 100 像素）。
*/
TEST(Renderer_Level_Test, 关卡Cluster聚光阴影)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光簇.level",
		"level 聚光簇\n"
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
	if (!renderer.compute().cluster_supported())
		GTEST_SKIP() << "本机不支持 Cluster 计算路径";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("聚光簇.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	//开启 Cluster（默认网格）
	renderer.compute().cluster_enable_set(true);

	//关闭阴影（Cluster 路径）
	灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//开启阴影（Cluster 路径）
	灯光.spot_shadow_enabled = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(level_bridge_draw(renderer, 场景) == 2u);
	std::vector<uint8_t> 有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(有影.data(), static_cast<uint32_t>(有影.size())));

	const uint32_t 差异 = changed_pixels(无影, 有影);
	std::cout << "[关卡] Cluster 聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "Cluster 路径聚光阴影未生效";
	renderer.compute().cluster_enable_set(false);
	level_bridge_unload(renderer, 场景);
}


/*
聚光灯阴影开关文本往返（第四百七十五轮）
口径：`light spot_shadow 0/1` 结构化保存；写出再读回一致。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影开关往返)
{
	write_text(level_dir() / L"聚光开关.level", "level 聚光开关\nlight spot_shadow 1\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光开关.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_enabled, 1);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("聚光开关_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光开关_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_enabled, 1);
}

/*
聚光灯阴影开关非法（第四百七十五轮）
口径：只接受 0 / 1，且不得有多余字段；越界 / 缺值 / 非数字一律拒绝。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影开关非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "开关为 2", "level 非法\nlight spot_shadow 2\n" },
		{ "缺值", "level 非法\nlight spot_shadow\n" },
		{ "非数字", "level 非法\nlight spot_shadow on\n" },
		{ "多余字段", "level 非法\nlight spot_shadow 1 2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_enabled, 0) << item.first;
	}
}

/*
聚光灯阴影质量参数往返（第四百七十八轮）
口径：`light spot_shadow 1 0.02 0.5 0.3` 读入三项并写出后再读回一致；仅写开关时三项保持默认。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影质量参数往返)
{
	write_text(level_dir() / L"聚光质量.level", "level 聚光质量\nlight spot_shadow 1 0.02 0.5 0.3\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光质量.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_enabled, 1);
	EXPECT_FLOAT_EQ(loaded.light.spot_shadow_bias, 0.02f);
	EXPECT_FLOAT_EQ(loaded.light.spot_shadow_softness, 0.5f);
	EXPECT_FLOAT_EQ(loaded.light.spot_shadow_strength, 0.3f);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("聚光质量_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光质量_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_enabled, 1);
	EXPECT_FLOAT_EQ(reread.light.spot_shadow_bias, 0.02f);
	EXPECT_FLOAT_EQ(reread.light.spot_shadow_softness, 0.5f);
	EXPECT_FLOAT_EQ(reread.light.spot_shadow_strength, 0.3f);

	//仅写开关：三项沿用默认（0.010 / 0 / 1）
	write_text(level_dir() / L"聚光默认.level", "level 聚光默认\nlight spot_shadow 1\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光默认.level"), 缺省));
	EXPECT_FLOAT_EQ(缺省.light.spot_shadow_bias, 0.010f);
	EXPECT_FLOAT_EQ(缺省.light.spot_shadow_softness, 0.0f);
	EXPECT_FLOAT_EQ(缺省.light.spot_shadow_strength, 1.0f);
}

/*
聚光灯阴影质量参数非法（第四百七十八轮）
口径：偏置 0..0.05、软度 0..1、强度 0..1；只写一两项 / 多余字段 / 越界一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影质量参数非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "偏置越界", "level 非法\nlight spot_shadow 1 0.06 0.5 0.3\n" },
		{ "软度越界", "level 非法\nlight spot_shadow 1 0.01 1.2 0.3\n" },
		{ "强度为负", "level 非法\nlight spot_shadow 1 0.01 0.5 -0.1\n" },
		{ "只写两项", "level 非法\nlight spot_shadow 1 0.01 0.5\n" },
		{ "多余字段", "level 非法\nlight spot_shadow 1 0.01 0.5 0.3 9\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_enabled, 0) << item.first;
		EXPECT_FLOAT_EQ(loaded.light.spot_shadow_bias, 0.010f) << item.first;
	}
}

/*
聚光灯阴影序号文本往返（第四百八十一轮）
口径：`light spot_shadow_index 1` 结构化保存；缺省为 0；写出再读回一致。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影序号往返)
{
	write_text(level_dir() / L"聚光序号文本.level", "level 聚光序号文本\nlight spot_shadow_index 1\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光序号文本.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_index, 1);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("聚光序号文本_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光序号文本_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_index, 1);

	//缺省：不写该行时序号为 0
	write_text(level_dir() / L"聚光序号缺省.level", "level 聚光序号缺省\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光序号缺省.level"), 缺省));
	EXPECT_EQ(缺省.light.spot_shadow_index, 0);
}

/*
聚光灯阴影序号非法（第四百八十一轮）
口径：只接受 0..7 的十进制整数，且不得有多余字段。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影序号非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "序号为 8", "level 非法\nlight spot_shadow_index 8\n" },
		{ "负数", "level 非法\nlight spot_shadow_index -1\n" },
		{ "非数字", "level 非法\nlight spot_shadow_index on\n" },
		{ "缺值", "level 非法\nlight spot_shadow_index\n" },
		{ "多余字段", "level 非法\nlight spot_shadow_index 1 2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_index, 0) << item.first;
	}
}

/*
多盏聚光灯阴影（第五百零七轮）
口径：两盏聚光各照一块地面与遮挡体；单盏（序号 0）只遮蔽左侧，多盏开启后两盏各自遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光阴影)
{
	write_cube_assets();
	write_text(level_dir() / L"多盏聚光.level",
		"level 多盏聚光\n"
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
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("多盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 2u);

	//单盏（序号 0）：只遮蔽左侧
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 0;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 4u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));

	//多盏：两盏各自投影
	灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 4u);
	std::vector<uint8_t> 多盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(多盏.data(), static_cast<uint32_t>(多盏.size())));

	const uint32_t 差异 = changed_pixels(单盏, 多盏);
	std::cout << "[关卡] 多盏聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "多盏聚光灯阴影未生效";
	level_bridge_unload(renderer, 场景);
}

/*
多盏聚光灯开关文本往返（第五百零七轮）
口径：`light spot_shadow_all 1` 结构化保存；缺省为 0；写出再读回一致。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光开关往返)
{
	write_text(level_dir() / L"多盏开关.level", "level 多盏开关\nlight spot_shadow_all 1\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("多盏开关.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_all, 1);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("多盏开关_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("多盏开关_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_all, 1);

	//缺省：不写该行时为 0
	write_text(level_dir() / L"多盏开关缺省.level", "level 多盏开关缺省\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("多盏开关缺省.level"), 缺省));
	EXPECT_EQ(缺省.light.spot_shadow_all, 0);
}

/*
多盏聚光灯开关非法（第五百零七轮）
口径：只接受 0 / 1，且不得有多余字段。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光开关非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "开关为 2", "level 非法\nlight spot_shadow_all 2\n" },
		{ "负数", "level 非法\nlight spot_shadow_all -1\n" },
		{ "非数字", "level 非法\nlight spot_shadow_all on\n" },
		{ "缺值", "level 非法\nlight spot_shadow_all\n" },
		{ "多余字段", "level 非法\nlight spot_shadow_all 1 2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_all, 0) << item.first;
	}
}

/*
四盏聚光灯阴影（第五百零八轮）
口径：四盏聚光各照一块地面与遮挡体；单盏（序号 0）只遮蔽最左，多盏开启后四盏各自遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡四盏聚光阴影)
{
	write_cube_assets();
	write_text(level_dir() / L"四盏聚光.level",
		"level 四盏聚光\n"
		"camera position 0 8 10\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -6 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot -2 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 2 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 6 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -6 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 -2 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 2 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 6 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 -6 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 -2 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 2 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 6 0.6 0 1.2 1.2 1.2\n");

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
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("四盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 4u);

	//单盏（序号 0）：只遮蔽最左
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 0;
		灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 8u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));

	//多盏：四盏各自投影
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 8u);
	std::vector<uint8_t> 四盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(四盏.data(), static_cast<uint32_t>(四盏.size())));

	const uint32_t 差异 = changed_pixels(单盏, 四盏);
	std::cout << "[关卡] 四盏聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "四盏聚光灯阴影未生效";
	level_bridge_unload(renderer, 场景);
}


/*
多盏聚光灯运行期开关（第五百一十一轮）
口径：场景先按单盏（序号 0）应用，再用 spot_shadow_all_set 在运行期切到四盏、再切回单盏，
不重新 scene_apply；四盏画面与单盏差异 > 100 像素，切回后与初始单盏画面基本一致（差异 < 50 像素）。
*/
TEST(Renderer_Level_Test, 聚光灯多盏运行期开关)
{
	write_cube_assets();
	write_text(level_dir() / L"四盏聚光.level",
		"level 四盏聚光\n"
		"camera position 0 8 10\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -6 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot -2 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 2 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 6 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -6 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 -2 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 2 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 6 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 -6 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 -2 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 2 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 6 0.6 0 1.2 1.2 1.2\n");

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
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("四盏聚光.level"), false));

	//单盏（序号 0）应用后：门面默认与场景一致
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 0;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_FALSE(renderer.scene().spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 8u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));

	//运行期切到四盏（不重新 scene_apply）
	renderer.scene().spot_shadow_all_set(true);
	EXPECT_TRUE(renderer.scene().spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 8u);
	std::vector<uint8_t> 四盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(四盏.data(), static_cast<uint32_t>(四盏.size())));

	const uint32_t 差异 = changed_pixels(单盏, 四盏);
	std::cout << "[关卡] 运行期多盏开关差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "运行期开启多盏聚光灯阴影未生效";

	//切回单盏：画面应与初始单盏基本一致
	renderer.scene().spot_shadow_all_set(false);
	EXPECT_FALSE(renderer.scene().spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 8u);
	std::vector<uint8_t> 回单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(回单盏.data(), static_cast<uint32_t>(回单盏.size())));

	const uint32_t 回差异 = changed_pixels(单盏, 回单盏);
	std::cout << "[关卡] 运行期切回单盏差异 " << 回差异 << std::endl;
	EXPECT_LT(回差异, 50u) << "运行期切回单盏后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
多盏聚光灯开销基线（第五百零八轮）
口径：同一场景（四盏聚光各照一块地面与遮挡体），分别登记 1 / 2 / 4 盏并全部投影；
记录阴影 pass 的单遍 GPU 耗时（gpu_shadow_ms 为最后一次聚光提交的耗时；设备不支持时间戳时各项为 0）；
断言 4 盏的单遍耗时不显著低于 1 盏（容差 0.1 ms），说明单遍成本与盏数无关、总成本随遍数线性增长。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光开销)
{
	write_cube_assets();
	write_text(level_dir() / L"多盏开销.level",
		"level 多盏开销\n"
		"camera position 0 8 10\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -6 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot -2 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 2 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 6 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -6 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 -2 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 2 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 6 -1.5 0 3 0.2 3\n"
		"place 方块 方块材质 -6 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 -2 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 2 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 6 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("多盏开销.level"), false));

	//分别按 1 / 2 / 4 盏登记，全部投影并预热 6 帧
	const uint32_t counts[3] = { 1u, 2u, 4u };
	double shadow_ms[3] = { 0.0, 0.0, 0.0 };
	std::string keys[3] = { "spot_shadow_gpu_ms_1", "spot_shadow_gpu_ms_2", "spot_shadow_gpu_ms_4" };
	for (std::size_t index = 0; index < 3; ++index)
	{
		engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
		灯光.spot_count = counts[index];
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		灯光.spot_count = counts[index];
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 8u);
		for (uint32_t frame = 0; frame < 6; ++frame)
			ASSERT_TRUE(renderer.frame_render());
		shadow_ms[index] = renderer.frame_stats_get().gpu_shadow_ms;
		RecordProperty(keys[index], shadow_ms[index]);
		std::cout << "[关卡] 多盏开销 " << counts[index] << " 盏阴影耗时 " << shadow_ms[index] << " ms" << std::endl;
	}

	//设备支持时间戳时：4 盏必须产生阴影耗时，且不显著低于 1 盏
	if (renderer.compute().gpu_timestamp_supported())
	{
		EXPECT_GT(shadow_ms[2], 0.0) << "4 盏聚光未产生阴影耗时";
		EXPECT_GE(shadow_ms[2], shadow_ms[0] - 0.1) << "4 盏阴影耗时异常低于 1 盏";
	}
	else
	{
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_EQ(shadow_ms[index], 0.0);
	}
	level_bridge_unload(renderer, 场景);
}
