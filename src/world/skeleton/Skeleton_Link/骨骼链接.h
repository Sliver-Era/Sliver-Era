#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取骨骼资产
#include "Engine/EngineSystem/src/world/skeleton/Skeleton_Asset/骨骼资产.h"
//获取骨骼姿态
#include "Engine/EngineSystem/src/world/skeleton/Skeleton_Pose/骨骼姿态.h"

//游戏引擎命名空间
namespace engine
{
	/*
	单条骨骼到碰撞子部件的绑定
	演员挂载时由资产中的 Bone_Part_Binding 复制而来；
	relative 为子部件相对骨骼的局部变换，用于把骨骼世界矩阵换算为部件世界矩阵。
	*/
	struct Skeleton_Part_Link
	{
		//子部件编号
		uint16_t part_ID = 0xFFFF;
		//骨骼下标
		uint32_t bone = 0;
		//相对骨骼的平移
		float translation[3] = { 0.0f, 0.0f, 0.0f };
		//相对骨骼的旋转（x、y、z、w）
		float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		//相对骨骼的缩放
		float scale[3] = { 1.0f, 1.0f, 1.0f };
	};

	/*
	单帧部件运动量（中立数据）
	相对上一帧的平移与旋转增量，坐标轴为碰撞体局部空间（演员根局部空间）；
	调用方将其转投碰撞系统（Collision/ColliderMotion 的 parts 项）。
	*/
	struct Skeleton_Part_Motion
	{
		//子部件编号
		uint16_t part = 0;
		//平移增量
		float translation[3] = { 0.0f, 0.0f, 0.0f };
		//旋转增量（x、y、z、w）
		float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	/*
	单帧根部整体运动量
	根骨骼相对上一帧的增量，坐标轴为演员根局部空间；
	调用方将其转投碰撞系统（Collision/ColliderMotion 的 motion 项）。
	*/
	struct Skeleton_Root_Motion
	{
		//是否存在有效增量
		bool active = false;
		//平移增量
		float translation[3] = { 0.0f, 0.0f, 0.0f };
		//旋转增量（x、y、z、w）
		float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	/*
	骨骼碰撞链接（ECS 组件）
	每演员一份：记录目标碰撞体编号、逐部件绑定、上一帧部件世界矩阵与本帧部件运动量。
	碰撞体编号为零表示只做姿态与蒙皮、不产生部件运动。
	*/
	struct Skeleton_Link
	{
		//归属实体槽位（由对象池写入）
		uint32_t owner = 0;
		//目标碰撞体编号（0 表示未绑定）
		uint64_t collider_ID = 0;
		//根骨骼下标（资产中第一个无父骨骼的骨骼）
		uint32_t root_bone = 0;
		//逐部件绑定
		std::vector<Skeleton_Part_Link> links;
		//上一帧部件局部矩阵（每链接 16 项，列主序；相对根骨骼）
		std::vector<float> previous_matrix;
		//上一帧根骨骼世界矩阵（列主序）
		float previous_root[16] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f };
		//本帧根部整体运动量
		Skeleton_Root_Motion root_motion;
		//本帧部件运动量（相对上一帧的增量）
		std::vector<Skeleton_Part_Motion> motions;

		//按资产绑定表构建（无任何绑定或姿态未求值时返回假）
		bool build(const Skeleton_Asset& asset, uint64_t collider_ID_value,
			const Skeleton_Pose& pose);
		//链接数量读取
		uint32_t link_count(void) const;
	};
}
