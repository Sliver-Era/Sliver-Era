#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取碰撞配置字段读取工具
#include "src/collision/Common/字段读取.h"

namespace engine
{
	//引入字段读取工具
	using namespace collision_detail;

	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//运动量读取（平移与旋转均可选，缺省为零平移 + 单位旋转）
		bool motion_read(const json& config, Motion& receiver)
		{
			//运动量对象格式检查
			if (!config.is_object())
				return false;

			//运动量
			Motion motion;
			//平移字段存在时读取
			if (config.contains(collision_event::field_translation))
			{
				//读取平移
				if (!vector_read(config, collision_event::field_translation, motion.translation))
					return false;
			}
			//旋转字段存在时读取
			if (config.contains(collision_event::field_rotation))
			{
				//四元数
				Quaternion rotation;
				//读取四元数
				if (!quaternion_read(config, collision_event::field_rotation, rotation))
					return false;
				//零四元数非法
				if (rotation.length2() <= 0.0f)
					return false;
				//写入单位化旋转
				motion.rotation = rotation.normalized();
			}

			receiver = motion;
			return true;
		}

		//部件运动集合读取（同一下标重复出现时后者覆盖前者）
		bool parts_read(const json& config, std::vector<Part_Motion>& receiver)
		{
			//部件集合
			std::vector<Part_Motion> parts;
			//逐项读取
			for (const json& item : config)
			{
				//子部件编号
				uint64_t part = 0;
				//读取子部件编号
				if (!number_read(item, collision_event::field_part, part))
					return false;
				//编号超出 16 位范围或为保留哨兵值
				if (part >= 0xFFFF)
					return false;

				//部件运动
				Part_Motion part_motion;
				part_motion.part = static_cast<uint16_t>(part);
				//读取该部件运动量（平移与旋转直接位于部件对象内）
				if (!motion_read(item, part_motion.motion))
					return false;

				//同一下标覆盖旧值
				bool replaced = false;
				for (Part_Motion& existing : parts)
				{
					//下标一致即覆盖
					if (existing.part == part_motion.part)
					{
						existing = part_motion;
						replaced = true;
						break;
					}
				}
				//首次出现时追加
				if (!replaced)
					parts.push_back(part_motion);
			}

			receiver = std::move(parts);
			return true;
		}
	}

	//运动事件登记（保存运动事件配置，同编号的新事件覆盖旧事件）
	void Collision_Proxy::motion_register(const json& config)
	{
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//读取目标碰撞体编号
		if (!number_read(config, collision_event::field_collider_ID, collider_ID))
		{
			logger.warn("Collision_Proxy::运动事件缺少有效字段(collider_ID)");
			return;
		}

		//整体运动
		Motion motion;
		//是否携带整体运动
		bool has_motion = config.contains(collision_event::field_motion);
		//读取整体运动
		if (has_motion && !motion_read(config[collision_event::field_motion], motion))
		{
			logger.warn("Collision_Proxy::碰撞体({})字段(motion)非法", collider_ID);
			return;
		}

		//部件运动集合
		std::vector<Part_Motion> parts;
		//是否携带部件运动
		bool has_parts = config.contains(collision_event::field_parts);
		//读取部件运动集合
		if (has_parts)
		{
			//部件集合必须为数组
			if (!config[collision_event::field_parts].is_array() ||
				!parts_read(config[collision_event::field_parts], parts))
			{
				logger.warn("Collision_Proxy::碰撞体({})字段(parts)非法", collider_ID);
				return;
			}
		}

		//整体运动与部件运动至少提供一个
		if (!has_motion && !has_parts)
		{
			logger.warn("Collision_Proxy::碰撞体({})运动事件未提供任何运动通道", collider_ID);
			return;
		}

		//离散步长
		uint32_t step_length = 0;
		//读取离散步长（编号不存在时拒绝）
		bool found = backend.collider_set_dispatch(collider_ID,
			[collider_ID, &step_length](Collision_Region& region)
			{
				return region.collider_step_length_get(collider_ID, step_length);
			});
		if (!found)
		{
			logger.warn("Collision_Proxy::运动事件目标碰撞体({})不存在", collider_ID);
			return;
		}

		//旋转或部件运动要求显式步长
		bool needs_steps = (has_motion && !motion.rotation.identity_check()) || has_parts;
		if (needs_steps && step_length == 0)
		{
			logger.warn("Collision_Proxy::碰撞体({})含旋转或部件运动但离散步长为零，事件拒绝",
				collider_ID);
			return;
		}

		//子部件编号存在性检查
		for (const Part_Motion& part_motion : parts)
		{
			//子部件是否存在
			bool exists = false;
			//查询子部件编号
			backend.collider_set_dispatch(collider_ID,
				[collider_ID, &part_motion, &exists](Collision_Region& region)
				{
					exists = region.collider_part_exists(collider_ID, part_motion.part);
					return true;
				});
			//编号不存在则整条拒绝
			if (!exists)
			{
				logger.warn("Collision_Proxy::碰撞体({})子部件编号({})不存在",
					collider_ID, part_motion.part);
				return;
			}
		}

		/*
		运动作用频率
		缺省或非法（为零、逻辑帧率不可用、不整除逻辑帧率）时取零，
		此时碰撞空间按旧行为每次检测都重读并施加全量运动。
		*/
		uint64_t frequency = 0;
		//读取作用频率（可选字段，缺省时不作提示）
		if (config.contains(collision_event::field_frequency))
			number_read(config, collision_event::field_frequency, frequency);
		//逻辑帧率
		uint64_t frame_rate = Logics::logic_frames_get();
		//作用频率合法性检查（作用频率必须为逻辑帧率的因数以保证整除）
		if (frequency != 0 && (frame_rate == 0 || frame_rate % frequency != 0))
		{
			logger.warn("Collision_Proxy::碰撞体({})字段(frequency={})非法，须整除逻辑帧率({})，改按每帧全量运动处理",
				collider_ID, frequency, frame_rate);
			frequency = 0;
		}

		//运动登记（仅随新载荷事件更新）
		Motion_Record record;
		//写入整体运动
		record.event.body = motion;
		//写入部件运动
		record.event.parts = std::move(parts);
		//写入作用频率
		record.frequency = frequency;
		//保存运动登记（同编号的新事件覆盖旧事件）
		motion_records[collider_ID] = std::move(record);

		//已保存的运动登记（下发期间保持有效）
		const Motion_Record& saved = motion_records[collider_ID];
		//下发到持有该编号的碰撞空间
		backend.collider_set_dispatch(collider_ID, [collider_ID, &saved](
			Collision_Region& region)
			{
				//先设置作用频率（重新起算生效计时并清零本帧施加量）
				bool frequency_set = region.collider_frequency_set(collider_ID, saved.frequency);
				//再写入运动量
				bool motion_set = region.collider_motion_set(collider_ID, saved.event);
				return frequency_set && motion_set;
			});
	}

	//运动量下发（把已登记的运动量写入碰撞空间）
	void Collision_Proxy::motion_apply(const uint64_t collider_ID) const
	{
		//查找运动登记
		auto it = motion_records.find(collider_ID);
		//未登记时无需下发
		if (it == motion_records.end())
			return;

		//已登记的运动事件
		const Motion_Event& event = it->second.event;
		//下发到持有该编号的碰撞空间
		backend.collider_set_dispatch(collider_ID, [collider_ID, &event](Collision_Region& region)
			{
				return region.collider_motion_set(collider_ID, event);
			});
	}

	//运动频率下发（把已登记的作用频率写入碰撞空间）
	void Collision_Proxy::motion_frequency_apply(const uint64_t collider_ID) const
	{
		//查找运动登记
		auto it = motion_records.find(collider_ID);
		//未登记时无需下发
		if (it == motion_records.end())
			return;

		//已登记的作用频率
		const uint64_t frequency = it->second.frequency;
		//下发到持有该编号的碰撞空间
		backend.collider_set_dispatch(collider_ID, [collider_ID, frequency](Collision_Region& region)
			{
				return region.collider_frequency_set(collider_ID, frequency);
			});
	}

	//运动登记清除（丢弃登记并清空碰撞空间内的运动）
	void Collision_Proxy::motion_clear(const uint64_t collider_ID)
	{
		//丢弃运动登记
		motion_records.erase(collider_ID);
		//下发空运动并清零作用频率
		backend.collider_set_dispatch(collider_ID, [collider_ID](Collision_Region& region)
			{
				//清零作用频率（同时清零本帧施加量）
				bool frequency_clear = region.collider_frequency_set(collider_ID, 0);
				//写入空运动
				bool motion_reset = region.collider_motion_set(collider_ID, Motion_Event());
				return frequency_clear && motion_reset;
			});
	}
}
