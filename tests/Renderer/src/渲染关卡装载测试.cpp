/*
渲染关卡装载测试（2026-11-21 自「渲染关卡测试.cpp」拆出）
覆盖：读写失败路径、装载端到端与手工构造一致、装载失败回退、变换矩阵与摆放旋转、实例数据校验、
      装载开销与反复装载释放、格式版本与写出前校验、随机关卡往返、规模装载与大量摆放。
*/

#include "渲染关卡测试辅助.h"

/*
写入与读取失败路径（第四百二十九轮）
口径：空路径 / 不存在的文件 → 返回假；读取失败时输出清空。
*/
TEST(Renderer_Level_Test, 关卡读写失败路径)
{
	engine::Level_Data loaded = rich_level();
	EXPECT_FALSE(engine::level_text_read("", loaded));
	EXPECT_TRUE(loaded.name.empty());

	EXPECT_FALSE(engine::level_text_read(level_utf8("不存在.level"), loaded));
	EXPECT_TRUE(loaded.name.empty());

	EXPECT_FALSE(engine::level_text_write(loaded, ""));
}

/*
关卡装载端到端（第四百三十轮 · 阶段 B）
口径：关卡文件内嵌资产段落 → `level_load` 建立 1 网格 / 1 材质 / 1 对象；绘制后覆盖达标；
`level_save` 可把装载结果原样写回并可再读；`level_unload` 幂等释放。
*/
TEST(Renderer_Level_Test, 关卡装载端到端)
{
	write_box_assets();
	write_text(level_dir() / L"端到端.level",
		"level 端到端关卡\n"
		"camera position 0 0 3\n"
		"camera target 0 0 0\n"
		"light direction 0 0 -1\n"
		"light color 1 1 1\n"
		"light intensity 1\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//基线（空场景）
	std::vector<uint8_t> reference(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("端到端.level")));
	EXPECT_EQ(关卡.name, "端到端关卡");
	EXPECT_EQ(关卡.asset_lines.size(), 3u);
	ASSERT_EQ(场景.meshes.size(), 1u);
	ASSERT_EQ(场景.materials.size(), 1u);
	ASSERT_EQ(场景.objects.size(), 1u);
	ASSERT_EQ(场景.objects[0].materials.size(), 1u);

	//逐段绑材质绘制（门面 scene_draw）
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 1u);
	std::vector<uint8_t> shown(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
	const uint32_t covered = changed_pixels(reference, shown);
	std::cout << "[关卡] 端到端覆盖 " << covered << std::endl;
	EXPECT_GE(covered, LEVEL_MIN_PIXELS) << "关卡装载的网格没有画出来";

	//门面保存：写回后再读，名称与资产行数量一致
	ASSERT_TRUE(engine::level_text_write(关卡, level_utf8("端到端_保存.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("端到端_保存.level"), reread));
	EXPECT_EQ(reread.name, 关卡.name);
	EXPECT_EQ(reread.asset_lines.size(), 关卡.asset_lines.size());

	//幂等释放
	level_bridge_unload(renderer, 场景);
	EXPECT_TRUE(场景.objects.empty());
	EXPECT_TRUE(场景.meshes.empty());
}

/*
关卡与手工构造画面一致（第四百三十轮 · 阶段 B）
口径：同一关卡经「level_load」与「手工 scene_apply ＋ scene_load」两条路径出帧，逐字节一致。
*/
TEST(Renderer_Level_Test, 关卡与手工构造画面一致)
{
	write_box_assets();
	//关卡：带相机 / 光照 / 雾与一段资产
	write_text(level_dir() / L"等价.level",
		"level 等价关卡\n"
		"camera position 1 0 3\n"
		"camera target 0 0 0\n"
		"light direction 0.3 -1 0.2\n"
		"light color 1 0.9 0.8\n"
		"light intensity 0.8\n"
		"fog color 0.3 0.35 0.4\n"
		"fog density 0.2\n"
		"fog range 2 20\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");
	//等价清单（仅资产段落）
	write_text(level_dir() / L"等价清单.txt",
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//路径一：关卡装载
	engine::Level_Data 关卡;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景甲, level_utf8("等价.level")));
	ASSERT_EQ(场景甲.objects.size(), 1u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景甲), 1u);
	std::vector<uint8_t> 关卡帧(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(关卡帧.data(), static_cast<uint32_t>(关卡帧.size())));
	level_bridge_unload(renderer, 场景甲);

	//路径二：手工三组 ＋ 场景清单
	engine::Light_Data 关卡灯光 = 关卡.light;
	renderer.scene().scene_apply(关卡.camera, 关卡灯光, 关卡.fog);
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(scene_bundle_load(renderer, 场景乙, level_utf8("等价清单.txt")));
	ASSERT_EQ(场景乙.objects.size(), 1u);
	ASSERT_TRUE(renderer.resource().mesh_draw_sections(场景乙.objects[0].mesh, nullptr,
		场景乙.objects[0].materials.data(), static_cast<uint32_t>(场景乙.objects[0].materials.size())));
	std::vector<uint8_t> 手工帧(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(手工帧.data(), static_cast<uint32_t>(手工帧.size())));
	level_bridge_unload(renderer, 场景乙);

	//逐字节比较
	ASSERT_EQ(关卡帧.size(), 手工帧.size());
	uint32_t 差异字节 = 0;
	for (std::size_t index = 0; index < 关卡帧.size(); ++index)
	{
		if (关卡帧[index] != 手工帧[index])
			++差异字节;
	}
	EXPECT_EQ(差异字节, 0u) << "关卡链路与手工构造画面不一致";
}

/*
关卡装载失败回退（第四百三十轮 · 阶段 B）
口径：资产不存在 / 相机参数非法 → `level_load` 返回假，输出清空且资产不残留。
*/
TEST(Renderer_Level_Test, 关卡装载失败回退)
{
	//资产不存在
	write_text(level_dir() / L"缺失资产.level",
		"level 缺失资产\n"
		"mesh 方盒 不存在.obj\n"
		"place 方盒 0 0 0 1 1 1\n");
	//相机非法（视场角 0）
	write_text(level_dir() / L"非法相机.level", "level 非法相机\ncamera fov 0\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	EXPECT_FALSE(level_bridge_load(renderer, 关卡, 场景, level_utf8("缺失资产.level")));
	EXPECT_TRUE(关卡.name.empty());
	EXPECT_TRUE(场景.meshes.empty());
	EXPECT_TRUE(场景.materials.empty());
	EXPECT_TRUE(场景.objects.empty());

	EXPECT_FALSE(level_bridge_load(renderer, 关卡, 场景, level_utf8("非法相机.level")));
	EXPECT_TRUE(关卡.name.empty());
	EXPECT_TRUE(场景.objects.empty());
}

/*
变换矩阵构造（第四百三十一轮 · 阶段 C）
口径：旋转全零 → 对角缩放 ＋ 末列平移；绕 Z 90° → 局部 (1,0,0) 变为 (0,1,0)（缩放 2 倍、平移 (1,2,3)）。
*/
TEST(Renderer_Level_Test, 变换矩阵构造)
{
	//单位旋转：对角缩放 ＋ 平移
	{
		const float position[3] = { 1.0f, 2.0f, 3.0f };
		const float rotation[3] = { 0.0f, 0.0f, 0.0f };
		const float scale[3] = { 2.0f, 3.0f, 4.0f };
		float matrix[16] = {};
		engine::level_transform_build(position, rotation, scale, matrix);
		EXPECT_FLOAT_EQ(matrix[0], 2.0f);
		EXPECT_FLOAT_EQ(matrix[5], 3.0f);
		EXPECT_FLOAT_EQ(matrix[10], 4.0f);
		EXPECT_FLOAT_EQ(matrix[12], 1.0f);
		EXPECT_FLOAT_EQ(matrix[13], 2.0f);
		EXPECT_FLOAT_EQ(matrix[14], 3.0f);
		EXPECT_FLOAT_EQ(matrix[15], 1.0f);
		EXPECT_FLOAT_EQ(matrix[1], 0.0f);
	}
	//绕 Z 轴 90°（列主序；局部 +X 转到 +Y）
	{
		const float position[3] = { 1.0f, 2.0f, 3.0f };
		const float rotation[3] = { 0.0f, 0.0f, 90.0f };
		const float scale[3] = { 2.0f, 2.0f, 2.0f };
		float matrix[16] = {};
		engine::level_transform_build(position, rotation, scale, matrix);
		//列主序矩阵作用于局部点 (1,0,0)：结果应为缩放 2 倍后旋转到 +Y，再平移
		const float local[3] = { 1.0f, 0.0f, 0.0f };
		float world[3] = {};
		for (std::size_t row = 0; row < 3; ++row)
			world[row] = matrix[row] * local[0] + matrix[4 + row] * local[1]
				+ matrix[8 + row] * local[2] + matrix[12 + row];
		EXPECT_NEAR(world[0], 1.0f, 1e-6f);
		EXPECT_NEAR(world[1], 4.0f, 1e-6f);
		EXPECT_NEAR(world[2], 3.0f, 1e-6f);
	}
}

/*
摆放旋转（第四百三十一轮 · 阶段 C）
口径：`place` 行带旋转三轴 → 对象 `rotation_deg` 生效；零旋转用构造矩阵绘制与旧（nullptr）画面逐字节一致；
绕 Y 45° 后画面变化。
*/
TEST(Renderer_Level_Test, 摆放旋转)
{
	write_box_assets();
	//零旋转关卡
	write_text(level_dir() / L"旋转零.level",
		"level 零旋转\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1 0 0 0\n");
	//绕 Y 45°
	write_text(level_dir() / L"旋转四十五.level",
		"level 旋转四十五\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1 0 45 0\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//零旋转：对象字段为 0；用构造矩阵绘制与 nullptr 画面一致
	engine::Level_Data 零关卡;
	engine::Asset_Scene 零场景;
	ASSERT_TRUE(level_bridge_load(renderer, 零关卡, 零场景, level_utf8("旋转零.level")));
	ASSERT_EQ(零场景.objects.size(), 1u);
	EXPECT_EQ(零场景.objects[0].rotation_deg[1], 0.0f);
	std::vector<uint8_t> 旧画面(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.resource().mesh_draw_sections(零场景.objects[0].mesh, nullptr,
		零场景.objects[0].materials.data(), static_cast<uint32_t>(零场景.objects[0].materials.size())));
	ASSERT_TRUE(renderer.frame_render(旧画面.data(), static_cast<uint32_t>(旧画面.size())));
	level_bridge_unload(renderer, 零场景);

	//重新装载（对象已释放）并改用构造矩阵
	ASSERT_TRUE(level_bridge_load(renderer, 零关卡, 零场景, level_utf8("旋转零.level")));
	ASSERT_EQ(零场景.objects.size(), 1u);
	float 矩阵[16] = {};
	engine::level_transform_build(零场景.objects[0].position, 零场景.objects[0].rotation_deg,
		零场景.objects[0].scale, 矩阵);
	ASSERT_TRUE(renderer.resource().mesh_draw_sections(零场景.objects[0].mesh, 矩阵,
		零场景.objects[0].materials.data(), static_cast<uint32_t>(零场景.objects[0].materials.size())));
	std::vector<uint8_t> 矩阵画面(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(矩阵画面.data(), static_cast<uint32_t>(矩阵画面.size())));
	EXPECT_EQ(changed_pixels(旧画面, 矩阵画面), 0u) << "零旋转构造矩阵应与 nullptr 画面一致";
	level_bridge_unload(renderer, 零场景);

	//绕 Y 45°：对象字段 45；画面应变化
	engine::Level_Data 旋转关卡;
	engine::Asset_Scene 旋转场景;
	ASSERT_TRUE(level_bridge_load(renderer, 旋转关卡, 旋转场景, level_utf8("旋转四十五.level")));
	ASSERT_EQ(旋转场景.objects.size(), 1u);
	EXPECT_FLOAT_EQ(旋转场景.objects[0].rotation_deg[1], 45.0f);
	float 旋转矩阵[16] = {};
	engine::level_transform_build(旋转场景.objects[0].position, 旋转场景.objects[0].rotation_deg,
		旋转场景.objects[0].scale, 旋转矩阵);
	ASSERT_TRUE(renderer.resource().mesh_draw_sections(旋转场景.objects[0].mesh, 旋转矩阵,
		旋转场景.objects[0].materials.data(), static_cast<uint32_t>(旋转场景.objects[0].materials.size())));
	std::vector<uint8_t> 旋转画面(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(旋转画面.data(), static_cast<uint32_t>(旋转画面.size())));
	const uint32_t 差异 = changed_pixels(旧画面, 旋转画面);
	std::cout << "[关卡] 旋转 45° 画面差异 " << 差异 << " 像素" << std::endl;
	EXPECT_GT(差异, 50u) << "旋转未生效";
	level_bridge_unload(renderer, 旋转场景);
}

/*
材质实例数据校验（第四百四十八轮 · 方向七阶段 A）
口径：模板须为已登记材质；覆盖 tint / 自发光须有限且在 [0,1]；越界与非有限值一律拒绝；不建资源、不改状态。
*/
TEST(Renderer_Level_Test, 材质实例数据校验)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	//模板材质（默认描述）
	engine::Material_Data 描述;
	engine::Material_Handle 模板 = 0;
	ASSERT_TRUE(renderer.resource().material_load(模板, 描述));
	ASSERT_NE(模板, 0u);

	//合法实例（不覆盖 / 覆盖 tint / 覆盖自发光）
	engine::Material_Instance_Data 实例;
	实例.模板 = 模板;
	EXPECT_TRUE(renderer.resource().material_instance_validate(实例));
	实例.use_tint = true;
	实例.tint[0] = 0.2f;
	实例.tint[1] = 0.4f;
	实例.tint[2] = 0.6f;
	实例.tint[3] = 1.0f;
	EXPECT_TRUE(renderer.resource().material_instance_validate(实例));
	实例.use_emissive = true;
	实例.emissive = 0.8f;
	EXPECT_TRUE(renderer.resource().material_instance_validate(实例));

	//模板非法
	engine::Material_Instance_Data 坏模板 = 实例;
	坏模板.模板 = 0;
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏模板));
	坏模板.模板 = 9999;
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏模板));

	//覆盖越界 / 非有限
	engine::Material_Instance_Data 坏色 = 实例;
	坏色.tint[0] = 1.5f;
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏色));
	坏色.tint[0] = -0.1f;
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏色));
	坏色.tint[0] = std::nanf("");
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏色));
	engine::Material_Instance_Data 坏光 = 实例;
	坏光.emissive = -0.2f;
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏光));
	坏光.emissive = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(renderer.resource().material_instance_validate(坏光));

	renderer.resource().material_unload(模板);
}

/*
关卡装载开销（第四百三十三轮 · 阶段 E）
口径：单关卡（1 网格 ＋ 1 材质 ＋ 4 对象）连续装载三次，取最短耗时；给出实测数值并断言不高于 300 ms（防抖动上界）。
*/
TEST(Renderer_Level_Test, 关卡装载开销)
{
	write_box_assets();
	write_text(level_dir() / L"开销.level",
		"level 开销关卡\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 -2 0 0 1 1 1\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 方盒材质 2 0 0 1 1 1\n"
		"place 方盒 方盒材质 0 2 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	//预热一次（首轮包含管线 / 采样器等惰性创建）
	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("开销.level")));
	EXPECT_EQ(场景.objects.size(), 4u);
	level_bridge_unload(renderer, 场景);

	double 最短毫秒 = 1e9;
	for (uint32_t 次数 = 0; 次数 < 3; ++次数)
	{
		engine::Asset_Scene 测场景;
		const std::chrono::steady_clock::time_point 开始 = std::chrono::steady_clock::now();
		ASSERT_TRUE(level_bridge_load(renderer, 关卡, 测场景, level_utf8("开销.level")));
		const std::chrono::steady_clock::time_point 结束 = std::chrono::steady_clock::now();
		const double 毫秒 = std::chrono::duration<double, std::milli>(结束 - 开始).count();
		if (毫秒 < 最短毫秒)
			最短毫秒 = 毫秒;
		level_bridge_unload(renderer, 测场景);
	}
	std::cout << "[关卡] 装载开销（1 网格 + 1 材质 + 4 对象）最短 " << 最短毫秒 << " ms" << std::endl;
	EXPECT_LT(最短毫秒, 300.0) << "关卡装载耗时异常";
}

/*
关卡反复装载与释放（第四百三十四轮 · 稳定性）
口径：同一关卡连续装载 / 释放 200 次全部成功；结束后资产缓存条目为 0、无句柄残留。
*/
TEST(Renderer_Level_Test, 关卡反复装载释放)
{
	write_box_assets();
	write_text(level_dir() / L"反复.level",
		"level 反复关卡\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 方盒材质 2 0 0 1 1 1 0 30 0\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	for (uint32_t 轮次 = 0; 轮次 < 200; ++轮次)
	{
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("反复.level"))) << "第 " << 轮次 << " 次装载失败";
		ASSERT_EQ(场景.objects.size(), 2u);
		level_bridge_unload(renderer, 场景);
		EXPECT_EQ(asset_cache_count(renderer), 0u) << "第 " << 轮次 << " 次释放后仍有缓存条目";
	}
	std::cout << "[关卡] 反复装载释放 200 次完成" << std::endl;
}

/*
格式版本与写出前校验（第四百三十六轮）
口径：版本行写出并可读回；未知版本 / 版本 < 1 / 非有限字段 → 拒绝；缺省版本按 1。
*/
TEST(Renderer_Level_Test, 格式版本与写出前校验)
{
	//版本往返
	engine::Level_Data source = rich_level();
	source.version = 1;
	ASSERT_TRUE(engine::level_text_write(source, level_utf8("版本.level")));
	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("版本.level"), loaded));
	EXPECT_EQ(loaded.version, 1);

	//缺省版本按 1
	write_text(level_dir() / L"无版本.level", "level 无版本\ncamera position 0 0 3\n");
	engine::Level_Data legacy;
	ASSERT_TRUE(engine::level_text_read(level_utf8("无版本.level"), legacy));
	EXPECT_EQ(legacy.version, 1);

	//未知版本拒绝
	write_text(level_dir() / L"未来版本.level", "level 未来\nversion 2\n");
	engine::Level_Data future;
	EXPECT_FALSE(engine::level_text_read(level_utf8("未来版本.level"), future));
	EXPECT_TRUE(future.name.empty());

	//版本 0 不写出
	engine::Level_Data bad_version = rich_level();
	bad_version.version = 0;
	EXPECT_FALSE(engine::level_text_write(bad_version, level_utf8("坏版本.level")));

	//非有限字段不写出
	engine::Level_Data bad_value = rich_level();
	bad_value.fog.density = std::nanf("");
	EXPECT_FALSE(engine::level_text_write(bad_value, level_utf8("坏数值.level")));
	bad_value = rich_level();
	bad_value.light.point_shininess[0] = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(engine::level_text_write(bad_value, level_utf8("坏点光.level")));
}

/*
随机关卡往返（第四百四十轮 · 属性用例）
口径：固定种子生成 100 份关卡（随机相机 / 光照 / 雾 / 0–2 点光 / 0–1 聚光），写出再读回后全字段逐位相等。
*/
TEST(Renderer_Level_Test, 随机关卡往返逐位一致)
{
	//线性同余发生器（固定种子，结果可复现）
	uint32_t seed = 20261102u;
	auto next_unit = [&seed]()
	{
		seed = seed * 1664525u + 1013904223u;
		return static_cast<float>((seed >> 8) & 0xFFFFFFu) / 16777215.0f;
	};
	//区间随机
	auto next_range = [&next_unit](float low, float high)
	{
		return low + (high - low) * next_unit();
	};

	for (uint32_t 轮次 = 0; 轮次 < 100; ++轮次)
	{
		engine::Level_Data 随机;
		随机.name = "随机关卡" + std::to_string(轮次);
		//相机（合法域内随机）
		随机.camera.position[0] = next_range(-50.0f, 50.0f);
		随机.camera.position[1] = next_range(-10.0f, 30.0f);
		随机.camera.position[2] = next_range(-200.0f, 50.0f);
		随机.camera.target[0] = next_range(-20.0f, 20.0f);
		随机.camera.target[1] = next_range(-10.0f, 10.0f);
		随机.camera.target[2] = next_range(-200.0f, 0.0f);
		随机.camera.fov = next_range(0.2f, 2.8f);
		随机.camera.near_plane = next_range(0.01f, 1.0f);
		随机.camera.far_plane = next_range(20.0f, 2000.0f);
		随机.camera.ortho = (next_unit() < 0.3f);
		随机.camera.ortho_height = next_range(0.5f, 50.0f);
		随机.camera.aspect = next_range(0.0f, 3.0f);
		随机.camera.clear_color[0] = next_unit();
		随机.camera.clear_color[1] = next_unit();
		随机.camera.clear_color[2] = next_unit();
		随机.camera.clear_color[3] = next_unit();
		//光照
		随机.light.direction[0] = next_range(-1.0f, 1.0f);
		随机.light.direction[1] = next_range(-1.0f, 1.0f);
		随机.light.direction[2] = next_range(-1.0f, 1.0f);
		随机.light.color[0] = next_unit();
		随机.light.color[1] = next_unit();
		随机.light.color[2] = next_unit();
		随机.light.intensity = next_range(0.0f, 8.0f);
		随机.light.ambient = next_range(0.0f, 1.0f);
		随机.light.specular_intensity = next_range(0.0f, 2.0f);
		随机.light.specular_shininess = next_range(1.0f, 1000.0f);
		随机.light.attenuation_mode = static_cast<uint32_t>(next_range(0.0f, 2.99f));
		随机.light.shadow_enabled = (next_unit() < 0.5f) ? 1 : 0;
		随机.light.pbr_enabled = (next_unit() < 0.5f) ? 1 : 0;
		随机.light.toon_steps = static_cast<uint32_t>(next_range(0.0f, 8.99f));
		//点光 0..2 盏
		随机.light.point_count = static_cast<uint32_t>(next_range(0.0f, 2.99f));
		for (uint32_t index = 0; index < 随机.light.point_count; ++index)
		{
			随机.light.point_position[index][0] = next_range(-20.0f, 20.0f);
			随机.light.point_position[index][1] = next_range(0.0f, 20.0f);
			随机.light.point_position[index][2] = next_range(-50.0f, 0.0f);
			随机.light.point_color[index][0] = next_unit();
			随机.light.point_color[index][1] = next_unit();
			随机.light.point_color[index][2] = next_unit();
			随机.light.point_radius[index] = next_range(0.5f, 30.0f);
			随机.light.point_intensity[index] = next_range(0.0f, 5.0f);
			随机.light.point_attenuation[index] = static_cast<uint32_t>(next_range(0.0f, 2.99f));
			随机.light.point_specular[index] = next_range(0.0f, 1.0f);
			随机.light.point_shininess[index] = next_range(0.0f, 500.0f);
		}
		//聚光 0..1 盏
		随机.light.spot_count = (next_unit() < 0.5f) ? 1u : 0u;
		if (随机.light.spot_count == 1u)
		{
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				随机.light.spot_position[0][axis] = next_range(-20.0f, 20.0f);
				随机.light.spot_direction[0][axis] = next_range(-1.0f, 1.0f);
				随机.light.spot_color[0][axis] = next_unit();
			}
			随机.light.spot_radius[0] = next_range(1.0f, 40.0f);
			随机.light.spot_intensity[0] = next_range(0.0f, 6.0f);
			随机.light.spot_inner_deg[0] = next_range(1.0f, 30.0f);
			随机.light.spot_outer_deg[0] = next_range(31.0f, 90.0f);
			随机.light.spot_attenuation[0] = static_cast<uint32_t>(next_range(0.0f, 2.99f));
			随机.light.spot_specular[0] = next_range(0.0f, 1.0f);
			随机.light.spot_shininess[0] = next_range(0.0f, 500.0f);
		}
		//雾
		随机.fog.color[0] = next_unit();
		随机.fog.color[1] = next_unit();
		随机.fog.color[2] = next_unit();
		随机.fog.density = next_range(0.0f, 1.0f);
		随机.fog.start = next_range(0.0f, 5.0f);
		随机.fog.end = next_range(6.0f, 500.0f);
		随机.fog.mode = static_cast<uint32_t>(next_range(0.0f, 1.99f));
		随机.fog.height_falloff = next_range(0.0f, 1.0f);
		//材质实例 0..2 条（随机覆盖项；第五百八十三轮）
		const uint32_t 实例数 = static_cast<uint32_t>(next_range(0.0f, 2.99f));
		for (uint32_t index = 0; index < 实例数; ++index)
		{
			engine::Level_Material material;
			material.实例名 = "实例" + std::to_string(index);
			material.模板 = "甲";
			material.use_tint = (next_unit() < 0.5f);
			//未启用项保持默认值（写出不落、读回即默认，逐位比较才成立）
			if (material.use_tint)
				for (uint32_t channel = 0; channel < 4; ++channel)
					material.tint[channel] = next_unit();
			material.use_emissive = (next_unit() < 0.5f);
			if (material.use_emissive)
				material.emissive = next_unit();
			material.use_toon = (next_unit() < 0.5f);
			if (material.use_toon)
			{
				material.toon_steps = static_cast<float>(static_cast<uint32_t>(next_range(0.0f, 8.99f)));
				material.toon_rim = next_unit();
				material.toon_specular = next_unit();
			}
			material.use_pbr = (next_unit() < 0.5f);
			if (material.use_pbr)
			{
				material.pbr_roughness = next_range(0.06f, 4.0f);
				material.pbr_metallic = next_unit();
			}
			material.use_clearcoat = (next_unit() < 0.5f);
			if (material.use_clearcoat)
			{
				material.clear_coat = next_range(0.01f, 1.0f);
				material.clear_coat_roughness = next_range(0.03f, 1.0f);
			}
			随机.materials.push_back(material);
		}
		//固定资产段落
		随机.asset_lines.push_back("mesh 甲 box.obj");
		随机.asset_lines.push_back("place 甲 0 0 0 1 1 1 0 30 0");

		ASSERT_TRUE(engine::level_text_write(随机, level_utf8("随机.level")));
		engine::Level_Data 读回;
		ASSERT_TRUE(engine::level_text_read(level_utf8("随机.level"), 读回)) << "第 " << 轮次 << " 份随机关卡读取失败";
		expect_level_equal(随机, 读回);
	}
	std::cout << "[关卡] 随机往返 100 份完成" << std::endl;
}

/*
关卡规模装载开销（第四百四十二轮 · 性能）
口径：同一网格摆放 100 / 400 / 1000 个对象，各装载两次取最短；断言不高于 1000 ms（防抖动上界）。
*/
TEST(Renderer_Level_Test, 关卡规模装载开销)
{
	write_box_assets();

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	for (uint32_t 数量 : { 100u, 400u, 1000u })
	{
		//生成关卡文本（同一网格 / 材质）
		std::string 文本 = "level 规模\ncamera position 0 0 8\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\n";
		文本.reserve(文本.size() + static_cast<std::size_t>(数量) * 64);
		for (uint32_t 序号 = 0; 序号 < 数量; ++序号)
		{
			char 行[96] = {};
			std::snprintf(行, sizeof(行), "place 方盒 方盒材质 %d %d 0 1 1 1 0 %d 0\n",
				static_cast<int>(序号 % 20) - 10, static_cast<int>(序号 / 20) - 10,
				static_cast<int>(序号 % 7) * 5);
			文本 += 行;
		}
		write_text(level_dir() / L"规模.level", 文本);

		//预热一次
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("规模.level")));
		ASSERT_EQ(场景.objects.size(), 数量);
		level_bridge_unload(renderer, 场景);

		double 最短毫秒 = 1e9;
		for (uint32_t 次数 = 0; 次数 < 2; ++次数)
		{
			engine::Asset_Scene 测场景;
			const std::chrono::steady_clock::time_point 开始 = std::chrono::steady_clock::now();
			ASSERT_TRUE(level_bridge_load(renderer, 关卡, 测场景, level_utf8("规模.level")));
			const std::chrono::steady_clock::time_point 结束 = std::chrono::steady_clock::now();
			const double 毫秒 = std::chrono::duration<double, std::milli>(结束 - 开始).count();
			if (毫秒 < 最短毫秒)
				最短毫秒 = 毫秒;
			level_bridge_unload(renderer, 测场景);
		}
		std::cout << "[关卡] " << 数量 << " 摆放装载最短 " << 最短毫秒 << " ms" << std::endl;
		EXPECT_LT(最短毫秒, 1000.0);
	}
}

/*
关卡大量摆放（第四百三十四轮 · 稳定性）
口径：同一网格 / 材质摆放 20 个对象（含旋转）；对象数、句柄复用与绘制登记量正确。
*/
TEST(Renderer_Level_Test, 关卡大量摆放)
{
	write_box_assets();
	std::string 文本 = "level 大量摆放\ncamera position 0 0 6\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\n";
	for (uint32_t 序号 = 0; 序号 < 20; ++序号)
	{
		char 行[128] = {};
		std::snprintf(行, sizeof(行), "place 方盒 方盒材质 %d %d 0 1 1 1 0 %d 0\n",
			static_cast<int>(序号 % 5) - 2, static_cast<int>(序号 / 5) - 2, static_cast<int>(序号) * 9);
		文本 += 行;
	}
	write_text(level_dir() / L"大量摆放.level", 文本);

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("大量摆放.level")));
	EXPECT_EQ(场景.meshes.size(), 1u);
	EXPECT_EQ(场景.materials.size(), 1u);
	ASSERT_EQ(场景.objects.size(), 20u);
	//全部对象共享同一网格句柄
	for (const engine::Asset_Object& 对象 : 场景.objects)
		EXPECT_EQ(对象.mesh, 场景.meshes[0]);

	//逐对象登记绘制（同一网格 20 份；门面 scene_draw）
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 20u);
	//出帧一次结算帧统计
	std::vector<uint8_t> 像素(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(像素.data(), static_cast<uint32_t>(像素.size())));
	const engine::Frame_Stats 统计 = renderer.frame_stats_get();
	std::cout << "[关卡] 大量摆放绘制项 " << 统计.drawn_items << std::endl;
	EXPECT_GE(统计.drawn_items, 1u);

	level_bridge_unload(renderer, 场景);
	EXPECT_TRUE(场景.objects.empty());
}
