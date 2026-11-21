#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
	/*
	骨骼模块事件词汇
	外部经事件中转站驱动骨骼资产、演员与姿态操作；本模块以结果事件回告。
	载荷字段集中于此，避免发送侧与接收侧各写一份字符串导致漂移。
	*/
	namespace skeleton_event
	{
		//事件类别
		inline constexpr const char* category = "Skeleton";

		// ———— 外部 → 本模块 ————

		//资产文件加载
		inline constexpr const char* asset_load = "AssetLoad";
		//资产内容登记
		inline constexpr const char* asset_add = "AssetAdd";
		//资产卸载
		inline constexpr const char* asset_remove = "AssetRemove";
		//演员挂载
		inline constexpr const char* attach = "Attach";
		//演员卸载
		inline constexpr const char* detach = "Detach";
		//骨骼局部变换写入
		inline constexpr const char* bone_local_set = "BoneLocalSet";
		//骨骼局部旋转写入
		inline constexpr const char* bone_rotation_set = "BoneRotationSet";
		//姿态复位
		inline constexpr const char* pose_reset = "PoseReset";
		//批量姿态写入
		inline constexpr const char* pose_apply = "PoseApply";
		//骨骼名解析
		inline constexpr const char* bone_seek = "BoneSeek";
		//绑定姿态读取
		inline constexpr const char* bone_bind_get = "BoneBindGet";
		//运动事件开关
		inline constexpr const char* event_output = "EventOutput";
		//帧求值
		inline constexpr const char* frame_update = "FrameUpdate";

		// ———— 本模块 → 外部 ————

		//资产加载结果
		inline constexpr const char* asset_load_result = "AssetLoadResult";
		//操作结果
		inline constexpr const char* action_result = "ActionResult";
		//骨骼名解析结果
		inline constexpr const char* bone_seek_result = "BoneSeekResult";
		//绑定姿态读取结果
		inline constexpr const char* bone_bind_result = "BoneBindResult";

		// ———— 载荷字段名 ————

		//资产文件路径
		inline constexpr const char* field_path = "path";
		//资产内容（JSON）
		inline constexpr const char* field_config = "config";
		//资产编号
		inline constexpr const char* field_asset_ID = "asset_ID";
		//实体句柄打包值
		inline constexpr const char* field_entity = "entity";
		//碰撞体编号
		inline constexpr const char* field_collider_ID = "collider_ID";
		//骨骼下标
		inline constexpr const char* field_bone = "bone";
		//平移
		inline constexpr const char* field_translation = "translation";
		//旋转
		inline constexpr const char* field_rotation = "rotation";
		//缩放
		inline constexpr const char* field_scale = "scale";
		//事件开关
		inline constexpr const char* field_enabled = "enabled";
		//请求编号
		inline constexpr const char* field_request_ID = "request_ID";
		//操作标签
		inline constexpr const char* field_action = "action";
		//操作结果
		inline constexpr const char* field_success = "success";
		//批量骨骼条目
		inline constexpr const char* field_bones = "bones";
		//骨骼名
		inline constexpr const char* field_name = "name";
		//成功应用条数
		inline constexpr const char* field_applied = "applied";
	}
}
