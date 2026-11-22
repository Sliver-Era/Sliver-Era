/*
渲染动画零差异与容量测试（方向十 · 收敛轮 862 · 阶段 E）
覆盖：未使用动画时画面零差异、多演员区间互不干扰、动画开启前后的静态路径一致。
本套件为收口守卫：任何动画相关改动若污染默认静态渲染，都会在此暴露。
*/
#include <chrono>
#include <vector>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Animation/动画数据.h"
#include "Engine/EngineRenderer/src/render/Animation/动画应用.h"

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

//构造链式骨架（每骨骼父下标为前一根）
static std::shared_ptr<engine::Animation_Skeleton> chain_skeleton(uint32_t count)
{
	std::vector<engine::Animation_Bone> bones;
	for (uint32_t index = 0; index < count; ++index)
	{
		engine::Animation_Bone bone;
		bone.name = "b" + std::to_string(index);
		bone.parent = index == 0 ? -1 : static_cast<int32_t>(index - 1);
		bones.push_back(bone);
	}
	auto skeleton = std::make_shared<engine::Animation_Skeleton>();
	engine::Animation_Skeleton::build(bones, *skeleton);
	return skeleton;
}

//构造只驱动根骨骼的定长平移片段
static engine::Animation_Clip offset_clip(float offset)
{
	engine::Animation_Clip clip;
	clip.duration = 1.0f;
	engine::Animation_Track track;
	track.bone = "b0";
	track.channel = engine::Animation_Channel::Translation;
	track.keys = {
		{ 0.0f, { offset, 0.0f, 0.0f, 0.0f } },
		{ 1.0f, { offset, 0.0f, 0.0f, 0.0f } },
	};
	clip.tracks.push_back(track);
	return clip;
}

//混合端点退化：权重 0 / 1 的调色板与单片段写入逐分量一致
TEST_F(Renderer_Animation_ZeroDiff_Test, 混合端点与单片段等价)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const auto skeleton = chain_skeleton(4);
	const engine::Animation_Clip first = offset_clip(2.0f);
	const engine::Animation_Clip second = offset_clip(9.0f);

	//单片段基准
	uint32_t single = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(4, single));
	ASSERT_TRUE(renderer.animation().actor_clip_write(single, first, skeleton, 0.5f, false));
	float reference[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(reference, 1, single));
	renderer.animation().actor_destroy(single);

	//权重 0：与 first 一致
	uint32_t blended = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(4, blended));
	ASSERT_TRUE(renderer.animation().actor_blend_write(blended, first, second, skeleton, 0.5f, 0.5f, 0.0f, false));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, blended));
	EXPECT_EQ(std::memcmp(reference, matrix, sizeof(matrix)), 0);
	//权重 1：与 second 一致
	ASSERT_TRUE(renderer.animation().actor_blend_write(blended, first, second, skeleton, 0.5f, 0.5f, 1.0f, false));
	ASSERT_TRUE(renderer.resource().animation_palette_readback(matrix, 1, blended));
	EXPECT_FLOAT_EQ(matrix[12], 9.0f);
	renderer.animation().actor_destroy(blended);
}

//混合路径性能基线：8 演员 × 32 骨骼 × 30 帧（不做硬阈值断言）
TEST_F(Renderer_Animation_ZeroDiff_Test, 混合路径性能基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const auto skeleton = chain_skeleton(32);
	const engine::Animation_Clip first = offset_clip(1.0f);
	const engine::Animation_Clip second = offset_clip(3.0f);
	uint32_t offsets[8] = {};
	for (uint32_t index = 0; index < 8; ++index)
		ASSERT_TRUE(renderer.animation().actor_create(32, offsets[index]));

	const uint32_t frame_count = 30;
	const auto start = std::chrono::steady_clock::now();
	for (uint32_t frame = 0; frame < frame_count; ++frame)
	{
		const float weight = static_cast<float>(frame % 10) / 9.0f;
		const float time = static_cast<float>(frame) / frame_count;
		for (uint32_t index = 0; index < 8; ++index)
			ASSERT_TRUE(renderer.animation().actor_blend_write(offsets[index], first, second, skeleton,
				time, time, weight, true));
	}
	const auto end = std::chrono::steady_clock::now();
	const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
	std::cout << "[动画混合基线] 8 演员 × 32 骨骼：" << (total_ms / frame_count) << " ms/帧（"
		<< frame_count << " 帧）" << std::endl;
	for (uint32_t offset : offsets)
		renderer.animation().actor_destroy(offset);
}

//大场景静态守卫：64 演员区间占用后静态画面逐通道不变
TEST_F(Renderer_Animation_ZeroDiff_Test, 大场景静态零差异)
{
	//无回读能力则跳过
	{
		engine::Renderer probe;
		ASSERT_TRUE(probe.init(hidden_config()));
		if (!probe.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	}

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, triangle_data()));
	//基准帧
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));

	//占用 64 个演员区间（每段 4 条）并写入非平凡矩阵
	uint32_t offsets[64] = {};
	for (uint32_t index = 0; index < 64; ++index)
		ASSERT_TRUE(renderer.resource().animation_palette_allocate(4, offsets[index]));
	float matrix[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.25f, 0.0f, 1.0f };
	for (uint32_t index = 0; index < 64; ++index)
		ASSERT_TRUE(renderer.resource().animation_palette_upload(matrix, 1, offsets[index]));

	//再画静态三角形：不得读取调色板
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	std::vector<uint8_t> tested(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(tested.data(), static_cast<uint32_t>(tested.size())));
	EXPECT_LE(max_channel_diff(baseline, tested), 2);
	for (uint32_t index = 0; index < 64; ++index)
		renderer.resource().animation_palette_release(offsets[index]);
	renderer.resource().mesh_unload(handle);
}

