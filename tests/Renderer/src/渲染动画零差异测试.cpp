/*
渲染动画零差异与容量测试（方向十 · 收敛轮 862 · 阶段 E）
覆盖：未使用动画时画面零差异、多演员区间互不干扰、动画开启前后的静态路径一致。
本套件为收口守卫：任何动画相关改动若污染默认静态渲染，都会在此暴露。
*/
#include <chrono>
#include <vector>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画数据.h"

using namespace byjy_render_test;

//渲染动画零差异测试夹具
class Renderer_Animation_ZeroDiff_Test : public ::testing::Test
{
};

//未使用动画与「分配后再释放」两种状态下，静态画面逐通道一致
TEST_F(Renderer_Animation_ZeroDiff_Test, 默认静态路径零差异)
{
	//先以探针实例确认本机回读能力
	{
		engine::Renderer probe;
		ASSERT_TRUE(probe.init(hidden_config()));
		if (!probe.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	}

	//基准：完全不触碰动画接口
	std::vector<uint8_t> baseline(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		renderer.resource().texture_bind_set(0);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, triangle_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));
	}

	//对照：分配演员 → 写单位矩阵 → 释放，再按相同步骤绘制
	std::vector<uint8_t> tested(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		uint32_t offset = UINT32_MAX;
		ASSERT_TRUE(renderer.animation().actor_create(2, offset));
		ASSERT_TRUE(renderer.animation().actor_destroy(offset));
		renderer.resource().texture_bind_set(0);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.resource().mesh_load(handle, triangle_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(tested.data(), static_cast<uint32_t>(tested.size())));
	}

	//动画关闭时两条路径逐通道差不超过抗锯齿容差
	EXPECT_LE(max_channel_diff(baseline, tested), 2);
}

//两个演员的调色板区间互不覆盖
TEST_F(Renderer_Animation_ZeroDiff_Test, 多演员区间独立)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint32_t first = UINT32_MAX;
	uint32_t second = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(2, first));
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(2, second));
	ASSERT_NE(first, second);
	//两个平移矩阵分别写入各自区间
	float first_matrix[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		1.0f, 2.0f, 3.0f, 1.0f };
	float second_matrix[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		4.0f, 5.0f, 6.0f, 1.0f };
	ASSERT_TRUE(renderer.resource().animation_palette_upload(first_matrix, 1, first));
	ASSERT_TRUE(renderer.resource().animation_palette_upload(second_matrix, 1, second));
	//读回各自首条互不影响
	float readback[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(readback, 1, first));
	EXPECT_FLOAT_EQ(readback[12], 1.0f);
	ASSERT_TRUE(renderer.resource().animation_palette_readback(readback, 1, second));
	EXPECT_FLOAT_EQ(readback[12], 4.0f);
	//释放首段不影响第二段
	ASSERT_TRUE(renderer.resource().animation_palette_release(first));
	ASSERT_TRUE(renderer.resource().animation_palette_readback(readback, 1, second));
	EXPECT_FLOAT_EQ(readback[12], 4.0f);
	renderer.resource().animation_palette_release(second);
}

/*
调色板参与着色：平移矩阵写入首条后，蒙皮画面必须发生位移
若 binding 5 未真正参与顶点变换，本用例会因两帧逐通道一致而失败。
*/
TEST_F(Renderer_Animation_ZeroDiff_Test, 蒙皮调色板平移改变画面)
{
	//先以探针实例确认本机回读能力
	{
		engine::Renderer probe;
		ASSERT_TRUE(probe.init(hidden_config()));
		if (!probe.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	}

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.resource().texture_bind_set(0);
	//单个蒙皮三角形：关节全指向 0，权重满值
	engine::Skinned_Mesh_Data skinned;
	skinned.vertices = {
		{ { -0.5f, -0.4f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.1f, -0.4f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.3f, 0.2f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
	};
	skinned.indices = { 0, 1, 2 };
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(handle, skinned));
	//首条调色板区间
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(1, offset));
	ASSERT_EQ(offset, 0u);

	//基准：单位调色板
	float identity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.resource().animation_palette_upload(identity, 1, offset));
	ASSERT_TRUE(renderer.resource().mesh_draw_skinned(handle, nullptr));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));

	//对照：首条平移 0.5（画面右移）
	float shifted[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.resource().animation_palette_upload(shifted, 1, offset));
	ASSERT_TRUE(renderer.resource().mesh_draw_skinned(handle, nullptr));
	std::vector<uint8_t> shifted_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(shifted_pixels.data(), static_cast<uint32_t>(shifted_pixels.size())));

	//画面必须变化：调色板确实参与顶点变换
	EXPECT_GT(max_channel_diff(baseline, shifted_pixels), 8);
	renderer.resource().animation_palette_release(offset);
	renderer.resource().mesh_unload(handle);
}

//蒙皮与静态绘制共用帧链路：每帧均成功推进
TEST_F(Renderer_Animation_ZeroDiff_Test, 蒙皮与静态同帧共存)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.resource().texture_bind_set(0);
	engine::Skinned_Mesh_Data skinned;
	skinned.vertices = {
		{ { -0.6f, -0.2f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.2f, -0.2f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.4f, 0.2f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
	};
	skinned.indices = { 0, 1, 2 };
	engine::Mesh_Handle skinned_handle = 0;
	engine::Mesh_Handle plain_handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(skinned_handle, skinned));
	ASSERT_TRUE(renderer.resource().mesh_load(plain_handle, triangle_data()));
	//同一帧登记两项：蒙皮 + 静态
	ASSERT_TRUE(renderer.resource().mesh_draw_skinned(skinned_handle, nullptr));
	ASSERT_TRUE(renderer.resource().mesh_draw(plain_handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//两帧连续推进（帧槽轮转下仍稳定）
	ASSERT_TRUE(renderer.resource().mesh_draw_skinned(skinned_handle, nullptr));
	ASSERT_TRUE(renderer.resource().mesh_draw(plain_handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	renderer.resource().mesh_unload(skinned_handle);
	renderer.resource().mesh_unload(plain_handle);
}

//性能基线：8 演员 × 32 骨骼，记录稳态帧耗时（不做硬阈值断言）
TEST_F(Renderer_Animation_ZeroDiff_Test, 性能基线八演员三十二骨骼)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.resource().texture_bind_set(0);
	//单个蒙皮四边形复用给 8 个演员，用变换错开位置
	engine::Skinned_Mesh_Data quads;
	quads.vertices = {
		{ { -0.1f, -0.1f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { 0.1f, -0.1f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { 0.1f, 0.1f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.1f, 0.1f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
	};
	quads.indices = { 0, 1, 2, 0, 2, 3 };
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(handle, quads));
	//8 个演员各占 32 条调色板
	uint32_t offsets[8] = {};
	for (uint32_t index = 0; index < 8; ++index)
		ASSERT_TRUE(renderer.resource().animation_palette_allocate(32, offsets[index]));

	//每个演员的平移变换（列主序）
	const float spacing = 0.22f;
	std::vector<uint8_t> pixels(pixel_bytes());
	const uint32_t frame_count = 30;
	const auto start = std::chrono::steady_clock::now();
	for (uint32_t frame = 0; frame < frame_count; ++frame)
	{
		for (uint32_t index = 0; index < 8; ++index)
		{
			float transform[16] = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				-0.8f + spacing * static_cast<float>(index), 0.0f, 0.0f, 1.0f };
			ASSERT_TRUE(renderer.resource().mesh_draw_skinned(handle, transform));
		}
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	}
	const auto end = std::chrono::steady_clock::now();
	const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
	std::cout << "[动画基线] 8 演员 × 32 骨骼：" << (total_ms / frame_count) << " ms/帧（"
		<< frame_count << " 帧）" << std::endl;
	renderer.resource().mesh_unload(handle);
}

