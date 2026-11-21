#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取骨骼数学工具
#include "src/world/skeleton/Common/骨骼数学.h"

//游戏引擎命名空间
namespace engine
{
	//按资产绑定表构建
	bool Skeleton_Link::build(const Skeleton_Asset& asset, uint64_t collider_ID_value,
		const Skeleton_Pose& pose)
	{
		//清空旧数据
		links.clear();
		previous_matrix.clear();
		motions.clear();
		//记录目标碰撞体
		collider_ID = collider_ID_value;

		//姿态必须与资产同规模
		uint32_t count = asset.bone_count();
		if (pose.bone_count() != count ||
			pose.world_matrix.size() != static_cast<uint64_t>(count) * 16)
		{
			logger.warn("Skeleton_Link::姿态与资产骨骼数量不一致，链接构建中止");
			return false;
		}

		//逐骨骼收集部件绑定
		for (uint32_t index = 0; index < count; index++)
		{
			//当前骨骼
			const Skeleton_Bone* bone = asset.bone_get(index);
			//跳过未绑定骨骼
			if (!bone->part.bound())
				continue;
			//绑定项
			Skeleton_Part_Link link;
			link.part_ID = bone->part.part_ID;
			link.bone = index;
			for (uint32_t element = 0; element < 3; element++)
			{
				link.translation[element] = bone->part.translation[element];
				link.scale[element] = bone->part.scale[element];
			}
			for (uint32_t element = 0; element < 4; element++)
				link.rotation[element] = bone->part.rotation[element];
			//收容绑定项
			links.push_back(link);
		}

		//无任何绑定
		if (links.empty())
		{
			logger.warn("Skeleton_Link::资产({})没有子部件绑定", asset.name());
			return false;
		}

		//首帧基准矩阵
		previous_matrix.assign(links.size() * 16, 0.0f);
		for (uint64_t index = 0; index < links.size(); index++)
		{
			//当前绑定
			const Skeleton_Part_Link& link = links[index];
			//相对变换矩阵
			float offset[16] = {};
			detail::bone_matrix_from_transform(link.translation, link.rotation, link.scale, offset);
			//部件世界矩阵 = 骨骼世界矩阵 × 相对变换
			detail::bone_matrix_multiply(&pose.world_matrix[static_cast<uint64_t>(link.bone) * 16],
				offset, &previous_matrix[index * 16]);
		}

		//预留运动量容量
		motions.reserve(links.size());
		return true;
	}

	//链接数量读取
	uint32_t Skeleton_Link::link_count(void) const
	{
		return static_cast<uint32_t>(links.size());
	}
}
