#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取随机数生成器
#include "Engine/EngineFoundation/src/detail/class/Random_Generator/随机数生成器.h"

namespace engine
{
	//新数值分配方式
	enum class Num_Allocate_Order
	{
		Less,
		Greater,
		Random
	};
	//回收数值复用方式
	enum class Num_Reuse_Order
	{
		LIFO,
		FIFO
	};

	//数值池 —— 模板参数 T 指定分配数值类型
	template <typename T = uint64_t>
		requires std::is_integral_v<T>
	class Number_Pool
	{
	private:
		//新数值分配方式 —— 默认升序
		Num_Allocate_Order allocate_order = Num_Allocate_Order::Less;
		//回收数值复用方式 —— 默认后进先出
		Num_Reuse_Order reuse_order = Num_Reuse_Order::LIFO;

		//起始分配数值
		T start_number = (std::numeric_limits<T>::min)();
		//可分配新数值
		T next_number = start_number;
		//随机数生成器
		Random_Generator<T> random;
		//数值容量
		T number_capacity = (std::numeric_limits<T>::max)();

		//已分配数值集合
		std::unordered_set<T> allocated_numbers;
		//回收数值集合
		std::deque<T> recycle_numbers;
		//回收数值映射
		std::unordered_set<T> mapping;
		//黑名单数值集合
		std::unordered_set<T> banned_numbers;

		//取当前游标并升序前进，越界显式回绕
		T cursor_take_up(void)
		{
			//当前游标值
			T number = next_number;
			//若已到类型最大值
			if (next_number == (std::numeric_limits<T>::max)())
				next_number = (std::numeric_limits<T>::min)();
			else
				next_number++;
			return number;
		}
		//取当前游标并降序前进，越界显式回绕
		T cursor_take_down(void)
		{
			//当前游标值
			T number = next_number;
			//若已到类型最小值
			if (next_number == (std::numeric_limits<T>::min)())
				next_number = (std::numeric_limits<T>::max)();
			else
				next_number--;
			return number;
		}

	public:
		//设置分配起点
		void set(T start_number)
		{
			this->start_number = start_number;
			//若为升序分配
			//且待分配新数值大于分配起点
			if(allocate_order == Num_Allocate_Order::Less && this->start_number < next_number)
			{
				logger.info("Number_Pool::当前为升序分配");
				logger.info("Number_Pool::新分配起点小于待分配新数值");
				logger.info("Number_Pool::待分配数值保持当前大小");
			}
			//若为降序分配
			//且待分配新数值小于分配起点
			else if (allocate_order == Num_Allocate_Order::Greater && this->start_number > next_number)
			{
				logger.info("Number_Pool::当前为降序分配");
				logger.info("Number_Pool::新分配起点大于待分配新数值");
				logger.info("Number_Pool::待分配数值保持当前大小");
			}
			else
				next_number = start_number;
		}
		//设置回收数值复用机制
		void set(Num_Reuse_Order reuse_order)
		{
			this->reuse_order = reuse_order;
		}
		//设置新数值分配机制
		void set(Num_Allocate_Order allocate_order)
		{
			this->allocate_order = allocate_order;
		}
		//获取可用数值
		std::optional<T> get(void)
		{
			//待返回数值记录
			T number;

			for (;;)
			{
				//若回收数值集合不为空
				if (!recycle_numbers.empty())
				{
					//若为后进先出分配机制
					if (reuse_order == Num_Reuse_Order::LIFO)
					{
						//弹出末元素
						number = recycle_numbers.back();
						recycle_numbers.pop_back();
					}
					//若为后进后出分配机制
					else
					{
						//弹出首元素
						number = recycle_numbers.front();
						recycle_numbers.pop_front();
					}

					//清除数值映射
					mapping.erase(number);
				}
				//若回收数值集合为空
				else
				{
					//若可分配数值耗尽
					if (allocated_numbers.size() == number_capacity)
					{
						logger.warn("Number_Pool::可分配数值耗尽\n无法返回有效值");
						return std::nullopt;
					}
					else
					{
						//获取新数值
						if (allocate_order == Num_Allocate_Order::Less)
							number = cursor_take_up();
						else if (allocate_order == Num_Allocate_Order::Greater)
							number = cursor_take_down();
						else
							number = random();
					}
				}

				//若当前数值未被禁用
				if (!banned_numbers.count(number) && !allocated_numbers.count(number))
				{
					//记录数值已分配
					allocated_numbers.insert(number);
					return number;
				}
			}
		}
		//回收数值 —— 单数值重载
		bool recycle(T recycle_number)
		{
			//获取待回收数值迭代器
			auto it = allocated_numbers.find(recycle_number);
			//若待回收数值已分配
			if (it != allocated_numbers.end())
			{
				//回收数值
				recycle_numbers.push_back(recycle_number);
				//建立映射
				mapping.insert(recycle_number);
				//标记数值可分配
				allocated_numbers.erase(it);
				//返回回收成功
				return true;
			}
			else
			{
				//若待回收数值被禁用
				if(banned_numbers.count(recycle_number))
				    logger.warn("Number_Pool::待回收数值{}被禁用!!!", recycle_number);
				else
					logger.warn("Number_Pool::待回收数值{}未分配!!!", recycle_number);
				return false;
			}
		}
		//回收数值 —— 多数值重载
		void recycle(const std::vector<T>& numbers)
		{
			//循环调用单数值重载
			for (auto number : numbers)
				recycle(number);
		}
		//禁用数值 —— 单数值重载
		bool ban(T banned_number)
		{
			//若目标数值已禁用
			if (banned_numbers.count(banned_number))
			{
				logger.warn("Number_Pool::目标数值{}已被禁用!!!", banned_number);
				return false;
			}
			else
			{
				//记录禁用数值
				banned_numbers.insert(banned_number);
				//获取禁用数值迭代器
				auto it = allocated_numbers.find(banned_number);
				//若禁用数值已分配则销毁分配记录
				if (it != allocated_numbers.end())
					allocated_numbers.erase(it);
				//更新数值容量
				number_capacity--;
				return true;
			}
		}
		//禁用数值 —— 单数值重载
		void ban(const std::vector<T>& numbers)
		{
			//循环调用单数值重载
			for (auto number : numbers)
				ban(number);
		}
		//解禁数值 —— 单数值重载
		bool unban(T unbanned_number)
		{
			//获取目标数值迭代器
			auto it = banned_numbers.find(unbanned_number);
			//若目标数值已禁用
			if (it != banned_numbers.end())
			{
				//解禁数值
				banned_numbers.erase(unbanned_number);
				//更新数值容量
				number_capacity++;
				return true;
			}
			else
			{
				logger.warn("Number_Pool::目标数值{}未被禁用!!!", unbanned_number);
				return false;
			}
		}
		//解禁数值 —— 单数值重载
		void unban(const std::vector<T>& numbers)
		{
			//循环调用单数值重载
			for (auto number : numbers)
				unban(number);
		}
		//数值分配计数获取
		uint64_t counts(void)const
		{
			return allocated_numbers.size();
		}
		//数值容量获取
		uint64_t capacity(void)const
		{
			return number_capacity;
		}
		//重置分配器
		void reset(void)
		{
			//重置新数值分配策略
			allocate_order = Num_Allocate_Order::Less;
			//重置回收数值复用策略
			reuse_order = Num_Reuse_Order::LIFO;

			//重置起始分配数值
			start_number = (std::numeric_limits<T>::min)();
			//重置可分配新数值
			next_number = start_number;
			//重置数值容量
			number_capacity = (std::numeric_limits<T>::max)();

			//清空分配数值集合
			allocated_numbers.clear();
			//重置回收数值集合
			recycle_numbers.clear();
			//重置回收数值映射
			mapping.clear();
			//清空禁用数值集合
			banned_numbers.clear();
		}
	};
}
