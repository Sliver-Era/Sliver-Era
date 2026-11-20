/*
渲染资产共享与异步测试（2026-11-21 自「渲染资产加载测试.cpp」拆出）
覆盖：场景清单端到端装载、共享网格 / 纹理复用与引用计数、共享加载隔离与失败、中文目录加载、
      OBJ 解析吞吐、异步加载完成可绘制 / 失败 / 提交入参保护。
*/

#include "渲染资产加载测试辅助.h"

/*
场景清单端到端装载（第四百二十四轮）
口径：写方盒 OBJ ＋ 单材质 MTL ＋ 清单（2 个 place，一个带材质一个不带）；
`scene_load` 应建立 1 个网格、1 个材质、2 个对象；带材质对象可用 `mesh_draw_sections` 绘制。
*/
TEST(Renderer_Asset_Load_Test, 场景清单端到端装载)
{
	//方盒几何（2×2 四边形，朝 +Z）
	write_text(asset_dir() / L"box.obj",
		"v -1 -1 0\n"
		"v 1 -1 0\n"
		"v 1 1 0\n"
		"v -1 1 0\n"
		"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
		"vn 0 0 1\n"
		"f 1/1/1 2/2/1 3/3/1\n"
		"f 1/1/1 3/3/1 4/4/1\n");
	//单材质
	write_text(asset_dir() / L"box.mtl", "newmtl 甲\nKd 0.8 0.2 0.2\nNs 64\n");
	//清单：两个摆放（第二个省略材质列）
	write_text(asset_dir() / L"场景.txt",
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 0 2 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//基线（空场景）
	std::vector<uint8_t> reference(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	engine::Asset_Scene 场景;
	ASSERT_TRUE(scene_bundle_load(renderer, 场景, asset_utf8("场景.txt")));
	ASSERT_EQ(场景.meshes.size(), 1u);
	ASSERT_EQ(场景.materials.size(), 1u);
	ASSERT_EQ(场景.objects.size(), 2u);
	EXPECT_NE(场景.objects[0].mesh, 0u);
	ASSERT_EQ(场景.objects[0].materials.size(), 1u);
	EXPECT_TRUE(场景.objects[1].materials.empty());
	EXPECT_FLOAT_EQ(场景.objects[1].position[1], 2.0f);

	//用带材质对象绘制（逐段绑材质入口）
	const engine::Material_Handle* 材质 = 场景.objects[0].materials.data();
	ASSERT_TRUE(renderer.resource().mesh_draw_sections(场景.objects[0].mesh, nullptr,
		材质, static_cast<uint32_t>(场景.objects[0].materials.size())));
	std::vector<uint8_t> shown(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
	const uint32_t covered = changed_pixels(reference, shown);
	std::cout << "[资产加载] 清单端到端覆盖 " << covered << std::endl;
	EXPECT_GE(covered, ASSET_MIN_PIXELS) << "场景清单装载的网格没有画出来";

	//释放（幂等）
	level_bridge_unload(renderer, 场景);
	EXPECT_TRUE(场景.objects.empty());
	EXPECT_TRUE(场景.meshes.empty());
}

/*
共享网格复用与引用计数（第四百二十五轮）
口径：同路径两次共享加载 → 句柄相同、缓存条目 1；释放一次仍可绘制，释放归零后绘制失败且条目 0。
*/
TEST(Renderer_Asset_Load_Test, 共享网格复用与引用计数)
{
	write_text(asset_dir() / L"shared.obj",
		"v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
		"vn 0 0 1\n"
		"f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Mesh_Handle 甲 = 0;
	engine::Mesh_Handle 乙 = 0;
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 甲, asset_utf8("shared.obj")));
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 乙, asset_utf8("shared.obj")));
	EXPECT_EQ(甲, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	//大小写不同但归一化后同键 → 仍是同一资源
	engine::Mesh_Handle 丙 = 0;
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 丙, "out/资产测试/SHARED.OBJ"));
	EXPECT_EQ(丙, 甲);
	EXPECT_EQ(asset_cache_count(renderer), 1u);

	//释放一次：仍可绘制
	asset_mesh_release(renderer, 甲);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	EXPECT_TRUE(renderer.resource().mesh_draw(乙, nullptr));

	//释放归零：真正卸载，缓存条目清零
	asset_mesh_release(renderer, 乙);
	asset_mesh_release(renderer, 丙);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
	EXPECT_FALSE(renderer.resource().mesh_draw(丙, nullptr)) << "引用计数归零后网格仍可用";
	std::cout << "[资产加载] 共享网格：复用 3 次、释放归零后条目 0" << std::endl;
}

/*
共享加载的隔离与失败（第四百二十五轮）
口径：不同路径各自一份（条目 2）；不存在的路径加载失败且不新增条目。
*/
TEST(Renderer_Asset_Load_Test, 共享加载隔离与失败)
{
	write_text(asset_dir() / L"iso1.obj",
		"v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
	write_text(asset_dir() / L"iso2.obj",
		"v -1 -1 0\nv 1 -1 0\nv 0 1 0\nv 0 -1 0\nvn 0 0 1\nf 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Mesh_Handle 甲 = 0;
	engine::Mesh_Handle 乙 = 0;
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 甲, asset_utf8("iso1.obj")));
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 乙, asset_utf8("iso2.obj")));
	EXPECT_NE(甲, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 2u);

	//失败不入缓存
	engine::Mesh_Handle 缺失 = 0;
	EXPECT_FALSE(asset_mesh_shared_load(renderer, 缺失, asset_utf8("不存在.obj")));
	EXPECT_EQ(缺失, 0u);
	EXPECT_EQ(asset_cache_count(renderer), 2u);

	asset_mesh_release(renderer, 甲);
	asset_mesh_release(renderer, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
}

/*
共享纹理复用（第四百二十五轮）
口径：同一 BMP 两次共享加载 → 同一句柄、条目不重复；释放两次后条目清零。
*/
TEST(Renderer_Asset_Load_Test, 共享纹理复用与引用计数)
{
	write_bmp(asset_dir() / L"shared_tex.bmp", 4, 4);

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	uint64_t 甲 = 0;
	uint64_t 乙 = 0;
	ASSERT_TRUE(asset_texture_shared_load(renderer, 甲, asset_utf8("shared_tex.bmp")));
	ASSERT_TRUE(asset_texture_shared_load(renderer, 乙, asset_utf8("shared_tex.bmp")));
	EXPECT_EQ(甲, 乙);
	EXPECT_NE(甲, 0u);
	EXPECT_EQ(asset_cache_count(renderer), 1u);

	asset_texture_release(renderer, 甲);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	asset_texture_release(renderer, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
	std::cout << "[资产加载] 共享纹理：复用 2 次、释放归零后条目 0" << std::endl;
}

/*
纹理中文目录加载（第四百二十八轮）
口径：贴图放在中文目录（`out/资产测试/`），`texture_load_file` 与共享入口均应成功；释放后缓存条目归零。
*/
TEST(Renderer_Asset_Load_Test, 纹理中文目录加载)
{
	write_bmp(asset_dir() / L"中文贴图.bmp", 4, 4);

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	//普通入口
	uint64_t 句柄 = 0;
	ASSERT_TRUE(renderer.resource().texture_load_file(句柄, asset_utf8("中文贴图.bmp")));
	ASSERT_NE(句柄, 0u);
	renderer.resource().texture_unload(句柄);

	//共享入口（同一中文路径）
	uint64_t 共享 = 0;
	ASSERT_TRUE(asset_texture_shared_load(renderer, 共享, asset_utf8("中文贴图.bmp")));
	ASSERT_NE(共享, 0u);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	asset_texture_release(renderer, 共享);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
	std::cout << "[资产加载] 中文目录贴图可加载（句柄 " << 共享 << "）" << std::endl;
}

/*
OBJ 解析吞吐（第四百四十二轮 · 性能）
口径：现场生成 50000 三角形（150000 独立顶点、无 vt / vn）的 OBJ，仅测解析（不建 GPU 资源）；断言不高于 5000 ms。
*/
TEST(Renderer_Asset_Load_Test, OBJ解析吞吐)
{
	const uint32_t 三角形数 = 50000u;
	std::string 文本;
	文本.reserve(static_cast<std::size_t>(三角形数) * 3 * 28 + static_cast<std::size_t>(三角形数) * 20);
	//顶点（每个三角形三个独立顶点，坐标落在一个网格上）
	for (uint32_t 序号 = 0; 序号 < 三角形数 * 3; ++序号)
	{
		char 行[64] = {};
		std::snprintf(行, sizeof(行), "v %d %d %d\n",
			static_cast<int>(序号 % 97), static_cast<int>((序号 / 97) % 97), static_cast<int>(序号 % 13));
		文本 += 行;
	}
	//面（扇形三角化不触发，全为三角形）
	for (uint32_t 序号 = 0; 序号 < 三角形数; ++序号)
	{
		char 行[64] = {};
		std::snprintf(行, sizeof(行), "f %u %u %u\n", 序号 * 3 + 1, 序号 * 3 + 2, 序号 * 3 + 3);
		文本 += 行;
	}
	write_text(asset_dir() / L"吞吐.obj", 文本);

	engine::Mesh_File_Data 文件;
	const std::chrono::steady_clock::time_point 开始 = std::chrono::steady_clock::now();
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("吞吐.obj"), 文件));
	const std::chrono::steady_clock::time_point 结束 = std::chrono::steady_clock::now();
	const double 毫秒 = std::chrono::duration<double, std::milli>(结束 - 开始).count();
	EXPECT_EQ(文件.data.indices.size(), static_cast<std::size_t>(三角形数) * 3);
	std::cout << "[资产加载] 50000 三角形解析 " << 毫秒 << " ms" << std::endl;
	EXPECT_LT(毫秒, 5000.0);
}

/*
异步加载完成后可绘制（第四百二十六轮）
口径：提交后主线程轮询 ≤ 5 秒应就绪；句柄非 0 且可绘制；再次轮询返回同一句柄（不重建）。
*/
TEST(Renderer_Asset_Load_Test, 异步加载完成后可绘制)
{
	write_text(asset_dir() / L"async.obj",
		"v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
		"vn 0 0 1\n"
		"f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Asset_Async_Task 任务;
	ASSERT_TRUE(asset_mesh_async_load(renderer, 任务, asset_utf8("async.obj")));
	//在途任务不允许被覆盖
	EXPECT_FALSE(asset_mesh_async_load(renderer, 任务, asset_utf8("async.obj")));

	engine::Mesh_Handle 句柄 = 0;
	bool 就绪 = false;
	//最长等待 5 秒（每 2 ms 轮询一次）
	for (uint32_t 尝试 = 0; 尝试 < 2500 && !就绪; ++尝试)
	{
		就绪 = asset_mesh_async_poll(renderer, 任务, 句柄);
		if (!就绪)
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	ASSERT_TRUE(就绪) << "异步解析在 5 秒内未完成";
	ASSERT_NE(句柄, 0u);
	EXPECT_TRUE(renderer.resource().mesh_draw(句柄, nullptr));
	//幂等：再次轮询同一句柄
	engine::Mesh_Handle 再次 = 0;
	EXPECT_TRUE(asset_mesh_async_poll(renderer, 任务, 再次));
	EXPECT_EQ(再次, 句柄);
	std::cout << "[资产加载] 异步加载就绪，句柄 " << 句柄 << std::endl;
	renderer.resource().mesh_unload(句柄);
}

/*
异步加载失败（第四百二十六轮）
口径：不存在路径 → 提交成功（任务建立），轮询始终返回假、句柄保持 0。
*/
TEST(Renderer_Asset_Load_Test, 异步加载失败)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Asset_Async_Task 任务;
	ASSERT_TRUE(asset_mesh_async_load(renderer, 任务, asset_utf8("不存在_async.obj")));
	engine::Mesh_Handle 句柄 = 0;
	bool 就绪 = false;
	//最长等待 5 秒
	for (uint32_t 尝试 = 0; 尝试 < 2500 && !就绪; ++尝试)
	{
		就绪 = asset_mesh_async_poll(renderer, 任务, 句柄);
		if (!就绪)
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	EXPECT_FALSE(就绪) << "不存在的路径不应异步加载成功";
	EXPECT_EQ(句柄, 0u);
}

/*
异步提交的入参保护（第四百二十六轮）
口径：空路径 → 提交失败；已有任务 → 再次提交失败。
*/
TEST(Renderer_Asset_Load_Test, 异步提交入参保护)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Asset_Async_Task 任务;
	//空路径
	EXPECT_FALSE(asset_mesh_async_load(renderer, 任务, ""));
	//未建立任务时轮询返回假
	engine::Mesh_Handle 句柄 = 0;
	EXPECT_FALSE(asset_mesh_async_poll(renderer, 任务, 句柄));
	EXPECT_EQ(句柄, 0u);
}
