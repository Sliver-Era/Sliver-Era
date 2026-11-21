#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//碰撞体构建
	optional<uint64_t> Collision_Proxy::collider_build(const string& region)
	{
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//经后端在目标空间内构建碰撞体
		if (!backend.collider_build(region, collider_ID))
			return nullopt;

		return collider_ID;
	}

	//碰撞体卸载
	bool Collision_Proxy::collider_unload(const uint64_t collider_ID, const string& region)
	{
		//在目标空间内卸载碰撞体
		if (!backend.collider_unload(collider_ID, region))
			return false;

		//归属空间名称
		string owner;
		//若该编号已无任何空间持有则丢弃运动登记（避免编号回收后被新碰撞体继承）
		if (!backend.collider_owner_seek(collider_ID, owner))
			motion_records.erase(collider_ID);
		return true;
	}

	//碰撞体转移
	bool Collision_Proxy::collider_transfer(const uint64_t collider_ID, const string& region)
	{
		//在空间间转移碰撞体
		if (!backend.collider_transfer(collider_ID, region))
			return false;

		//目标空间内为新建碰撞体，按原编号恢复运动登记
		motion_frequency_apply(collider_ID);
		motion_apply(collider_ID);
		return true;
	}

	//碰撞体镜像
	bool Collision_Proxy::collider_mirror(const uint64_t collider_ID, const string& region)
	{
		return backend.collider_mirror(collider_ID, region);
	}

	//碰撞体设置 —— 检测方式重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const Detection_Mode& vector)
	{
		//对持有该编号的空间施加检测方式设置
		return backend.collider_set_dispatch(collider_ID, [collider_ID, &vector](
			Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}

	//碰撞体设置 —— 豁免标记重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const uint64_t& vector)
	{
		//对持有该编号的空间施加豁免标记设置
		return backend.collider_set_dispatch(collider_ID, [collider_ID, &vector](
			Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}
}