#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//构造函数
	Focus_System::Focus_System(void)
	{
		//生成权限密钥（一次性生成，重复调用返回空，故兜底为 0）
		acl_key = event_terminal.key().value_or(0);
	}

	//事件中转站接入
	bool Focus_System::attach(void)
	{
		//若已经接入
		if (attached)
			return true;

		//构造事件接收入口
		auto receive_entry = [this](shared_ptr<Event> evt) -> void
			{
				this->event_process(evt);
			};
		//注册事件接收入口
		event_terminal->bind(Port_ID::Evt_Receiver, receive_entry);

		//接入事件中转站
		if (!event_terminal.attach("Focus_System", acl_key))
		{
			logger.error("Focus_System::事件中转站接入失败");
			return false;
		}

		//订阅事件清单
		vector<Event> needed_events{
			Event("", "", input_event::category, input_event::raw),
			Event("", "", input_event::category, input_event::command),
			Event("", "", input_event::category, input_event::mouse_focus),
			Event("", "", input_event::category, input_event::child_focus),
			Event("", "", input_event::category, input_event::object_linked),
			Event("", "", input_event::category, input_event::object_destroyed),
			Event("", "", input_event::category, input_event::modal_push),
			Event("", "", input_event::category, input_event::modal_pop),
			Event("", "", input_event::category, input_event::text_session)
		};
		//构造订阅事件清单
		json subscribed_events = json::array();
		//逐条写入订阅事件标识
		for (const auto& needed_event : needed_events)
		{
			json subscribed_event;
			subscribed_event[event_protocol::field_category] = needed_event.category;
			subscribed_event[event_protocol::field_tag] = needed_event.tag;
			subscribed_events.push_back(subscribed_event);
		}

		//构造订阅事件
		auto attach_evt = event_terminal.build("", "", event_protocol::subscribed_events, event_protocol::add);
		//若事件构造失败
		if (!attach_evt)
		{
			logger.error("Focus_System::订阅事件构造失败");
			return false;
		}
		//写入目标对象名、权限密钥与订阅清单
		attach_evt->config[event_protocol::field_object] = "Focus_System";
		attach_evt->config[event_protocol::field_acl_key] = static_cast<uint64_t>(acl_key);
		attach_evt->config[event_protocol::field_subscribed_events] = subscribed_events;
		//发送订阅事件
		if (!event_terminal.send(attach_evt, acl_key))
		{
			logger.error("Focus_System::订阅事件发送失败");
			return false;
		}

		//标记已接入
		attached = true;
		return true;
	}

	//帧首应用待生效转移
	void Focus_System::frame_begin(void)
	{
		transition_apply();
	}
	//当前主对象获取
	const string& Focus_System::current_main_get(void) const
	{
		return current_main;
	}

	//当前子对象获取
	const string& Focus_System::current_child_get(void) const
	{
		return current_child;
	}

	//主对象栈深度获取
	std::size_t Focus_System::stack_size(void) const
	{
		return main_stack.size();
	}

	//文本模式状态获取
	bool Focus_System::text_mode_get(void) const
	{
		return text_mode;
	}

	//模态栈深度获取
	std::size_t Focus_System::modal_size(void) const
	{
		return modal_stack.size();
	}
	//焦点变更广播
	void Focus_System::focus_changed_publish(void)
	{
		//构造变更载荷
		json config = json::object();
		config["main"] = current_main;
		config["child"] = current_child;
		config["modal"] = modal_stack.empty() ? string{} : modal_stack.back().object;
		config["text"] = text_mode;
		//广播变更
		focus_send(input_event::focus_changed, config, "");
	}

	//文本模式变更广播
	void Focus_System::text_mode_publish(void)
	{
		//构造模式载荷
		json config = json::object();
		config["active"] = text_mode;
		config["main"] = current_main;
		config["child"] = current_child;
		//广播模式变更（渲染层据此启停平台文本输入）
		focus_send(input_event::text_mode, config, "");
	}
	//文本模式重算
	void Focus_System::text_mode_refresh(void)
	{
		//当前活跃对象
		const string active = current_child.empty() ? current_main : current_child;
		//文本模式标记
		bool active_text = false;
		//若活跃对象是文本接收者且未禁用
		if (!active.empty())
		{
			//查找文本接收者标记
			auto text_it = text_sink_of.find(active);
			if (text_it != text_sink_of.end() && text_it->second)
			{
				//查找启用标记
				auto enabled_it = enabled_of.find(active);
				if (enabled_it == enabled_of.end() || enabled_it->second)
					active_text = true;
			}
		}
		//若文本模式发生变化
		if (active_text != text_mode)
		{
			text_mode = active_text;
			text_mode_publish();
		}
	}
	//事件发送
	void Focus_System::focus_send(const string& tag, const json& config, const string& receiver)
	{
		//构造事件
		auto evt = event_terminal.build("Focus_System", receiver, input_event::category, tag);
		//若事件构造失败
		if (!evt)
		{
			logger.error("Focus_System::事件构造失败：{}", tag);
			return;
		}
		//写入载荷
		evt->config = config;
		//发送事件
		if (!event_terminal.send(evt, acl_key))
			logger.warn("Focus_System::事件发送失败：{}", tag);
	}
}