/*
蒙皮求值测试
覆盖：绑定姿态下调色板为单位矩阵、根骨骼平移后的调色板平移、子骨骼旋转后的旋转分量、
未挂载实体的查询失败。
*/
#include "骨骼测试辅助.h"

//绑定姿态下调色板为单位矩阵
TEST_F(Skeleton_Test, 绑定姿态下蒙皮矩阵为单位矩阵)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));
	skeleton.frame_update(store);

	//调色板查询
	const float* palette = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.skin_palette_get(store, entity, palette, count));
	ASSERT_NE(palette, nullptr);
	EXPECT_EQ(count, 2u);

	//每根骨骼的蒙皮矩阵均为单位矩阵
	float identity[16] = {};
	engine::detail::bone_matrix_identity(identity);
	for (uint32_t index = 0; index < count; index++)
	{
		EXPECT_TRUE(engine::detail::bone_matrix_identity_check(&palette[index * 16], 1e-5f))
			<< "第 " << index << " 根骨骼蒙皮矩阵非单位";
	}
}

//根骨骼平移
TEST_F(Skeleton_Test, 根骨骼平移反映到调色板平移)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//根骨骼平移到 X=1
	float translation[3] = { 1.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 0, translation, rotation, scale));
	skeleton.frame_update(store);

	//调色板平移
	const float* palette = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.skin_palette_get(store, entity, palette, count));
	ASSERT_EQ(count, 2u);
	EXPECT_TRUE(skeleton_float_near(palette[12], 1.0f));
	EXPECT_TRUE(skeleton_float_near(palette[13], 0.0f));
	EXPECT_TRUE(skeleton_float_near(palette[16 + 12], 1.0f));
	EXPECT_TRUE(skeleton_float_near(palette[16 + 13], 0.0f));
}

//子骨骼旋转
TEST_F(Skeleton_Test, 子骨骼旋转反映到调色板旋转分量)
{
	//登记资产并挂载
	engine::Skeleton_Asset asset;
	ASSERT_TRUE(engine::Skeleton_Asset::build(skeleton_two_bone_json(), asset));
	uint32_t asset_ID = skeleton.asset_add(std::move(asset));
	ASSERT_NE(asset_ID, 0u);

	engine::Entity entity = store.entity_create();
	ASSERT_TRUE(skeleton.actor_attach(store, entity, asset_ID));

	//子骨骼绕 Z 轴旋转 90 度
	float translation[3] = { 0.0f, 1.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.7071068f, 0.7071068f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(skeleton.bone_local_set(store, entity, 1, translation, rotation, scale));
	skeleton.frame_update(store);

	//子骨骼调色板：平移（1,1,0）、旋转 90 度
	const float* palette = nullptr;
	uint32_t count = 0;
	ASSERT_TRUE(skeleton.skin_palette_get(store, entity, palette, count));
	ASSERT_EQ(count, 2u);
	const float* skin = &palette[16];
	EXPECT_TRUE(skeleton_float_near(skin[12], 1.0f));
	EXPECT_TRUE(skeleton_float_near(skin[13], 1.0f));
	EXPECT_TRUE(skeleton_float_near(skin[14], 0.0f));
	//第一列（局部 X 轴）映射到 Y 轴
	EXPECT_TRUE(skeleton_float_near(skin[0], 0.0f));
	EXPECT_TRUE(skeleton_float_near(skin[1], 1.0f));
	//第二列（局部 Y 轴）映射到 -X 轴
	EXPECT_TRUE(skeleton_float_near(skin[4], -1.0f));
	EXPECT_TRUE(skeleton_float_near(skin[5], 0.0f));
}

//未挂载查询失败
TEST_F(Skeleton_Test, 未挂载实体查询调色板失败)
{
	//未挂载实体
	engine::Entity entity = store.entity_create();
	const float* palette = nullptr;
	uint32_t count = 0;
	EXPECT_FALSE(skeleton.skin_palette_get(store, entity, palette, count));
	EXPECT_EQ(palette, nullptr);
	EXPECT_EQ(count, 0u);
}
