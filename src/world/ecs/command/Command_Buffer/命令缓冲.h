#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取实体句柄
#include "../../entity/Entity/实体.h"
//获取组件约束
#include "../../storage/Component/组件存储.h"
//获取 ECS 存储定义
#include "../../storage/Store/存储.h"

//游戏引擎命名空间
namespace engine
{
	/*
	命令缓冲
	遍历中只记录结构变更命令，遍历结束后 flush 统一提交。
	entity_create 不进入缓冲（需要立即返回句柄），仅缓冲销毁与组件增删。
	*/
	class Command_Buffer
	{
	private:
		//待提交命令集合
		std::vector<std::function<void(ECS_Store&)>> commands;

	public:
		//延迟销毁实体
		void entity_destroy(Entity entity);
		//延迟添加组件（组件需可拷贝，std::function 要求可拷贝闭包）
		template <Component_Type C>
		void component_add(Entity entity, C component)
		{
			commands.push_back(
				[entity, component = std::move(component)](ECS_Store& store) mutable
				{
					store.component_add<C>(entity, std::move(component));
				});
		}
		//延迟删除组件
		template <Component_Type C>
		void component_remove(Entity entity)
		{
			commands.push_back(
				[entity](ECS_Store& store)
				{
					store.component_remove<C>(entity);
				});
		}
		//提交全部命令
		void flush(ECS_Store& store);
		//清空命令队列
		void clear(void);
		//命令数量获取
		std::size_t size(void) const;
	};
}
