#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取数值分配器
#include "Engine/EngineFoundation/src/detail/class/Number_Pool/数值池.h"
//获取二分查找算法
#include "Engine/EngineFoundation/src/detail/methods/二分查找.h"

namespace engine
{
	//对象排列顺序
	enum class Object_Order
	{
		Unorder,
		Sorted
	};

	//对象池 —— 模板参数 T 为任意可默认构造、可移动的类型
	//Key 为对象定位键类型，定位键由构造时注入的投影器给出可写引用
	//对象编号不由对象池分配，构建时必须提供定位键，由对象池写入对象
	template <typename T, typename Key = uint64_t>
	class Object_Pool
	{
	private:
		//索引池
		Number_Pool<uint64_t> index_pool;

		//有效索引起点 —— [min_valid_index, objects.size()) 为有效区
		uint64_t min_valid_index = 0;
		//对象集合
		std::vector<T> objects;

		//对象定位投影器(返回对象内定位键字段的可写引用)
		std::function<Key&(T&)> projector;
		//对象定位方式标记
		bool is_sorted = false;
		//对象映射
		std::unordered_map<Key, uint64_t> mapping;
		//投影键比较方式(默认升序，运行期可替换)
		std::function<bool(const Key&, const Key&)> compare = std::ranges::less{};

		//定位键读取 —— 常量上下文
		const Key& key_get(const T& object) const
		{
			//投影器需要可写引用，常量上下文下转为非常量引用后读取
			return projector(const_cast<T&>(object));
		}
		//槽位构建 —— 分配槽位、写入定位键并登记映射
		uint64_t slot_build(const Key& key, T&& object)
		{
			//分配新对象索引
			std::optional<uint64_t> object_index = index_pool.get();
			//若索引分配失败
			if (!object_index.has_value())
			{
				logger.error("Object_Pool::索引分配失败");
				return (std::numeric_limits<uint64_t>::max)();
			}
			//获取新对象索引
			uint64_t slot_index = object_index.value();
			//若槽位不足则直接创建新对象
			if (slot_index >= objects.size())
				objects.push_back(std::move(object));
			//若获得旧槽位则销毁旧对象并原地重建新对象
			else
			{
				//析构旧对象
				std::destroy_at(&objects[slot_index]);
				//在旧槽位原地构造新对象
				std::construct_at(&objects[slot_index], std::move(object));
			}

			//若当前存在无效区间
			if (min_valid_index > 0)
			{
				//将新对象交换至无效区边界并收缩无效区
				std::swap(objects[slot_index], objects[--min_valid_index]);
				//更新新对象索引
				slot_index = min_valid_index;
			}

			//写入定位键，保证映射与对象内键字段一致
			projector(objects[slot_index]) = key;
			//若为哈希定位模式则记录索引映射
			if (!is_sorted)
				mapping.insert_or_assign(key, slot_index);

			return slot_index;
		}
		//定位键集合收集 —— 供批量构建判重
		std::unordered_set<Key> keys_collect(void) const
		{
			//定位键集合
			std::unordered_set<Key> keys{};
			//遍历有效区收集定位键
			for (uint64_t index = min_valid_index; index < objects.size(); index++)
				keys.insert(key_get(objects[index]));
			return keys;
		}

	public:
		//构造函数 —— 必须注入键投影器
		explicit Object_Pool(std::function<Key&(T&)> proj)
		{
			//设置后进先出机制分配回收索引
			index_pool.set(Num_Reuse_Order::LIFO);
			//显示指定起始索引
			index_pool.set(0);
			//记录键投影器
			projector = std::move(proj);
		}
		//析构函数
		~Object_Pool()
		{}

		//对象排列方式设置
		void mode_set(Object_Order mode,
			std::function<bool(const Key&, const Key&)> cmp = {})
		{
			//若为哈希定位模式设置
			if (mode == Object_Order::Unorder && is_sorted)
			{
				//标记回到稳定模式
				is_sorted = false;
				//重建映射
				for (uint64_t rebuild_index = min_valid_index; rebuild_index < objects.size(); rebuild_index++)
					mapping.insert({ key_get(objects[rebuild_index]), rebuild_index });
				//重置比较方式
				compare = std::ranges::less{};
			}
			//若为排序模式设置
			else if (mode == Object_Order::Sorted)
			{
				//若当前为哈希定位模式
				if (!is_sorted)
				{
					//标记切换排序模式
					is_sorted = true;
					//清空映射
					mapping.clear();
				}
				//记录排序方式(未指定则按升序)
				compare = cmp ? std::move(cmp)
					: std::function<bool(const Key&, const Key&)>(std::ranges::less{});
				//重排序对象
				sort();
			}
			//若为无效模式切换指令
			else
				logger.warn("Object_Pool::当前已为目标模式");
		}

		//对象查找 —— 非常量重载
		typename std::vector<T>::iterator find(const Key& key)
		{
			//若为哈希定位模式
			if (!is_sorted)
			{
				//获取索引映射迭代器
				auto it = mapping.find(key);
				//若迭代器有效
				if (it != mapping.end())
					return objects.begin() + it->second;
				//若不存在目标对象则返回超尾迭代器
				else
					return objects.end();
			}
			//若为排序模式
			else
			{
				//获取目标对象索引
				std::optional<uint64_t> object_index = detail::binary_search
				(begin(), end(), key, compare,
					[this](const T& object) -> const Key& { return key_get(object); });
				//若返回索引有效
				if (object_index.has_value())
					return begin() + object_index.value();
				//若不存在目标对象则返回超尾迭代器
				else
					return end();
			}
		}
		//对象查找 —— 常量重载
		typename std::vector<T>::const_iterator find(const Key& key) const
		{
			//复用非常量重载完成查找，迭代器隐式转换为常量迭代器
			return const_cast<Object_Pool*>(this)->find(key);
		}
		//对象添加 —— 单对象重载
		uint64_t build(const Key& key, T object = {})
		{
			//若定位键已存在
			if (find(key) != objects.end())
			{
				logger.warn("Object_Pool::定位键已存在: {}", key);
				return (std::numeric_limits<uint64_t>::max)();
			}
			//构建目标对象
			uint64_t object_index = slot_build(key, std::move(object));
			//若槽位分配失败
			if (object_index == (std::numeric_limits<uint64_t>::max)())
				return object_index;
			//若为排序模式则重排序对象
			if (is_sorted)
				sort();
			return object_index;
		}
		//对象添加 —— 多对象重载(仅提供定位键)
		std::vector<uint64_t> build(const std::vector<Key>& keys)
		{
			//构造默认实参集合并转调带实参重载
			return build(keys, std::vector<T>(keys.size()));
		}
		//对象添加 —— 多对象重载(同步提供定位键与实参)
		std::vector<uint64_t> build(const std::vector<Key>& keys, std::vector<T> object_set)
		{
			//若定位键与实参数量不一致
			if (keys.size() != object_set.size())
				logger.warn("Object_Pool::批量构建键与实参数量不一致: {} / {}", keys.size(), object_set.size());

			//槽位索引缓冲
			std::vector<uint64_t> buffer{};
			//既有定位键集合
			std::unordered_set<Key> exist_keys = keys_collect();
			//取两者较小规模，避免越界
			uint64_t count = static_cast<uint64_t>(std::min(keys.size(), object_set.size()));
			//构建对象
			for (uint64_t time = 0; time < count; time++)
			{
				//获取当前定位键
				const Key& key = keys[time];
				//若定位键重复则略过
				if (exist_keys.count(key))
				{
					logger.warn("Object_Pool::定位键已存在: {}", key);
					continue;
				}
				//登记本次构建的定位键
				exist_keys.insert(key);
				//构建目标对象
				uint64_t object_index = slot_build(key, std::move(object_set[time]));
				//若槽位分配失败则终止批量构建
				if (object_index == (std::numeric_limits<uint64_t>::max)())
					break;
				//记录对象索引
				buffer.push_back(object_index);
			}
			//若为排序模式则重排序对象
			if (is_sorted)
				sort();

			//返回对象索引集合
			return buffer;
		}
		//对象排序
		bool sort(void)
		{
			//若非排序模式则返回排序失败
			if (!is_sorted)
				return false;
			//若为排序模式则进行重排序
			else
				std::ranges::sort(begin(), end(), compare,
					[this](const T& object) -> const Key& { return key_get(object); });

			return true;
		}
		//对象尝试卸载 —— 目标不存在时静默返回
		bool try_unload(const Key& key)
		{
			//获取目标对象迭代器
			auto it = find(key);
			//若目标对象不存在则返回卸载失败
			if (it == objects.end())
				return false;
			//记录目标对象索引
			uint64_t target_index = static_cast<uint64_t>(it - objects.begin());
			//若为哈希定位模式则卸载索引映射
			if (!is_sorted)
				mapping.erase(key);
			//若目标不在无效区边界则交换到边界
			if (target_index != min_valid_index)
			{
				//交换目标与无效区边界对象
				std::swap(objects[target_index], objects[min_valid_index]);
				//若为哈希定位模式则同步更新被交换有效对象映射索引
				if (!is_sorted)
					mapping.insert_or_assign(key_get(objects[target_index]), target_index);
			}
			//回收目标对象索引
			index_pool.recycle(min_valid_index);
			//更新有效索引起点
			min_valid_index++;
			//若当前为排序模式则重排序对象集合
			if (is_sorted)
				sort();
			//返回卸载成功
			return true;
		}
		//对象卸载 —— 单对象重载（目标不存在时告警）
		void unload(const Key& key)
		{
			//尝试卸载目标对象
			if (!try_unload(key))
				logger.warn("Object_Pool::目标对象不存在: {}", key);
		}
		//对象卸载 —— 多对象重载
		void unload(const std::vector<Key>& keys)
		{
			//去重哈希容器
			std::unordered_set<Key> removed_keys;
			//目标对象索引集合
			std::vector<uint64_t> records{};
			//收集目标卸载对象索引
			for (const Key& key : keys)
			{
				//若出现重复键则略过
				if (removed_keys.count(key))
					continue;
				//记录当前查找键
				removed_keys.insert(key);
				//获取目标对象迭代器
				auto it = find(key);
				//若目标迭代器无效
				if (it == objects.end())
				{
					logger.warn("Object_Pool::目标对象不存在: {}", key);
					continue;
				}
				//记录目标对象索引
				records.push_back(static_cast<uint64_t>(it - objects.begin()));
				//若当前为哈希定位模式则卸载索引映射
				if (!is_sorted)
					mapping.erase(key);
			}

			//升序排列索引防止失效
			std::ranges::sort(records);

			//逐个卸载目标对象
			for (const uint64_t& record_index : records)
			{
				//若目标不在无效区边界则交换到边界
				if (record_index != min_valid_index)
				{
					//交换目标与无效区边界对象
					std::swap(objects[record_index], objects[min_valid_index]);
					//若为哈希定位模式则同步更新被交换有效对象映射索引
					if (!is_sorted)
						mapping.insert_or_assign(key_get(objects[record_index]), record_index);
				}
				//回收目标对象索引
				index_pool.recycle(min_valid_index);
				//更新有效索引起点
				min_valid_index++;
			}

			//若当前为排序模式则重排序对象集合
			if (is_sorted)
				sort();
		}

		//对象清除
		void clear(void)
		{
			//清空所有对象
			objects.clear();

			//清空所有索引记录
			index_pool.reset();
			//恢复后进先出分配机制
			index_pool.set(Num_Reuse_Order::LIFO);
			//恢复显式指定起始索引
			index_pool.set(0);
			//重置有效索引起点
			min_valid_index = 0;

			//若为哈希定位模式则清空所有索引映射
			if (!is_sorted)
				mapping.clear();
		}

		//对象池槽位预分配
		bool reserve(const uint64_t& num)
		{
			//若当前对象池非空
			if (!objects.empty())
				return false;
			else
			{
				//若对象池为空则预创建对象槽位
				for (uint64_t time = 0; time < num; time++)
					objects.push_back({});
				//将预留槽位划入无效区
				min_valid_index = num;
				//返回槽位分配成功
				return true;
			}
		}
		//对象池槽位数量获取
		uint64_t capacity(void) const
		{
			return objects.size();
		}
		//合法对象数量获取
		uint64_t size(void) const
		{
			return objects.size() - min_valid_index;
		}

		//有效元素首迭代器获取 —— 非常量重载
		typename std::vector<T>::iterator begin(void)
		{
			return objects.begin() + min_valid_index;
		}
		//有效元素首迭代器获取 —— 常量重载
		typename std::vector<T>::const_iterator begin(void) const
		{
			//转调非常量重载获取有效元素首迭代器
			return const_cast<Object_Pool*>(this)->begin();
		}
		//超尾迭代器获取 —— 非常量重载
		typename std::vector<T>::iterator end(void)
		{
			return objects.end();
		}
		//超尾迭代器获取 —— 常量重载
		typename std::vector<T>::const_iterator end(void) const
		{
			return objects.end();
		}

	};
}
