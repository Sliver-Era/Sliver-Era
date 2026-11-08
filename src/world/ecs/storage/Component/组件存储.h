#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取对象池
#include "Engine/EngineFoundation/src/object/Object_Pool/对象池.h"
//获取对象基类
#include "Engine/EngineFoundation/src/object/Object/对象.h"

//游戏引擎命名空间
namespace engine
{
	//组件类型约束 —— 派生 Object、含可赋值的 owner 字段、可移动
	template <typename C>
	concept Component_Type =
		std::is_base_of_v<Object, C> &&
		std::is_move_constructible_v<C> &&
		requires(C& component, const uint32_t& index)
	{
		component.owner = index;
	};

	//组件存储基类 —— 类型擦除后的统一句柄
	class Component_Storage
	{
	public:
		//析构函数
		virtual ~Component_Storage() = default;
		//组件删除
		virtual bool erase(const uint32_t& entity_index) = 0;
		//组件清空
		virtual void clear(void) = 0;
		//组件数量获取
		virtual std::size_t size(void) const = 0;
		//组件掩码位获取
		virtual uint32_t bit_get(void) const = 0;
	};

	//组件存储实现 —— 以实体槽位索引为键的对象池
	template <Component_Type C>
	class Component_Storage_Impl final : public Component_Storage
	{
	private:
		//组件池
		Object_Pool<C, uint32_t> pool;
		//组件掩码位
		uint32_t bit = 0;

	public:
		//构造函数
		explicit Component_Storage_Impl(const uint32_t& mask_bit)
			: pool(std::function<uint32_t(const C&)>(
				[](const C& component) { return component.owner; }))
			, bit(mask_bit)
		{}
		//组件查找 —— 非常量重载
		C* find(const uint32_t& entity_index)
		{
			auto it = pool.find(entity_index);
			//若目标实体没有该组件
			if (it == pool.end())
				return nullptr;
			return &(*it);
		}
		//组件查找 —— 常量重载
		const C* find(const uint32_t& entity_index) const
		{
			auto it = pool.find(entity_index);
			//若目标实体没有该组件
			if (it == pool.end())
				return nullptr;
			return &(*it);
		}
		//组件添加（已存在则返回空指针）
		C* add(const uint32_t& entity_index, C component)
		{
			//若目标实体已有同类组件
			if (find(entity_index) != nullptr)
				return nullptr;
			//记录归属实体
			component.owner = entity_index;
			//构建组件
			pool.build(std::move(component));
			//返回新增组件
			return find(entity_index);
		}
		//组件删除
		bool erase(const uint32_t& entity_index) override
		{
			return pool.try_unload(entity_index);
		}
		//组件清空
		void clear(void) override
		{
			pool.clear();
		}
		//组件数量获取
		std::size_t size(void) const override
		{
			return static_cast<std::size_t>(pool.size());
		}
		//组件掩码位获取
		uint32_t bit_get(void) const override
		{
			return bit;
		}
		//组件池获取
		Object_Pool<C, uint32_t>& pool_get(void)
		{
			return pool;
		}
	};
}
