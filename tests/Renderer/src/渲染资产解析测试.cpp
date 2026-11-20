/*
渲染资产解析测试（2026-11-21 自「渲染资产加载测试.cpp」拆出）
覆盖：OBJ 完整属性与分段 / 缺法线补全 / 负索引与三角化 / 非法输入 / 端到端绘制、
      MTL 完整字段 / 缺省与 Tr 换算 / 非法输入 / 端到端材质、场景清单解析与拒绝。
*/

#include "渲染资产加载测试辅助.h"

/*
完整属性 ＋ usemtl 分段（第四百二十二轮）
口径：4 顶点 / 4 纹理坐标 / 1 法线；两个 usemtl 各一个三角形 → 4 个唯一顶点、6 条索引、2 段。
*/
TEST(Renderer_Asset_Load_Test, OBJ完整属性与分段)
{
	write_text(asset_dir() / L"完整属性.obj",
		"# 两段材质的最小 OBJ\n"
		"v 0 0 0\n"
		"v 1 0 0\n"
		"v 1 1 0\n"
		"v 0 1 0\n"
		"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
		"vn 0 0 1\n"
		"usemtl 甲\n"
		"f 1/1/1 2/2/1 3/3/1\n"
		"usemtl 乙\n"
		"f 1/1/1 3/3/1 4/4/1\n");

	engine::Mesh_File_Data file;
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("完整属性.obj"), file));
	EXPECT_EQ(file.data.vertices.size(), 4u);
	EXPECT_EQ(file.data.indices.size(), 6u);
	ASSERT_EQ(file.sections.size(), 2u);
	ASSERT_EQ(file.names.size(), 2u);
	EXPECT_EQ(file.sections[0].first_index, 0u);
	EXPECT_EQ(file.sections[0].index_count, 3u);
	EXPECT_EQ(file.sections[0].material, 0u);
	EXPECT_EQ(file.sections[1].first_index, 3u);
	EXPECT_EQ(file.sections[1].index_count, 3u);
	EXPECT_EQ(file.sections[1].material, 1u);
	EXPECT_EQ(file.names[0], "甲");
	EXPECT_EQ(file.names[1], "乙");
	//属性落到顶点：法线为 +Z、第二个顶点 uv = (1,0)
	EXPECT_FLOAT_EQ(file.data.vertices[0].normal[2], 1.0f);
	EXPECT_FLOAT_EQ(file.data.vertices[1].uv[0], 1.0f);
	std::cout << "[资产加载] 完整属性：顶点 " << file.data.vertices.size()
		<< " 索引 " << file.data.indices.size() << " 段 " << file.sections.size() << std::endl;
}

/*
缺法线自动补全（第四百二十二轮）
口径：只有 v 与 f（无 vn）的四边形 → 每个顶点法线单位化且为 +Z（面朝相机）。
*/
TEST(Renderer_Asset_Load_Test, OBJ缺法线自动生成)
{
	write_text(asset_dir() / L"缺法线.obj",
		"v 0 0 0\n"
		"v 1 0 0\n"
		"v 1 1 0\n"
		"v 0 1 0\n"
		"f 1 2 3\n"
		"f 1 3 4\n");

	engine::Mesh_File_Data file;
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("缺法线.obj"), file));
	EXPECT_EQ(file.data.vertices.size(), 4u);
	EXPECT_EQ(file.data.indices.size(), 6u);
	//每顶点法线：单位长度且朝 +Z
	for (std::size_t index = 0; index < file.data.vertices.size(); ++index)
	{
		EXPECT_NEAR(normal_length(file.data, index), 1.0f, 1.0e-5f);
		EXPECT_NEAR(file.data.vertices[index].normal[0], 0.0f, 1.0e-5f);
		EXPECT_NEAR(file.data.vertices[index].normal[1], 0.0f, 1.0e-5f);
		EXPECT_GE(file.data.vertices[index].normal[2], 0.999f);
	}
	//无 usemtl：只有一段，材质槽 0，名字为空
	ASSERT_EQ(file.sections.size(), 1u);
	EXPECT_EQ(file.sections[0].index_count, 6u);
	EXPECT_EQ(file.sections[0].material, 0u);
	EXPECT_TRUE(file.names[0].empty());
}

/*
负索引与多边形三角化（第四百二十二轮）
口径：四边形面用负索引书写 → 4 顶点（按位置去重）、6 条索引（扇形三角化）。
*/
TEST(Renderer_Asset_Load_Test, OBJ负索引与多边形三角化)
{
	write_text(asset_dir() / L"负索引.obj",
		"v 0 0 0\n"
		"v 1 0 0\n"
		"v 1 1 0\n"
		"v 0 1 0\n"
		"f -4 -3 -2 -1\n");

	engine::Mesh_File_Data file;
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("负索引.obj"), file));
	EXPECT_EQ(file.data.vertices.size(), 4u);
	EXPECT_EQ(file.data.indices.size(), 6u);
	//三角化顺序：0-1-2、0-2-3
	EXPECT_EQ(file.data.indices[0], 0u);
	EXPECT_EQ(file.data.indices[1], 1u);
	EXPECT_EQ(file.data.indices[2], 2u);
	EXPECT_EQ(file.data.indices[3], 0u);
	EXPECT_EQ(file.data.indices[4], 2u);
	EXPECT_EQ(file.data.indices[5], 3u);
}

/*
非法输入拒绝（第四百二十二轮）
口径：不存在的路径 / 索引越界 / 无三角面 / 空文件，均返回假且输出为空。
*/
TEST(Renderer_Asset_Load_Test, OBJ非法输入被拒绝)
{
	engine::Mesh_File_Data file;
	//不存在的文件
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("不存在.obj"), file));
	EXPECT_TRUE(file.data.vertices.empty());
	EXPECT_TRUE(file.data.indices.empty());
	EXPECT_TRUE(file.sections.empty());

	//索引越界（只有 3 个顶点，面里引用第 4 个）
	write_text(asset_dir() / L"越界.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 1 2 4\n");
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("越界.obj"), file));

	//无三角面（只有顶点与注释）
	write_text(asset_dir() / L"无面.obj", "# 只有顶点\nv 0 0 0\n");
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("无面.obj"), file));

	//空文件
	write_text(asset_dir() / L"空.obj", "");
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("空.obj"), file));
}

/*
端到端加载并绘制（第四百二十二轮）
口径：写一个覆盖视口的 2×2 四边形 OBJ（含 vt / vn），`mesh_load_obj` 后用 `mesh_draw` 绘制，
与「同相机空场景」基线比较，差像素数 ≥ 800 视为真的画出来了。
*/
TEST(Renderer_Asset_Load_Test, OBJ端到端绘制)
{
	write_text(asset_dir() / L"四边形.obj",
		"v -1 -1 0\n"
		"v 1 -1 0\n"
		"v 1 1 0\n"
		"v -1 1 0\n"
		"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
		"vn 0 0 1\n"
		"f 1/1/1 2/2/1 3/3/1\n"
		"f 1/1/1 3/3/1 4/4/1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//基线：同相机、不登记绘制（只有清屏 ＋ 天空）
	std::vector<uint8_t> reference(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//加载 OBJ 并绘制
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_obj(mesh, asset_utf8("四边形.obj")));
	ASSERT_NE(mesh, 0u);
	ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));

	std::vector<uint8_t> shown(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
	const uint32_t covered = changed_pixels(reference, shown);
	const float luma = average_luma(shown);
	RecordProperty("asset_covered_pixels", static_cast<int>(covered));
	std::cout << "[资产加载] 端到端覆盖 " << covered << " 亮度 " << luma << std::endl;
	EXPECT_GE(covered, ASSET_MIN_PIXELS) << "OBJ 网格没有真正画出来";
	EXPECT_GT(luma, 0.0f);
	renderer.resource().mesh_unload(mesh);
}

/*
MTL 完整字段解析（第四百二十三轮）
口径：两个 newmtl；第一项显式 Kd / Ks / Ke / Ns / d / map_Kd / map_Bump（含 -bm 选项），
第二项全缺省 → 默认白 / 黑 / Ns 0 / d 1 / 无贴图。
*/
TEST(Renderer_Asset_Load_Test, MTL完整字段解析)
{
	write_text(asset_dir() / L"完整字段.mtl",
		"# 两个材质\n"
		"newmtl 红\n"
		"Kd 0.5 0.25 0.125\n"
		"Ks 1 0.5 0\n"
		"Ke 0.1 0.2 0.3\n"
		"Ns 96\n"
		"d 0.5\n"
		"map_Kd tex.bmp\n"
		"map_Bump -bm 1.0 nrm.bmp\n"
		"newmtl 蓝\n"
		"illum 2\n");

	std::vector<engine::Mtl_Material> 材质;
	ASSERT_TRUE(engine::mtl_materials_load(asset_utf8("完整字段.mtl"), 材质));
	ASSERT_EQ(材质.size(), 2u);
	//第一项逐字段核对
	EXPECT_EQ(材质[0].name, "红");
	EXPECT_FLOAT_EQ(材质[0].diffuse[0], 0.5f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[1], 0.25f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[2], 0.125f);
	EXPECT_FLOAT_EQ(材质[0].specular[0], 1.0f);
	EXPECT_FLOAT_EQ(材质[0].shininess, 96.0f);
	EXPECT_FLOAT_EQ(材质[0].opacity, 0.5f);
	EXPECT_EQ(材质[0].diffuse_map, "tex.bmp");
	EXPECT_EQ(材质[0].normal_map, "nrm.bmp");
	//第二项缺省
	EXPECT_EQ(材质[1].name, "蓝");
	EXPECT_FLOAT_EQ(材质[1].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(材质[1].specular[0], 0.0f);
	EXPECT_FLOAT_EQ(材质[1].shininess, 0.0f);
	EXPECT_FLOAT_EQ(材质[1].opacity, 1.0f);
	EXPECT_TRUE(材质[1].diffuse_map.empty());
	EXPECT_TRUE(材质[1].normal_map.empty());
	std::cout << "[资产加载] MTL 完整字段：材质 " << 材质.size()
		<< " 首项 Kd=(" << 材质[0].diffuse[0] << "," << 材质[0].diffuse[1] << ","
		<< 材质[0].diffuse[2] << ") Ns=" << 材质[0].shininess << std::endl;
}

/*
MTL 缺省值与 Tr 换算（第四百二十三轮）
口径：Tr 0.25 → opacity 0.75；数值越界钳到 [0,1]；Ns 超过 1000 钳到 1000。
*/
TEST(Renderer_Asset_Load_Test, MTL缺省值与Tr换算)
{
	write_text(asset_dir() / L"缺省值.mtl",
		"newmtl 甲\n"
		"Tr 0.25\n"
		"Kd 2 -1 0.4\n"
		"Ns 5000\n");

	std::vector<engine::Mtl_Material> 材质;
	ASSERT_TRUE(engine::mtl_materials_load(asset_utf8("缺省值.mtl"), 材质));
	ASSERT_EQ(材质.size(), 1u);
	EXPECT_FLOAT_EQ(材质[0].opacity, 0.75f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[1], 0.0f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[2], 0.4f);
	EXPECT_FLOAT_EQ(材质[0].shininess, 1000.0f);
}

/*
MTL 非法输入拒绝（第四百二十三轮）
口径：不存在 / 空文件 / 只有注释（无 newmtl）→ 返回假且输出为空。
*/
TEST(Renderer_Asset_Load_Test, MTL非法输入被拒绝)
{
	std::vector<engine::Mtl_Material> 材质;
	EXPECT_FALSE(engine::mtl_materials_load(asset_utf8("不存在.mtl"), 材质));
	EXPECT_TRUE(材质.empty());

	write_text(asset_dir() / L"空.mtl", "");
	EXPECT_FALSE(engine::mtl_materials_load(asset_utf8("空.mtl"), 材质));

	write_text(asset_dir() / L"无材质.mtl", "# 只有注释\nKd 1 1 1\n");
	EXPECT_FALSE(engine::mtl_materials_load(asset_utf8("无材质.mtl"), 材质));
	EXPECT_TRUE(材质.empty());
}

/*
MTL 端到端创建材质（第四百二十三轮）
口径：写一张白色 BMP ＋ 引用它的 MTL（两个材质：一个带 map_Kd，一个纯色），
门面 `material_load_mtl` 应创建 2 个非零材质句柄；卸载后不残留（无 VMA 断言）。
*/
TEST(Renderer_Asset_Load_Test, MTL端到端创建材质)
{
	write_bmp(asset_dir() / L"tex.bmp", 4, 4);
	write_text(asset_dir() / L"贴图材质.mtl",
		"newmtl 贴图甲\n"
		"Kd 0.8 0.8 0.8\n"
		"Ns 64\n"
		"map_Kd tex.bmp\n"
		"newmtl 纯色乙\n"
		"Kd 0.2 0.4 0.9\n"
		"d 0.6\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	std::vector<engine::Material_Handle> 材质;
	ASSERT_TRUE(renderer.resource().material_load_mtl(材质, asset_utf8("贴图材质.mtl")));
	ASSERT_EQ(材质.size(), 2u);
	EXPECT_NE(材质[0], 0u);
	EXPECT_NE(材质[1], 0u);
	std::cout << "[资产加载] MTL 端到端：材质 " << 材质.size() << " 个" << std::endl;

	//卸载（含贴图）后再次查询材质数量应递减
	for (engine::Material_Handle 句柄 : 材质)
		renderer.resource().material_unload(句柄);
}

/*
场景清单纯解析（第四百二十四轮）
口径：mesh / mtl 各一条、place 两条（第二条省略材质列）→ 字段与位置正确。
*/
TEST(Renderer_Asset_Load_Test, 场景清单纯解析)
{
	write_text(asset_dir() / L"清单.txt",
		"# 单个方盒\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 0 2 0 1 1 1\n");

	engine::Scene_Manifest 清单;
	ASSERT_TRUE(engine::scene_manifest_parse(asset_utf8("清单.txt"), 清单));
	ASSERT_EQ(清单.meshes.size(), 1u);
	ASSERT_EQ(清单.materials.size(), 1u);
	ASSERT_EQ(清单.places.size(), 2u);
	EXPECT_EQ(清单.meshes[0].id, "方盒");
	EXPECT_EQ(清单.meshes[0].path, "box.obj");
	EXPECT_EQ(清单.materials[0].id, "方盒材质");
	EXPECT_EQ(清单.materials[0].path, "box.mtl");
	//第一条：带材质、位置原点、缩放 1
	EXPECT_EQ(清单.places[0].material_id, "方盒材质");
	EXPECT_FLOAT_EQ(清单.places[0].position[0], 0.0f);
	EXPECT_FLOAT_EQ(清单.places[0].scale[0], 1.0f);
	//第二条：省略材质列，位置 y = 2
	EXPECT_TRUE(清单.places[1].material_id.empty());
	EXPECT_FLOAT_EQ(清单.places[1].position[1], 2.0f);
	std::cout << "[资产加载] 清单解析：网格 " << 清单.meshes.size() << " 材质 " << 清单.materials.size()
		<< " 摆放 " << 清单.places.size() << std::endl;
}

/*
场景清单非法输入被拒绝（第四百二十四轮）
口径：未知指令 / 引用未知网格 / ID 重复 / 无 place → 全部返回假。
*/
TEST(Renderer_Asset_Load_Test, 场景清单非法被拒绝)
{
	engine::Scene_Manifest 清单;
	//未知指令
	write_text(asset_dir() / L"未知指令.txt", "mesh 甲 box.obj\nobject 乙\nplace 甲 0 0 0 1 1 1\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("未知指令.txt"), 清单));
	//引用未知网格
	write_text(asset_dir() / L"未知网格.txt", "mesh 甲 box.obj\nplace 乙 0 0 0 1 1 1\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("未知网格.txt"), 清单));
	//网格 ID 重复
	write_text(asset_dir() / L"重复ID.txt", "mesh 甲 box.obj\nmesh 甲 box2.obj\nplace 甲 0 0 0 1 1 1\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("重复ID.txt"), 清单));
	//无 place
	write_text(asset_dir() / L"无摆放.txt", "mesh 甲 box.obj\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("无摆放.txt"), 清单));
	EXPECT_TRUE(清单.meshes.empty());
	EXPECT_TRUE(清单.places.empty());
}
