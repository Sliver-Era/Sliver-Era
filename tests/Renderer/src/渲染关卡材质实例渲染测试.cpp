/*
渲染关卡材质实例渲染测试（2026-11-21 自「渲染关卡测试.cpp」拆出）
覆盖：清漆 / PBR / 卡通组合与状态复位、实例写出拒绝、MSAA 组合、材质实例查找 / 更新 / 增删。
*/

#include "渲染关卡测试辅助.h"

/*
材质实例清漆行往返（第五百九十四轮）
口径：`material` 行可带 `clearcoat <强度> <粗糙度>`（pbr 之后）；结构化保存、写出再读回逐位一致。
*/
TEST(Renderer_Level_Test, 关卡材质实例清漆往返)
{
	const std::string text =
		"level 清漆关卡\n"
		"material 瓷甲 甲 clearcoat 0.8 0.05\n"
		"material 素甲 甲\n";

	write_text(level_dir() / L"实例清漆.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例清漆.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 2u);
	EXPECT_TRUE(loaded.materials[0].use_clearcoat);
	EXPECT_TRUE(bit_equal(loaded.materials[0].clear_coat, 0.8f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].clear_coat_roughness, 0.05f));
	EXPECT_FALSE(loaded.materials[1].use_clearcoat);

	//写出再读回：清漆两项一致
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例清漆_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例清漆_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 2u);
	EXPECT_EQ(reread.materials[0].use_clearcoat, loaded.materials[0].use_clearcoat);
	EXPECT_TRUE(bit_equal(reread.materials[0].clear_coat, loaded.materials[0].clear_coat));
	EXPECT_TRUE(bit_equal(reread.materials[0].clear_coat_roughness, loaded.materials[0].clear_coat_roughness));
}

/*
材质实例清漆非法（第五百九十四轮）
口径：强度须落在 (0, 1]、粗糙度 [0.03, 1]；越界 / 缺字段 / 重复 / 顺序非法整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例清漆非法)
{
	//强度下界等于 0（不含）→ 拒绝
	write_text(level_dir() / L"清漆强度零.level",
		"level 清漆强度零\nmaterial 甲 模板 clearcoat 0 0.2\n");
	engine::Level_Data zero;
	EXPECT_FALSE(engine::level_text_read(level_utf8("清漆强度零.level"), zero));
	//强度越界 → 拒绝
	write_text(level_dir() / L"清漆强度高.level",
		"level 清漆强度高\nmaterial 甲 模板 clearcoat 1.5 0.2\n");
	engine::Level_Data high;
	EXPECT_FALSE(engine::level_text_read(level_utf8("清漆强度高.level"), high));
	//粗糙度低于下界 → 拒绝
	write_text(level_dir() / L"清漆粗糙低.level",
		"level 清漆粗糙低\nmaterial 甲 模板 clearcoat 0.5 0.02\n");
	engine::Level_Data rough_low;
	EXPECT_FALSE(engine::level_text_read(level_utf8("清漆粗糙低.level"), rough_low));
	//缺一个字段 → 拒绝
	write_text(level_dir() / L"清漆缺字段.level",
		"level 清漆缺字段\nmaterial 甲 模板 clearcoat 0.5\n");
	engine::Level_Data missing;
	EXPECT_FALSE(engine::level_text_read(level_utf8("清漆缺字段.level"), missing));
	//重复覆盖 → 拒绝
	write_text(level_dir() / L"清漆重复.level",
		"level 清漆重复\nmaterial 甲 模板 clearcoat 0.5 0.2 clearcoat 0.6 0.3\n");
	engine::Level_Data repeated;
	EXPECT_FALSE(engine::level_text_read(level_utf8("清漆重复.level"), repeated));
	//顺序非法（clearcoat 之后出现 pbr）→ 拒绝
	write_text(level_dir() / L"清漆顺序.level",
		"level 清漆顺序\nmaterial 甲 模板 clearcoat 0.5 0.2 pbr 0.3 0.5\n");
	engine::Level_Data order;
	EXPECT_FALSE(engine::level_text_read(level_utf8("清漆顺序.level"), order));
}

/*
材质实例清漆生效（第五百九十四轮）
口径：同模板「无清漆」与「clearcoat 1 0.05」两个实例 → 画面差异明显（覆盖即启用清漆层）。
*/
TEST(Renderer_Level_Test, 关卡材质实例清漆生效)
{
	write_cube_assets();
	write_text(level_dir() / L"清漆甲.level",
		"level 清漆甲\n"
		"camera position 0 0 3\n"
		"light direction 0 0 -1\n"
		"light intensity 1.2\n"
		"light ambient 0.2\n"
		"light specular 1 32\n"
		"light pbr 1 0.5 0\n"
		"material 素 甲 tint 1 1 1 1\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"清漆乙.level",
		"level 清漆乙\n"
		"camera position 0 0 3\n"
		"light direction 0 0 -1\n"
		"light intensity 1.2\n"
		"light ambient 0.2\n"
		"light specular 1 32\n"
		"light pbr 1 0.5 0\n"
		"material 瓷甲 甲 tint 1 1 1 1 clearcoat 1 0.05\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 瓷甲 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：无清漆
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("清漆甲.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：实例清漆
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("清漆乙.level")));
	//装配结果携带清漆覆盖（模板句柄 ＋ 两项参数）
	ASSERT_EQ(场景乙.objects.size(), 1u);
	ASSERT_EQ(场景乙.objects[0].material_instances.size(), 1u);
	EXPECT_TRUE(场景乙.objects[0].material_instances[0].use_clearcoat);
	EXPECT_TRUE(bit_equal(场景乙.objects[0].material_instances[0].clear_coat, 1.0f));
	EXPECT_TRUE(bit_equal(场景乙.objects[0].material_instances[0].clear_coat_roughness, 0.05f));
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));

	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例清漆差异 " << 差异 << std::endl;
	//清漆为锐利高光（粗糙度 0.05），改动只落在高光斑内；阈值取 10
	EXPECT_GT(差异, 10u) << "实例 clearcoat 覆盖未生效";
	level_bridge_unload(renderer, 场景乙);
}

/*
实例清漆与 PBR 组合（第五百九十七轮）
口径：同一实例同时带 `pbr` 与 `clearcoat`，PBR 覆盖不被清漆吞并：相对「仅清漆」画面应出现可见差异。
注：清漆以 `max` 叠加到高光上，故「基础高光已更强」时清漆不产生额外差异（组合宜以「仅清漆」为基准）。
*/
TEST(Renderer_Level_Test, 实例清漆与PBR组合)
{
	write_cube_assets();
	write_text(level_dir() / L"清漆PBR甲.level",
		"level 清漆PBR甲\n"
		"camera position 0 0 3\n"
		"light direction 0 0 -1\n"
		"light intensity 1.2\n"
		"light ambient 0.2\n"
		"light specular 1 32\n"
		"light pbr 1 0.5 0\n"
		"material 瓷甲 甲 tint 1 1 1 1 clearcoat 1 0.05\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 瓷甲 0 0 0 1 1 1\n");
	write_text(level_dir() / L"清漆PBR乙.level",
		"level 清漆PBR乙\n"
		"camera position 0 0 3\n"
		"light direction 0 0 -1\n"
		"light intensity 1.2\n"
		"light ambient 0.2\n"
		"light specular 1 32\n"
		"light pbr 1 0.5 0\n"
		"material 组合甲 甲 tint 1 1 1 1 pbr 1 0 clearcoat 1 0.05\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 组合甲 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：仅清漆
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("清漆PBR甲.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：PBR ＋ 清漆
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("清漆PBR乙.level")));
	ASSERT_EQ(场景乙.objects.size(), 1u);
	ASSERT_EQ(场景乙.objects[0].material_instances.size(), 1u);
	EXPECT_TRUE(场景乙.objects[0].material_instances[0].use_clearcoat);
	EXPECT_TRUE(场景乙.objects[0].material_instances[0].use_pbr);
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));

	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 清漆叠加 PBR 差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 10u) << "PBR 覆盖被清漆吞并";
	level_bridge_unload(renderer, 场景乙);
}

/*
实例清漆状态复位（第五百九十七轮）
口径：实例清漆只在本次绘制生效；`scene_draw` 结束后清漆强度 / 粗糙度回到绘制前状态，不污染后续对象。
*/
TEST(Renderer_Level_Test, 实例清漆状态复位)
{
	write_cube_assets();
	write_text(level_dir() / L"清漆复位.level",
		"level 清漆复位\n"
		"camera position 0 0 3\n"
		"light direction 0 0 -1\n"
		"light intensity 1.2\n"
		"light ambient 0.2\n"
		"light specular 1 32\n"
		"light pbr 1 0.5 0\n"
		"material 瓷甲 甲 tint 1 1 1 1 clearcoat 1 0.05\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 瓷甲 0 0 0 1 1 1\n");

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
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("清漆复位.level")));
	//绘制前状态（关卡装配不改变全局清漆状态）
	const bool 前激活 = renderer.resource().material_clear_coat_active();
	const float 前强度 = renderer.resource().material_clear_coat_strength();
	const float 前粗糙 = renderer.resource().material_clear_coat_roughness();

	//绘制：实例覆盖生效后应复位
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 1u);
	std::vector<uint8_t> 帧(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧.data(), static_cast<uint32_t>(帧.size())));
	EXPECT_EQ(renderer.resource().material_clear_coat_active(), 前激活) << "实例清漆状态在绘制后泄漏";
	EXPECT_TRUE(bit_equal(renderer.resource().material_clear_coat_strength(), 前强度)) << "清漆强度状态泄漏";
	EXPECT_TRUE(bit_equal(renderer.resource().material_clear_coat_roughness(), 前粗糙)) << "清漆粗糙度状态泄漏";
	level_bridge_unload(renderer, 场景);
}

/*
实例 PBR 状态复位（第五百七十二轮）
口径：实例覆盖只在本次绘制生效；`scene_draw` 结束后 `material_pbr_active` 必须回到绘制前的状态，
避免后续对象被上一次实例的 PBR 状态污染。
*/
TEST(Renderer_Level_Test, 实例PBR状态复位)
{
	write_cube_assets();
	write_text(level_dir() / L"PBR复位.level",
		"level PBR复位\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 金属甲 甲 tint 1 1 1 1 pbr 0.2 0.9\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 金属甲 0 0 0 1 1 1\n");

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
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("PBR复位.level")));
	//绘制前状态（关卡装配不改变全局材质状态）
	const bool 前激活 = renderer.resource().material_pbr_active();
	const float 前粗糙 = renderer.resource().material_pbr_roughness();
	const float 前金属 = renderer.resource().material_pbr_metallic();

	//绘制：实例覆盖生效后应复位
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 1u);
	std::vector<uint8_t> 帧(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧.data(), static_cast<uint32_t>(帧.size())));
	EXPECT_EQ(renderer.resource().material_pbr_active(), 前激活) << "实例 PBR 状态在绘制后泄漏";
	EXPECT_TRUE(bit_equal(renderer.resource().material_pbr_roughness(), 前粗糙)) << "粗糙度状态泄漏";
	EXPECT_TRUE(bit_equal(renderer.resource().material_pbr_metallic(), 前金属)) << "金属度状态泄漏";
	level_bridge_unload(renderer, 场景);
}

/*
实例 PBR 与卡通组合（第五百七十四轮）
口径：同一实例同时带 `toon` 与 `pbr`，两者互不吞并：相对「仅卡通」与「仅 PBR」两张图都应产生可见差异。
*/
TEST(Renderer_Level_Test, 实例PBR与卡通组合)
{
	write_cube_assets();
	write_text(level_dir() / L"组合甲.level",
		"level 组合甲\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 甲实例 甲 tint 1 1 1 1 toon 1 1.0 0.5\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 甲实例 0 0 0 1 1 1\n");
	write_text(level_dir() / L"组合乙.level",
		"level 组合乙\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 乙实例 甲 tint 1 1 1 1 pbr 0.2 0.9\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 乙实例 0 0 0 1 1 1\n");
	write_text(level_dir() / L"组合丙.level",
		"level 组合丙\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 丙实例 甲 tint 1 1 1 1 toon 1 1.0 0.5 pbr 0.2 0.9\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 丙实例 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//三张图：仅卡通 / 仅 PBR / 组合
	std::vector<uint8_t> 帧列表[3];
	const char* const 关卡名[3] = { "组合甲.level", "组合乙.level", "组合丙.level" };
	for (std::size_t index = 0; index < 3; ++index)
	{
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8(关卡名[index])));
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 1u);
		帧列表[index].assign(LEVEL_WIDTH * LEVEL_HEIGHT * 4, 0);
		ASSERT_TRUE(renderer.frame_render(帧列表[index].data(), static_cast<uint32_t>(帧列表[index].size())));
		level_bridge_unload(renderer, 场景);
	}

	//组合相对两类单独覆盖都应不同
	const uint32_t 对卡通差异 = changed_pixels(帧列表[0], 帧列表[2]);
	const uint32_t 对PBR差异 = changed_pixels(帧列表[1], 帧列表[2]);
	std::cout << "[关卡] 组合 PBR 对卡通差异 " << 对卡通差异 << " 对 PBR 差异 " << 对PBR差异 << std::endl;
	EXPECT_GT(对卡通差异, 10u) << "组合中的 PBR 项未生效";
	EXPECT_GT(对PBR差异, 10u) << "组合中的卡通项未生效";
}

/*
实例 PBR 写出拒绝（第五百七十四轮）
口径：`Level_Data` 中实例 `pbr` 值越界时，`level_text_write` 写出前校验失败且不落盘。
*/
TEST(Renderer_Level_Test, 实例PBR写出拒绝)
{
	engine::Level_Data level;
	level.name = "PBR写出";
	engine::Level_Material material;
	material.实例名 = "甲实例";
	material.模板 = "甲";
	material.use_pbr = true;
	material.pbr_roughness = 5.0f;
	material.pbr_metallic = 0.5f;
	level.materials.push_back(material);
	EXPECT_FALSE(engine::level_text_write(level, level_utf8("PBR写出坏粗.level")));

	//金属度越界
	level.materials[0].pbr_roughness = 1.0f;
	level.materials[0].pbr_metallic = 1.5f;
	EXPECT_FALSE(engine::level_text_write(level, level_utf8("PBR写出坏金属.level")));

	//合法值可写出
	level.materials[0].pbr_metallic = 0.5f;
	EXPECT_TRUE(engine::level_text_write(level, level_utf8("PBR写出好.level")));
}

/*
实例 PBR 与 MSAA 组合（第五百八十一轮）
口径：1× 与 4× 采样下，实例 `pbr 0.2 0.9` 相对无覆盖均产生可见差异（> 20 像素）。
*/
TEST(Renderer_Level_Test, 实例PBR与MSAA组合)
{
	write_cube_assets();
	write_text(level_dir() / L"PBRMSAA甲.level",
		"level PBRMSAA甲\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 素 甲 tint 1 1 1 1\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"PBRMSAA乙.level",
		"level PBRMSAA乙\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 金属甲 甲 tint 1 1 1 1 pbr 0.2 0.9\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 金属甲 0 0 0 1 1 1\n");

	const uint32_t msaa_list[2] = { 1u, 4u };
	for (uint32_t msaa_index = 0; msaa_index < 2; ++msaa_index)
	{
		engine::Renderer_Config config;
		config.width = LEVEL_WIDTH;
		config.height = LEVEL_HEIGHT;
		config.hidden = true;
		config.msaa_samples = msaa_list[msaa_index];
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";

		//甲：无 PBR
		engine::Level_Data 甲;
		engine::Asset_Scene 场景甲;
		ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("PBRMSAA甲.level")));
		EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
		std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
		level_bridge_unload(renderer, 场景甲);

		//乙：实例 PBR
		engine::Level_Data 乙;
		engine::Asset_Scene 场景乙;
		ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("PBRMSAA乙.level")));
		EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
		std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
		const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
		std::cout << "[关卡] 实例 PBR " << msaa_list[msaa_index] << "x 差异 " << 差异 << std::endl;
		EXPECT_GT(差异, 20u) << "MSAA " << msaa_list[msaa_index] << "x 下实例 pbr 未生效";
		level_bridge_unload(renderer, 场景乙);
	}
}

/*
关卡材质实例查找（第五百六十三轮）
口径：`level_material_find` 按实例名取首个匹配，字段（底色 / 自发光 / 卡通）与关卡文本逐位一致；
空实例名与未登记名返回假，且不改写出参。
*/
TEST(Renderer_Level_Test, 关卡材质实例查找)
{
	write_text(level_dir() / L"实例查找.level",
		"level 实例查找\n"
		"material 红甲 甲 tint 0.8 0.2 0.2 1 emissive 0.35 toon 4 0.6 0.5\n"
		"material 素甲 甲\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例查找.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 2u);

	//命中：逐字段一致
	engine::Level_Material found;
	ASSERT_TRUE(engine::level_material_find(loaded, "红甲", found));
	EXPECT_EQ(found.实例名, "红甲");
	EXPECT_EQ(found.模板, "甲");
	EXPECT_TRUE(found.use_tint);
	EXPECT_TRUE(bit_equal(found.tint[0], 0.8f));
	EXPECT_TRUE(bit_equal(found.tint[2], 0.2f));
	EXPECT_TRUE(found.use_emissive);
	EXPECT_TRUE(bit_equal(found.emissive, 0.35f));
	EXPECT_TRUE(found.use_toon);
	EXPECT_TRUE(bit_equal(found.toon_steps, 4.0f));
	EXPECT_TRUE(bit_equal(found.toon_rim, 0.6f));
	EXPECT_TRUE(bit_equal(found.toon_specular, 0.5f));

	//缺省实例：两项覆盖为假
	engine::Level_Material plain;
	ASSERT_TRUE(engine::level_material_find(loaded, "素甲", plain));
	EXPECT_FALSE(plain.use_tint);
	EXPECT_FALSE(plain.use_emissive);
	EXPECT_FALSE(plain.use_toon);

	//未登记名：返回假且出参不变
	engine::Level_Material untouched;
	untouched.实例名 = "占位";
	EXPECT_FALSE(engine::level_material_find(loaded, "蓝甲", untouched));
	EXPECT_EQ(untouched.实例名, "占位");

	//空名：返回假
	EXPECT_FALSE(engine::level_material_find(loaded, "", untouched));
}

/*
关卡材质实例更新（第五百六十七轮）
口径：按实例名整体替换声明；替换后查找 / 写出 / 读回一致，未涉及项不变；
空名与未登记名返回假且关卡不变。
*/
TEST(Renderer_Level_Test, 关卡材质实例更新)
{
	write_text(level_dir() / L"实例更新.level",
		"level 实例更新\n"
		"material 红甲 甲 tint 0.8 0.2 0.2 1 emissive 0.35\n"
		"material 素甲 甲\n");
	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例更新.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 2u);

	//替换红甲：改底色 ＋ 增 PBR
	engine::Level_Material 新声明;
	新声明.实例名 = "红甲";
	新声明.模板 = "甲";
	新声明.use_tint = true;
	新声明.tint[0] = 0.2f;
	新声明.tint[1] = 0.4f;
	新声明.tint[2] = 0.9f;
	新声明.tint[3] = 1.0f;
	新声明.use_pbr = true;
	新声明.pbr_roughness = 0.3f;
	新声明.pbr_metallic = 0.8f;
	ASSERT_TRUE(engine::level_material_update(loaded, "红甲", 新声明));

	//替换后查找：新值生效、旧自发光被清掉
	engine::Level_Material after;
	ASSERT_TRUE(engine::level_material_find(loaded, "红甲", after));
	EXPECT_FALSE(after.use_emissive);
	EXPECT_TRUE(after.use_pbr);
	EXPECT_TRUE(bit_equal(after.pbr_roughness, 0.3f));
	EXPECT_TRUE(bit_equal(after.tint[2], 0.9f));
	//未涉及项：素甲不变
	EXPECT_FALSE(loaded.materials[1].use_tint);
	EXPECT_FALSE(loaded.materials[1].use_pbr);

	//写出再读回：替换值保留
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例更新_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例更新_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 2u);
	EXPECT_TRUE(reread.materials[0].use_pbr);
	EXPECT_TRUE(bit_equal(reread.materials[0].pbr_metallic, 0.8f));

	//未登记名 / 空名：返回假
	EXPECT_FALSE(engine::level_material_update(loaded, "蓝甲", 新声明));
	EXPECT_FALSE(engine::level_material_update(loaded, "", 新声明));
}

/*
关卡材质实例增删（第五百九十三轮）
口径：`level_material_add` 追加声明（重名 / 空名 / 达上限 256 条拒绝）；`level_material_remove` 按名删除首个匹配，
其余声明顺序不变；增删后写出再读回只剩剩余声明。
*/
TEST(Renderer_Level_Test, 关卡材质实例增删)
{
	write_text(level_dir() / L"实例增删.level",
		"level 实例增删\n"
		"material 红甲 甲 tint 0.8 0.2 0.2 1 emissive 0.35\n"
		"material 素甲 甲\n");
	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例增删.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 2u);

	//追加蓝甲：落到末尾且字段保留
	engine::Level_Material 新实例;
	新实例.实例名 = "蓝甲";
	新实例.模板 = "甲";
	新实例.use_tint = true;
	新实例.tint[0] = 0.2f;
	新实例.tint[2] = 0.9f;
	新实例.tint[3] = 1.0f;
	ASSERT_TRUE(engine::level_material_add(loaded, 新实例));
	ASSERT_EQ(loaded.materials.size(), 3u);
	EXPECT_EQ(loaded.materials[2].实例名, "蓝甲");
	engine::Level_Material found;
	ASSERT_TRUE(engine::level_material_find(loaded, "蓝甲", found));
	EXPECT_TRUE(bit_equal(found.tint[2], 0.9f));

	//重名 / 空名：返回假且声明数不变
	EXPECT_FALSE(engine::level_material_add(loaded, 新实例));
	engine::Level_Material 空名实例;
	EXPECT_FALSE(engine::level_material_add(loaded, 空名实例));
	EXPECT_EQ(loaded.materials.size(), 3u);

	//删除红甲：查找失败，其余顺序不变
	ASSERT_TRUE(engine::level_material_remove(loaded, "红甲"));
	ASSERT_EQ(loaded.materials.size(), 2u);
	EXPECT_EQ(loaded.materials[0].实例名, "素甲");
	EXPECT_EQ(loaded.materials[1].实例名, "蓝甲");
	EXPECT_FALSE(engine::level_material_find(loaded, "红甲", found));

	//重复删除 / 未登记名 / 空名：返回假
	EXPECT_FALSE(engine::level_material_remove(loaded, "红甲"));
	EXPECT_FALSE(engine::level_material_remove(loaded, "蓝乙"));
	EXPECT_FALSE(engine::level_material_remove(loaded, ""));

	//写出再读回：只剩素甲 / 蓝甲
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例增删_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例增删_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 2u);
	EXPECT_EQ(reread.materials[0].实例名, "素甲");
	EXPECT_EQ(reread.materials[1].实例名, "蓝甲");
	EXPECT_TRUE(reread.materials[1].use_tint);

	//上限：填满 256 条后追加被拒，删除后可再追加
	loaded.materials.clear();
	for (uint32_t index = 0; index < 256u; ++index)
	{
		engine::Level_Material 批量实例;
		批量实例.实例名 = "批" + std::to_string(index);
		批量实例.模板 = "甲";
		ASSERT_TRUE(engine::level_material_add(loaded, 批量实例));
	}
	engine::Level_Material 超额实例;
	超额实例.实例名 = "超额";
	超额实例.模板 = "甲";
	EXPECT_FALSE(engine::level_material_add(loaded, 超额实例));
	ASSERT_TRUE(engine::level_material_remove(loaded, "批0"));
	EXPECT_TRUE(engine::level_material_add(loaded, 超额实例));
	EXPECT_EQ(loaded.materials.size(), 256u);
}
