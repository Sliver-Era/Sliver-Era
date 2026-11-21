#include "../../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//碰撞体构建
	bool Proxy_Backend::collider_build(const string& region, uint64_t& collider_ID)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Proxy_Backend::待构建碰撞体的碰撞空间({})不存在", region);
			return false;
		}

		//在目标空间内构建碰撞体
		collider_ID = target->collider_build();
		//登记编号归属
		collider_mapping_link(collider_ID, region);
		return true;
	}

	//碰撞体卸载
	bool Proxy_Backend::collider_unload(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Proxy_Backend::待卸载碰撞体的碰撞空间({})不存在", region);
			return false;
		}

		//在目标空间内卸载碰撞体
		if (!target->collider_unload(collider_ID))
			return false;
		//注销编号归属
		collider_mapping_unlink(collider_ID, region);
		return true;
	}

	//碰撞体转移
	bool Proxy_Backend::collider_transfer(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Proxy_Backend::待转移至的碰撞空间({})不存在", region);
			return false;
		}

		//查找编号归属空间
		string owner;
		//若该编号未被任何空间持有
		if (!collider_owner_seek(collider_ID, owner))
		{
			logger.warn("Proxy_Backend::待转移碰撞体({})不存在", collider_ID);
			return false;
		}
		//归属空间即目标空间时无需转移
		if (owner == region)
		{
			logger.warn("Proxy_Backend::待转移碰撞体({})已在目标空间({})内", collider_ID, region);
			return false;
		}

		//查找源碰撞空间
		Collision_Region* source = region_seek(owner);
		//若源碰撞空间不存在
		if (!source)
		{
			logger.warn("Proxy_Backend::碰撞体({})的归属空间({})不存在", collider_ID, owner);
			return false;
		}

		//源配置快照（几何 ＋ 检测方式 ＋ 豁免标记）
		json geometry = source->collider_geometry(collider_ID);
		Detection_Mode detection_mode;
		uint64_t exemption_flag = 0;
		//若无法读取源配置则转移中止
		if (!source->collider_config_get(collider_ID, detection_mode, exemption_flag))
		{
			logger.warn("Proxy_Backend::碰撞体({})源配置读取失败，转移中止", collider_ID);
			return false;
		}

		//在目标空间内按原编号接管（接管失败时源空间尚未改动）
		if (!target->collider_adopt(collider_ID))
		{
			logger.warn("Proxy_Backend::碰撞体({})编号在目标空间({})内冲突，转移中止", collider_ID,
				region);
			return false;
		}
		//卸载源碰撞体
		if (!source->collider_unload(collider_ID))
		{
			//回滚目标空间内的接管
			target->collider_unload(collider_ID);
			return false;
		}

		//在目标空间内按原编号重建几何体
		if (!geometry.is_null() && !geometry.empty())
		{
			geometry["collider_ID"] = collider_ID;
			if (!target->collider_set(geometry))
			{
				logger.warn("Proxy_Backend::碰撞体({})几何体在目标空间({})重建失败，回滚转移",
					collider_ID, region);
				//移除目标空间内的占位碰撞体
				target->collider_unload(collider_ID);
				//在源空间内恢复碰撞体与配置
				if (source->collider_adopt(collider_ID))
				{
					source->collider_set(geometry);
					source->collider_set(collider_ID, detection_mode);
					source->collider_set(collider_ID, exemption_flag);
				}
				return false;
			}
		}

		//回填检测方式与豁免标记
		target->collider_set(collider_ID, detection_mode);
		target->collider_set(collider_ID, exemption_flag);

		//更新编号归属
		collider_mapping_unlink(collider_ID, owner);
		collider_mapping_link(collider_ID, region);
		return true;
	}

	//碰撞体镜像
	bool Proxy_Backend::collider_mirror(const uint64_t collider_ID, const string& region,
		uint64_t& mirror_ID)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Proxy_Backend::待镜像至的碰撞空间({})不存在", region);
			return false;
		}

		//查找编号归属空间
		string owner;
		//若该编号未被任何空间持有
		if (!collider_owner_seek(collider_ID, owner))
		{
			logger.warn("Proxy_Backend::待镜像碰撞体({})不存在", collider_ID);
			return false;
		}

		//查找源碰撞空间
		Collision_Region* source = region_seek(owner);
		//若源碰撞空间不存在
		if (!source)
		{
			logger.warn("Proxy_Backend::碰撞体({})的归属空间({})不存在", collider_ID, owner);
			return false;
		}

		//源配置快照（几何 ＋ 检测方式 ＋ 豁免标记；运动不复制）
		json geometry = source->collider_geometry(collider_ID);
		Detection_Mode detection_mode;
		uint64_t exemption_flag = 0;
		//若无法读取源配置则镜像中止
		if (!source->collider_config_get(collider_ID, detection_mode, exemption_flag))
		{
			logger.warn("Proxy_Backend::碰撞体({})源配置读取失败，镜像中止", collider_ID);
			return false;
		}

		//在目标空间内构建全新编号的碰撞体
		uint64_t new_ID = 0;
		if (!collider_build(region, new_ID))
			return false;

		//按新编号重建几何体
		if (!geometry.is_null() && !geometry.empty())
		{
			geometry["collider_ID"] = new_ID;
			if (!target->collider_set(geometry))
			{
				logger.warn("Proxy_Backend::镜像碰撞体({})几何体构建失败，回滚镜像", new_ID);
				//移除目标空间内的占位碰撞体
				collider_unload(new_ID, region);
				return false;
			}
		}

		//回填检测方式与豁免标记
		target->collider_set(new_ID, detection_mode);
		target->collider_set(new_ID, exemption_flag);
		//输出新编号
		mirror_ID = new_ID;
		return true;
	}

	//碰撞体设置分发
	bool Proxy_Backend::collider_set_dispatch(const uint64_t collider_ID,
		const std::function<bool(Collision_Region&)>& setter) const
	{
		//取该编号的全部登记项
		auto range = collider_mapping.equal_range(collider_ID);
		//若该编号未被任何空间持有
		if (range.first == range.second)
		{
			logger.warn("Proxy_Backend::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//是否存在设置成功的空间
		bool any_set = false;
		//逐个对持有该编号的空间施加设置
		for (auto it = range.first; it != range.second; ++it)
		{
			//查找目标碰撞空间
			Collision_Region* target = region_seek(it->second);
			//若目标碰撞空间不存在则跳过
			if (!target)
				continue;
			//施加设置
			if (setter(*target))
				any_set = true;
		}

		return any_set;
	}
}