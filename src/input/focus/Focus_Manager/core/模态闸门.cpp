#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//目标是否在模态闸门内（无模态时恒为真）
	bool Focus_System::object_within_modal(const string& name) const
	{
		//若无模态则全部放行
		if (modal_stack.empty())
			return true;
		//获取当前生效的模态根
		const string& root = modal_stack.back().object;
		//模态根自身放行
		if (name == root)
			return true;

		//深度优先查找目标是否为模态根子孙
		vector<string> pending{ root };
		while (!pending.empty())
		{
			//取当前节点
			const string current = pending.back();
			pending.pop_back();
			//查找其子对象集合
			auto it = children_of.find(current);
			//若无子对象
			if (it == children_of.end())
				continue;
			//逐个子对象比对
			for (const auto& child : it->second)
			{
				//命中目标即放行
				if (child == name)
					return true;
				pending.push_back(child);
			}
		}

		return false;
	}

	//模态压栈处理
	void Focus_System::modal_push_process(shared_ptr<Event> evt)
	{
		//获取模态根对象名
		const string object = evt->config.value(input_event::field_object, string{});
		//若对象名为空
		if (object.empty())
		{
			logger.warn("Focus_System::模态压栈未指定对象");
			return;
		}
		//若目标对象未知
		if (!host_of.count(object))
		{
			logger.warn("Focus_System::模态对象未知：{}", object);
			return;
		}
		//若已是当前模态根
		if (!modal_stack.empty() && modal_stack.back().object == object)
			return;

		//压入模态栈（记录压栈者供弹栈校验）
		modal_stack.push_back({ object, evt->sender });
		//广播焦点状态（含模态）
		focus_changed_publish();
		logger.info("Focus_System::模态已压栈：{}", object);
	}

	//模态弹栈处理
	void Focus_System::modal_pop_process(shared_ptr<Event> evt)
	{
		//若无模态
		if (modal_stack.empty())
		{
			logger.warn("Focus_System::模态栈为空，弹栈已忽略");
			return;
		}
		//若请求者不是当前模态的压栈者
		if (evt->sender != modal_stack.back().owner)
		{
			logger.warn("Focus_System::模态弹栈请求者与压栈者不匹配，已拒绝：{}", evt->sender);
			return;
		}

		//弹出当前模态
		modal_stack.pop_back();
		//广播焦点状态（含模态）
		focus_changed_publish();
		logger.info("Focus_System::模态已弹栈");
	}

}