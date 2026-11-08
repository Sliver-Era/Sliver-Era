#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//配置加载处理
	void Input_Manager::config_apply(const json& config)
	{
		//若配置缺少对象数组
		if (!config.contains("objects") || !config["objects"].is_array())
		{
			logger.warn("Input_Manager::配置缺少 objects 数组\n已忽略");
			return;
		}

		//逐项应用
		for (const auto& item : config["objects"])
		{
			//若配置项非对象或缺少对象名
			if (!item.is_object() || !detail::field_check<string>(item, "name"))
			{
				logger.warn("Input_Manager::配置对象项缺少有效 name\n已跳过");
				continue;
			}
			//获取对象名
			const string name = item["name"];
			//不存在则创建，已存在则更新
			if (!object_exist(name))
				object_create(item);
			else
				object_update(item);
		}
	}

	//对象状态通知（能力变化同步给焦点系统）
	void Input_Manager::object_state_notify(const uint64_t& ID)
	{
		//获取目标对象
		Input_Object* object = object_find(ID);
		//若目标对象不存在
		if (object == nullptr)
			return;

		//构造对象状态载荷
		json linked = json::object();
		linked[input_event::field_object] = object->object_name;
		linked["host"] = object->host_name;
		linked["focusable"] = object->focusable;
		linked["enabled"] = object->enabled;
		linked["text_sink"] = object->text_sink;
		//通知焦点系统
		focus_notify(input_event::object_linked, linked);
	}

}