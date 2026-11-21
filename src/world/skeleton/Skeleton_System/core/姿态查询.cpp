#include "../局部命名空间使用.h"

//游戏引擎命名空间
namespace engine
{
	//骨骼局部变换写入
	bool Skeleton_System::bone_local_set(ECS_Store& store, Entity entity, uint32_t bone,
		const float translation[3], const float rotation[4], const float scale[3])
	{
		//入参非空检查
		if (!translation || !rotation || !scale)
			return false;
		//获取姿态
		Skeleton_Pose* pose = store.component_get<Skeleton_Pose>(entity);
		//姿态缺失
		if (!pose)
			return false;
		//写入局部变换
		return pose->local_set(bone, translation, rotation, scale);
	}

	//骨骼局部旋转写入
	bool Skeleton_System::bone_rotation_set(ECS_Store& store, Entity entity, uint32_t bone,
		const float rotation[4])
	{
		//入参非空检查
		if (!rotation)
			return false;
		//获取姿态
		Skeleton_Pose* pose = store.component_get<Skeleton_Pose>(entity);
		//姿态缺失
		if (!pose)
			return false;
		//写入局部旋转
		return pose->rotation_set(bone, rotation);
	}

	//姿态复位
	bool Skeleton_System::pose_reset(ECS_Store& store, Entity entity)
	{
		//获取姿态
		Skeleton_Pose* pose = store.component_get<Skeleton_Pose>(entity);
		//姿态缺失
		if (!pose)
			return false;
		//回到绑定姿态
		pose->reset();
		return true;
	}

	//骨骼世界矩阵查询
	bool Skeleton_System::pose_matrix_get(const ECS_Store& store, Entity entity, uint32_t bone,
		float out[16]) const
	{
		//入参非空检查
		if (!out)
			return false;
		//获取姿态
		const Skeleton_Pose* pose = store.component_get<Skeleton_Pose>(entity);
		//姿态缺失
		if (!pose)
			return false;
		//读取世界矩阵
		return pose->world_get(bone, out);
	}

	//蒙皮调色板查询
	bool Skeleton_System::skin_palette_get(const ECS_Store& store, Entity entity,
		const float*& palette, uint32_t& count) const
	{
		//先清空输出
		palette = nullptr;
		count = 0;
		//获取姿态
		const Skeleton_Pose* pose = store.component_get<Skeleton_Pose>(entity);
		//姿态缺失或调色板为空
		if (!pose || pose->skin_matrix.empty())
			return false;
		//输出调色板
		palette = pose->skin_matrix.data();
		count = pose->bone_count();
		return true;
	}

	//本帧部件运动查询
	bool Skeleton_System::part_motions_get(const ECS_Store& store, Entity entity,
		const Skeleton_Part_Motion*& motions, uint32_t& count) const
	{
		//先清空输出
		motions = nullptr;
		count = 0;
		//获取链接
		const Skeleton_Link* link = store.component_get<Skeleton_Link>(entity);
		//链接缺失
		if (!link)
			return false;
		//输出运动量
		count = static_cast<uint32_t>(link->motions.size());
		if (count > 0)
			motions = link->motions.data();
		return true;
	}
}
