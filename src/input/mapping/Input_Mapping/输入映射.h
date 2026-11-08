#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取输入类型
#include "../../common/输入类型.h"
//获取映射对象
#include "../Input_Object/映射对象.h"
//获取 JSON 解析外部库
#include <nlohmann/json.hpp>

//游戏引擎命名空间
namespace engine
{
	/*
	输入映射工具
	只做“配置 → 数据结构”的解析与轴值变换，不持有任何状态；
	对象池、事件分发与宿主关系由 Input_Manager 负责。
	*/
	namespace input_mapping
	{
		//键位解析（位域数字或名称）
		bool key_parse(const nlohmann::json& spec, Input_Key& key_out);
		//物理键位 → 规范化修饰掩码（非修饰键返回 0）
		uint8_t modifier_mask_of(const Input_Key& key);
		//组合修饰项解析（名称或位域数字，多项按位或合并）
		bool modifier_parse(const nlohmann::json& spec, uint8_t& mask_out);
		//映射项解析
		bool binding_parse(const nlohmann::json& spec, Input_Key& key_out,
			Input_Binding& binding_out);
		//映射表解析（同一主键可挂多条绑定，任一项非法则整体失败）
		bool mapping_parse(const nlohmann::json& spec,
			std::unordered_map<Input_Key, std::vector<Input_Binding>>& table);
		//监听表解析（只保留宿主自身映射表存在的键位）
		bool listening_parse(const nlohmann::json& spec, const Input_Object& host,
			std::unordered_set<Input_Key>& table);
		//轴绑定表解析（任一项非法则整体失败）
		bool axis_parse(const nlohmann::json& spec,
			std::unordered_map<std::string, Axis_Binding>& table);

		//轴变换结果
		struct Axis_Result
		{
			//轴值（速率型为速率，位移型为位移）
			double value = 0.0;
			//本帧位移增量
			double delta = 0.0;
		};
		//轴值变换（死区、缩放、反相、钳制；速率型乘时间步长得到增量）
		Axis_Result axis_transform(const Axis_Binding& binding, const double& raw_value,
			const double& dt);
	}

}
