#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取 JSON 解析外部库
#include <nlohmann/json.hpp>

//游戏引擎命名空间
namespace engine
{
	/*
	骨骼与碰撞子部件的绑定模板
	资产中只记录模板，演员挂载时复制为 Skeleton_Link 的逐条绑定；
	part_ID 为 0xFFFF 表示该骨骼不绑定碰撞子部件。
	*/
	struct Bone_Part_Binding
	{
		//子部件编号（0xFFFF 表示不绑定）
		uint16_t part_ID = 0xFFFF;
		//相对骨骼的平移
		float translation[3] = { 0.0f, 0.0f, 0.0f };
		//相对骨骼的旋转（x、y、z、w）
		float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		//相对骨骼的缩放
		float scale[3] = { 1.0f, 1.0f, 1.0f };

		//绑定有效性判定
		bool bound(void) const
		{
			return part_ID != 0xFFFF;
		}
	};

	/*
	单根骨骼
	局部变换按「先缩放、再旋转、最后平移」作用于父骨骼空间；
	inverse_bind 为列主序逆绑定矩阵，资产解析时缺省由绑定姿态推导。
	*/
	struct Skeleton_Bone
	{
		//骨骼名
		std::string name;
		//父骨骼下标（-1 表示根）
		int32_t parent = -1;
		//绑定局部平移
		float bind_translation[3] = { 0.0f, 0.0f, 0.0f };
		//绑定局部旋转（x、y、z、w）
		float bind_rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		//绑定局部缩放
		float bind_scale[3] = { 1.0f, 1.0f, 1.0f };
		//逆绑定矩阵（列主序）
		float inverse_bind[16] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f };
		//碰撞子部件绑定模板
		Bone_Part_Binding part;
	};

	/*
	骨骼资产
	由多个演员共享的只读数据；同一份资产可被多个 Skeleton_Pose 引用。
	资产编号由 Skeleton_System 分配（从 1 起，0 表示无效）。
	*/
	class Skeleton_Asset
	{
	private:
		//资产名
		std::string asset_name;
		//骨骼集合（父骨骼下标必须小于自身下标）
		std::vector<Skeleton_Bone> bones;
		//骨骼名到下标映射
		std::unordered_map<std::string, uint32_t> bone_indices;
		//绑定碰撞部件的骨骼数量
		uint32_t bound_bone_count = 0;

	public:
		//默认构造
		Skeleton_Asset(void) = default;

		//从文件加载（失败时清空输出）
		static bool load_file(const std::string& path, Skeleton_Asset& receiver);
		//从 JSON 内容构建（失败时清空输出）
		static bool build(const nlohmann::json& config, Skeleton_Asset& receiver);

		//资产名读取
		const std::string& name(void) const
		{
			return asset_name;
		}
		//骨骼数量读取
		uint32_t bone_count(void) const
		{
			return static_cast<uint32_t>(bones.size());
		}
		//骨骼读取（下标越界返回空指针）
		const Skeleton_Bone* bone_get(uint32_t index) const
		{
			//若下标越界
			if (index >= bones.size())
				return nullptr;
			return &bones[index];
		}
		//骨骼名查下标（不存在返回 -1）
		int32_t bone_seek(const std::string& name) const
		{
			//查找骨骼名
			auto it = bone_indices.find(name);
			//若骨骼名不存在
			if (it == bone_indices.end())
				return -1;
			return static_cast<int32_t>(it->second);
		}
		//绑定碰撞部件的骨骼数量读取
		uint32_t bound_bone_count_get(void) const
		{
			return bound_bone_count;
		}
		//资产有效性判定
		bool valid(void) const
		{
			return !bones.empty();
		}
	};
}
