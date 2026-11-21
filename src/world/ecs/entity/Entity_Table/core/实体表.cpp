#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

//游戏引擎命名空间
namespace engine
{
	//构造函数
	Entity_Table::Entity_Table()
	{
		//槽位 0 预留为无效
		index_allocator.set(1);
	}

	//实体创建
	Entity Entity_Table::entity_create(void)
	{
		//分配可用槽位
		std::optional<uint32_t> allocation = index_allocator.get();
		//若槽位分配失败
		if (!allocation.has_value())
		{
			logger.error("Entity_Table::槽位分配失败");
			return Entity{};
		}
		//获取可用槽位
		uint32_t index = allocation.value();
		//若槽位不足则扩展到目标槽位（槽位 0 预留，故需覆盖 index 本身）
		while (index >= records.size())
			records.push_back({});
		//获取目标记录
		Entity_Record& record = records[index];
		//重置存活与掩码
		record.alive = true;
		record.mask = 0;
		//若代际为零则修正为 1
		if (record.generation == 0)
			record.generation = 1;
		//增加存活计数
		alive_count++;
		//返回新句柄
		return entity_make(index, record.generation);
	}

	//实体销毁
	bool Entity_Table::entity_destroy(Entity entity)
	{
		//若实体不存活
		if (!entity_alive(entity))
			return false;
		//获取目标记录
		Entity_Record& record = records[entity.index()];
		//清除存活与掩码
		record.alive = false;
		record.mask = 0;
		//推进代际
		record.generation++;
		//若代际回绕为零则修正为 1
		if (record.generation == 0)
			record.generation = 1;
		//回收槽位
		index_allocator.recycle(entity.index());
		//减少存活计数
		alive_count--;
		return true;
	}

	//实体存活判定
	bool Entity_Table::entity_alive(Entity entity) const
	{
		//若句柄为空
		if (entity.empty())
			return false;
		//若槽位越界
		if (entity.index() >= records.size())
			return false;
		//获取目标记录
		const Entity_Record& record = records[entity.index()];
		//存活且代际一致
		return record.alive && record.generation == entity.generation();
	}

	//按槽位索引取当前代际句柄
	Entity Entity_Table::entity_from_index(const uint32_t& index) const
	{
		//若槽位越界
		if (index >= records.size())
			return Entity{};
		//获取目标记录
		const Entity_Record& record = records[index];
		//若目标不存活
		if (!record.alive)
			return Entity{};
		return entity_make(index, record.generation);
	}

	//组件掩码获取
	uint64_t Entity_Table::mask_get(Entity entity) const
	{
		//若实体不存活
		if (!entity_alive(entity))
			return 0;
		return records[entity.index()].mask;
	}

	//组件掩码位设置
	void Entity_Table::mask_bit_assign(Entity entity, const uint32_t& bit, bool value)
	{
		//若实体不存活或掩码位越界
		if (!entity_alive(entity) || bit >= 64)
			return;
		//获取目标掩码
		uint64_t& mask = records[entity.index()].mask;
		//置位或清位
		if (value)
			mask |= (static_cast<uint64_t>(1) << bit);
		else
			mask &= ~(static_cast<uint64_t>(1) << bit);
	}

	//组件掩码位获取
	bool Entity_Table::mask_bit_get(Entity entity, const uint32_t& bit) const
	{
		//若实体不存活或掩码位越界
		if (!entity_alive(entity) || bit >= 64)
			return false;
		return (records[entity.index()].mask & (static_cast<uint64_t>(1) << bit)) != 0;
	}

	//存活实体数量获取
	uint64_t Entity_Table::size(void) const
	{
		return alive_count;
	}

	//实体表清空
	void Entity_Table::clear(void)
	{
		//逐个失效记录并推进代际，避免旧句柄在清空后误命中新实体
		for (Entity_Record& record : records)
		{
			record.alive = false;
			record.mask = 0;
			record.generation++;
			//若代际回绕为零则修正为 1
			if (record.generation == 0)
				record.generation = 1;
		}
		//重置槽位分配器并恢复预留槽位 0
		index_allocator.reset();
		index_allocator.set(1);
		//清零存活计数
		alive_count = 0;
	}
}
