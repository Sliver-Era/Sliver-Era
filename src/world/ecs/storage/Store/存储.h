#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取实体与实体表
#include "../../entity/Entity/实体.h"
#include "../../entity/Entity_Table/实体表.h"
//获取组件存储
#include "../Component/组件存储.h"
//获取类型索引
#include <typeindex>

//游戏引擎命名空间
namespace engine
{
	/*
	世界 —— ECS 统一入口
	负责实体生命周期、组件注册与存取、单/双组件遍历、系统调度。
	遍历期间禁止直接增删（会失效迭代器/引用），应改用 Command_Buffer 延迟提交。
	*/
	class ECS_Store
	{
	private:
		//实体表
		Entity_Table entities;
		//组件存储注册表
		std::unordered_map<std::type_index, std::unique_ptr<Component_Storage>> storages;
		//组件类型 → 掩码位
		std::unordered_map<std::type_index, uint32_t> bits;
		//下一个可分配掩码位
		uint32_t next_bit = 0;

		//组件存储获取 —— 非常量重载
		template <Component_Type C>
		Component_Storage_Impl<C>* storage_get(void)
		{
			//查找组件类型
			auto it = storages.find(std::type_index(typeid(C)));
			//若未注册
			if (it == storages.end())
				return nullptr;
			return static_cast<Component_Storage_Impl<C>*>(it->second.get());
		}
		//组件存储获取 —— 常量重载
		template <Component_Type C>
		const Component_Storage_Impl<C>* storage_get(void) const
		{
			//查找组件类型
			auto it = storages.find(std::type_index(typeid(C)));
			//若未注册
			if (it == storages.end())
				return nullptr;
			return static_cast<const Component_Storage_Impl<C>*>(it->second.get());
		}
		//组件存储注册（已注册则直接返回）
		template <Component_Type C>
		Component_Storage_Impl<C>* storage_register(void)
		{
			//若已注册
			if (auto* existing = storage_get<C>())
				return existing;
			//若掩码位耗尽
			if (next_bit >= 64)
			{
				logger.error("ECS_Store::组件类型数量超过64，注册失败");
				return nullptr;
			}
			//分配掩码位
			uint32_t bit = next_bit;
			//创建组件存储
			std::unique_ptr<Component_Storage> storage(
				new (std::nothrow) Component_Storage_Impl<C>(bit));
			//若内存不足
			if (!storage)
			{
				logger.error("ECS_Store::组件存储创建失败");
				return nullptr;
			}
			//记录掩码位
			next_bit++;
			std::type_index type = std::type_index(typeid(C));
			bits.emplace(type, bit);
			storages.emplace(type, std::move(storage));
			//返回组件存储
			return storage_get<C>();
		}

	public:
		//构造函数
		ECS_Store(void);
		//析构函数
		~ECS_Store(void);
		//禁止拷贝
		ECS_Store(const ECS_Store&) = delete;
		ECS_Store& operator=(const ECS_Store&) = delete;

		//———— 实体 ————

		//实体创建
		Entity entity_create(void);
		//实体销毁（级联清理组件）
		bool entity_destroy(Entity entity);
		//实体存活判定
		bool entity_alive(Entity entity) const;
		//存活实体数量获取
		uint64_t entity_size(void) const;
		//世界清空（清组件与实体，保留组件类型注册）
		void clear(void);

		//———— 组件 ————

		//组件添加（已存在则返回空指针）
		template <Component_Type C, typename... Args>
		C* component_add(Entity entity, Args&&... args)
		{
			//若实体句柄无效
			if (!entities.entity_alive(entity))
				return nullptr;
			//获取或注册组件存储
			Component_Storage_Impl<C>* storage = storage_register<C>();
			//若组件类型注册失败
			if (!storage)
				return nullptr;
			//若目标实体已有同类组件
			if (storage->find(entity.index()) != nullptr)
				return nullptr;
			//构造组件
			C component{ std::forward<Args>(args)... };
			//添加组件
			if (storage->add(entity.index(), std::move(component)) == nullptr)
				return nullptr;
			//置位组件掩码
			entities.mask_bit_assign(entity, storage->bit_get(), true);
			//返回组件
			return storage->find(entity.index());
		}
		//组件获取 —— 非常量重载
		template <Component_Type C>
		C* component_get(Entity entity)
		{
			//若实体句柄无效
			if (!entities.entity_alive(entity))
				return nullptr;
			//获取组件存储
			Component_Storage_Impl<C>* storage = storage_get<C>();
			//若组件类型未注册
			if (!storage)
				return nullptr;
			return storage->find(entity.index());
		}
		//组件获取 —— 常量重载
		template <Component_Type C>
		const C* component_get(Entity entity) const
		{
			//若实体句柄无效
			if (!entities.entity_alive(entity))
				return nullptr;
			//获取组件存储
			const Component_Storage_Impl<C>* storage = storage_get<C>();
			//若组件类型未注册
			if (!storage)
				return nullptr;
			return storage->find(entity.index());
		}
		//组件存在性判定
		template <Component_Type C>
		bool component_has(Entity entity) const
		{
			return component_get<C>(entity) != nullptr;
		}
		//组件删除
		template <Component_Type C>
		bool component_remove(Entity entity)
		{
			//若实体句柄无效
			if (!entities.entity_alive(entity))
				return false;
			//获取组件存储
			Component_Storage_Impl<C>* storage = storage_get<C>();
			//若组件类型未注册
			if (!storage)
				return false;
			//删除组件
			bool removed = storage->erase(entity.index());
			//若删除成功则清除掩码位
			if (removed)
				entities.mask_bit_assign(entity, storage->bit_get(), false);
			return removed;
		}
		//组件数量获取
		template <Component_Type C>
		uint64_t component_size(void) const
		{
			//获取组件存储
			const Component_Storage_Impl<C>* storage = storage_get<C>();
			//若组件类型未注册
			if (!storage)
				return 0;
			return static_cast<uint64_t>(storage->size());
		}
		//单组件遍历（回调参数：实体句柄、组件引用）
		template <Component_Type C, typename Fn>
		void component_each(Fn&& fn)
		{
			//获取组件存储
			Component_Storage_Impl<C>* storage = storage_get<C>();
			//若组件类型未注册
			if (!storage)
				return;
			for (C& component : storage->pool_get())
			{
				//按归属槽位还原实体句柄
				Entity owner = entities.entity_from_index(component.owner);
				//若归属实体不存活
				if (owner.empty())
					continue;
				fn(owner, component);
			}
		}
		//双组件交集遍历（遍历较小的池，回调参数：实体句柄、组件一引用、组件二引用）
		template <Component_Type C1, Component_Type C2, typename Fn>
		void each(Fn&& fn)
		{
			//获取两个组件存储
			Component_Storage_Impl<C1>* first = storage_get<C1>();
			Component_Storage_Impl<C2>* second = storage_get<C2>();
			//若任一组件类型未注册
			if (!first || !second)
				return;
			//以较小的池作为驱动
			if (first->size() <= second->size())
			{
				for (C1& component : first->pool_get())
				{
					//按归属槽位还原实体句柄
					Entity owner = entities.entity_from_index(component.owner);
					//若归属实体不存活
					if (owner.empty())
						continue;
					//查询另一组件
					C2* other = second->find(component.owner);
					//若另一组件存在则回调
					if (other != nullptr)
						fn(owner, component, *other);
				}
			}
			else
			{
				for (C2& component : second->pool_get())
				{
					//按归属槽位还原实体句柄
					Entity owner = entities.entity_from_index(component.owner);
					//若归属实体不存活
					if (owner.empty())
						continue;
					//查询另一组件
					C1* other = first->find(component.owner);
					//若另一组件存在则回调
					if (other != nullptr)
						fn(owner, *other, component);
				}
			}
		}
	};
}
