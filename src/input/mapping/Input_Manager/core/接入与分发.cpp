#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//构造函数
	Input_Manager::Input_Manager(void)
	{
		//编号 0 保留为非法哨兵
		id_pool.set(1);
		//生成权限密钥（一次性生成，重复调用返回空，故兜底为 0）
		acl_key = event_terminal.key().value_or(0);
	}

	//事件中转站接入
	bool Input_Manager::attach(void)
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

		//接入事件中转站（仅登记订阅者身份与接收入口）
		if (!event_terminal.attach("Input_Manager", acl_key))
		{
			logger.error("Input_Manager::事件中转站接入失败");
			return false;
		}

		//订阅事件清单
		vector<Event> needed_events{
			Event("", "", input_event::category, input_event::create),
			Event("", "", input_event::category, input_event::destroy),
			Event("", "", input_event::category, input_event::reload),
			Event("", "", input_event::category, input_event::forward),
			Event("", "", input_event::category, input_event::text_route),
			Event("", "", event_protocol::config, event_protocol::load)
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
			logger.error("Input_Manager::订阅事件构造失败");
			return false;
		}
		//写入目标对象名、权限密钥与订阅清单
		attach_evt->config[event_protocol::field_object] = "Input_Manager";
		attach_evt->config[event_protocol::field_acl_key] = static_cast<uint64_t>(acl_key);
		attach_evt->config[event_protocol::field_subscribed_events] = subscribed_events;
		//发送订阅事件
		if (!event_terminal.send(attach_evt, acl_key))
		{
			logger.error("Input_Manager::订阅事件发送失败");
			return false;
		}

		//标记已接入
		attached = true;
		return true;
	}

	//事件处理
	void Input_Manager::event_process(shared_ptr<Event> evt)
	{
		//若事件为空
		if (!evt)
			return;

		//若为配置加载
		if (evt->category == event_protocol::config && evt->tag == event_protocol::load)
		{
			config_apply(evt->config);
			return;
		}
		//若事件分类不是输入
		if (evt->category != input_event::category)
			return;

		//若为对象创建
		if (evt->tag == input_event::create)
		{
			object_create(evt->config);
			return;
		}
		//若为对象销毁
		if (evt->tag == input_event::destroy)
		{
			//若目标对象字段无效
			if (!detail::field_check<string>(evt->config, input_event::field_object))
				return;
			object_destroy(evt->config[input_event::field_object]);
			return;
		}
		//若为映射表重载
		if (evt->tag == input_event::reload)
		{
			object_reload(evt->config);
			return;
		}
		//若为原始输入转发
		if (evt->tag == input_event::forward)
		{
			raw_forward(evt);
			return;
		}
		//若为文本会话转发
		if (evt->tag == input_event::text_route)
		{
			text_forward(evt);
			return;
		}
	}

	//语义命令中转
	void Input_Manager::command_relay(shared_ptr<Event> evt)
	{
		//若事件为空
		if (!evt)
		{
			logger.warn("Input_Manager::语义命令为空\n已忽略");
			return;
		}

		//获取映射语义与目标对象
		const string command = evt->config.value("command", string{});
		const string target = evt->config.value("target", string{});

		//若 Enter 的目标是发件对象自家子对象
		if (command == "Enter" && !target.empty() && object_is_child_of(target, evt->sender))
		{
			//构造子对象焦点请求
			json config = json::object();
			config["host"] = evt->sender;
			config["child"] = target;
			//若携带帧号则一并透传
			if (evt->config.contains("frame"))
				config["frame"] = evt->config["frame"];
			//通知焦点系统
			focus_notify(input_event::child_focus, config);
			return;
		}

		//其余语义命令经中转站广播
		if (!event_terminal.send(evt, acl_key))
			logger.error("Input_Manager::语义命令中转失败");
	}

	//焦点系统定向通知
	void Input_Manager::focus_notify(const string& tag, const json& config)
	{
		//构造通知事件
		auto evt = event_terminal.build("Input_Manager", "Focus_System", input_event::category, tag);
		//若事件构造失败
		if (!evt)
		{
			logger.error("Input_Manager::焦点系统通知构造失败：{}", tag);
			return;
		}
		//写入通知载荷
		evt->config = config;
		//定向发送到焦点系统
		if (!event_terminal.send(evt, acl_key))
			logger.warn("Input_Manager::焦点系统通知发送失败：{}", tag);
	}

	//对象接收入口分发
	void Input_Manager::object_dispatch(const uint64_t& ID, shared_ptr<Event> evt)
	{
		//若事件为空
		if (!evt)
			return;
		//若事件分类不是输入
		if (evt->category != input_event::category)
			return;
		//获取目标对象
		Input_Object* object = object_find(ID);
		//若目标对象不存在
		if (object == nullptr)
			return;

		//若为文本会话
		if (evt->tag == input_event::text_session)
		{
			//构造文本语义事件
			auto text_evt = object->event_terminal.build(object->object_name, "", input_event::category, input_event::text);
			//若事件构造失败
			if (!text_evt)
			{
				logger.error("Input_Manager::文本语义事件构造失败");
				return;
			}
			//写入会话载荷并广播
			text_evt->config = evt->config;
			object->event_terminal.send(text_evt, object->acl_key);
			return;
		}
		//非原始输入帧不处理
		if (evt->tag != input_event::raw_frame)
			return;
		//执行原始帧映射
		object_map(ID, evt->config);
	}
}