/*
渲染动画蒙皮资产测试（方向十 · 收敛轮 868）
覆盖：OBJ ＋ 侧车权重导入（几何与关节正确）、权重条数不符、关节越界、侧车缺失 / 非法 JSON。
口径：只验证纯数据组装，不创建 GPU 资源。
临时文件写在 `out/anim_skin_asset_test/`（ASCII 路径）。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/Asset/蒙皮资产.h"

namespace
{
	//临时目录
	const char* const TEST_DIR = "out/anim_skin_asset_test/";

	//写文本文件（目录自动创建）
	void write_text(const std::string& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(std::filesystem::path(TEST_DIR), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//单个三角形 OBJ
	std::string triangle_obj(void)
	{
		return "v -0.5 -0.4 0\nv -0.1 -0.4 0\nv -0.3 0.2 0\n"
			"vt 0 1\nvt 1 1\nvt 0.5 0\nvn 0 0 1\n"
			"f 1/1/1 2/2/1 3/3/1\n";
	}

	//三条权重项的合法侧车
	std::string skin_json(uint32_t bone_count, std::size_t count)
	{
		std::string text = "{\"bone_count\":" + std::to_string(bone_count) + ",\"weights\":[";
		for (std::size_t index = 0; index < count; ++index)
		{
			if (index > 0)
				text += ',';
			text += "{\"joint\":[0,1,0,0],\"weight\":[32768,32767,0,0]}";
		}
		text += "]}";
		return text;
	}
}

//渲染动画蒙皮资产测试夹具
class Renderer_Animation_SkinAsset_Test : public ::testing::Test
{
protected:
	void SetUp(void) override { write_text(std::string(TEST_DIR) + "tri.obj", triangle_obj()); }
	void TearDown(void) override
	{
		std::error_code code;
		std::filesystem::remove_all(std::filesystem::path(TEST_DIR), code);
	}
};

//正常导入：几何与关节权重逐项正确
TEST_F(Renderer_Animation_SkinAsset_Test, 合法资产导入)
{
	write_text(std::string(TEST_DIR) + "tri.skin.json", skin_json(2, 3));
	engine::Skinned_Mesh_Data data;
	uint32_t bone_count = 0;
	ASSERT_TRUE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "tri.skin.json", data, bone_count));
	EXPECT_EQ(bone_count, 2u);
	ASSERT_EQ(data.vertices.size(), 3u);
	ASSERT_EQ(data.indices.size(), 3u);
	//逐顶点关节与权重
	for (const engine::Skinned_Vertex& vertex : data.vertices)
	{
		EXPECT_EQ(vertex.joint[0], 0u);
		EXPECT_EQ(vertex.joint[1], 1u);
		EXPECT_EQ(vertex.weight[0], 32768u);
		EXPECT_EQ(vertex.weight[1], 32767u);
	}
	//几何位置取自 OBJ
	EXPECT_NEAR(data.vertices[0].position[0], -0.5f, 1e-5f);
}

//权重条数与顶点数不符：整体拒绝并清空
TEST_F(Renderer_Animation_SkinAsset_Test, 权重条数不符拒绝)
{
	write_text(std::string(TEST_DIR) + "tri.skin.json", skin_json(2, 2));
	engine::Skinned_Mesh_Data data;
	uint32_t bone_count = 7;
	EXPECT_FALSE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "tri.skin.json", data, bone_count));
	EXPECT_TRUE(data.vertices.empty());
	EXPECT_TRUE(data.indices.empty());
	EXPECT_EQ(bone_count, 0u);
}

//关节越界：声明 2 根骨骼但 joint 为 2
TEST_F(Renderer_Animation_SkinAsset_Test, 关节越界拒绝)
{
	write_text(std::string(TEST_DIR) + "tri.skin.json",
		R"({"bone_count":2,"weights":[
			{"joint":[2,0,0,0],"weight":[65535,0,0,0]},
			{"joint":[0,1,0,0],"weight":[32768,32767,0,0]},
			{"joint":[0,1,0,0],"weight":[16384,16384,0,0]}]})");
	engine::Skinned_Mesh_Data data;
	uint32_t bone_count = 0;
	EXPECT_FALSE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "tri.skin.json", data, bone_count));
	EXPECT_EQ(bone_count, 0u);
}

//侧车缺失 / 非法 JSON / 骨骼数越界：全部拒绝
TEST_F(Renderer_Animation_SkinAsset_Test, 非法侧车拒绝)
{
	engine::Skinned_Mesh_Data data;
	uint32_t bone_count = 0;
	//缺失
	EXPECT_FALSE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "missing.json", data, bone_count));
	//非法 JSON
	write_text(std::string(TEST_DIR) + "broken.json", "{ not json");
	EXPECT_FALSE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "broken.json", data, bone_count));
	//骨骼数越界（0 与超上限）
	write_text(std::string(TEST_DIR) + "zero.json", skin_json(0, 3));
	EXPECT_FALSE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "zero.json", data, bone_count));
	write_text(std::string(TEST_DIR) + "over.json", skin_json(65, 3));
	EXPECT_FALSE(engine::skinned_asset_load(std::string(TEST_DIR) + "tri.obj",
		std::string(TEST_DIR) + "over.json", data, bone_count));
	EXPECT_EQ(bone_count, 0u);
}
