#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取数值池
#include "Engine/EngineFoundation/src/detail/class/Number_Pool/数值池.h"
//获取实体句柄
#include "../Entity/实体.h"

//游戏引擎命名空间
namespace engine
{
	//实体记录
	struct Entity_Record
	{
		//代际
		uint32_t generation = 1;
		//组件掩码
		uint64_t mask = 0;
		//存活标记
		bool alive = false;
	};

	//实体表 —— 负责槽位分配、代际推进与组件掩码
	class Entity_Table
	{
	private:
		//实体记录集合（下标即槽位索引）
		std::vector<Entity_Record> records;
		//槽位分配器
		Number_Pool<uint32_t> index_allocator;
		//存活实体数量
		uint64_t alive_count = 0;

	public:
		//构造函数
		Entity_Table();
		//实体创建
		Entity entity_create(void);
		//实体销毁
		bool entity_destroy(Entity entity);
		//实体存活判定
		bool entity_alive(Entity entity) const;
		//按槽位索引取当前代际句柄（无效返回空句柄）
		Entity entity_from_index(const uint32_t& index) const;
		//组件掩码获取
		uint64_t mask_get(Entity entity) const;
		//组件掩码位设置
		void mask_bit_assign(Entity entity, const uint32_t& bit, bool value);
		//组件掩码位获取
		bool mask_bit_get(Entity entity, const uint32_t& bit) const;
		//存活实体数量获取
		uint64_t size(void) const;
		//实体表清空
		void clear(void);
	};
}
