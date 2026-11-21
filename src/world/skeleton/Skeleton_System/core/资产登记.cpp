#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

//游戏引擎命名空间
namespace engine
{
	//资产登记
	uint32_t Skeleton_System::asset_add(Skeleton_Asset asset)
	{
		//资产必须有效
		if (!asset.valid())
		{
			logger.warn("Skeleton_System::资产无效，登记中止");
			return 0;
		}

		//共享资产（禁用抛出式分配）
		shared_ptr<const Skeleton_Asset> shared(
			new (nothrow) Skeleton_Asset(std::move(asset)));
		//若内存不足
		if (!shared)
		{
			logger.error("Skeleton_System::资产内存分配失败");
			return 0;
		}

		//收容资产
		assets.push_back(std::move(shared));
		//编号从 1 起
		return static_cast<uint32_t>(assets.size());
	}

	//资产文件加载并登记
	uint32_t Skeleton_System::asset_load_file(const string& path)
	{
		//加载资产
		Skeleton_Asset asset;
		if (!Skeleton_Asset::load_file(path, asset))
			return 0;
		//登记资产
		return asset_add(std::move(asset));
	}

	//资产卸载
	bool Skeleton_System::asset_remove(uint32_t asset_ID)
	{
		//编号越界
		if (asset_ID == 0 || asset_ID > assets.size())
			return false;
		//目标槽位
		shared_ptr<const Skeleton_Asset>& slot = assets[asset_ID - 1];
		//已卸载
		if (!slot)
			return false;
		//置空槽位（已挂载演员持有的共享引用继续有效）
		slot.reset();
		return true;
	}

	//资产读取
	const Skeleton_Asset* Skeleton_System::asset_get(uint32_t asset_ID) const
	{
		//编号越界
		if (asset_ID == 0 || asset_ID > assets.size())
			return nullptr;
		return assets[asset_ID - 1].get();
	}

	//有效资产数量读取
	uint32_t Skeleton_System::asset_count(void) const
	{
		//有效槽位计数
		uint32_t count = 0;
		for (const shared_ptr<const Skeleton_Asset>& slot : assets)
		{
			if (slot)
				count++;
		}
		return count;
	}
}
