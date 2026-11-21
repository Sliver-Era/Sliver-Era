/*
骨骼动画支持测试
覆盖：批量姿态写入（直接 API / 事件）、骨骼名解析、绑定姿态读取、非法条目部分成功。
*/
#include "骨骼测试辅助.h"

//文件内部辅助设施
namespace
{
	//构建骨骼模块事件
	std::shared_ptr<engine::Event> skeleton_support_event(const std::string& tag,
		const nlohmann::json& config)
	{
		return std::make_shared<engine::Event>("", "", "Skeleton", tag, config);
	}

	//查找指定标签的骨骼结果事件
	bool skeleton_support_result(const std::vector<std::shared_ptr<engine::Event>>& events,
		const std::string& tag, std::shared_ptr<engine::Event>& receiver)
	{
		for (const std::shared_ptr<engine::Event>& evt : events)
		{
			if (evt->category == "Skeleton" && evt->tag == tag)
			{
				receiver = evt;
				return true;
			}
		}
		return false;
	}
}

//批量姿态写入（直接 API）
TEST_F(Skeleton_Test, 批量姿态写入后求值正确)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));
	skeleton.frame_update(store);

	//两条局部变换
	engine::Skeleton_Local_Transform entries[2];
	entries[0].bone = 0;
	entries[0].translation[0] = 2.0f;
	entries[1].bone = 1;
	entries[1].translation[1] = 1.0f;
	entries[1].rotation[2] = 0.7071068f;
	entries[1].rotation[3] = 0.7071068f;

	//批量写入并求值
	EXPECT_EQ(skeleton.pose_apply(store, entity, entries, 2), 2u);
	skeleton.frame_update(store);

	//根骨骼世界平移（2,0,0）
	float root_world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 0, root_world));
	EXPECT_TRUE(skeleton_float_near(root_world[12], 2.0f));
	EXPECT_TRUE(skeleton_float_near(root_world[13], 0.0f));
	//子骨骼世界平移（2,1,0）
	float child_world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 1, child_world));
	EXPECT_TRUE(skeleton_float_near(child_world[12], 2.0f));
	EXPECT_TRUE(skeleton_float_near(child_world[13], 1.0f));
}

//骨骼名解析与绑定姿态读取
TEST_F(Skeleton_Test, 骨骼名解析与绑定姿态读取)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//骨骼名解析
	EXPECT_EQ(skeleton.bone_seek(store, entity, "root"), 0);
	EXPECT_EQ(skeleton.bone_seek(store, entity, "child"), 1);
	EXPECT_EQ(skeleton.bone_seek(store, entity, "missing"), -1);

	//绑定姿态读取
	float translation[3] = {};
	float rotation[4] = {};
	float scale[3] = {};
	ASSERT_TRUE(skeleton.bone_bind_get(store, entity, 1, translation, rotation, scale));
	EXPECT_TRUE(skeleton_float_near(translation[1], 1.0f));
	EXPECT_TRUE(skeleton_float_near(std::abs(rotation[3]), 1.0f, 1e-5f));
	EXPECT_TRUE(skeleton_float_near(scale[0], 1.0f));
}

//非法条目部分成功
TEST_F(Skeleton_Test, 批量写入非法条目被跳过)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//一条合法、一条骨骼下标越界
	engine::Skeleton_Local_Transform entries[2];
	entries[0].bone = 0;
	entries[0].translation[0] = 1.0f;
	entries[1].bone = 99;
	entries[1].translation[0] = 5.0f;

	//仅合法条目生效
	EXPECT_EQ(skeleton.pose_apply(store, entity, entries, 2), 1u);
	skeleton.frame_update(store);
	float world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 0, world));
	EXPECT_TRUE(skeleton_float_near(world[12], 1.0f));
}

//事件批量姿态写入与查询
TEST_F(Skeleton_Test, 事件批量姿态写入与骨骼查询)
{
	//捕获发送事件与接入入口
	std::vector<std::shared_ptr<engine::Event>> sent;
	std::function<void(std::shared_ptr<engine::Event>)> entry;
	skeleton.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	skeleton.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	skeleton.store_bind(store);
	skeleton.attach();
	ASSERT_TRUE(static_cast<bool>(entry));
	sent.clear();

	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//事件批量写入
	nlohmann::json apply_config;
	apply_config["entity"] = entity.value;
	apply_config["bones"] = nlohmann::json::array();
	nlohmann::json root_entry;
	root_entry["bone"] = 0;
	root_entry["translation"] = nlohmann::json::array({ 2.0, 0.0, 0.0 });
	apply_config["bones"].push_back(root_entry);
	nlohmann::json child_entry;
	child_entry["bone"] = 1;
	child_entry["translation"] = nlohmann::json::array({ 0.0, 1.0, 0.0 });
	child_entry["rotation"] = nlohmann::json::array({ 0.0, 0.0, 0.7071068, 0.7071068 });
	apply_config["bones"].push_back(child_entry);
	sent.clear();
	entry(skeleton_support_event("PoseApply", apply_config));

	//结果含成功条数
	std::shared_ptr<engine::Event> apply_result;
	ASSERT_TRUE(skeleton_support_result(sent, "ActionResult", apply_result));
	EXPECT_TRUE(apply_result->config["success"].get<bool>());
	EXPECT_EQ(apply_result->config["applied"].get<uint32_t>(), 2u);

	//事件求值
	entry(skeleton_support_event("FrameUpdate", nlohmann::json::object()));
	float child_world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 1, child_world));
	EXPECT_TRUE(skeleton_float_near(child_world[12], 2.0f));
	EXPECT_TRUE(skeleton_float_near(child_world[13], 1.0f));

	//事件骨骼名解析
	nlohmann::json seek_config;
	seek_config["entity"] = entity.value;
	seek_config["name"] = "child";
	seek_config["request_ID"] = 21;
	sent.clear();
	entry(skeleton_support_event("BoneSeek", seek_config));
	std::shared_ptr<engine::Event> seek_result;
	ASSERT_TRUE(skeleton_support_result(sent, "BoneSeekResult", seek_result));
	EXPECT_EQ(seek_result->config["bone"].get<int32_t>(), 1);
	EXPECT_EQ(seek_result->config["request_ID"].get<uint64_t>(), 21u);

	//事件绑定姿态读取
	nlohmann::json bind_config;
	bind_config["entity"] = entity.value;
	bind_config["bone"] = 1;
	bind_config["request_ID"] = 22;
	sent.clear();
	entry(skeleton_support_event("BoneBindGet", bind_config));
	std::shared_ptr<engine::Event> bind_result;
	ASSERT_TRUE(skeleton_support_result(sent, "BoneBindResult", bind_result));
	EXPECT_TRUE(bind_result->config["success"].get<bool>());
	EXPECT_TRUE(skeleton_float_near(
		bind_result->config["translation"][1].get<float>(), 1.0f));
}
