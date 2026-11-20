/*
渲染关卡文本测试（2026-11-21 自「渲染关卡测试.cpp」拆出）
覆盖：关卡文本往返 / 缺省补齐 / 非法拒绝 / 资产行保留、材质实例行（往返 / 缺省 / 越界 / 结构非法 / 重名）、
      材质实例装配与覆盖、卡通与 PBR 字段往返 / 非法 / 生效。
*/

#include "渲染关卡测试辅助.h"

/*
关卡文本往返（第四百二十九轮）
口径：写出后再读回，全字段（含浮点）逐位相等。
*/
TEST(Renderer_Level_Test, 关卡文本往返逐位一致)
{
	const engine::Level_Data source = rich_level();
	ASSERT_TRUE(engine::level_text_write(source, level_utf8("往返.level")));

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("往返.level"), loaded));
	expect_level_equal(source, loaded);
	std::cout << "[关卡] 往返：点光 " << loaded.light.point_count
		<< " 盏 / 聚光 " << loaded.light.spot_count
		<< " 盏 / 资产行 " << loaded.asset_lines.size() << std::endl;
}

/*
缺省补齐（第四百二十九轮）
口径：只写关卡名与相机位置，其余字段等于结构默认值。
*/
TEST(Renderer_Level_Test, 关卡缺省补齐)
{
	write_text(level_dir() / L"极简.level", "level 极简关卡\ncamera position 1 2 3\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("极简.level"), loaded));
	EXPECT_EQ(loaded.name, "极简关卡");
	EXPECT_EQ(loaded.camera.position[0], 1.0f);
	EXPECT_EQ(loaded.camera.position[1], 2.0f);
	EXPECT_EQ(loaded.camera.position[2], 3.0f);
	//相机其余字段
	EXPECT_EQ(loaded.camera.target[2], 0.0f);
	EXPECT_EQ(loaded.camera.fov, 1.0472f);
	EXPECT_EQ(loaded.camera.near_plane, 0.1f);
	EXPECT_EQ(loaded.camera.far_plane, 100.0f);
	EXPECT_FALSE(loaded.camera.ortho);
	EXPECT_EQ(loaded.camera.clear_color[0], 0.05f);
	//光照与雾默认
	EXPECT_EQ(loaded.light.intensity, 1.0f);
	EXPECT_EQ(loaded.light.direction[2], -1.0f);
	EXPECT_EQ(loaded.light.point_count, 0u);
	EXPECT_EQ(loaded.light.spot_count, 0u);
	EXPECT_EQ(loaded.fog.density, 0.0f);
	EXPECT_EQ(loaded.fog.end, 10.0f);
	EXPECT_TRUE(loaded.asset_lines.empty());
}

/*
非法文本拒绝（第四百二十九轮）
口径：任何一条非法记录 → 读取失败且输出清空（名称空、相机位置回落默认 0,0,3）。
*/
TEST(Renderer_Level_Test, 关卡非法被拒绝)
{
	//点光超限：9 盏
	std::string too_many_points = "level 超限\n";
	for (uint32_t index = 0; index < 9; ++index)
		too_many_points += "light point 0 0 0 1 1 1 5 1 0 1 0\n";

	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "未知段", "level 测试\nunknown_section 1 2 3\n" },
		{ "未知相机键", "level 测试\ncamera zoom 2\n" },
		{ "相机位置字段数不符", "level 测试\ncamera position 1 2\n" },
		{ "非有限值", "level 测试\ncamera fov nan\n" },
		{ "正交开关非 0/1", "level 测试\ncamera ortho 2\n" },
		{ "资产行缺路径", "level 测试\nmesh 甲\n" },
		{ "点光超 8 盏", too_many_points },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		//失败契约：输出清空
		EXPECT_TRUE(loaded.name.empty()) << item.first;
		EXPECT_EQ(loaded.camera.position[0], 0.0f) << item.first;
		EXPECT_EQ(loaded.camera.position[2], 3.0f) << item.first;
		EXPECT_EQ(loaded.light.point_count, 0u) << item.first;
	}
}

/*
资产声明行原样保留（第四百二十九轮）
口径：注释与空行被忽略；mesh / mtl / place 行按去首尾空白后的原文保存；写出再读回仍一致。
*/
TEST(Renderer_Level_Test, 关卡资产行原样保留)
{
	const std::string text =
		"# 关卡资产段落\n"
		"level 资产关卡\n"
		"\n"
		"mesh 甲  box.obj\n"
		"mtl 乙  甲.mtl\n"
		"place 甲 乙 0 1 2 1 1 1\n";

	write_text(level_dir() / L"资产.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("资产.level"), loaded));
	ASSERT_EQ(loaded.asset_lines.size(), 3u);
	EXPECT_EQ(loaded.asset_lines[0], "mesh 甲  box.obj");
	EXPECT_EQ(loaded.asset_lines[1], "mtl 乙  甲.mtl");
	EXPECT_EQ(loaded.asset_lines[2], "place 甲 乙 0 1 2 1 1 1");

	//写出再读回：资产行逐字相同
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("资产_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("资产_回写.level"), reread));
	ASSERT_EQ(reread.asset_lines.size(), 3u);
	for (std::size_t index = 0; index < reread.asset_lines.size(); ++index)
		EXPECT_EQ(reread.asset_lines[index], loaded.asset_lines[index]);
}

/*
材质实例行往返（第四百六十六轮；方向七阶段 B）
口径：`material` 行结构化保存、不进入 asset_lines；写出再读回逐字段（含浮点）逐位相等。
*/
TEST(Renderer_Level_Test, 关卡材质实例行往返)
{
	const std::string text =
		"level 实例关卡\n"
		"material 红 甲 tint 0.9 0.2 0.2 1\n"
		"material 亮 乙 tint 0.5 0.5 0.5 1 emissive 0.35\n"
		"mesh 甲 box.obj\n"
		"mtl 乙 甲.mtl\n"
		"place 甲 红 0 1 2 1 1 1\n";

	write_text(level_dir() / L"实例.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例.level"), loaded));
	//结构化：两条实例；asset_lines 仍只有 mesh / mtl / place 三行
	ASSERT_EQ(loaded.materials.size(), 2u);
	ASSERT_EQ(loaded.asset_lines.size(), 3u);
	//第一条：仅覆盖底色
	EXPECT_EQ(loaded.materials[0].实例名, "红");
	EXPECT_EQ(loaded.materials[0].模板, "甲");
	EXPECT_TRUE(loaded.materials[0].use_tint);
	EXPECT_FALSE(loaded.materials[0].use_emissive);
	EXPECT_TRUE(bit_equal(loaded.materials[0].tint[0], 0.9f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].tint[3], 1.0f));
	//第二条：底色 ＋ 自发光
	EXPECT_TRUE(loaded.materials[1].use_emissive);
	EXPECT_TRUE(bit_equal(loaded.materials[1].emissive, 0.35f));

	//写出再读回：实例逐字段相等
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 2u);
	for (std::size_t material_index = 0; material_index < reread.materials.size(); ++material_index)
	{
		const engine::Level_Material& left = loaded.materials[material_index];
		const engine::Level_Material& right = reread.materials[material_index];
		EXPECT_EQ(left.实例名, right.实例名);
		EXPECT_EQ(left.模板, right.模板);
		EXPECT_EQ(left.use_tint, right.use_tint);
		EXPECT_EQ(left.use_emissive, right.use_emissive);
		EXPECT_TRUE(bit_equal(left.emissive, right.emissive));
		for (std::size_t index = 0; index < 4; ++index)
			EXPECT_TRUE(bit_equal(left.tint[index], right.tint[index]));
	}
}

/*
材质实例缺省不覆盖（第四百六十六轮）
口径：只给实例名与模板名时，两项覆盖均为假，底色全 1、自发光 0。
*/
TEST(Renderer_Level_Test, 关卡材质实例缺省)
{
	write_text(level_dir() / L"实例缺省.level", "level 实例缺省\nmaterial 素 甲\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例缺省.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 1u);
	EXPECT_EQ(loaded.materials[0].实例名, "素");
	EXPECT_EQ(loaded.materials[0].模板, "甲");
	EXPECT_FALSE(loaded.materials[0].use_tint);
	EXPECT_FALSE(loaded.materials[0].use_emissive);
	for (std::size_t index = 0; index < 4; ++index)
		EXPECT_TRUE(bit_equal(loaded.materials[0].tint[index], 1.0f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].emissive, 0.0f));
	EXPECT_TRUE(loaded.asset_lines.empty());
}

/*
材质实例数值越界拒绝（第四百六十六轮）
口径：覆盖值必须有限且在 [0,1]；越界 / NaN / Inf 一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例数值越界)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "底色超上界", "level 越界\nmaterial 甲 乙 tint 1.5 0.2 0.2 1\n" },
		{ "底色为负", "level 越界\nmaterial 甲 乙 tint -0.1 0.2 0.2 1\n" },
		{ "底色 NaN", "level 越界\nmaterial 甲 乙 tint nan 0.2 0.2 1\n" },
		{ "自发光超上界", "level 越界\nmaterial 甲 乙 emissive 1.2\n" },
		{ "自发光为负", "level 越界\nmaterial 甲 乙 emissive -0.2\n" },
		{ "自发光 Inf", "level 越界\nmaterial 甲 乙 emissive inf\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		//失败契约：输出清空
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例结构非法拒绝（第四百六十六轮）
口径：未知覆盖项 / 缺值 / 缺模板名 / 顺序颠倒 / 多余字段一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例结构非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "缺模板名", "level 非法\nmaterial 甲\n" },
		{ "未知覆盖项", "level 非法\nmaterial 甲 乙 gloss 0.5\n" },
		{ "底色缺值", "level 非法\nmaterial 甲 乙 tint 0.2 0.3 0.4\n" },
		{ "顺序颠倒", "level 非法\nmaterial 甲 乙 emissive 0.5 tint 0.2 0.3 0.4 1\n" },
		{ "多余字段", "level 非法\nmaterial 甲 乙 tint 0.2 0.3 0.4 0.5 0.6\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例重名拒绝（第四百六十六轮）
口径：同一关卡内实例名唯一；重复即整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例重名)
{
	const std::string text =
		"level 重名\n"
		"material 甲 乙 tint 0.2 0.2 0.2 1\n"
		"material 甲 丙\n";

	write_text(level_dir() / L"实例重名.level", text);

	engine::Level_Data loaded;
	EXPECT_FALSE(engine::level_text_read(level_utf8("实例重名.level"), loaded));
	EXPECT_TRUE(loaded.materials.empty());
}

/*
材质实例装配拒绝（第四百六十七轮；方向七阶段 B-B）
口径：place 引用未声明实例 / 实例模板名未命中材质库 → 关卡装载整份失败，输出清空。
*/
TEST(Renderer_Level_Test, 关卡材质实例装配拒绝)
{
	write_box_assets();
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "引用未声明实例", "level 拒绝\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\nplace 方盒 蓝 0 0 0 1 1 1\n" },
		{ "模板名未命中", "level 拒绝\nmaterial 红 不存在\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\nplace 方盒 红 0 0 0 1 1 1\n" },
	};

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / L"实例装配.level", item.second);
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		EXPECT_FALSE(level_bridge_load(renderer, 关卡, 场景, level_utf8("实例装配.level"))) << item.first;
		//失败契约：关卡与场景输出清空
		EXPECT_TRUE(场景.objects.empty()) << item.first;
		EXPECT_TRUE(场景.meshes.empty()) << item.first;
		EXPECT_TRUE(场景.materials.empty()) << item.first;
	}
}

/*
材质实例等价装配（第四百六十七轮；方向七阶段 B-B）
口径：默认覆盖（tint 1/1/1/1、emissive 0）经实例装配，画面与「直接引用材质库」逐字节一致。
*/
TEST(Renderer_Level_Test, 关卡材质实例等价装配)
{
	write_box_assets();
	//直接引用材质库
	write_text(level_dir() / L"实例等价甲.level",
		"level 等价甲\n"
		"camera position 1 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");
	//同模板实例（默认覆盖）
	write_text(level_dir() / L"实例等价乙.level",
		"level 等价乙\n"
		"camera position 1 0 3\n"
		"material 红 甲 tint 1 1 1 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 红 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//空场景基线
	std::vector<uint8_t> reference(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//甲：直接引用材质库
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("实例等价甲.level")));
	ASSERT_EQ(场景甲.objects.size(), 1u);
	EXPECT_TRUE(场景甲.objects[0].material_instances.empty());
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：同模板实例（默认覆盖）
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("实例等价乙.level")));
	ASSERT_EQ(场景乙.objects.size(), 1u);
	ASSERT_EQ(场景乙.objects[0].material_instances.size(), 1u);
	//实例模板句柄与绑定句柄一致
	EXPECT_EQ(场景乙.objects[0].material_instances[0].模板, 场景乙.objects[0].materials[0]);
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	//默认覆盖等价：画面逐字节 0 差异
	EXPECT_EQ(changed_pixels(帧甲, 帧乙), 0u);
	level_bridge_unload(renderer, 场景乙);
}

/*
材质实例覆盖生效（第四百六十九轮；方向七阶段 C）
口径：同模板不同 tint → 画面差异明显；绘制后 tint / 自发光吸附状态复位（不污染后续绘制项）。
*/
TEST(Renderer_Level_Test, 关卡材质实例覆盖生效)
{
	write_box_assets();
	//实例：白色（默认覆盖）
	write_text(level_dir() / L"覆盖甲.level",
		"level 覆盖甲\n"
		"camera position 1 0 3\n"
		"material 素 甲 tint 1 1 1 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	//实例：深灰覆盖
	write_text(level_dir() / L"覆盖乙.level",
		"level 覆盖乙\n"
		"camera position 1 0 3\n"
		"material 暗 甲 tint 0.2 0.2 0.2 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 暗 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：默认覆盖
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("覆盖甲.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	//绘制后吸附状态复位
	EXPECT_FALSE(renderer.resource().material_tint_active());
	EXPECT_EQ(renderer.resource().material_emissive(), 0.0f);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：深灰覆盖
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("覆盖乙.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	//不同 tint → 明显差异
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例色调差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 200u) << "实例 tint 覆盖未生效";
	level_bridge_unload(renderer, 场景乙);
}

/*
材质实例自发光生效（第四百六十九轮）
口径：同模板 emissive 0 与 0.6 两个实例 → 画面差异明显。
*/
TEST(Renderer_Level_Test, 关卡材质实例自发光生效)
{
	write_box_assets();
	write_text(level_dir() / L"自发甲.level",
		"level 自发甲\n"
		"camera position 1 0 3\n"
		"material 素 甲 tint 0.2 0.2 0.2 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"自发乙.level",
		"level 自发乙\n"
		"camera position 1 0 3\n"
		"material 亮 甲 tint 0.2 0.2 0.2 1 emissive 0.6\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 亮 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：emissive 0
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("自发甲.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：emissive 0.6
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("自发乙.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例自发光差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "实例 emissive 覆盖未生效";
	level_bridge_unload(renderer, 场景乙);
}

/*
材质实例卡通覆盖往返（第四百七十一轮；方向七延伸）
口径：`toon` 三值结构化保存并随写出一并回读，字段逐位相等。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通往返)
{
	const std::string text =
		"level 卡通关卡\n"
		"material 卡通 甲 tint 0.8 0.8 0.8 1 emissive 0.1 toon 4 0.5 0.3\n"
		"mesh 甲 box.obj\n"
		"mtl 乙 甲.mtl\n"
		"place 甲 卡通 0 1 2 1 1 1\n";

	write_text(level_dir() / L"实例卡通.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例卡通.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 1u);
	ASSERT_EQ(loaded.asset_lines.size(), 3u);
	EXPECT_TRUE(loaded.materials[0].use_toon);
	EXPECT_TRUE(bit_equal(loaded.materials[0].toon_steps, 4.0f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].toon_rim, 0.5f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].toon_specular, 0.3f));

	//写出再读回
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例卡通_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例卡通_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 1u);
	EXPECT_TRUE(reread.materials[0].use_toon);
	EXPECT_TRUE(bit_equal(reread.materials[0].toon_steps, loaded.materials[0].toon_steps));
	EXPECT_TRUE(bit_equal(reread.materials[0].toon_rim, loaded.materials[0].toon_rim));
	EXPECT_TRUE(bit_equal(reread.materials[0].toon_specular, loaded.materials[0].toon_specular));
}

/*
材质实例卡通数值越界（第四百七十一轮）
口径：色阶 0..8、边缘光 / 高光 0..1；越界 / NaN 一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通越界)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "色阶超上限", "level 越界\nmaterial 甲 乙 toon 9 0.5 0.3\n" },
		{ "边缘光越界", "level 越界\nmaterial 甲 乙 toon 4 1.5 0.3\n" },
		{ "高光为负", "level 越界\nmaterial 甲 乙 toon 4 0.5 -0.1\n" },
		{ "色阶 NaN", "level 越界\nmaterial 甲 乙 toon nan 0.5 0.3\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例卡通结构非法（第四百七十一轮）
口径：toon 缺值 / 多余字段 / toon 后再出现 tint 或 emissive → 整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通结构非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "toon 缺值", "level 非法\nmaterial 甲 乙 toon 4 0.5\n" },
		{ "toon 多余字段", "level 非法\nmaterial 甲 乙 toon 4 0.5 0.3 0.9\n" },
		{ "toon 后再 tint", "level 非法\nmaterial 甲 乙 toon 4 0.5 0.3 tint 1 1 1 1\n" },
		{ "toon 后再 emissive", "level 非法\nmaterial 甲 乙 toon 4 0.5 0.3 emissive 0.2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例卡通生效（第四百七十二轮；方向七延伸）
口径：同模板「无卡通」与「toon 1 / 边缘光 1.0 / 高光 0.5」两个实例 → 画面差异明显。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通生效)
{
	write_cube_assets();
	write_text(level_dir() / L"卡通甲.level",
		"level 卡通甲\n"
		"camera position 1 0 3\n"
		"material 素 甲 tint 1 1 1 1\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"卡通乙.level",
		"level 卡通乙\n"
		"camera position 1 0 3\n"
		"material 纹 甲 tint 1 1 1 1 toon 1 1.0 0.5\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 纹 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：无卡通
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("卡通甲.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：实例卡通
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("卡通乙.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例卡通差异 " << 差异 << std::endl;
	//立方体各面照明接近饱和，色阶量化差异有限；可观测差异主要来自边缘光（阈值取 10）
	EXPECT_GT(差异, 10u) << "实例 toon 覆盖未生效";
	level_bridge_unload(renderer, 场景乙);
}

/*
材质实例 PBR 行往返（第五百六十四轮）
口径：`material` 行可带 `pbr <粗糙度> <金属度>`；结构化保存、写出再读回逐位一致。
*/
TEST(Renderer_Level_Test, 关卡材质实例PBR往返)
{
	const std::string text =
		"level PBR关卡\n"
		"material 金属 甲 tint 0.5 0.5 0.5 1 pbr 0.25 0.9\n"
		"material 素甲 甲\n";

	write_text(level_dir() / L"实例PBR.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例PBR.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 2u);
	EXPECT_TRUE(loaded.materials[0].use_pbr);
	EXPECT_TRUE(bit_equal(loaded.materials[0].pbr_roughness, 0.25f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].pbr_metallic, 0.9f));
	EXPECT_FALSE(loaded.materials[1].use_pbr);

	//写出再读回：PBR 三项一致
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例PBR_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例PBR_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 2u);
	EXPECT_EQ(reread.materials[0].use_pbr, loaded.materials[0].use_pbr);
	EXPECT_TRUE(bit_equal(reread.materials[0].pbr_roughness, loaded.materials[0].pbr_roughness));
	EXPECT_TRUE(bit_equal(reread.materials[0].pbr_metallic, loaded.materials[0].pbr_metallic));
}

/*
材质实例 PBR 非法（第五百六十四轮）
口径：粗糙度须落在 (0.05, 4]、金属度 [0, 1]；越界 / 缺字段 / 重复 / 顺序非法整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例PBR非法)
{
	//粗糙度下界等于 0.05（不含）→ 拒绝
	write_text(level_dir() / L"PBR下界.level",
		"level PBR下界\nmaterial 甲 模板 pbr 0.05 0.5\n");
	engine::Level_Data lower;
	EXPECT_FALSE(engine::level_text_read(level_utf8("PBR下界.level"), lower));
	//粗糙度上界越界 → 拒绝
	write_text(level_dir() / L"PBR上界.level",
		"level PBR上界\nmaterial 甲 模板 pbr 4.5 0.5\n");
	engine::Level_Data upper;
	EXPECT_FALSE(engine::level_text_read(level_utf8("PBR上界.level"), upper));
	//金属度越界 → 拒绝
	write_text(level_dir() / L"PBR金属.level",
		"level PBR金属\nmaterial 甲 模板 pbr 0.3 -0.1\n");
	engine::Level_Data metallic;
	EXPECT_FALSE(engine::level_text_read(level_utf8("PBR金属.level"), metallic));
	//缺一个字段 → 拒绝
	write_text(level_dir() / L"PBR缺字段.level",
		"level PBR缺字段\nmaterial 甲 模板 pbr 0.3\n");
	engine::Level_Data missing;
	EXPECT_FALSE(engine::level_text_read(level_utf8("PBR缺字段.level"), missing));
	//重复覆盖 → 拒绝
	write_text(level_dir() / L"PBR重复.level",
		"level PBR重复\nmaterial 甲 模板 pbr 0.3 0.5 pbr 0.4 0.6\n");
	engine::Level_Data repeated;
	EXPECT_FALSE(engine::level_text_read(level_utf8("PBR重复.level"), repeated));
	//顺序非法（pbr 之后出现 tint）→ 拒绝
	write_text(level_dir() / L"PBR顺序.level",
		"level PBR顺序\nmaterial 甲 模板 pbr 0.3 0.5 tint 1 1 1 1\n");
	engine::Level_Data order;
	EXPECT_FALSE(engine::level_text_read(level_utf8("PBR顺序.level"), order));
}

/*
材质实例 PBR 生效（第五百六十四轮）
口径：同模板「无 PBR」与「pbr 0.2 0.9」两个实例 → 画面差异明显（覆盖即启用 PBR）。
*/
TEST(Renderer_Level_Test, 关卡材质实例PBR生效)
{
	write_cube_assets();
	write_text(level_dir() / L"PBR甲.level",
		"level PBR甲\n"
		"camera position 1 0 3\n"
		"light pbr 1 1 0\n"
		"material 素 甲 tint 1 1 1 1\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"PBR乙.level",
		"level PBR乙\n"
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

	//甲：无 PBR
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 甲, 场景甲, level_utf8("PBR甲.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	level_bridge_unload(renderer, 场景甲);

	//乙：实例 PBR
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(level_bridge_load(renderer, 乙, 场景乙, level_utf8("PBR乙.level")));
	EXPECT_EQ(level_bridge_draw(renderer, 场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例 PBR 差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 20u) << "实例 pbr 覆盖未生效";
	level_bridge_unload(renderer, 场景乙);
}
