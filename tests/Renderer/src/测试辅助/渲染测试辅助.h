#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
/*
渲染测试辅助（2026-11-17 测试层改造 A）
集中提供渲染用例的共用前提：隐藏窗口配置、像素读取与结构指标、几何 / 纹理 / 矩阵构造、回读能力门控。
所有函数为 inline 外部链接，放在 byjy_render_test 命名空间；各用例文件以 using 声明引入，避免按文件复制。
*/
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

//获取渲染器门面（含场景描述）与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"

namespace byjy_render_test
{
	//窗口尺寸（与回读断言配套）
	inline constexpr uint32_t WINDOW_WIDTH = 64;
	inline constexpr uint32_t WINDOW_HEIGHT = 36;

	//构造隐藏窗口配置（默认 64×36；自定义尺寸用例显式传入）
	inline engine::Renderer_Config hidden_config(uint32_t width = WINDOW_WIDTH, uint32_t height = WINDOW_HEIGHT)
	{
		engine::Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}

	//构造多重采样配置（尺寸与 hidden_config 相同，仅改采样数）
	inline engine::Renderer_Config msaa_config(uint32_t samples)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = samples;
		return config;
	}

	//本帧像素字节数（RGBA8 紧排；默认 64×36）
	inline uint32_t pixel_bytes(uint32_t width = WINDOW_WIDTH, uint32_t height = WINDOW_HEIGHT)
	{
		return width * height * 4;
	}

	//取指定像素的四个字节（row_width 为整帧行宽）
	inline std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y,
		uint32_t row_width = WINDOW_WIDTH)
	{
		//行内偏移（每行宽 × 4 字节）
		std::size_t offset = (static_cast<std::size_t>(y) * row_width + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//失败诊断文本
	inline std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//外通道较大者（R 与 B 通道；避开 std::max —— 本层包含链有 min/max 宏）
	inline uint8_t outer_max(const std::array<uint8_t, 4>& value)
	{
		return value[0] > value[2] ? value[0] : value[2];
	}

	//中心像素的 RGB 均值（亮度粗量；通道序无关；默认 64×36）
	inline uint32_t center_brightness(const std::vector<uint8_t>& pixels,
		uint32_t width = WINDOW_WIDTH, uint32_t height = WINDOW_HEIGHT)
	{
		std::array<uint8_t, 4> center = pixel_get(pixels, width / 2, height / 2, width);
		return (static_cast<uint32_t>(center[0]) + center[1] + center[2]) / 3;
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	inline engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造带法线的四边形（nx / ny / nz 为四个顶点共用的法线）
	inline engine::Mesh_Data quad_data_with_normal(float nx, float ny, float nz)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { nx, ny, nz } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { nx, ny, nz } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { nx, ny, nz } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { nx, ny, nz } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造三角形网格（三顶点、三索引）
	inline engine::Mesh_Data triangle_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2 };
		return data;
	}

	//构造带逐面法线的立方体（与网格模块 cube_generate 一致：24 顶点 / 36 索引）
	inline engine::Mesh_Data cube_data(float half_extent)
	{
		//半边长
		const float h = half_extent;
		engine::Mesh_Data data;
		data.vertices = {
			// +X 面
			{ { +h, -h, -h }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +h, +h, -h }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +h, +h, +h }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +h, -h, +h }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			// -X 面
			{ { -h, -h, +h }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
			{ { -h, +h, +h }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
			{ { -h, +h, -h }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
			{ { -h, -h, -h }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
			// +Y 面
			{ { -h, +h, +h }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { +h, +h, +h }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { +h, +h, -h }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { -h, +h, -h }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			// -Y 面
			{ { -h, -h, -h }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
			{ { +h, -h, -h }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
			{ { +h, -h, +h }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
			{ { -h, -h, +h }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
			// +Z 面
			{ { -h, -h, +h }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { +h, -h, +h }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { +h, +h, +h }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -h, +h, +h }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			// -Z 面
			{ { +h, -h, -h }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -h, -h, -h }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -h, +h, -h }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { +h, +h, -h }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
		};
		data.indices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
			12, 13, 14, 14, 15, 12,
			16, 17, 18, 18, 19, 16,
			20, 21, 22, 22, 23, 20,
		};
		return data;
	}

	//构造纯红纹理（RGBA8：rgb = 红，alpha = 255）
	inline std::vector<uint8_t> solid_red(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 255;
			pixels[index + 1] = 0;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造纯白纹理（RGBA8：rgb = 255，alpha = 255）
	inline std::vector<uint8_t> solid_white(void)
	{
		return std::vector<uint8_t>(2 * 2 * 4, 255);
	}

	//构造中灰纹理（RGBA8：rgb = 128，alpha = 255；用于观察高光叠加而不被满照饱和）
	inline std::vector<uint8_t> solid_gray(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 128;
			pixels[index + 1] = 128;
			pixels[index + 2] = 128;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造纯绿纹理（RGBA8：rgb = 绿，alpha = 255；默认 2×2）
	inline std::vector<uint8_t> solid_green(uint32_t width = 2, uint32_t height = 2)
	{
		std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 0;
			pixels[index + 1] = 255;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//判断像素是否近似纯绿（绿通道高、外通道低；通道序无关）
	inline bool pixel_is_green(const std::array<uint8_t, 4>& value)
	{
		//绿通道（下标 1）须为高
		if (value[1] < 200)
			return false;
		//外通道须为低
		if (outer_max(value) > 60)
			return false;
		return true;
	}

	//构造绕 Y 轴旋转的模型矩阵（列主序；角度为弧度）
	inline std::array<float, 16> rotation_y_matrix(float angle)
	{
		//余弦 / 正弦
		float c = std::cos(angle);
		float s = std::sin(angle);
		return {
			c, 0.0f, -s, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			s, 0.0f, c, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	}

	//构造「铺满全屏」模型矩阵（列主序；放大 4 倍越过 NDC 边界被裁剪）
	inline std::array<float, 16> full_screen_matrix(void)
	{
		return {
			4.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 4.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	}

	//本机是否具备像素回读能力
	inline bool readback_supported(engine::Renderer& renderer)
	{
		return renderer.compute().readback_supported();
	}

	//按 Rec.709 求像素亮度
	inline float luma_of(const std::array<uint8_t, 4>& value)
	{
		return 0.2126f * static_cast<float>(value[0]) + 0.7152f * static_cast<float>(value[1])
			+ 0.0722f * static_cast<float>(value[2]);
	}

	//两帧回读画面的最大逐通道差
	inline int max_channel_diff(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		int maximum = 0;
		for (std::size_t index = 0; index < left.size(); ++index)
		{
			int delta = static_cast<int>(left[index]) - static_cast<int>(right[index]);
			if (delta < 0)
				delta = -delta;
			if (delta > maximum)
				maximum = delta;
		}
		return maximum;
	}

	/*
	统计「本帧比参考帧暗至少 threshold」的像素数（用于验证描边 / 阴影压暗）
	逐像素按 Rec.709 亮度差判定，通道序无关。
	*/
	inline uint32_t darkened_count(const std::vector<uint8_t>& tested, const std::vector<uint8_t>& reference, float threshold,
		uint32_t width = WINDOW_WIDTH, uint32_t height = WINDOW_HEIGHT)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				//两帧亮度差
				float difference = luma_of(pixel_get(reference, x, y, width)) - luma_of(pixel_get(tested, x, y, width));
				if (difference >= threshold)
					++count;
			}
		}
		return count;
	}

	//统计「本帧比参考帧亮至少 threshold」的像素数（用于验证泛光扩散范围）
	inline uint32_t brightened_count(const std::vector<uint8_t>& tested, const std::vector<uint8_t>& reference, float threshold,
		uint32_t width = WINDOW_WIDTH, uint32_t height = WINDOW_HEIGHT)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				//两帧亮度差
				float difference = luma_of(pixel_get(tested, x, y, width)) - luma_of(pixel_get(reference, x, y, width));
				if (difference >= threshold)
					++count;
			}
		}
		return count;
	}

	//逐通道差达到 threshold 的像素数（结构化整帧差异）
	inline uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, int threshold)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				if (delta < 0)
					delta = -delta;
				if (maximum < delta)
					maximum = delta;
			}
			if (maximum >= threshold)
				++count;
		}
		return count;
	}
}

/*
回读能力门控（只能在用例体内展开）
用法：BYJY_REQUIRE_READBACK(renderer);
*/
#define BYJY_REQUIRE_READBACK(renderer) \
	if (!(renderer).compute().readback_supported()) \
		GTEST_SKIP() << "本机交换链图像不具备传输源用法"
