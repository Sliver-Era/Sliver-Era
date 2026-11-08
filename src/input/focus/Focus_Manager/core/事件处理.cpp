#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//事件处理
	void Focus_System::event_process(shared_ptr<Event> evt)
	{
		//若事件为空
		if (!evt)
			return;
		//若事件分类不是输入
		if (evt->category != input_event::category)
			return;

		//若为原始输入
		if (evt->tag == input_event::raw)
		{
			raw_process(evt);
			return;
		}
		//若为语义命令
		if (evt->tag == input_event::command)
		{
			command_process(evt);
			return;
		}
		//若为鼠标焦点
		if (evt->tag == input_event::mouse_focus)
		{
			mouse_focus_process(evt);
			return;
		}
		//若为子对象焦点请求
		if (evt->tag == input_event::child_focus)
		{
			child_focus_process(evt);
			return;
		}
		//若为对象接入
		if (evt->tag == input_event::object_linked)
		{
			object_linked_process(evt);
			return;
		}
		//若为对象卸载
		if (evt->tag == input_event::object_destroyed)
		{
			object_destroyed_process(evt);
			return;
		}
		//若为模态压栈
		if (evt->tag == input_event::modal_push)
		{
			modal_push_process(evt);
			return;
		}
		//若为模态弹栈
		if (evt->tag == input_event::modal_pop)
		{
			modal_pop_process(evt);
			return;
		}
		//若为文本会话
		if (evt->tag == input_event::text_session)
		{
			text_session_process(evt);
			return;
		}
	}

	//原始输入处理
	void Focus_System::raw_process(shared_ptr<Event> evt)
	{
		//当前活跃对象（子对象接管时直投子对象）
		const string target = current_child.empty() ? current_main : current_child;
		//若当前无活跃对象
		if (target.empty())
		{
			logger.warn("Focus_System::当前无活跃对象\n原始输入已丢弃");
			return;
		}
		//若目标不可接收输入
		auto enabled = enabled_of.find(target);
		if (enabled != enabled_of.end() && !enabled->second)
		{
			logger.warn("Focus_System::活跃对象已禁用，原始输入已丢弃：{}", target);
			return;
		}
		//若目标在模态闸门外
		if (!object_within_modal(target))
		{
			logger.warn("Focus_System::活跃对象在模态闸门外，原始输入已丢弃：{}", target);
			return;
		}

		//复制原始帧载荷并写入目标对象与文本模式
		json config = evt->config;
		config[input_event::field_object] = target;
		config["text_mode"] = text_mode;
		//定向转发给输入系统
		focus_send(input_event::forward, config, "Input_Manager");
	}

	//语义命令处理
	void Focus_System::command_process(shared_ptr<Event> evt)
	{
		//若命令字段无效
		if (!detail::field_check<string>(evt->config, "command"))
			return;

		//若命令来自模态闸门外的对象
		if (!object_within_modal(evt->sender))
		{
			logger.warn("Focus_System::命令来自模态闸门外，已丢弃：{}", evt->sender);
			return;
		}
		//若命令来自已禁用对象
		auto enabled = enabled_of.find(evt->sender);
		if (enabled != enabled_of.end() && !enabled->second)
			return;

		//获取映射语义
		const string command = evt->config["command"];
		//若为退出语义
		if (command == "Escape")
		{
			main_pop_request();
			return;
		}
		//若为非进入语义
		if (command != "Enter")
			return;

		//获取目标对象名
		const string target = evt->config.value("target", string{});
		//按目标请求转移
		target_request(target);
	}

	//鼠标焦点处理
	void Focus_System::mouse_focus_process(shared_ptr<Event> evt)
	{
		//若目标对象字段无效
		if (!detail::field_check<string>(evt->config, input_event::field_object))
			return;
		//按目标请求转移
		target_request(evt->config[input_event::field_object]);
	}

	//子对象焦点请求处理
	void Focus_System::child_focus_process(shared_ptr<Event> evt)
	{
		//获取宿主与子对象
		const string host = evt->config.value("host", string{});
		const string child = evt->config.value("child", string{});
		//子对象名为空表示焦点回到宿主
		if (child.empty())
		{
			//若宿主不是当前主对象
			if (host != current_main)
			{
				logger.warn("Focus_System::子焦点清空请求与当前主对象不匹配");
				return;
			}
			//请求回到宿主
			child_request(string{});
			return;
		}
		//若宿主不是当前主对象
		if (host != current_main)
		{
			logger.warn("Focus_System::子对象焦点请求与当前主对象不匹配（{} / {}）", host, child);
			return;
		}
		//若目标子对象不可聚焦
		auto focusable = focusable_of.find(child);
		if (focusable != focusable_of.end() && !focusable->second)
		{
			logger.warn("Focus_System::子对象不可聚焦：{}", child);
			return;
		}
		//子对象接管输入
		child_request(child);
	}

	//对象接入处理
	void Focus_System::object_linked_process(shared_ptr<Event> evt)
	{
		//获取对象名与宿主名
		const string name = evt->config.value(input_event::field_object, string{});
		const string host = evt->config.value("host", string{});
		//若对象名为空
		if (name.empty())
			return;

		//登记对象归属
		host_of[name] = host;
		//登记对象能力
		focusable_of[name] = evt->config.value("focusable", true);
		enabled_of[name] = evt->config.value("enabled", true);
		text_sink_of[name] = evt->config.value("text_sink", false);

		//若为子对象则登记到宿主
		if (!host.empty())
			children_of[host].insert(name);
		//若为第一个主对象则默认获取焦点
		else if (main_stack.empty() && current_main.empty() && !pending_valid)
			main_push_request(name);

		//能力变化可能影响文本模式
		text_mode_refresh();
	}

	//对象卸载处理
	void Focus_System::object_destroyed_process(shared_ptr<Event> evt)
	{
		//获取对象名
		const string name = evt->config.value(input_event::field_object, string{});
		//若对象名为空
		if (name.empty())
			return;

		//清理前焦点记录
		const string before_main = current_main;
		const string before_child = current_child;
		const string before_modal = modal_stack.empty() ? string{} : modal_stack.back().object;

		//查找对象归属
		auto it = host_of.find(name);
		//记录宿主名
		const string host = it != host_of.end() ? it->second : string{};
		//移除归属与能力记录
		host_of.erase(name);
		focusable_of.erase(name);
		enabled_of.erase(name);
		text_sink_of.erase(name);

		//从宿主子对象集合移除
		if (!host.empty())
		{
			//查找宿主子对象集合
			auto host_it = children_of.find(host);
			//若宿主仍挂载子对象
			if (host_it != children_of.end())
			{
				//移除目标对象
				host_it->second.erase(name);
				//若宿主已无子对象
				if (host_it->second.empty())
					children_of.erase(host_it);
			}
		}
		//清除目标对象的子对象集合
		children_of.erase(name);
		//从主对象栈移除全部同名项
		std::erase(main_stack, name);
		//从模态栈移除全部同名项
		std::erase_if(modal_stack,
			[&name](const Modal_Entry& entry) -> bool
			{
				return entry.object == name;
			});

		//清理当前子对象与待生效状态
		if (current_child == name)
			current_child.clear();
		if (pending_child == name)
			pending_child.clear();
		if (pending_main == name)
		{
			pending_main.clear();
			pending_valid = false;
		}

		//同步当前主对象
		current_main = main_stack.empty() ? string{} : main_stack.back();

		//重算文本模式
		text_mode_refresh();

		//若焦点或模态发生变化则广播
		const string after_modal = modal_stack.empty() ? string{} : modal_stack.back().object;
		if (before_main != current_main || before_child != current_child || before_modal != after_modal)
			focus_changed_publish();
	}

	//文本会话处理
	void Focus_System::text_session_process(shared_ptr<Event> evt)
	{
		//非文本模式下不转交文本会话
		if (!text_mode)
		{
			logger.warn("Focus_System::非文本模式，文本会话已丢弃");
			return;
		}

		//当前活跃对象
		const string target = current_child.empty() ? current_main : current_child;
		//若无活跃对象
		if (target.empty())
			return;

		//复制会话载荷并写入目标对象
		json config = evt->config;
		config[input_event::field_object] = target;
		//定向转发给输入系统，由映射对象转成文本语义
		focus_send(input_event::text_route, config, "Input_Manager");
	}
}