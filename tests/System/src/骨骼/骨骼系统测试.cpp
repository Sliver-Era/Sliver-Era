/*
骨骼系统测试
覆盖：资产登记 / 加载 / 卸载 / 计数、演员挂载与卸载、重复挂载与无效参数拒绝、
实体销毁后的组件级联、资产卸载后已挂载演员继续求值、多演员独立求值。
*/
#include "骨骼测试辅助.h"

//资产登记与卸载
TEST_F(Skeleton_Test, 资产登记卸载与计数)
{
	//登记合法资产
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_EQ(asset_ID, 1u);
	EXPECT_EQ(skeleton.asset_count(), 1u);
	ASSERT_NE(skeleton.asset_get(asset_ID), nullptr);
	EXPECT_EQ(skeleton.asset_get(asset_ID)->bone_count(), 2u);

	//卸载
	EXPECT_TRUE(skeleton.asset_remove(asset_ID));
	EXPECT_EQ(skeleton.asset_count(), 0u);
	EXPECT_EQ(skeleton.asset_get(asset_ID), nullptr);
	//重复卸载
	EXPECT_FALSE(skeleton.asset_remove(asset_ID));

	//无效资产不登记
	EXPECT_EQ(skeleton.asset_add(engine::Skeleton_Asset()), 0u);
}

//资产文件加载
TEST_F(Skeleton_Test, 资产文件加载并登记)
{
	//写入临时资产文件
	std::string path = skeleton_asset_write("系统加载资产.json", skeleton_two_bone_json());

	//加载并登记
	uint32_t asset_ID = skeleton.asset_load_file(path);
	ASSERT_EQ(asset_ID, 1u);
	EXPECT_EQ(skeleton.asset_count(), 1u);
	EXPECT_EQ(skeleton.asset_get(asset_ID)->name(), "双骨骼");

	//缺失路径不登记
	EXPECT_EQ(skeleton.asset_load_file(
		engine::detail::path_to_string(engine::detail::exe_dir_get()
			/ engine::detail::string_to_path("out/骨骼测试/不存在资产.json"))), 0u);
}

//演员挂载与卸载
TEST_F(Skeleton_Test, 演员挂载卸载与重复挂载拒绝)
{
	//登记资产
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	//挂载
	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 200));
	EXPECT_NE(store.component_get<engine::Skeleton_Pose>(entity), nullptr);
	EXPECT_NE(store.component_get<engine::Skeleton_Link>(entity), nullptr);

	//重复挂载拒绝
	EXPECT_FALSE(skeleton.actor_attach(store, entity, asset_ID, 200));

	//卸载
	EXPECT_TRUE(skeleton.actor_detach(store, entity));
	EXPECT_EQ(store.component_get<engine::Skeleton_Pose>(entity), nullptr);
	EXPECT_EQ(store.component_get<engine::Skeleton_Link>(entity), nullptr);
	//重复卸载
	EXPECT_FALSE(skeleton.actor_detach(store, entity));
}

//无效参数
TEST_F(Skeleton_Test, 无效实体与无效资产挂载失败)
{
	//空实体
	engine::Entity empty;
	EXPECT_FALSE(skeleton.actor_attach(store, empty, 1));

	//无效资产
	engine::Entity entity = store.entity_create();
	EXPECT_FALSE(skeleton.actor_attach(store, entity, 999));
	EXPECT_EQ(store.component_get<engine::Skeleton_Pose>(entity), nullptr);
}

//实体销毁级联
TEST_F(Skeleton_Test, 实体销毁级联清理姿态与链接组件)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 201));
	EXPECT_EQ(store.component_size<engine::Skeleton_Pose>(), 1u);
	EXPECT_EQ(store.component_size<engine::Skeleton_Link>(), 1u);

	//销毁实体
	ASSERT_TRUE(store.entity_destroy(entity));
	EXPECT_EQ(store.component_size<engine::Skeleton_Pose>(), 0u);
	EXPECT_EQ(store.component_size<engine::Skeleton_Link>(), 0u);
}

//资产卸载后仍可求值
TEST_F(Skeleton_Test, 资产卸载后已挂载演员继续求值)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID, 202));

	//卸载资产
	ASSERT_TRUE(skeleton.asset_remove(asset_ID));

	//已挂载演员继续求值
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	const float* palette = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.skin_palette_get(store, entity, palette, count));
	EXPECT_EQ(count, 2u);
	EXPECT_TRUE(skeleton_float_near(palette[12], 1.0f));
}

//多演员独立
TEST_F(Skeleton_Test, 多演员姿态互相独立)
{
	//登记资产
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	//两个演员
	engine::Entity first = store.entity_create();
	engine::Entity second = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, first, asset_ID));
	ASSERT_TRUE(skeleton.actor_attach(store, second, asset_ID));

	//分别平移
	float first_translation[3] = { 1.0f, 0.0f, 0.0f };
	float second_translation[3] = { 0.0f, 2.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, first, 0, first_translation, rotation, scale));
	ASSERT_TRUE(skeleton.bone_local_set(store, second, 0, second_translation, rotation, scale));
	skeleton.frame_update(store);

	//各自调色板平移独立
	const float* first_palette = nullptr;
	const float* second_palette = nullptr;
	uint32_t first_count = 0;
	uint32_t second_count = 0;
	ASSERT_TRUE(skeleton.skin_palette_get(store, first, first_palette, first_count));
	ASSERT_TRUE(skeleton.skin_palette_get(store, second, second_palette, second_count));
	EXPECT_TRUE(skeleton_float_near(first_palette[12], 1.0f));
	EXPECT_TRUE(skeleton_float_near(first_palette[13], 0.0f));
	EXPECT_TRUE(skeleton_float_near(second_palette[12], 0.0f));
	EXPECT_TRUE(skeleton_float_near(second_palette[13], 2.0f));
}
