#pragma once
#include "渲染场景测试辅助.h"

namespace
{
	//构造中性灰立方体场景（方向光正对相机）
	void pbr_param_scene_register(engine::Renderer& renderer, engine::Mesh_Handle& mesh, uint64_t& texture)
	{
		std::vector<uint8_t> texels = solid_gray();
		ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		renderer.resource().texture_bind_set(texture);
		ASSERT_TRUE(renderer.resource().mesh_load(mesh, cube_data(1.0f)));
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.2f);
		renderer.scene().specular_set(1.0f, 64.0f);
		//统一走绝对粗糙度口径，避免光泽度换算干扰
		renderer.scene().pbr_enable_set(true);
		renderer.scene().pbr_roughness_absolute_set(true);
		renderer.scene().pbr_metallic_set(0.0f);
	}

	//画一帧并回读
	std::vector<uint8_t> pbr_param_frame(engine::Renderer& renderer, engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	}

	//构造沿 X 平移的模型矩阵（列主序）
	std::array<float, 16> quad_offset_matrix(float offset_x)
	{
		std::array<float, 16> matrix{};
		matrix[0] = 1.0f;
		matrix[5] = 1.0f;
		matrix[10] = 1.0f;
		matrix[12] = offset_x;
		matrix[15] = 1.0f;
		return matrix;
	}

	//统计某一半画面（0 = 左半、1 = 右半）相对参考帧的变化像素数
	uint32_t half_changed(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, uint32_t half, int threshold)
	{
		uint32_t count = 0;
		uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
		uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = begin; x < end; ++x)
			{
				std::array<uint8_t, 4> a = pixel_get(left, x, y);
				std::array<uint8_t, 4> b = pixel_get(right, x, y);
				int red = std::abs(static_cast<int>(a[0]) - static_cast<int>(b[0]));
				int green = std::abs(static_cast<int>(a[1]) - static_cast<int>(b[1]));
				int blue = std::abs(static_cast<int>(a[2]) - static_cast<int>(b[2]));
				if (red >= threshold || green >= threshold || blue >= threshold)
					++count;
			}
		}
		return count;
	}
}
