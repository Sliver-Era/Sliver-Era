#include "../局部命名空间使用.h"
//获取骨骼数学工具
#include "src/world/skeleton/Common/骨骼数学.h"

//游戏引擎命名空间
namespace engine
{
	//姿态求值
	bool Skeleton_System::pose_evaluate(Skeleton_Pose& pose) const
	{
		//姿态必须持有资产
		if (!pose.asset)
			return false;

		//骨骼数量
		uint32_t count = pose.asset->bone_count();
		//数据规模检查
		if (pose.world_matrix.size() < static_cast<uint64_t>(count) * 16 ||
			pose.local_translation.size() < static_cast<uint64_t>(count) * 3 ||
			pose.local_rotation.size() < static_cast<uint64_t>(count) * 4 ||
			pose.local_scale.size() < static_cast<uint64_t>(count) * 3)
			return false;

		//逐骨骼求值（资产已保证父下标小于自身）
		for (uint32_t index = 0; index < count; index++)
		{
			//当前骨骼
			const Skeleton_Bone* bone = pose.asset->bone_get(index);
			//局部矩阵
			float local[16] = {};
			detail::bone_matrix_from_transform(
				&pose.local_translation[static_cast<uint64_t>(index) * 3],
				&pose.local_rotation[static_cast<uint64_t>(index) * 4],
				&pose.local_scale[static_cast<uint64_t>(index) * 3], local);
			//根骨骼直接写入
			if (bone->parent < 0)
			{
				for (uint32_t element = 0; element < 16; element++)
					pose.world_matrix[static_cast<uint64_t>(index) * 16 + element] = local[element];
			}
			//子骨骼 = 父世界矩阵 × 局部矩阵
			else
				detail::bone_matrix_multiply(
					&pose.world_matrix[static_cast<uint64_t>(bone->parent) * 16],
					local, &pose.world_matrix[static_cast<uint64_t>(index) * 16]);
		}
		return true;
	}

	//蒙皮求值
	bool Skeleton_System::skin_evaluate(Skeleton_Pose& pose) const
	{
		//姿态必须持有资产
		if (!pose.asset)
			return false;

		//骨骼数量
		uint32_t count = pose.asset->bone_count();
		//数据规模检查
		if (pose.world_matrix.size() < static_cast<uint64_t>(count) * 16 ||
			pose.skin_matrix.size() < static_cast<uint64_t>(count) * 16)
			return false;

		//逐骨骼求蒙皮矩阵（世界矩阵 × 逆绑定矩阵）
		for (uint32_t index = 0; index < count; index++)
		{
			//当前骨骼
			const Skeleton_Bone* bone = pose.asset->bone_get(index);
			detail::bone_matrix_multiply(
				&pose.world_matrix[static_cast<uint64_t>(index) * 16],
				bone->inverse_bind,
				&pose.skin_matrix[static_cast<uint64_t>(index) * 16]);
		}
		return true;
	}

	//帧求值
	void Skeleton_System::frame_update(ECS_Store& store)
	{
		//先求值全部姿态与蒙皮
		store.component_each<Skeleton_Pose>([this](Entity, Skeleton_Pose& pose)
			{
				//姿态脏则重算世界矩阵
				if (pose.pose_dirty)
				{
					pose_evaluate(pose);
					pose.pose_dirty = false;
					pose.skin_dirty = true;
				}
				//蒙皮脏则重建调色板
				if (pose.skin_dirty)
				{
					skin_evaluate(pose);
					pose.skin_dirty = false;
				}
			});

		//再求值部件运动（依赖本帧世界矩阵）
		store.component_each<Skeleton_Link>([this, &store](Entity entity, Skeleton_Link& link)
			{
				//链接必须依附同实体的姿态
				const Skeleton_Pose* pose = store.component_get<Skeleton_Pose>(entity);
				//姿态缺失时清空运动量
				if (!pose)
				{
					link.motions.clear();
					return;
				}
				//求值部件运动
				link_evaluate(*pose, link);
				//发布运动事件（未接入中转站时由发送通道缺失静默失败）
				motion_publish(link);
			});
	}
}
