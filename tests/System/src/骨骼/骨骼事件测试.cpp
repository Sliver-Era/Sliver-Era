/*
骨骼事件测试
覆盖：Collision/ColliderMotion 事件载荷（根部整体运动、部件运动、开关）、
与真实 Collision_Proxy 的端到端联动（整身平移、部件旋转触发碰撞）。
*/
#include "骨骼测试辅助.h"
//获取碰撞代理器
#include "Engine/EngineSystem/src/collision/Collision_Proxy/碰撞代理器.h"

//文件内部辅助设施
namespace
{
	//构建碰撞事件
	std::shared_ptr<engine::Event> skeleton_event_make(const std::string& tag,
		const nlohmann::json& config)
	{
		return std::make_shared<engine::Event>("", "", "Collision", tag, config);
	}

	//空间事件载荷
	nlohmann::json skeleton_region_payload(const std::string& region)
	{
		//载荷
		nlohmann::json payload = nlohmann::json::object();
		payload["region"] = region;
		return payload;
	}

	//单盒体几何（带子部件编号）
	nlohmann::json skeleton_box_part(const uint16_t part_ID, const double x,
		const double y, const double half)
	{
		//几何项
		nlohmann::json item = nlohmann::json::object();
		item["type"] = "box";
		item["half_extent"] = nlohmann::json::array({ half, half, half });
		item["position"] = nlohmann::json::array({ x, y, 0.0 });
		item["part_ID"] = part_ID;
		return item;
	}

	//碰撞结果中是否包含指定碰撞对
	bool skeleton_pair_found(const std::vector<engine::Collision_Detail>& details,
		const uint64_t first, const uint64_t second)
	{
		//逐条碰撞对检查
		for (const engine::Collision_Detail& detail : details)
		{
			if ((detail.collider_A == first && detail.collider_B == second) ||
				(detail.collider_A == second && detail.collider_B == first))
				return true;
		}
		return false;
	}
}

//事件载荷：根部整体运动
TEST_F(Skeleton_Test, 根部整体运动发布为运动事件)
{
	//捕获发送事件
	std::vector<std::shared_ptr<engine::Event>> sent;
	skeleton.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	//占位接入入口（本用例只验证发布）
	skeleton.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[](auto&&, auto&&, auto&&) {});
	skeleton.attach();

	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 500));

	//无运动时不发布
	sent.clear();
	skeleton.frame_update(store);
	EXPECT_TRUE(sent.empty());

	//根骨骼平移
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	//事件载荷
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0]->category, "Collision");
	EXPECT_EQ(sent[0]->tag, "ColliderMotion");
	EXPECT_EQ(sent[0]->config["collider_ID"].get<uint64_t>(), 500u);
	ASSERT_TRUE(sent[0]->config.contains("motion"));
	EXPECT_TRUE(skeleton_float_near(
		sent[0]->config["motion"]["translation"][0].get<float>(), 1.0f));
	EXPECT_FALSE(sent[0]->config.contains("parts"));
}

//事件载荷：部件运动与开关
TEST_F(Skeleton_Test, 部件运动发布为部件项且可关闭)
{
	//捕获发送事件
	std::vector<std::shared_ptr<engine::Event>> sent;
	skeleton.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	skeleton.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[](auto&&, auto&&, auto&&) {});
	skeleton.attach();

	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 501));

	//子骨骼绕 Z 轴旋转 90 度
	float translation[3] = { 0.0f, 1.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.7071068f, 0.7071068f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 1, translation, rotation, scale));
	sent.clear();
	skeleton.frame_update(store);

	//部件项
	ASSERT_EQ(sent.size(), 1u);
	ASSERT_TRUE(sent[0]->config.contains("parts"));
	ASSERT_EQ(sent[0]->config["parts"].size(), 1u);
	EXPECT_EQ(sent[0]->config["parts"][0]["part"].get<uint16_t>(), 1u);
	EXPECT_TRUE(skeleton_float_near(
		sent[0]->config["parts"][0]["translation"][0].get<float>(), -0.5f));
	EXPECT_TRUE(skeleton_float_near(
		sent[0]->config["parts"][0]["translation"][1].get<float>(), -0.5f));

	//关闭事件输出
	ASSERT_TRUE(skeleton.event_output_set(false));
	sent.clear();
	ASSERT_TRUE(skeleton.pose_reset(store, entity));
	skeleton.frame_update(store);
	EXPECT_TRUE(sent.empty());
}

//端到端：整身平移驱动碰撞
TEST_F(Skeleton_Test, 根骨骼平移事件驱动碰撞检测)
{
	//碰撞代理器
	engine::Collision_Proxy proxy;
	//代理器事件
	std::vector<std::shared_ptr<engine::Event>> proxy_sent;
	//接入入口
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//代理器发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&proxy_sent](std::shared_ptr<engine::Event> evt) { proxy_sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//骨骼系统发送通道直接投递到同一入口
	skeleton.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&entry](std::shared_ptr<engine::Event> evt) { entry(evt); });
	skeleton.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[](auto&&, auto&&, auto&&) {});
	skeleton.attach();

	//构建空间与两个碰撞体
	proxy_sent.clear();
	nlohmann::json region_payload = skeleton_region_payload("骨骼空间");
	entry(skeleton_event_make("RegionBuild", region_payload));
	entry(skeleton_event_make("ColliderBuild", region_payload));
	entry(skeleton_event_make("ColliderBuild", region_payload));
	ASSERT_EQ(proxy_sent.size(), 2u);
	const uint64_t collider_A = proxy_sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = proxy_sent[1]->config["collider_ID"].get<uint64_t>();

	//A：原点单盒体（子部件 0）
	nlohmann::json set_A = region_payload;
	set_A["collider_ID"] = collider_A;
	set_A["geometry"] = nlohmann::json::array();
	set_A["geometry"].push_back(skeleton_box_part(0, 0.0, 0.0, 1.0));
	set_A["detection_mode"] = nlohmann::json::object();
	set_A["detection_mode"]["is_swept_volume"] = false;
	set_A["detection_mode"]["step_length"] = 1u;
	entry(skeleton_event_make("ColliderSet", set_A));

	//B：X=2.5 的静态盒体（初始不重叠）
	nlohmann::json set_B = region_payload;
	set_B["collider_ID"] = collider_B;
	set_B["geometry"] = nlohmann::json::object();
	set_B["geometry"]["type"] = "box";
	set_B["geometry"]["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	set_B["geometry"]["position"] = nlohmann::json::array({ 2.5, 0.0, 0.0 });
	entry(skeleton_event_make("ColliderSet", set_B));

	//激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(skeleton_event_make("RegionState", state_payload));

	//基准检测无碰撞
	std::optional<std::vector<engine::Collision_Detail>> baseline =
		proxy.region_detect("骨骼空间");
	ASSERT_TRUE(baseline.has_value());
	EXPECT_FALSE(skeleton_pair_found(*baseline, collider_A, collider_B));

	//骨骼演员：根骨骼平移 X=1
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, collider_A));
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	//碰撞体 A 随整身运动到 X=1，与 B 重叠
	std::optional<std::vector<engine::Collision_Detail>> moved =
		proxy.region_detect("骨骼空间");
	ASSERT_TRUE(moved.has_value());
	EXPECT_TRUE(skeleton_pair_found(*moved, collider_A, collider_B));
}

//端到端：部件旋转驱动碰撞
TEST_F(Skeleton_Test, 子骨骼旋转事件驱动部件碰撞)
{
	//碰撞代理器
	engine::Collision_Proxy proxy;
	std::vector<std::shared_ptr<engine::Event>> proxy_sent;
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//代理器发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&proxy_sent](std::shared_ptr<engine::Event> evt) { proxy_sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//骨骼系统发送通道直接投递到同一入口
	skeleton.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&entry](std::shared_ptr<engine::Event> evt) { entry(evt); });
	skeleton.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[](auto&&, auto&&, auto&&) {});
	skeleton.attach();

	//构建空间与两个碰撞体
	proxy_sent.clear();
	nlohmann::json region_payload = skeleton_region_payload("部件空间");
	entry(skeleton_event_make("RegionBuild", region_payload));
	entry(skeleton_event_make("ColliderBuild", region_payload));
	entry(skeleton_event_make("ColliderBuild", region_payload));
	ASSERT_EQ(proxy_sent.size(), 2u);
	const uint64_t collider_A = proxy_sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = proxy_sent[1]->config["collider_ID"].get<uint64_t>();

	//A：根部盒体（部件 0，原点）＋ 子部件盒体（部件 1，Y=1.5）
	nlohmann::json set_A = region_payload;
	set_A["collider_ID"] = collider_A;
	set_A["geometry"] = nlohmann::json::array();
	set_A["geometry"].push_back(skeleton_box_part(0, 0.0, 0.0, 0.3));
	set_A["geometry"].push_back(skeleton_box_part(1, 0.0, 1.5, 0.3));
	set_A["detection_mode"] = nlohmann::json::object();
	set_A["detection_mode"]["is_swept_volume"] = false;
	set_A["detection_mode"]["step_length"] = 1u;
	entry(skeleton_event_make("ColliderSet", set_A));

	//B：Y=0.8 的静态盒体（与初始部件 1 不重叠）
	nlohmann::json set_B = region_payload;
	set_B["collider_ID"] = collider_B;
	set_B["geometry"] = nlohmann::json::object();
	set_B["geometry"]["type"] = "box";
	set_B["geometry"]["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	set_B["geometry"]["position"] = nlohmann::json::array({ 0.0, 0.8, 0.0 });
	entry(skeleton_event_make("ColliderSet", set_B));

	//激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(skeleton_event_make("RegionState", state_payload));

	//基准检测无碰撞
	std::optional<std::vector<engine::Collision_Detail>> baseline =
		proxy.region_detect("部件空间");
	ASSERT_TRUE(baseline.has_value());
	EXPECT_FALSE(skeleton_pair_found(*baseline, collider_A, collider_B));

	//骨骼演员：子骨骼旋转 90 度
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, collider_A));
	float translation[3] = { 0.0f, 1.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.7071068f, 0.7071068f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 1, translation, rotation, scale));
	skeleton.frame_update(store);

	//子部件旋转到（-0.5, 1.0），与 B 重叠
	std::optional<std::vector<engine::Collision_Detail>> moved =
		proxy.region_detect("部件空间");
	ASSERT_TRUE(moved.has_value());
	EXPECT_TRUE(skeleton_pair_found(*moved, collider_A, collider_B));
}
