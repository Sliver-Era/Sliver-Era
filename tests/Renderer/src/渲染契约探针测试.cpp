/*
渲染契约探针（2026-11-17 测试层改造 B）
把原先散落在功能用例中的实现常量集中到一处断言：
场景 UBO 整块字节、实例上传步长、初始化默认资源计数。
功能用例改为引用 测试辅助/渲染契约常量.h，只保留行为断言。
*/
#include "渲染场景测试辅助.h"

//契约常量
#include "测试辅助/渲染契约常量.h"

//场景 UBO 首帧整块上传字节数
TEST_F(Renderer_Scene_Test, 场景UBO整块字节)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	//首帧前累计值（应为 0）
	uint64_t previous = renderer.scene_upload_bytes();
	ASSERT_TRUE(renderer.frame_render());
	//首帧写满整块
	uint64_t delta = renderer.scene_upload_bytes() - previous;
	EXPECT_EQ(delta, byjy_render_test::SCENE_UBO_BYTES) << "场景 UBO 整块字节与契约常量不符";
}

//每实例上传步长
TEST_F(Renderer_Scene_Test, 实例上传步长)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	BYJY_REQUIRE_READBACK(renderer);
	//登记一个三角形绘制项
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(mesh, triangle_data()));
	ASSERT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
	//首帧前累计值
	uint64_t previous = renderer.instance_upload_bytes();
	ASSERT_TRUE(renderer.frame_render());
	//单实例首帧写入一个步长
	uint64_t delta = renderer.instance_upload_bytes() - previous;
	EXPECT_EQ(delta, byjy_render_test::INSTANCE_STRIDE_BYTES) << "实例上传步长与契约常量不符";
	renderer.resource().mesh_unload(mesh);
}

//初始化默认资源计数
TEST_F(Renderer_Scene_Test, 初始化默认资源计数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Renderer_Resource_Counts counts = renderer.resource_counts();
	//默认纹理 / 管线 / 渲染目标
	EXPECT_EQ(counts.textures, byjy_render_test::DEFAULT_TEXTURE_COUNT);
	EXPECT_EQ(counts.pipelines, byjy_render_test::DEFAULT_PIPELINE_COUNT);
	EXPECT_EQ(counts.render_targets, byjy_render_test::DEFAULT_RENDER_TARGET_COUNT);
	//默认不建网格 / 字体 / 材质
	EXPECT_EQ(counts.meshes, 0u);
	EXPECT_EQ(counts.fonts, 0u);
	EXPECT_EQ(counts.materials, 0u);
}
