#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取骨骼资产
#include "Engine/EngineSystem/src/world/skeleton/Skeleton_Asset/骨骼资产.h"

//游戏引擎命名空间
namespace engine
{
	/*
	骨骼姿态（ECS 组件）
	每演员一份：持有共享骨骼资产的引用、逐骨骼局部变换、世界矩阵与蒙皮矩阵。
	世界矩阵与蒙皮矩阵均在「演员根局部空间」，根变换由调用方承担。
	*/
	struct Skeleton_Pose
	{
		//归属实体槽位（由对象池写入）
		uint32_t owner = 0;
		//共享骨骼资产
		std::shared_ptr<const Skeleton_Asset> asset;
		//逐骨骼局部平移（每骨骼 3 项）
		std::vector<float> local_translation;
		//逐骨骼局部旋转（每骨骼 4 项）
		std::vector<float> local_rotation;
		//逐骨骼局部缩放（每骨骼 3 项）
		std::vector<float> local_scale;
		//逐骨骼世界矩阵（每骨骼 16 项，列主序）
		std::vector<float> world_matrix;
		//逐骨骼蒙皮矩阵（每骨骼 16 项，列主序）
		std::vector<float> skin_matrix;
		//姿态脏标记（为真时下一帧重新求值）
		bool pose_dirty = true;
		//蒙皮脏标记（为真时下一帧重建调色板）
		bool skin_dirty = true;

		//按资产初始化（清空姿态并回到绑定姿态）
		bool build(const std::shared_ptr<const Skeleton_Asset>& shared_asset);
		//骨骼数量读取
		uint32_t bone_count(void) const;
		//姿态复位到绑定姿态
		void reset(void);
		//骨骼局部变换写入（下标越界或参数非法返回假）
		bool local_set(uint32_t bone, const float translation[3],
			const float rotation[4], const float scale[3]);
		//骨骼局部平移写入
		bool translation_set(uint32_t bone, const float translation[3]);
		//骨骼局部旋转写入（非零四元数，内部单位化）
		bool rotation_set(uint32_t bone, const float rotation[4]);
		//骨骼局部缩放写入（分量有限且非零）
		bool scale_set(uint32_t bone, const float scale[3]);
		//骨骼世界矩阵读取（下标越界返回假）
		bool world_get(uint32_t bone, float out[16]) const;
	};
}
