/*
渲染蒙皮绘制测试（方向十 · 收敛轮 859）
覆盖：蒙皮网格绘制入口与非法输入拒绝、静态网格不读调色板（binding 5 零差异）、
      调色板旋转真实改变蒙皮顶点位置（像素级验证 GPU 蒙皮通路）。
口径：需要交换链像素回读能力；不具备时用例跳过而不是失败。
*/
#include <vector>

#include "测试辅助/渲染测试辅助.h"

using namespace byjy_render_test;

//渲染蒙皮绘制测试夹具
class Renderer_Animation_SkinDraw_Test : public ::testing::Test
{
protected:
	//本机是否具备像素回读能力
	static bool readback_ok(void)
	{
		engine::Renderer probe;
		if (!probe.init(hidden_config()))
			return false;
		return probe.compute().readback_supported();
	}
};

//单个蒙皮三角形（关节全 0、权重满值）
static engine::Skinned_Mesh_Data skinned_triangle(void)
{
	engine::Skinned_Mesh_Data data;
	data.vertices = {
		{ { -0.5f, -0.4f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.1f, -0.4f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
		{ { -0.3f, 0.2f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0, 0, 0, 0 }, { 65535, 0, 0, 0 } },
	};
	data.indices = { 0, 1, 2 };
	return data;
}

//单个静态三角形（与蒙皮三角形同位置，便于逐像素对照）
static engine::Mesh_Data plain_triangle(void)
{
	engine::Mesh_Data data;
	data.vertices = {
		{ { -0.5f, -0.4f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -0.1f, -0.4f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -0.3f, 0.2f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
	};
	data.indices = { 0, 1, 2 };
	return data;
}

//绕 Z 轴 90° 旋转（列主序）
static void rotation_z_90(float out[16])
{
	const float values[16] = {
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };
	std::memcpy(out, values, sizeof(values));
}

//单位矩阵
static void identity_16(float out[16])
{
	std::memset(out, 0, sizeof(float) * 16);
	out[0] = 1.0f;
	out[5] = 1.0f;
	out[10] = 1.0f;
	out[15] = 1.0f;
}

//蒙皮绘制入口：合法网格成功，静态网格与空句柄拒绝
TEST_F(Renderer_Animation_SkinDraw_Test, 蒙皮绘制入口与非法输入拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle skinned = 0;
	engine::Mesh_Handle plain = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(skinned, skinned_triangle()));
	ASSERT_TRUE(renderer.resource().mesh_load(plain, plain_triangle()));
	//合法蒙皮网格登记成功
	EXPECT_TRUE(renderer.resource().mesh_draw_skinned(skinned, nullptr));
	//静态网格与空句柄拒绝（步长不足 48 字节）
	EXPECT_FALSE(renderer.resource().mesh_draw_skinned(plain, nullptr));
	EXPECT_FALSE(renderer.resource().mesh_draw_skinned(0, nullptr));
	renderer.resource().mesh_unload(skinned);
	renderer.resource().mesh_unload(plain);
}

//静态网格不读调色板：调色板旋转后静态画面逐通道不变
TEST_F(Renderer_Animation_SkinDraw_Test, 静态网格不受调色板影响)
{
	//无回读能力则跳过
	if (!readback_ok())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle plain = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(plain, plain_triangle()));
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(1, offset));

	//单位调色板
	float matrix[16] = {};
	identity_16(matrix);
	ASSERT_TRUE(renderer.resource().animation_palette_upload(matrix, 1, offset));
	ASSERT_TRUE(renderer.resource().mesh_draw(plain, nullptr));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));

	//旋转调色板：静态路径不得读取
	rotation_z_90(matrix);
	ASSERT_TRUE(renderer.resource().animation_palette_upload(matrix, 1, offset));
	ASSERT_TRUE(renderer.resource().mesh_draw(plain, nullptr));
	std::vector<uint8_t> rotated(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rotated.data(), static_cast<uint32_t>(rotated.size())));

	EXPECT_LE(max_channel_diff(baseline, rotated), 2);
	renderer.resource().animation_palette_release(offset);
	renderer.resource().mesh_unload(plain);
}

//调色板旋转移动蒙皮顶点：画面必然改变
TEST_F(Renderer_Animation_SkinDraw_Test, 调色板旋转移动蒙皮顶点)
{
	//无回读能力则跳过
	if (!readback_ok())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.resource().texture_bind_set(0);
	engine::Mesh_Handle skinned = 0;
	ASSERT_TRUE(renderer.resource().mesh_load_skinned(skinned, skinned_triangle()));
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.resource().animation_palette_allocate(1, offset));

	//单位调色板
	float matrix[16] = {};
	identity_16(matrix);
	ASSERT_TRUE(renderer.resource().animation_palette_upload(matrix, 1, offset));
	ASSERT_TRUE(renderer.resource().mesh_draw_skinned(skinned, nullptr));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<uint32_t>(baseline.size())));

	//绕 Z 旋转 90°：顶点从左侧转到下方
	rotation_z_90(matrix);
	ASSERT_TRUE(renderer.resource().animation_palette_upload(matrix, 1, offset));
	ASSERT_TRUE(renderer.resource().mesh_draw_skinned(skinned, nullptr));
	std::vector<uint8_t> rotated(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rotated.data(), static_cast<uint32_t>(rotated.size())));

	EXPECT_GT(max_channel_diff(baseline, rotated), 8);
	renderer.resource().animation_palette_release(offset);
	renderer.resource().mesh_unload(skinned);
}
