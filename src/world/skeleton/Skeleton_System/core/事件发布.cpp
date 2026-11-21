#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取碰撞事件词汇（运动事件载荷字段）
#include "Engine/EngineSystem/src/collision/Common/碰撞事件.h"

//游戏引擎命名空间
namespace engine
{
	//构造函数
	Skeleton_System::Skeleton_System(void)
	{
		//生成事件权限密钥
		acl_key = event_terminal.key().value();
	}

	//部件运动事件开关设置
	bool Skeleton_System::event_output_set(bool enabled)
	{
		//记录开关
		event_output = enabled;
		return true;
	}

	//部件运动事件开关读取
	bool Skeleton_System::event_output_get(void) const
	{
		return event_output;
	}

	//部件运动事件发布
	void Skeleton_System::motion_publish(const Skeleton_Link& link)
	{
		//开关关闭或未绑定碰撞体时不发布
		if (!event_output || link.collider_ID == 0)
			return;
		//根部与部件均无增量时不发布
		if (!link.root_motion.active && link.motions.empty())
			return;

		//事件载荷
		json payload;
		//目标碰撞体编号
		payload[collision_event::field_collider_ID] = link.collider_ID;

		//根部整体运动
		if (link.root_motion.active)
		{
			//整体运动对象
			json motion;
			//平移与旋转
			motion[collision_event::field_translation] = json::array({
				link.root_motion.translation[0],
				link.root_motion.translation[1],
				link.root_motion.translation[2] });
			motion[collision_event::field_rotation] = json::array({
				link.root_motion.rotation[0],
				link.root_motion.rotation[1],
				link.root_motion.rotation[2],
				link.root_motion.rotation[3] });
			//写入整体运动
			payload[collision_event::field_motion] = motion;
		}

		//部件运动集合
		if (!link.motions.empty())
		{
			//部件数组
			json parts = json::array();
			//逐部件写入
			for (const Skeleton_Part_Motion& motion : link.motions)
			{
				//单条部件运动
				json item;
				item[collision_event::field_part] = motion.part;
				item[collision_event::field_translation] = json::array({
					motion.translation[0],
					motion.translation[1],
					motion.translation[2] });
				item[collision_event::field_rotation] = json::array({
					motion.rotation[0],
					motion.rotation[1],
					motion.rotation[2],
					motion.rotation[3] });
				parts.push_back(item);
			}
			//写入部件运动
			payload[collision_event::field_parts] = parts;
		}

		//构造发布事件
		shared_ptr<Event> evt = event_terminal.build(module_name, "",
			collision_event::category, collision_event::collider_motion);
		//若事件内存分配失败
		if (!evt)
		{
			logger.error("Skeleton_System::事件内存分配失败，发布中止");
			return;
		}
		//写入事件载荷
		evt->config = payload;

		//发送事件
		if (!event_terminal.send(evt, acl_key))
			logger.warn("Skeleton_System::运动事件发送失败，事件发送通道不可用");
	}
}
