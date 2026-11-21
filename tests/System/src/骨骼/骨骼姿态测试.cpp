/*
骨骼姿态测试
覆盖：挂载后绑定姿态、局部变换写入与脏标记、帧求值前后的世界矩阵、
层级旋转传播、姿态复位、非法写入拒绝。
*/
#include "骨骼测试辅助.h"

//挂载后为绑定姿态
TEST_F(Skeleton_Test, 挂载后为绑定姿态且世界矩阵已求值)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//姿态组件
	engine::Skeleton_Pose* pose = store.component_get<engine::Skeleton_Pose>(entity);
	ASSERT_NE(pose, nullptr);
	EXPECT_EQ(pose->bone_count(), 2u);
	//归属槽位由对象池写入
	EXPECT_EQ(pose->owner, entity.index());

	//局部变换等于绑定姿态
	EXPECT_TRUE(skeleton_float_near(pose->local_translation[0], 0.0f));
	EXPECT_TRUE(skeleton_float_near(pose->local_translation[3], 0.0f));
	EXPECT_TRUE(skeleton_float_near(pose->local_translation[4], 1.0f));

	//子骨骼世界矩阵平移分量
	float world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 1, world));
	EXPECT_TRUE(skeleton_float_near(world[12], 0.0f));
	EXPECT_TRUE(skeleton_float_near(world[13], 1.0f));
	EXPECT_TRUE(skeleton_float_near(world[14], 0.0f));
}

//帧求值前后的世界矩阵
TEST_F(Skeleton_Test, 局部写入后帧求值才更新世界矩阵)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//根骨骼平移到 X=2
	float translation[3] = { 2.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));

	//帧求值前仍是旧世界矩阵
	float world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 0, world));
	EXPECT_TRUE(skeleton_float_near(world[12], 0.0f));

	//帧求值后更新
	skeleton.frame_update(store);
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 0, world));
	EXPECT_TRUE(skeleton_float_near(world[12], 2.0f));
}

//层级旋转传播
TEST_F(Skeleton_Test, 父骨骼旋转传播到子骨骼世界矩阵)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//根骨骼绕 Z 轴旋转 90 度（半角 45 度）
	float translation[3] = { 0.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.7071068f, 0.7071068f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	//子骨骼世界平移（0,1,0）旋转 90 度后为（-1,0,0）
	float world[16] = {};
	ASSERT_TRUE(skeleton.pose_matrix_get(store, entity, 1, world));
	EXPECT_TRUE(skeleton_float_near(world[12], -1.0f));
	EXPECT_TRUE(skeleton_float_near(world[13], 0.0f));
	EXPECT_TRUE(skeleton_float_near(world[14], 0.0f));
}

//姿态复位
TEST_F(Skeleton_Test, 姿态复位回到绑定姿态)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//修改子骨骼平移
	float translation[3] = { 0.0f, 5.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 1, translation, rotation, scale));

	//复位后局部平移回到绑定值
	ASSERT_TRUE(skeleton.pose_reset(store, entity));
	engine::Skeleton_Pose* pose = store.component_get<engine::Skeleton_Pose>(entity);
	ASSERT_NE(pose, nullptr);
	EXPECT_TRUE(skeleton_float_near(pose->local_translation[4], 1.0f));
}

//非法写入拒绝
TEST_F(Skeleton_Test, 非法局部写入被拒绝且不改动姿态)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//合法参数
	float translation[3] = { 1.0f, 2.0f, 3.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };

	//下标越界
	EXPECT_FALSE(skeleton.bone_local_set(store, entity, 2, translation, rotation, scale));
	//零四元数
	float zero_rotation[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	EXPECT_FALSE(skeleton.bone_local_set(store, entity, 0, translation, zero_rotation, scale));
	//零缩放
	float zero_scale[3] = { 1.0f, 0.0f, 1.0f };
	EXPECT_FALSE(skeleton.bone_local_set(store, entity, 0, translation, rotation, zero_scale));

	//姿态保持绑定值
	engine::Skeleton_Pose* pose = store.component_get<engine::Skeleton_Pose>(entity);
	ASSERT_NE(pose, nullptr);
	EXPECT_TRUE(skeleton_float_near(pose->local_translation[0], 0.0f));
	EXPECT_TRUE(skeleton_float_near(pose->local_translation[1], 0.0f));
}
