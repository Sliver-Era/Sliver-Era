#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取实体与 ECS 存储
#include "Engine/EngineSystem/src/world/ecs/ECS系统运行包.h"
//获取骨骼资产
#include "Engine/EngineSystem/src/world/skeleton/Skeleton_Asset/骨骼资产.h"
//获取骨骼姿态
#include "Engine/EngineSystem/src/world/skeleton/Skeleton_Pose/骨骼姿态.h"
//获取骨骼链接
#include "Engine/EngineSystem/src/world/skeleton/Skeleton_Link/骨骼链接.h"
//获取事件系统运行包
#include "Engine/EngineFoundation/src/event/事件系统运行包.h"

//游戏引擎命名空间
namespace engine
{
	/*
	骨骼与蒙皮系统
	负责骨骼资产登记、演员挂载、逐帧求值（姿态 → 蒙皮 → 部件运动）与只读查询。

	坐标空间：全部在演员根局部空间；演员根变换由调用方通过碰撞整体运动或
	渲染实例模型矩阵承担，本系统不感知世界变换。

	求值顺序：先姿态与蒙皮，再部件运动。调用方在同一逻辑帧内必须先调用
	frame_update，再执行碰撞检测；顺序颠倒会使用上一帧骨骼位置。

	线程口径：与 ECS 存储一致，不做内部加锁，调用方保证单线程访问。
	*/
	class Skeleton_System
	{
	private:
		//模块名称（事件发送者标识）
		inline static const std::string module_name = "Skeleton_System";

		//资产槽位（下标 = 资产编号 - 1；空槽表示已卸载，编号不回收）
		std::vector<std::shared_ptr<const Skeleton_Asset>> assets;
		//部件运动事件开关（接入事件中转站后生效）
		bool event_output = true;

		//姿态求值（单演员；写世界矩阵）
		bool pose_evaluate(Skeleton_Pose& pose) const;
		//蒙皮求值（单演员；写调色板）
		bool skin_evaluate(Skeleton_Pose& pose) const;
		//部件运动求值（单演员；写本帧运动量）
		void link_evaluate(const Skeleton_Pose& pose, Skeleton_Link& link) const;
		//部件运动事件发布（Collision/ColliderMotion）
		void motion_publish(const Skeleton_Link& link);

	public:
		//事件终端（发布 Collision/ColliderMotion）
		Event_Terminal event_terminal;
	private:
		//事件发送权限密钥
		int64_t acl_key = 0;

	public:
		//构造函数（生成事件权限密钥）
		Skeleton_System(void);
		//默认析构
		~Skeleton_System(void) = default;
		//禁止拷贝
		Skeleton_System(const Skeleton_System&) = delete;
		Skeleton_System& operator=(const Skeleton_System&) = delete;

		//———— 资产 ————

		//资产登记（返回资产编号，1 起；0 表示失败）
		uint32_t asset_add(Skeleton_Asset asset);
		//资产文件加载并登记（返回资产编号，0 表示失败）
		uint32_t asset_load_file(const std::string& path);
		//资产卸载（槽位置空，编号不再复用；已挂载演员持有的共享指针继续有效）
		bool asset_remove(uint32_t asset_ID);
		//资产读取（编号无效或已卸载返回空指针）
		const Skeleton_Asset* asset_get(uint32_t asset_ID) const;
		//有效资产数量读取
		uint32_t asset_count(void) const;

		//———— 演员 ————

		/*
		演员挂载
		为实体创建姿态组件；collider_ID 非零且资产含部件绑定时同时创建链接组件。
		绑定姿态在挂载时即完成求值，作为部件运动的首帧基准。
		*/
		bool actor_attach(ECS_Store& store, Entity entity, uint32_t asset_ID,
			uint64_t collider_ID = 0);
		//演员卸载（移除姿态与链接组件；实体本身不销毁）
		bool actor_detach(ECS_Store& store, Entity entity);

		//———— 事件 ————

		/*
		事件中转站接入
		接入后 frame_update 会把本帧根部/部件运动量以 Collision/ColliderMotion 事件发布；
		事件载荷与碰撞模块的运动登记格式一致，部件运动要求碰撞体已配置非零离散步长。
		*/
		void attach(void);
		//部件运动事件开关设置
		bool event_output_set(bool enabled);
		//部件运动事件开关读取
		bool event_output_get(void) const;

		//———— 姿态写入 ————

		//骨骼局部变换写入
		bool bone_local_set(ECS_Store& store, Entity entity, uint32_t bone,
			const float translation[3], const float rotation[4], const float scale[3]);
		//骨骼局部旋转写入
		bool bone_rotation_set(ECS_Store& store, Entity entity, uint32_t bone,
			const float rotation[4]);
		//姿态复位到绑定姿态
		bool pose_reset(ECS_Store& store, Entity entity);

		//———— 帧求值 ————

		//帧求值（姿态 → 蒙皮 → 部件运动；仅在脏标记置位时重算姿态）
		void frame_update(ECS_Store& store);

		//———— 查询 ————

		//骨骼世界矩阵查询（演员根局部空间）
		bool pose_matrix_get(const ECS_Store& store, Entity entity, uint32_t bone,
			float out[16]) const;
		//蒙皮调色板查询（每骨骼 16 项，列主序；指针在下一次帧求值前有效）
		bool skin_palette_get(const ECS_Store& store, Entity entity,
			const float*& palette, uint32_t& count) const;
		/*
		本帧部件运动查询
		指针在下一次帧求值前有效；未挂载链接组件返回假，运动量为空时指针为空。
		*/
		bool part_motions_get(const ECS_Store& store, Entity entity,
			const Skeleton_Part_Motion*& motions, uint32_t& count) const;
	};
}
