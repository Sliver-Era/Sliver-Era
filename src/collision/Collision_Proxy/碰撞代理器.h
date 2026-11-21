#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取路径操作工具
#include "Engine/EngineFoundation/src/detail/methods/package/路径操作工具.h"
//获取逻辑帧操作工具
#include "Engine/EngineSystem/common/runtime/逻辑帧.h"
//获取自有几何类型
#include "../Common/几何类型.h"
//获取运动量类型
#include "../Common/运动类型.h"
//获取碰撞空间系统运行包
#include "../碰撞系统运行包.h"
//获取碰撞代理后端
#include "backend/代理后端.h"
//获取事件系统运行包
#include "Engine/EngineFoundation/src/event/事件系统运行包.h"

namespace engine
{
	//碰撞代理器
	class Collision_Proxy
	{
	private:		
		//模块名称（事件中转站登记名与事件发送者标识）
		inline static const std::string module_name = "Collision_Proxy";

		//运动登记（解析后的运动事件与作用频率，仅随新载荷事件更新）
		struct Motion_Record
		{
			//运动事件（整体运动 ＋ 部件运动）
			Motion_Event event;
			//运动作用频率
			uint64_t frequency = 0;
		};

		//空间与编号编排后端
		Proxy_Backend backend;
		//运动登记保存（键为碰撞体编号，同编号的新事件覆盖旧事件）
		std::unordered_map<uint64_t, Motion_Record> motion_records;
		//简略碰撞结果事件开关（键为空间名称；未登记时默认开启）
		std::unordered_map<std::string, bool> event_output;
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//事件发送权限密钥
		int64_t acl_key = 0;
	public:		
		//构造函数
		Collision_Proxy();
		//默认析构
		~Collision_Proxy() = default;

		//碰撞空间构建
		bool region_build(const std::string& region);
		//碰撞空间卸载
		bool region_unload(const std::string& region);
		/*
		碰撞空间检测执行
		同步返回碰撞详情（碰撞对 + 接触点/法线/穿透深度/扫掠比例）；
		是否同时经事件协议发送简略碰撞结果由空间的事件开关决定。
		*/
		std::optional<std::vector<Collision_Detail>> region_detect(const std::string& region);
		//简略碰撞结果事件开关设置（false 时检测不再发布 DetectResult）
		bool region_event_output_set(const std::string& region, bool enabled);
		//简略碰撞结果事件开关读取
		bool region_event_output_get(const std::string& region) const;
		//碰撞空间边界设置
		bool region_boundary_set(const std::string& path);
		//碰撞空间活跃性设置
		bool region_state_set(const std::string& region,bool active);

		//碰撞体构建
		std::optional<uint64_t> collider_build(const std::string& region);
		//碰撞体卸载
		bool collider_unload(const uint64_t collider_ID,const std::string& region);
		//碰撞体转移 
		bool collider_transfer(const uint64_t collider_ID, const std::string& region);
		//碰撞体镜像
		bool collider_mirror(const uint64_t collider_ID, const std::string& region);
		//碰撞体设置 —— 检测方式重载
		bool collider_set(const uint64_t collider_ID, const Detection_Mode& vector);
		//碰撞体设置 —— 豁免标记重载
		bool collider_set(const uint64_t collider_ID, const uint64_t& vector);
		//事件中转站接入
		void attach(void);

	private:
		//事件处理
		void event_process(std::shared_ptr<Event> evt);

		//碰撞体配置应用（几何体、检测方式、豁免标记）
		void collider_config_apply(const std::string& region, uint64_t collider_ID,
			const nlohmann::json& config);

		//运动事件登记（解析后保存，并下发到持有该编号的碰撞空间）
		void motion_register(const nlohmann::json& config);
		//运动量下发（把已登记的运动量写入碰撞空间）
		void motion_apply(const uint64_t collider_ID) const;
		//运动频率下发（把已登记的作用频率写入碰撞空间）
		void motion_frequency_apply(const uint64_t collider_ID) const;
		//运动登记清除（丢弃登记并清空碰撞空间内的运动）
		void motion_clear(const uint64_t collider_ID);

		//事件发布
		bool event_publish(const std::string& tag, const nlohmann::json& payload);
		//配置内容应用（Config/Load 路由指令）
		void config_apply(const nlohmann::json& config);
		//边界配置文件读取
		bool boundary_config_read(const std::string& path, nlohmann::json& receiver) const;
	};
}
