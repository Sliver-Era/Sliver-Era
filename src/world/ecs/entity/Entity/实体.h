#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
	/*
	实体句柄
	高 32 位记录代际，低 32 位记录实体表槽位；
	句柄值为 0 表示无效句柄（故槽位 0 预留、代际从 1 起）。
	*/
	struct Entity
	{
		//句柄打包值
		uint64_t value = 0;

		//槽位索引获取
		uint32_t index(void) const
		{
			return static_cast<uint32_t>(value & 0xFFFFFFFFu);
		}
		//代际获取
		uint32_t generation(void) const
		{
			return static_cast<uint32_t>(value >> 32);
		}
		//空句柄判定
		bool empty(void) const
		{
			return value == 0;
		}
		//句柄相等判定
		bool operator==(const Entity& other) const
		{
			return value == other.value;
		}
		//句柄不等判定
		bool operator!=(const Entity& other) const
		{
			return value != other.value;
		}
	};

	//实体句柄构造
	inline Entity entity_make(const uint32_t& index, const uint32_t& generation)
	{
		//句柄记录
		Entity entity;
		//拼装槽位索引与代际
		entity.value = (static_cast<uint64_t>(generation) << 32)
			| static_cast<uint64_t>(index);
		return entity;
	}
}
