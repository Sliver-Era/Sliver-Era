#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

//引擎命名空间
namespace engine
{
	//按目标对象请求转移（主对象压栈；子对象内部转移）
	void Focus_System::target_request(const string& target)
	{
		//若目标对象名为空
		if (target.empty())
		{
			logger.warn("Focus_System::焦点转移未指定目标对象");
			return;
		}

		//查找目标对象归属
		auto it = host_of.find(target);
		//若目标对象未知
		if (it == host_of.end())
		{
			logger.warn("Focus_System::焦点转移目标对象未知：{}", target);
			return;
		}

		//若目标不可聚焦
		auto focusable = focusable_of.find(target);
		if (focusable != focusable_of.end() && !focusable->second)
		{
			logger.warn("Focus_System::目标对象不可聚焦：{}", target);
			return;
		}
		//若目标在模态闸门外
		if (!object_within_modal(target))
		{
			logger.warn("Focus_System::目标对象在模态闸门外，已拒绝：{}", target);
			return;
		}

		//若目标为主对象
		if (it->second.empty())
		{
			main_push_request(target);
			return;
		}
		//若目标为当前主对象的子对象
		if (it->second == current_main)
		{
			child_request(target);
			return;
		}
		//若目标为其他主对象的子对象：先切换主对象，再交给子对象
		main_push_request(it->second);
		child_request(target);
	}

	//主对象压栈请求
	void Focus_System::main_push_request(const string& name)
	{
		//若目标对象名为空
		if (name.empty())
			return;

		//若目标已是当前主对象，撤销本帧先前请求并回到宿主
		if (current_main == name)
		{
			pending_main.clear();
			pending_child.clear();
			//标记“待生效子焦点清空”
			pending_child_clear = true;
			pending_pop = false;
			pending_valid = true;
			return;
		}

		//登记待生效主对象（同帧后写覆盖前写）
		pending_main = name;
		pending_child.clear();
		pending_child_clear = false;
		pending_pop = false;
		pending_valid = true;
	}

	//主对象弹栈请求
	void Focus_System::main_pop_request(void)
	{
		//若主对象栈没有可回退对象
		if (main_stack.size() < 2)
		{
			logger.warn("Focus_System::主对象栈无可回退对象\nEscape 已忽略");
			return;
		}

		//登记待生效弹栈
		pending_main.clear();
		pending_child.clear();
		pending_child_clear = false;
		pending_pop = true;
		pending_valid = true;
	}

	//子对象接管请求
	void Focus_System::child_request(const string& name)
	{
		//若目标对象名为空，表示焦点回到宿主
		if (name.empty())
		{
			pending_child.clear();
			pending_child_clear = true;
			pending_valid = true;
			return;
		}

		//登记待生效子对象
		pending_child = name;
		pending_child_clear = false;
		pending_valid = true;
	}

	//帧首统一应用
	void Focus_System::transition_apply(void)
	{
		//若无待生效转移
		if (!pending_valid)
			return;

		//变化前焦点记录
		const string before_main = current_main;
		const string before_child = current_child;

		//应用弹栈
		if (pending_pop)
		{
			//若栈内仍有可回退对象
			if (main_stack.size() >= 2)
				main_stack.pop_back();
			pending_pop = false;
		}

		//应用压栈
		if (!pending_main.empty())
		{
			//若目标不在栈顶
			if (main_stack.empty() || main_stack.back() != pending_main)
			{
				//移除历史同名项，避免循环压栈
				std::erase(main_stack, pending_main);
				//压入目标主对象
				main_stack.push_back(pending_main);
			}
			//清空待生效主对象
			pending_main.clear();
		}

		//同步当前主对象
		current_main = main_stack.empty() ? string{} : main_stack.back();

		//若主对象发生变化且未指定新子对象，清空子焦点
		if (current_main != before_main && pending_child.empty() && !pending_child_clear)
			current_child.clear();

		//应用子焦点清空（与主对象是否变化无关）
		if (pending_child_clear)
		{
			current_child.clear();
			pending_child.clear();
			pending_child_clear = false;
		}
		//应用子对象接管
		else if (!pending_child.empty())
		{
			//查找当前主对象的子对象集合
			auto host_it = children_of.find(current_main);
			//若子对象不属于当前主对象
			if (current_main.empty() || host_it == children_of.end() ||
				!host_it->second.count(pending_child))
				logger.warn("Focus_System::待生效子对象 {} 不属于当前主对象\n已忽略", pending_child);
			else
				//子对象接管输入
				current_child = pending_child;
			//清空待生效子对象
			pending_child.clear();
		}

		//清理待生效标记
		pending_valid = false;

		//按当前活跃对象重算文本模式
		text_mode_refresh();

		//若焦点发生变化则广播
		if (before_main != current_main || before_child != current_child)
			focus_changed_publish();
	}
}