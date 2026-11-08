#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取字段校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
	//输入映射工具
	namespace input_mapping
	{
	//映射项解析
	bool binding_parse(const json& spec, Input_Key& key_out,
		Input_Binding& binding_out)
	{
		//若映射项非对象
		if (!spec.is_object())
		{
			logger.warn("映射项非对象格式");
			return false;
		}
		//若缺少键位字段
		if (!spec.contains("key"))
		{
			logger.warn("映射项缺少 key 字段");
			return false;
		}
		//若语义字段无效
		if (!detail::field_check<string>(spec, "command"))
		{
			logger.warn("映射项缺少 command 字段");
			return false;
		}
		//解析键位
		if (!key_parse(spec["key"], key_out))
		{
			logger.warn("映射键位无法识别");
			return false;
		}

		//写入语义
		binding_out.command = spec["command"];
		binding_out.target.clear();
		binding_out.modifier_mask = 0;
		binding_out.allow_repeat = false;
		//若指定组合修饰项（配置词汇，不经事件总线，故此处用字面量）
		if (spec.contains("modifiers") &&
			!modifier_parse(spec["modifiers"], binding_out.modifier_mask))
		{
			logger.warn("映射项修饰键无法识别");
			return false;
		}
		//若指定长按重复开关
		if (spec.contains("repeat") && spec["repeat"].is_boolean())
			binding_out.allow_repeat = spec["repeat"].get<bool>();
		//若指定目标对象
		if (spec.contains("target") && spec["target"].is_string())
			binding_out.target = spec["target"].get<string>();

		//Enter 必须指定目标对象
		if (binding_out.command == "Enter" && binding_out.target.empty())
		{
			logger.warn("Enter 映射未指定目标对象");
			return false;
		}
		//Escape 不使用目标对象
		if (binding_out.command == "Escape")
			binding_out.target.clear();

		return true;
	}

	//映射表解析（失败时保持旧表）
	bool mapping_parse(const json& spec,
		std::unordered_map<Input_Key, std::vector<Input_Binding>>& table)
	{
		//若字段为空值
		if (spec.is_null())
			return true;
		//若字段非数组
		if (!spec.is_array())
		{
			logger.warn("映射表字段非数组");
			return false;
		}

		//映射表缓冲
		std::unordered_map<Input_Key, std::vector<Input_Binding>> buffer{};
		//解析结果标记
		bool parsed = true;
		//逐项解析
		for (const auto& item : spec)
		{
			Input_Key key = 0;
			Input_Binding binding;
			//若单项解析失败
			if (!binding_parse(item, key, binding))
			{
				parsed = false;
				continue;
			}
			//若该主键下已有同修饰掩码的绑定
			auto it = buffer.find(key);
			if (it != buffer.end())
			{
				//查找修饰掩码重复项
				const bool duplicate = std::any_of(it->second.begin(), it->second.end(),
					[&binding](const Input_Binding& existing)
					{
						return existing.modifier_mask == binding.modifier_mask;
					});
				//若修饰组合重复
				if (duplicate)
				{
					logger.warn("映射键位与修饰组合重复，保留先注册项");
					parsed = false;
					continue;
				}
			}
			//登记映射
			buffer[key].push_back(binding);
		}

		//任一项非法则整体拒绝，避免半生效
		if (!parsed)
			return false;

		//提交缓冲
		table = std::move(buffer);
		return true;
	}

	//监听表解析（只保留宿主自身映射表存在的键位）
	bool listening_parse(const json& spec, const Input_Object& host,
		std::unordered_set<Input_Key>& table)
	{
		//若字段为空值
		if (spec.is_null())
			return true;
		//若字段非数组
		if (!spec.is_array())
		{
			logger.warn("监听表字段非数组");
			return false;
		}

		//监听表缓冲
		std::unordered_set<Input_Key> buffer{};
		//逐项解析
		for (const auto& item : spec)
		{
			Input_Key key = 0;
			//若键位无法识别
			if (!key_parse(item, key))
			{
				logger.warn("监听键位无法识别，已丢弃");
				continue;
			}
			//监听表只能引用宿主自身映射表
			if (!host.mapping_has(key))
			{
				logger.warn("监听键位不在宿主映射表，已丢弃（键位 {}）", key);
				continue;
			}
			//登记监听键位
			buffer.insert(key);
		}

		//提交缓冲
		table = std::move(buffer);
		return true;
	}

	}
}