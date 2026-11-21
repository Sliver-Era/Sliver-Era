/*
骨骼链接测试
覆盖：挂载首帧无部件运动、根骨骼平移产生两条部件运动、无变化帧运动量为空、
子骨骼旋转产生旋转增量、未绑定碰撞体不建链接、无部件绑定资产只建姿态。
*/
#include "骨骼测试辅助.h"

//挂载首帧无运动量
TEST_F(Skeleton_Test, 挂载首帧不产生部件运动)
{
	//登记资产并挂载（带碰撞体编号）
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 100));

	//链接组件已创建
	ASSERT_NE(store.component_get<engine::Skeleton_Link>(entity), nullptr);

	//首帧求值无运动量
	skeleton.frame_update(store);
	const engine::Skeleton_Part_Motion* motions = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.part_motions_get(store, entity, motions, count));
	EXPECT_EQ(count, 0u);
	EXPECT_EQ(motions, nullptr);
}

//根骨骼平移
TEST_F(Skeleton_Test, 根骨骼平移为两个部件产生平移增量)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 101));

	//根骨骼平移到 X=1
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	//两条部件运动均为 X=1 平移
	const engine::Skeleton_Part_Motion* motions = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.part_motions_get(store, entity, motions, count));
	ASSERT_EQ(count, 2u);
	for (uint32_t index = 0; index < count; index++)
	{
		EXPECT_TRUE(skeleton_float_near(motions[index].translation[0], 1.0f));
		EXPECT_TRUE(skeleton_float_near(motions[index].translation[1], 0.0f));
		EXPECT_TRUE(skeleton_float_near(motions[index].translation[2], 0.0f));
		//旋转近似单位
		EXPECT_TRUE(skeleton_float_near(std::abs(motions[index].rotation[3]), 1.0f, 1e-5f));
	}
}

//无变化帧
TEST_F(Skeleton_Test, 姿态无变化时运动量为空)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 102));

	//先产生一帧运动
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	//无变化的下一次求值
	skeleton.frame_update(store);
	const engine::Skeleton_Part_Motion* motions = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.part_motions_get(store, entity, motions, count));
	EXPECT_EQ(count, 0u);
}

//子骨骼旋转
TEST_F(Skeleton_Test, 子骨骼旋转产生旋转增量)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 103));
	skeleton.frame_update(store);

	//子骨骼绕 Z 轴旋转 90 度
	float translation[3] = { 0.0f, 1.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.7071068f, 0.7071068f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 1, translation, rotation, scale));
	skeleton.frame_update(store);

	//仅子部件产生运动
	const engine::Skeleton_Part_Motion* motions = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.part_motions_get(store, entity, motions, count));
	ASSERT_EQ(count, 1u);
	EXPECT_EQ(motions[0].part, 1);
	//旋转增量 90 度（半角 45 度）
	EXPECT_TRUE(skeleton_float_near(motions[0].rotation[3], 0.7071068f, 1e-4f));
	//部件偏移随之产生的平移增量
	EXPECT_TRUE(skeleton_float_near(motions[0].translation[0], -0.5f));
	EXPECT_TRUE(skeleton_float_near(motions[0].translation[1], -0.5f));
}

//未绑定碰撞体
TEST_F(Skeleton_Test, 未指定碰撞体时不创建链接组件)
{
	//登记资产并挂载（碰撞体编号为零）
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//无链接组件
	EXPECT_EQ(store.component_get<engine::Skeleton_Link>(entity), nullptr);
	const engine::Skeleton_Part_Motion* motions = nullptr;
	uint32_t count = 0;
	EXPECT_FALSE(skeleton.part_motions_get(store, entity, motions, count));
}

//无部件绑定资产
TEST_F(Skeleton_Test, 资产无部件绑定时只创建姿态)
{
	//移除两根骨骼的部件绑定
	nlohmann::json config = skeleton_two_bone_json();
	config["bones"][0].erase("part");
	config["bones"][1].erase("part");

	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(config, asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 104));

	//姿态存在、链接不存在
	EXPECT_NE(store.component_get<engine::Skeleton_Pose>(entity), nullptr);
	EXPECT_EQ(store.component_get<engine::Skeleton_Link>(entity), nullptr);
}
