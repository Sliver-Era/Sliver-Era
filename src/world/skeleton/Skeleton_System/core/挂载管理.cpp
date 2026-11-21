#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

//游戏引擎命名空间
namespace engine
{
	//演员挂载
	bool Skeleton_System::actor_attach(ECS_Store& store, Entity entity, uint32_t asset_ID,
		uint64_t collider_ID)
	{
		//目标资产
		const Skeleton_Asset* asset = asset_get(asset_ID);
		//资产不存在
		if (!asset)
		{
			logger.warn("Skeleton_System::资产({})不存在，挂载中止", asset_ID);
			return false;
		}

		//共享资产引用
		shared_ptr<const Skeleton_Asset> shared = assets[asset_ID - 1];
		//待挂载姿态
		Skeleton_Pose pose;
		//按资产初始化
		if (!pose.build(shared))
			return false;
		//创建姿态组件
		Skeleton_Pose* added = store.component_add<Skeleton_Pose>(entity, std::move(pose));
		//实体无效或已有姿态组件
		if (!added)
		{
			logger.warn("Skeleton_System::实体({})姿态组件创建失败", entity.value);
			return false;
		}

		//求值绑定姿态（作为部件运动的首帧基准）
		pose_evaluate(*added);
		added->pose_dirty = false;
		//求值绑定姿态下的蒙皮调色板
		skin_evaluate(*added);
		added->skin_dirty = false;

		//未指定碰撞体时只建姿态
		if (collider_ID == 0)
			return true;

		//待挂载链接
		Skeleton_Link link;
		//资产无部件绑定
		if (!link.build(*asset, collider_ID, *added))
		{
			logger.warn("Skeleton_System::资产({})无子部件绑定，实体({})仅创建姿态",
				asset->name(), entity.value);
			return true;
		}
		//创建链接组件
		store.component_add<Skeleton_Link>(entity, std::move(link));
		return true;
	}

	//演员卸载
	bool Skeleton_System::actor_detach(ECS_Store& store, Entity entity)
	{
		//移除链接组件（可能不存在）
		store.component_remove<Skeleton_Link>(entity);
		//移除姿态组件
		return store.component_remove<Skeleton_Pose>(entity);
	}
}
