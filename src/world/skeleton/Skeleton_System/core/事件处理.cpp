#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取事件协议词汇
#include "Engine/EngineFoundation/src/event/Common/事件协议.h"
//获取骨骼事件词汇
#include "src/world/skeleton/Common/骨骼事件.h"

//游戏引擎命名空间
namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//整数字段读取
		bool number_read(const json& config, const char* field, uint64_t& receiver)
		{
			//字段存在性与类型检查
			if (!config.contains(field) || !config[field].is_number_integer())
				return false;
			//负值非法
			int64_t value = config[field].get<int64_t>();
			if (value < 0)
				return false;
			//写入结果
			receiver = static_cast<uint64_t>(value);
			return true;
		}

		//字符串字段读取
		bool text_read(const json& config, const char* field, string& receiver)
		{
			//字段存在性与类型检查
			if (!config.contains(field) || !config[field].is_string())
				return false;
			//空字符串非法
			if (config[field].get_ref<const string&>().empty())
				return false;
			//写入结果
			receiver = config[field].get<string>();
			return true;
		}

		//布尔字段读取
		bool bool_read(const json& config, const char* field, bool& receiver)
		{
			//字段存在性与类型检查
			if (!config.contains(field) || !config[field].is_boolean())
				return false;
			//写入结果
			receiver = config[field].get<bool>();
			return true;
		}

		//浮点数组读取（长度精确匹配、逐项有限）
		bool number_array_read(const json& config, const char* field, float* receiver, uint64_t count)
		{
			//字段存在性检查
			if (!config.contains(field))
				return false;
			//数组格式检查
			if (!config[field].is_array() || config[field].size() != count)
				return false;
			//逐项读取
			for (uint64_t index = 0; index < count; index++)
			{
				//元素必须为数值
				if (!config[field][index].is_number())
					return false;
				//写入并检查有限性
				float value = config[field][index].get<float>();
				if (!std::isfinite(value))
					return false;
				receiver[index] = value;
			}
			return true;
		}

		//实体句柄读取
		bool entity_read(const json& config, Entity& receiver)
		{
			//句柄打包值
			uint64_t value = 0;
			if (!number_read(config, skeleton_event::field_entity, value))
				return false;
			//写入句柄
			receiver.value = value;
			return true;
		}
	}

	//事件命令所用的 ECS 存储绑定
	void Skeleton_System::store_bind(ECS_Store& store)
	{
		this->store = &store;
	}

	//事件中转站接入
	void Skeleton_System::attach(void)
	{
		//若事件中转站接入入口尚未注册（接入入口由事件中转站的持有者注册）
		if (!event_terminal->check(Port_ID::Broker_Attacher))
		{
			logger.error("Skeleton_System::事件中转站接入入口未注册，接入中止");
			return;
		}

		//注册本模块事件接收入口
		event_terminal->bind(Port_ID::Evt_Receiver,
			[this](shared_ptr<Event> evt) { this->event_process(evt); });
		if (!event_terminal->check(Port_ID::Evt_Receiver))
		{
			logger.error("Skeleton_System::事件接收入口注册失败，接入中止");
			return;
		}

		//待订阅事件清单
		vector<Event> needed_events{
			Event("", "", skeleton_event::category, skeleton_event::asset_load),
			Event("", "", skeleton_event::category, skeleton_event::asset_add),
			Event("", "", skeleton_event::category, skeleton_event::asset_remove),
			Event("", "", skeleton_event::category, skeleton_event::attach),
			Event("", "", skeleton_event::category, skeleton_event::detach),
			Event("", "", skeleton_event::category, skeleton_event::bone_local_set),
			Event("", "", skeleton_event::category, skeleton_event::bone_rotation_set),
			Event("", "", skeleton_event::category, skeleton_event::pose_reset),
			Event("", "", skeleton_event::category, skeleton_event::pose_apply),
			Event("", "", skeleton_event::category, skeleton_event::bone_seek),
			Event("", "", skeleton_event::category, skeleton_event::bone_bind_get),
			Event("", "", skeleton_event::category, skeleton_event::event_output),
			Event("", "", skeleton_event::category, skeleton_event::frame_update)
		};

		//接入事件中转站
		if (!event_terminal.attach(module_name, acl_key))
		{
			logger.error("Skeleton_System::事件中转站接入失败");
			return;
		}

		//构造订阅事件清单
		json subscribed_events = json::array();
		for (const Event& needed_event : needed_events)
		{
			//单条订阅事件标识
			json subscribed_event;
			subscribed_event[event_protocol::field_category] = needed_event.category;
			subscribed_event[event_protocol::field_tag] = needed_event.tag;
			subscribed_events.push_back(subscribed_event);
		}

		//构造订阅事件
		auto attach_evt = event_terminal.build("", "", event_protocol::subscribed_events,
			event_protocol::add);
		//若事件构造失败
		if (attach_evt == nullptr)
		{
			logger.error("Skeleton_System::订阅事件构造失败");
			return;
		}
		//写入目标对象名、权限密钥与订阅清单
		attach_evt->config[event_protocol::field_object] = module_name;
		attach_evt->config[event_protocol::field_acl_key] = static_cast<uint64_t>(acl_key);
		attach_evt->config[event_protocol::field_subscribed_events] = subscribed_events;
		//发送订阅事件
		if (!event_terminal.send(attach_evt, acl_key))
			logger.error("Skeleton_System::订阅事件发送失败");
	}

	//事件发布
	bool Skeleton_System::event_publish(const string& tag, const json& payload)
	{
		//构造发布事件
		shared_ptr<Event> evt = event_terminal.build(module_name, "",
			skeleton_event::category, tag);
		//若事件内存分配失败
		if (!evt)
		{
			logger.error("Skeleton_System::事件内存分配失败，发布中止");
			return false;
		}
		//写入事件载荷
		evt->config = payload;

		//发送事件
		if (!event_terminal.send(evt, acl_key))
		{
			logger.warn("Skeleton_System::事件({})发送失败，事件发送通道不可用", tag);
			return false;
		}
		return true;
	}

	//操作结果发布
	void Skeleton_System::action_result_publish(const json& config, const string& action,
		bool success, const json& extra)
	{
		//结果载荷
		json payload;
		//请求编号（配置中提供时原样回传）
		uint64_t request_ID = 0;
		if (number_read(config, skeleton_event::field_request_ID, request_ID))
			payload[skeleton_event::field_request_ID] = request_ID;
		//操作标签与结果
		payload[skeleton_event::field_action] = action;
		payload[skeleton_event::field_success] = success;
		//合并附加字段
		if (extra.is_object())
			payload.update(extra);
		//发布结果
		event_publish(skeleton_event::action_result, payload);
	}

	//事件命令处理
	void Skeleton_System::event_process(shared_ptr<Event> evt)
	{
		//空事件检查
		if (!evt)
			return;
		//非本模块事件一律忽略
		if (evt->category != skeleton_event::category)
			return;

		//———— 资产文件加载 ————
		if (evt->tag == skeleton_event::asset_load)
		{
			//资产路径
			string path;
			if (!text_read(evt->config, skeleton_event::field_path, path))
			{
				logger.warn("Skeleton_System::资产加载事件缺少有效字段(path)");
				return;
			}
			//加载并登记
			uint32_t asset_ID = asset_load_file(path);
			//结果载荷
			json payload;
			uint64_t request_ID = 0;
			if (number_read(evt->config, skeleton_event::field_request_ID, request_ID))
				payload[skeleton_event::field_request_ID] = request_ID;
			payload[skeleton_event::field_path] = path;
			payload[skeleton_event::field_asset_ID] = asset_ID;
			payload[skeleton_event::field_success] = asset_ID != 0;
			//发布结果
			event_publish(skeleton_event::asset_load_result, payload);
			return;
		}

		//———— 资产内容登记 ————
		if (evt->tag == skeleton_event::asset_add)
		{
			//资产内容
			if (!evt->config.contains(skeleton_event::field_config) ||
				!evt->config[skeleton_event::field_config].is_object())
			{
				logger.warn("Skeleton_System::资产登记事件缺少有效字段(config)");
				return;
			}
			//构建并登记
			Skeleton_Asset asset;
			bool built = Skeleton_Asset::build(evt->config[skeleton_event::field_config], asset);
			uint32_t asset_ID = built ? asset_add(std::move(asset)) : 0;
			//结果载荷
			json payload;
			uint64_t request_ID = 0;
			if (number_read(evt->config, skeleton_event::field_request_ID, request_ID))
				payload[skeleton_event::field_request_ID] = request_ID;
			payload[skeleton_event::field_asset_ID] = asset_ID;
			payload[skeleton_event::field_success] = asset_ID != 0;
			//发布结果
			event_publish(skeleton_event::asset_load_result, payload);
			return;
		}

		//———— 资产卸载 ————
		if (evt->tag == skeleton_event::asset_remove)
		{
			//资产编号
			uint64_t asset_ID = 0;
			if (!number_read(evt->config, skeleton_event::field_asset_ID, asset_ID))
			{
				logger.warn("Skeleton_System::资产卸载事件缺少有效字段(asset_ID)");
				return;
			}
			//执行并回告
			action_result_publish(evt->config, evt->tag,
				asset_remove(static_cast<uint32_t>(asset_ID)));
			return;
		}

		//———— 运动事件开关 ————
		if (evt->tag == skeleton_event::event_output)
		{
			//开关值
			bool enabled = true;
			if (!bool_read(evt->config, skeleton_event::field_enabled, enabled))
			{
				logger.warn("Skeleton_System::事件开关指令缺少有效字段(enabled)");
				return;
			}
			//执行并回告
			action_result_publish(evt->config, evt->tag, event_output_set(enabled));
			return;
		}

		//———— 需要 ECS 存储的指令 ————
		if (!store)
		{
			logger.warn("Skeleton_System::事件指令({})执行失败：ECS 存储未绑定", evt->tag);
			action_result_publish(evt->config, evt->tag, false);
			return;
		}

		//演员挂载
		if (evt->tag == skeleton_event::attach)
		{
			//实体句柄、资产编号与碰撞体编号
			Entity entity;
			uint64_t asset_ID = 0;
			uint64_t collider_ID = 0;
			if (!entity_read(evt->config, entity) ||
				!number_read(evt->config, skeleton_event::field_asset_ID, asset_ID))
			{
				logger.warn("Skeleton_System::挂载事件缺少有效字段(entity/asset_ID)");
				return;
			}
			//碰撞体编号可选
			number_read(evt->config, skeleton_event::field_collider_ID, collider_ID);
			//执行并回告
			action_result_publish(evt->config, evt->tag,
				actor_attach(*store, entity, static_cast<uint32_t>(asset_ID), collider_ID));
			return;
		}

		//演员卸载
		if (evt->tag == skeleton_event::detach)
		{
			//实体句柄
			Entity entity;
			if (!entity_read(evt->config, entity))
			{
				logger.warn("Skeleton_System::卸载事件缺少有效字段(entity)");
				return;
			}
			//执行并回告
			action_result_publish(evt->config, evt->tag, actor_detach(*store, entity));
			return;
		}

		//骨骼局部变换写入
		if (evt->tag == skeleton_event::bone_local_set)
		{
			//实体、骨骼下标与变换
			Entity entity;
			uint64_t bone = 0;
			float translation[3] = {};
			float rotation[4] = {};
			float scale[3] = { 1.0f, 1.0f, 1.0f };
			if (!entity_read(evt->config, entity) ||
				!number_read(evt->config, skeleton_event::field_bone, bone) ||
				!number_array_read(evt->config, skeleton_event::field_translation, translation, 3) ||
				!number_array_read(evt->config, skeleton_event::field_rotation, rotation, 4))
			{
				logger.warn("Skeleton_System::局部变换指令缺少有效字段");
				return;
			}
			//缩放可选
			if (evt->config.contains(skeleton_event::field_scale) &&
				!number_array_read(evt->config, skeleton_event::field_scale, scale, 3))
			{
				logger.warn("Skeleton_System::局部变换指令字段(scale)非法");
				return;
			}
			//执行并回告
			action_result_publish(evt->config, evt->tag,
				bone_local_set(*store, entity, static_cast<uint32_t>(bone),
					translation, rotation, scale));
			return;
		}

		//骨骼局部旋转写入
		if (evt->tag == skeleton_event::bone_rotation_set)
		{
			//实体、骨骼下标与旋转
			Entity entity;
			uint64_t bone = 0;
			float rotation[4] = {};
			if (!entity_read(evt->config, entity) ||
				!number_read(evt->config, skeleton_event::field_bone, bone) ||
				!number_array_read(evt->config, skeleton_event::field_rotation, rotation, 4))
			{
				logger.warn("Skeleton_System::旋转指令缺少有效字段");
				return;
			}
			//执行并回告
			action_result_publish(evt->config, evt->tag,
				bone_rotation_set(*store, entity, static_cast<uint32_t>(bone), rotation));
			return;
		}

		//姿态复位
		if (evt->tag == skeleton_event::pose_reset)
		{
			//实体句柄
			Entity entity;
			if (!entity_read(evt->config, entity))
			{
				logger.warn("Skeleton_System::复位事件缺少有效字段(entity)");
				return;
			}
			//执行并回告
			action_result_publish(evt->config, evt->tag, pose_reset(*store, entity));
			return;
		}

		//批量姿态写入
		if (evt->tag == skeleton_event::pose_apply)
		{
			//实体句柄
			Entity entity;
			if (!entity_read(evt->config, entity))
			{
				logger.warn("Skeleton_System::批量姿态指令缺少有效字段(entity)");
				return;
			}
			//骨骼条目数组
			if (!evt->config.contains(skeleton_event::field_bones) ||
				!evt->config[skeleton_event::field_bones].is_array() ||
				evt->config[skeleton_event::field_bones].empty())
			{
				logger.warn("Skeleton_System::批量姿态指令缺少有效字段(bones)");
				return;
			}
			//待写入条目
			vector<Skeleton_Local_Transform> entries;
			for (const json& item : evt->config[skeleton_event::field_bones])
			{
				//条目必须为对象
				if (!item.is_object())
				{
					logger.warn("Skeleton_System::批量姿态条目不是对象");
					return;
				}
				//当前条目
				Skeleton_Local_Transform entry;
				//骨骼下标
				uint64_t bone = 0;
				if (!number_read(item, skeleton_event::field_bone, bone))
				{
					logger.warn("Skeleton_System::批量姿态条目缺少有效字段(bone)");
					return;
				}
				entry.bone = static_cast<uint32_t>(bone);
				//平移、旋转、缩放（均可选）
				if (item.contains(skeleton_event::field_translation) &&
					!number_array_read(item, skeleton_event::field_translation, entry.translation, 3))
				{
					logger.warn("Skeleton_System::批量姿态条目字段(translation)非法");
					return;
				}
				if (item.contains(skeleton_event::field_rotation) &&
					!number_array_read(item, skeleton_event::field_rotation, entry.rotation, 4))
				{
					logger.warn("Skeleton_System::批量姿态条目字段(rotation)非法");
					return;
				}
				if (item.contains(skeleton_event::field_scale) &&
					!number_array_read(item, skeleton_event::field_scale, entry.scale, 3))
				{
					logger.warn("Skeleton_System::批量姿态条目字段(scale)非法");
					return;
				}
				//收容条目
				entries.push_back(entry);
			}
			//执行并回告（附加成功条数）
			uint32_t applied = pose_apply(*store, entity, entries.data(),
				static_cast<uint32_t>(entries.size()));
			json extra;
			extra[skeleton_event::field_applied] = applied;
			action_result_publish(evt->config, evt->tag,
				applied == entries.size(), extra);
			return;
		}

		//骨骼名解析
		if (evt->tag == skeleton_event::bone_seek)
		{
			//实体与骨骼名
			Entity entity;
			string name;
			if (!entity_read(evt->config, entity) ||
				!text_read(evt->config, skeleton_event::field_name, name))
			{
				logger.warn("Skeleton_System::骨骼名解析指令缺少有效字段(entity/name)");
				return;
			}
			//解析下标
			int32_t bone = bone_seek(*store, entity, name);
			//结果载荷
			json payload;
			uint64_t request_ID = 0;
			if (number_read(evt->config, skeleton_event::field_request_ID, request_ID))
				payload[skeleton_event::field_request_ID] = request_ID;
			payload[skeleton_event::field_name] = name;
			payload[skeleton_event::field_bone] = bone;
			payload[skeleton_event::field_success] = bone >= 0;
			//发布结果
			event_publish(skeleton_event::bone_seek_result, payload);
			return;
		}

		//绑定姿态读取
		if (evt->tag == skeleton_event::bone_bind_get)
		{
			//实体与骨骼下标
			Entity entity;
			uint64_t bone = 0;
			if (!entity_read(evt->config, entity) ||
				!number_read(evt->config, skeleton_event::field_bone, bone))
			{
				logger.warn("Skeleton_System::绑定姿态指令缺少有效字段(entity/bone)");
				return;
			}
			//读取绑定姿态
			float translation[3] = {};
			float rotation[4] = {};
			float scale[3] = {};
			bool success = bone_bind_get(*store, entity, static_cast<uint32_t>(bone),
				translation, rotation, scale);
			//结果载荷
			json payload;
			uint64_t request_ID = 0;
			if (number_read(evt->config, skeleton_event::field_request_ID, request_ID))
				payload[skeleton_event::field_request_ID] = request_ID;
			payload[skeleton_event::field_translation] = json::array({
				translation[0], translation[1], translation[2] });
			payload[skeleton_event::field_rotation] = json::array({
				rotation[0], rotation[1], rotation[2], rotation[3] });
			payload[skeleton_event::field_scale] = json::array({
				scale[0], scale[1], scale[2] });
			payload[skeleton_event::field_success] = success;
			//发布结果
			event_publish(skeleton_event::bone_bind_result, payload);
			return;
		}

		//帧求值
		if (evt->tag == skeleton_event::frame_update)
		{
			//执行求值并回告
			frame_update(*store);
			action_result_publish(evt->config, evt->tag, true);
			return;
		}

		//未识别的指令
		logger.debug("Skeleton_System::未识别事件({}/{})", evt->category, evt->tag);
	}
}
