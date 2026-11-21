/*
骨骼资产测试
覆盖：合法资产解析（层级 / 名字索引 / 部件绑定）、逆绑定矩阵缺省推导与显式覆盖、
非法结构拒绝、文件加载成功与路径缺失。
*/
#include "骨骼测试辅助.h"

//合法资产解析
TEST(Skeleton_Asset_Test, 合法资产解析出层级与部件绑定)
{
	//构建资产
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));

	//基础字段
	EXPECT_TRUE(asset.valid());
	EXPECT_EQ(asset.name(), "双骨骼");
	EXPECT_EQ(asset.bone_count(), 2u);
	EXPECT_EQ(asset.bound_bone_count_get(), 2u);

	//名字索引
	EXPECT_EQ(asset.bone_seek("root"), 0);
	EXPECT_EQ(asset.bone_seek("child"), 1);
	EXPECT_EQ(asset.bone_seek("missing"), -1);

	//层级
	ASSERT_NE(asset.bone_get(1), nullptr);
	EXPECT_EQ(asset.bone_get(0)->parent, -1);
	EXPECT_EQ(asset.bone_get(1)->parent, 0);

	//部件绑定
	EXPECT_EQ(asset.bone_get(0)->part.part_ID, 0);
	EXPECT_EQ(asset.bone_get(1)->part.part_ID, 1);
	EXPECT_TRUE(skeleton_float_near(asset.bone_get(1)->part.translation[1], 0.5f));
}

//缺省逆绑定矩阵推导
TEST(Skeleton_Asset_Test, 缺省逆绑定矩阵由绑定姿态推导)
{
	//构建资产
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));

	//子骨骼绑定世界矩阵（父根为零变换，子骨骼沿 Y 偏移一个单位）
	const engine::Skeleton_Bone* child = asset.bone_get(1);
	float bind_world[16] = {};
	engine::detail::bone_matrix_from_transform(child->bind_translation,
		child->bind_rotation, child->bind_scale, bind_world);

	//绑定世界矩阵 × 逆绑定矩阵应为单位矩阵
	float product[16] = {};
	engine::detail::bone_matrix_multiply(bind_world, child->inverse_bind, product);
	EXPECT_TRUE(engine::detail::bone_matrix_identity_check(product, 1e-5f));
}

//显式逆绑定矩阵优先
TEST(Skeleton_Asset_Test, 显式逆绑定矩阵不被绑定姿态覆盖)
{
	//构造带显式单位逆绑定的资产
	nlohmann::json config = skeleton_two_bone_json();
	config["bones"][0]["translation"] = nlohmann::json::array({ 2.0, 0.0, 0.0 });
	config["bones"][0]["inverse_bind"] = nlohmann::json::array({
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 });

	//构建资产
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(config, asset));

	//单位逆绑定应原样保留
	float identity[16] = {};
	engine::detail::bone_matrix_identity(identity);
	EXPECT_TRUE(skeleton_matrix_near(asset.bone_get(0)->inverse_bind, identity));
}

//非法结构拒绝
TEST(Skeleton_Asset_Test, 非法结构被拒绝且输出清空)
{
	//拒绝判定
	auto expect_reject = [](const nlohmann::json& config, const char* label)
		{
			//待构建资产
			engine::Skeleton_Asset asset;
			//构建必须失败
			EXPECT_FALSE(engine::Skeleton_Asset::build(config, asset)) << label;
			//输出必须清空
			EXPECT_FALSE(asset.valid()) << label;
			EXPECT_EQ(asset.bone_count(), 0u) << label;
		};

	//空对象
	expect_reject(nlohmann::json::object(), "缺少骨骼数组");

	//骨骼数组为空
	nlohmann::json empty_bones = nlohmann::json::object();
	empty_bones["bones"] = nlohmann::json::array();
	expect_reject(empty_bones, "骨骼数组为空");

	//骨骼数组类型错误
	nlohmann::json wrong_type = nlohmann::json::object();
	wrong_type["bones"] = "not-array";
	expect_reject(wrong_type, "骨骼数组类型错误");

	//骨骼项非对象
	nlohmann::json wrong_item = skeleton_two_bone_json();
	wrong_item["bones"][0] = 1;
	expect_reject(wrong_item, "骨骼项非对象");

	//缺少骨骼名
	nlohmann::json missing_name = skeleton_two_bone_json();
	missing_name["bones"][1].erase("name");
	expect_reject(missing_name, "缺少骨骼名");

	//骨骼名重复
	nlohmann::json duplicate_name = skeleton_two_bone_json();
	duplicate_name["bones"][1]["name"] = "root";
	expect_reject(duplicate_name, "骨骼名重复");

	//父下标越界（不得大于等于自身）
	nlohmann::json wrong_parent = skeleton_two_bone_json();
	wrong_parent["bones"][1]["parent"] = 1;
	expect_reject(wrong_parent, "父下标越界");

	//零四元数
	nlohmann::json zero_rotation = skeleton_two_bone_json();
	zero_rotation["bones"][1]["rotation"] = nlohmann::json::array({ 0.0, 0.0, 0.0, 0.0 });
	expect_reject(zero_rotation, "零四元数");

	//零缩放
	nlohmann::json zero_scale = skeleton_two_bone_json();
	zero_scale["bones"][1]["scale"] = nlohmann::json::array({ 1.0, 0.0, 1.0 });
	expect_reject(zero_scale, "零缩放");

	//部件编号重复
	nlohmann::json duplicate_part = skeleton_two_bone_json();
	duplicate_part["bones"][1]["part"]["part_ID"] = 0;
	expect_reject(duplicate_part, "部件编号重复");

	//部件编号越界（哨兵值保留）
	nlohmann::json sentinel_part = skeleton_two_bone_json();
	sentinel_part["bones"][1]["part"]["part_ID"] = 0xFFFF;
	expect_reject(sentinel_part, "部件编号越界");

	//部件缺少编号
	nlohmann::json missing_part_ID = skeleton_two_bone_json();
	missing_part_ID["bones"][1]["part"].erase("part_ID");
	expect_reject(missing_part_ID, "部件缺少编号");

	//逆绑定矩阵长度错误
	nlohmann::json wrong_inverse = skeleton_two_bone_json();
	wrong_inverse["bones"][1]["inverse_bind"] = nlohmann::json::array({ 1.0, 0.0, 0.0 });
	expect_reject(wrong_inverse, "逆绑定矩阵长度错误");
}

//文件加载
TEST(Skeleton_Asset_Test, 文件加载成功且缺失路径失败)
{
	//写入临时资产文件
	std::string path = skeleton_asset_write(skeleton_asset_file_name, skeleton_two_bone_json());

	//加载成功
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::load_file(path, asset));
	EXPECT_TRUE(asset.valid());
	EXPECT_EQ(asset.bone_count(), 2u);

	//缺失路径失败并清空
	engine::Skeleton_Asset missing;
	EXPECT_FALSE(engine::Skeleton_Asset::load_file(
		engine::detail::path_to_string(engine::detail::exe_dir_get()
			/ engine::detail::string_to_path("out/骨骼测试/不存在.json")), missing));
	EXPECT_FALSE(missing.valid());
}
