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
	//轴绑定表解析
	bool axis_parse(const json& spec,
		std::unordered_map<string, Axis_Binding>& table)
	{
		//若字段为空值
		if (spec.is_null())
			return true;
		//若字段非数组
		if (!spec.is_array())
		{
			logger.warn("轴表字段非数组");
			return false;
		}

		//轴表缓冲
		std::unordered_map<string, Axis_Binding> buffer{};
		//解析结果标记
		bool parsed = true;
		//逐项解析
		for (const auto& item : spec)
		{
			//若缺少轴名或来源
			if (!item.is_object() || !detail::field_check<string>(item, "axis") ||
				!detail::field_check<string>(item, "source"))
			{
				logger.warn("轴项缺少 axis 或 source 字段");
				parsed = false;
				continue;
			}

			//获取轴名
			const string axis_name = item["axis"];
			//若轴名重复
			if (buffer.count(axis_name))
			{
				logger.warn("轴名重复，保留先注册项：{}", axis_name);
				parsed = false;
				continue;
			}

			//构造轴绑定
			Axis_Binding binding{};
			binding.source = item["source"];
			//非指针来源依赖原始轴载荷（渲染层桥不提供，需上层自行投递 Raw 的 axes）
			if (binding.source != "pointer.x" && binding.source != "pointer.y" &&
				binding.source != "pointer.dx" && binding.source != "pointer.dy")
				logger.info("轴 {} 来源 {}：需上层自行投递 Raw 的 axes 字段", axis_name, binding.source);
			//模式解析（缺省为位移型）
			const string mode = item.contains("mode") && item["mode"].is_string()
				? item["mode"].get<string>() : string{ "displacement" };
			if (mode == "rate")
				binding.rate = true;
			else if (mode == "displacement")
				binding.rate = false;
			else
			{
				logger.warn("轴模式无法识别：{}", mode);
				parsed = false;
				continue;
			}
			//数值与开关项
			if (item.contains("scale") && item["scale"].is_number())
				binding.scale = item["scale"].get<double>();
			if (item.contains("deadzone") && item["deadzone"].is_number())
				binding.deadzone = item["deadzone"].get<double>();
			if (item.contains("clamp") && item["clamp"].is_number())
				binding.clamp = item["clamp"].get<double>();
			if (item.contains("invert") && item["invert"].is_boolean())
				binding.invert = item["invert"].get<bool>();

			//登记轴绑定
			buffer.emplace(axis_name, binding);
		}

		//任一项非法则整体拒绝
		if (!parsed)
			return false;

		//提交缓冲
		table = std::move(buffer);
		return true;
	}


	//轴值变换（死区、缩放、反相、钳制；速率型乘时间步长得到增量）
	Axis_Result axis_transform(const Axis_Binding& binding, const double& raw_value,
		const double& dt)
	{
		//轴值
		double value = raw_value;
		//死区内归零
		if (binding.deadzone > 0.0 && std::abs(value) < binding.deadzone)
			value = 0.0;
		//缩放
		value *= binding.scale;
		//反相
		if (binding.invert)
			value = -value;
		//钳制
		if (binding.clamp > 0.0 && std::abs(value) > binding.clamp)
			value = value > 0.0 ? binding.clamp : -binding.clamp;

		//装配结果
		Axis_Result result{};
		result.value = value;
		result.delta = binding.rate ? value * dt : value;
		return result;
	}
	}
}