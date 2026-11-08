#include "../局部命名空间使用.h"

//游戏引擎命名空间
namespace engine
{
	//构造函数
	ECS_Store::ECS_Store(void) = default;

	//析构函数
	ECS_Store::~ECS_Store(void) = default;

	//实体创建
	Entity ECS_Store::entity_create(void)
	{
		return entities.entity_create();
	}

	//实体销毁
	bool ECS_Store::entity_destroy(Entity entity)
	{
		//若实体句柄无效
		if (!entities.entity_alive(entity))
			return false;
		//逐池删除该实体的全部组件
		uint32_t index = entity.index();
		for (auto& entry : storages)
			entry.second->erase(index);
		//销毁实体记录
		return entities.entity_destroy(entity);
	}

	//实体存活判定
	bool ECS_Store::entity_alive(Entity entity) const
	{
		return entities.entity_alive(entity);
	}

	//存活实体数量获取
	uint64_t ECS_Store::entity_size(void) const
	{
		return entities.size();
	}

	//世界清空
	void ECS_Store::clear(void)
	{
		//清空全部组件
		for (auto& entry : storages)
			entry.second->clear();
		//清空实体表
		entities.clear();
	}

}
