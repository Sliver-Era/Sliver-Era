#pragma once
/*
骨骼测试辅助
提供标准双骨骼资产、临时资产文件写入、矩阵近似比较与公共夹具，
避免各用例重复构造资产与路径处理逻辑。
*/
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

//获取骨骼系统运行包
#include "Engine/EngineSystem/src/world/skeleton/骨骼系统运行包.h"
//获取骨骼数学工具
#include "Engine/EngineSystem/src/world/skeleton/Common/骨骼数学.h"
//获取可执行文件目录（临时文件基址）
#include "Engine/EngineFoundation/src/detail/methods/文件路径获取.h"
//获取路径字符串转换工具
#include "Engine/EngineFoundation/src/detail/methods/路径字符串转换.h"

//骨骼测试资产文件名
static const std::string skeleton_asset_file_name = "双骨骼资产.json";

//标准双骨骼资产（根骨骼绑定子部件 0，子骨骼绑定子部件 1）
static nlohmann::json skeleton_two_bone_json(void)
{
	//资产配置
	nlohmann::json config = nlohmann::json::object();
	config["name"] = "双骨骼";

	//骨骼数组
	nlohmann::json bones = nlohmann::json::array();

	//根骨骼
	nlohmann::json root = nlohmann::json::object();
	root["name"] = "root";
	root["parent"] = -1;
	root["part"] = nlohmann::json::object();
	root["part"]["part_ID"] = 0;
	bones.push_back(root);

	//子骨骼（沿 Y 轴偏移一个单位）
	nlohmann::json child = nlohmann::json::object();
	child["name"] = "child";
	child["parent"] = 0;
	child["translation"] = nlohmann::json::array({ 0.0, 1.0, 0.0 });
	child["part"] = nlohmann::json::object();
	child["part"]["part_ID"] = 1;
	child["part"]["translation"] = nlohmann::json::array({ 0.0, 0.5, 0.0 });
	bones.push_back(child);

	config["bones"] = bones;
	return config;
}

//临时资产文件写入（返回 UTF-8 绝对路径）
static std::string skeleton_asset_write(const std::string& file_name,
	const nlohmann::json& config)
{
	//临时目录
	std::filesystem::path directory = engine::detail::exe_dir_get()
		/ engine::detail::string_to_path("out") / engine::detail::string_to_path("骨骼测试");
	//确保目录存在
	std::error_code error_info;
	std::filesystem::create_directories(directory, error_info);
	//目标文件
	std::filesystem::path path = directory / engine::detail::string_to_path(file_name);
	//写入 JSON
	std::ofstream file(path, std::ios::binary);
	file << config.dump(2);
	file.close();
	//返回读取用路径
	return engine::detail::path_to_string(path);
}

//浮点近似比较
static bool skeleton_float_near(float left, float right, float epsilon = 1e-4f)
{
	return std::abs(left - right) <= epsilon;
}

//矩阵近似比较
static bool skeleton_matrix_near(const float* left, const float* right, float epsilon = 1e-4f)
{
	//逐元素比较
	for (uint32_t index = 0; index < 16; index++)
	{
		if (std::abs(left[index] - right[index]) > epsilon)
			return false;
	}
	return true;
}

//骨骼测试夹具
class Skeleton_Test : public ::testing::Test
{
protected:
	//ECS 存储
	engine::ECS_Store store;
	//骨骼系统
	engine::Skeleton_System skeleton;
};
