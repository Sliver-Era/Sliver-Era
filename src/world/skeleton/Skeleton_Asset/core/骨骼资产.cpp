#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取文件路径校验工具
#include "Engine/EngineFoundation/src/detail/methods/文件路径可用性校验.h"
//获取骨骼数学工具
#include "src/world/skeleton/Common/骨骼数学.h"

//游戏引擎命名空间
namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//骨骼数量上限
		constexpr uint64_t bone_limit = 4096;
		//资产文件体量上限（字节）
		constexpr uintmax_t file_limit = 16ull * 1024ull * 1024ull;
		//子部件编号哨兵值（与碰撞模块一致）
		constexpr uint64_t part_ID_sentinel = 0xFFFF;

		//字符串字段读取
		bool string_read(const json& config, const char* field, string& receiver)
		{
			//字段存在性与类型检查
			if (!config.contains(field) || !config[field].is_string())
				return false;
			//空字符串非法
			if (config[field].get_ref<const string&>().empty())
				return false;
			//写入结果
			receiver = config[field].get<string>();
			return true;
		}

		//整数字段读取
		bool integer_read(const json& config, const char* field, int64_t& receiver)
		{
			//字段存在性与类型检查
			if (!config.contains(field) || !config[field].is_number_integer())
				return false;
			//写入结果
			receiver = config[field].get<int64_t>();
			return true;
		}

		//浮点数组读取（长度须精确匹配、逐项有限）
		bool number_array_read(const json& config, const char* field, float* receiver, uint64_t count)
		{
			//字段存在性检查
			if (!config.contains(field))
				return false;
			//数组格式检查
			if (!config[field].is_array() || config[field].size() != count)
				return false;
			//逐项读取
			for (uint64_t index = 0; index < count; index++)
			{
				//元素必须为数值
				if (!config[field][index].is_number())
					return false;
				//写入并检查有限性
				float value = config[field][index].get<float>();
				if (!std::isfinite(value))
					return false;
				receiver[index] = value;
			}
			return true;
		}

		/*
		变换读取（平移 / 旋转 / 缩放均可选）
		旋转必须为非零四元数，读取后单位化；缩放分量必须有限且非零。
		*/
		bool transform_read(const json& config, float translation[3], float rotation[4], float scale[3])
		{
			//平移（可选）
			if (config.contains("translation"))
			{
				if (!number_array_read(config, "translation", translation, 3))
					return false;
			}
			//旋转（可选）
			if (config.contains("rotation"))
			{
				if (!number_array_read(config, "rotation", rotation, 4))
					return false;
				//零四元数非法
				float length2 = rotation[0] * rotation[0] + rotation[1] * rotation[1]
					+ rotation[2] * rotation[2] + rotation[3] * rotation[3];
				if (length2 <= 0.0f)
					return false;
				//单位化
				float normalized[4] = {};
				detail::bone_quaternion_normalize(rotation, normalized);
				for (uint32_t index = 0; index < 4; index++)
					rotation[index] = normalized[index];
			}
			//缩放（可选）
			if (config.contains("scale"))
			{
				if (!number_array_read(config, "scale", scale, 3))
					return false;
				//零缩放不可逆
				for (uint32_t index = 0; index < 3; index++)
				{
					if (scale[index] == 0.0f)
						return false;
				}
			}
			return true;
		}

		//单根骨骼解析
		bool bone_parse(const json& config, const uint32_t& index,
			const vector<float>& bind_world, Skeleton_Bone& receiver,
			unordered_map<string, uint32_t>& indices, unordered_set<uint16_t>& part_IDs,
			uint32_t& bound_bone_count)
		{
			//骨骼项必须为对象
			if (!config.is_object())
			{
				logger.warn("Skeleton_Asset::第 {} 根骨骼不是对象", index);
				return false;
			}

			//待解析骨骼
			Skeleton_Bone bone;
			//骨骼名
			if (!string_read(config, "name", bone.name))
			{
				logger.warn("Skeleton_Asset::第 {} 根骨骼缺少有效字段(name)", index);
				return false;
			}
			//骨骼名重复
			if (indices.count(bone.name) != 0)
			{
				logger.warn("Skeleton_Asset::骨骼名({})重复", bone.name);
				return false;
			}

			//父骨骼下标（可选，缺省为根）
			if (config.contains("parent"))
			{
				//父下标
				int64_t parent = -1;
				//读取父下标
				if (!integer_read(config, "parent", parent))
				{
					logger.warn("Skeleton_Asset::骨骼({})字段(parent)非法", bone.name);
					return false;
				}
				//父下标必须小于自身下标（保证父先于子求值）
				if (parent < -1 || parent >= static_cast<int64_t>(index))
				{
					logger.warn("Skeleton_Asset::骨骼({})字段(parent={})越界", bone.name, parent);
					return false;
				}
				//写入父下标
				bone.parent = static_cast<int32_t>(parent);
			}

			//绑定局部变换
			if (!transform_read(config, bone.bind_translation, bone.bind_rotation, bone.bind_scale))
			{
				logger.warn("Skeleton_Asset::骨骼({})绑定变换非法", bone.name);
				return false;
			}

			//绑定局部矩阵
			float local[16] = {};
			detail::bone_matrix_from_transform(bone.bind_translation, bone.bind_rotation,
				bone.bind_scale, local);
			//绑定世界矩阵（父世界 × 局部）
			float world[16] = {};
			if (bone.parent < 0)
			{
				for (uint32_t element = 0; element < 16; element++)
					world[element] = local[element];
			}
			else
				detail::bone_matrix_multiply(&bind_world[static_cast<uint64_t>(bone.parent) * 16],
					local, world);

			//逆绑定矩阵（显式提供或由绑定世界矩阵推导）
			if (config.contains("inverse_bind"))
			{
				//读取显式逆绑定矩阵
				if (!number_array_read(config, "inverse_bind", bone.inverse_bind, 16))
				{
					logger.warn("Skeleton_Asset::骨骼({})字段(inverse_bind)非法", bone.name);
					return false;
				}
			}
			//缺省推导
			else if (!detail::bone_matrix_inverse(world, bone.inverse_bind))
			{
				logger.warn("Skeleton_Asset::骨骼({})绑定世界矩阵不可逆", bone.name);
				return false;
			}

			//碰撞子部件绑定（可选）
			if (config.contains("part"))
			{
				//绑定项必须为对象
				const json& part = config["part"];
				if (!part.is_object())
				{
					logger.warn("Skeleton_Asset::骨骼({})字段(part)非法", bone.name);
					return false;
				}
				//子部件编号
				int64_t part_ID = 0;
				if (!integer_read(part, "part_ID", part_ID))
				{
					logger.warn("Skeleton_Asset::骨骼({})缺少有效字段(part_ID)", bone.name);
					return false;
				}
				//编号范围（0xFFFF 保留为未绑定哨兵）
				if (part_ID < 0 || part_ID >= static_cast<int64_t>(part_ID_sentinel))
				{
					logger.warn("Skeleton_Asset::骨骼({})字段(part_ID={})越界", bone.name, part_ID);
					return false;
				}
				//编号重复
				if (!part_IDs.insert(static_cast<uint16_t>(part_ID)).second)
				{
					logger.warn("Skeleton_Asset::子部件编号({})重复", part_ID);
					return false;
				}
				//写入编号
				bone.part.part_ID = static_cast<uint16_t>(part_ID);
				//相对变换
				if (!transform_read(part, bone.part.translation, bone.part.rotation, bone.part.scale))
				{
					logger.warn("Skeleton_Asset::骨骼({})部件相对变换非法", bone.name);
					return false;
				}
				//累计绑定数量
				bound_bone_count++;
			}

			//登记骨骼名
			indices.emplace(bone.name, index);
			//输出骨骼
			receiver = bone;
			return true;
		}
	}

	//从 JSON 内容构建
	bool Skeleton_Asset::build(const json& config, Skeleton_Asset& receiver)
	{
		//先清空输出
		receiver = Skeleton_Asset();

		//顶层必须为对象
		if (!config.is_object())
		{
			logger.warn("Skeleton_Asset::资产内容不是对象");
			return false;
		}

		//资产名（可选）
		string asset_name_value = "unnamed";
		if (config.contains("name"))
		{
			if (!string_read(config, "name", asset_name_value))
			{
				logger.warn("Skeleton_Asset::字段(name)非法");
				return false;
			}
		}

		//骨骼数组
		if (!config.contains("bones") || !config["bones"].is_array() || config["bones"].empty())
		{
			logger.warn("Skeleton_Asset::资产缺少有效字段(bones)");
			return false;
		}
		//骨骼数量上限
		const json& bone_list = config["bones"];
		if (bone_list.size() > bone_limit)
		{
			logger.warn("Skeleton_Asset::骨骼数量({})超过上限({})", bone_list.size(), bone_limit);
			return false;
		}

		//骨骼集合
		vector<Skeleton_Bone> bones;
		bones.reserve(bone_list.size());
		//骨骼名下标映射
		unordered_map<string, uint32_t> indices;
		//已占用子部件编号
		unordered_set<uint16_t> part_IDs;
		//绑定骨骼数量
		uint32_t bound_bone_count = 0;
		//绑定世界矩阵缓存（逐骨骼）
		vector<float> bind_world(bone_list.size() * 16, 0.0f);

		//逐骨骼解析
		for (uint32_t index = 0; index < static_cast<uint32_t>(bone_list.size()); index++)
		{
			//当前骨骼
			Skeleton_Bone bone;
			//解析并暂存
			if (!bone_parse(bone_list[index], index, bind_world, bone, indices, part_IDs, bound_bone_count))
				return false;
			//记录绑定世界矩阵供子骨骼使用
			float local[16] = {};
			detail::bone_matrix_from_transform(bone.bind_translation, bone.bind_rotation,
				bone.bind_scale, local);
			if (bone.parent < 0)
			{
				for (uint32_t element = 0; element < 16; element++)
					bind_world[static_cast<uint64_t>(index) * 16 + element] = local[element];
			}
			else
				detail::bone_matrix_multiply(&bind_world[static_cast<uint64_t>(bone.parent) * 16],
					local, &bind_world[static_cast<uint64_t>(index) * 16]);
			//收容骨骼
			bones.push_back(bone);
		}

		//写入结果
		receiver.asset_name = std::move(asset_name_value);
		receiver.bones = std::move(bones);
		receiver.bone_indices = std::move(indices);
		receiver.bound_bone_count = bound_bone_count;
		return true;
	}

	//从文件加载
	bool Skeleton_Asset::load_file(const std::string& path, Skeleton_Asset& receiver)
	{
		//先清空输出
		receiver = Skeleton_Asset();

		//路径有效性检查
		if (!detail::path_check(path))
		{
			logger.warn("Skeleton_Asset::资产路径无效：{}", path);
			return false;
		}

		//文件体量
		uintmax_t size = 0;
		std::error_code error_info;
		if (!detail::file_size_get(path, size, error_info) || size > file_limit)
		{
			logger.warn("Skeleton_Asset::资产文件体量非法：{}", path);
			return false;
		}

		//整文件读取
		vector<unsigned char> bytes;
		if (!detail::file_read_whole(path, bytes, error_info))
		{
			logger.warn("Skeleton_Asset::资产文件读取失败：{}", path);
			return false;
		}

		//UTF-8 文本
		string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		//解析 JSON（不抛异常）
		json config = json::parse(text.begin(), text.end(), nullptr, false);
		//若解析失败
		if (config.is_discarded())
		{
			logger.warn("Skeleton_Asset::资产 JSON 解析失败：{}", path);
			return false;
		}

		//从内容构建
		return build(config, receiver);
	}
}
