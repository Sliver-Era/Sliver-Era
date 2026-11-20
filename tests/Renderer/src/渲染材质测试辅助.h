#pragma once
#include <gtest/gtest.h>

//非数参数构造（第二百三十八轮）

//获取渲染器门面（含材质 / 混合档枚举）与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		return byjy_render_test::hidden_config(WINDOW_WIDTH, WINDOW_HEIGHT);
	}

	//本帧像素字节数（RGBA8 紧排）
	inline uint32_t pixel_bytes(void)
	{
		return byjy_render_test::pixel_bytes(WINDOW_WIDTH, WINDOW_HEIGHT);
	}

	//取指定像素的四个字节
	inline std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		return byjy_render_test::pixel_get(pixels, x, y, WINDOW_WIDTH);
	}

	//失败诊断文本
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//外通道是否「恰有一个饱和」（R 与 B 通道；避开 std::max —— 本层包含链有 min/max 宏）
	bool outer_saturated(const std::array<uint8_t, 4>& value)
	{
		//R / B 通道是否各自饱和
		bool red_high = value[0] >= 200;
		bool blue_high = value[2] >= 200;
		//恰有一个饱和即为真
		return red_high != blue_high;
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	inline engine::Mesh_Data quad_data(void)
	{
		return byjy_render_test::quad_data();
	}

	//构造纯色纹理（RGBA8：rgb = 给定色，alpha = 255）
	std::vector<uint8_t> solid_color(uint8_t r, uint8_t g, uint8_t b)
	{
		std::vector<uint8_t> pixels(4 * 4 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = r;
			pixels[index + 1] = g;
			pixels[index + 2] = b;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造「铺满全屏」模型矩阵（列主序；放大 4 倍越过 NDC 边界被裁剪）
	std::array<float, 16> full_screen_matrix(void)
	{
		return {
			4.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 4.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	}
}

//渲染材质测试夹具
class Renderer_Material_Test : public ::testing::Test
{
};
