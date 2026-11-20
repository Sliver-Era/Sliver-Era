#pragma once
#include <gtest/gtest.h>


//获取渲染器门面与包围体构造
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "Engine/EngineRenderer/src/render/GPU驱动/包围体.h"
//视锥平面与 CPU 参考判定（第二百四十八轮）
#include "Engine/EngineRenderer/src/render/GPU驱动/剔除数据.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//隐藏窗口尺寸（功能用例）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//性能基线场景参数（方向一 §目标场景）
	constexpr uint32_t BASELINE_WIDTH = 1920;
	constexpr uint32_t BASELINE_HEIGHT = 1080;
	constexpr uint32_t BASELINE_INSTANCES = 10000;
	constexpr uint32_t BASELINE_FRAMES = 6;
	//实例横向铺开边界（部分实例落在视锥外）
	constexpr float BASELINE_SPAN = 40.0f;
	//小四边形半边长
	constexpr float QUAD_HALF = 0.25f;

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		return byjy_render_test::hidden_config(WINDOW_WIDTH, WINDOW_HEIGHT);
	}

	//构造中心小四边形（两个三角形）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -QUAD_HALF, -QUAD_HALF, 0.0f }, { 0.0f, 0.0f } },
			{ { QUAD_HALF, -QUAD_HALF, 0.0f }, { 1.0f, 0.0f } },
			{ { QUAD_HALF, QUAD_HALF, 0.0f }, { 1.0f, 1.0f } },
			{ { -QUAD_HALF, QUAD_HALF, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//单位模型矩阵（列主序）
	std::array<float, 16> identity_matrix(void)
	{
		return std::array<float, 16>{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	//绕 Y 轴 90 度旋转（列主序：x' = z，z' = −x）
	std::array<float, 16> rotate_y_90_matrix(void)
	{
		return std::array<float, 16>{
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	//变换一个局部点（列主序矩阵）
	std::array<float, 3> transform_point(const float model[16], float x, float y, float z)
	{
		return std::array<float, 3>{
			model[0] * x + model[4] * y + model[8] * z + model[12],
			model[1] * x + model[5] * y + model[9] * z + model[13],
			model[2] * x + model[6] * y + model[10] * z + model[14],
		};
	}

	//构造大量实例矩阵（横向铺开，部分在视锥外）
	std::vector<std::array<float, 16>> baseline_matrices(uint32_t count)
	{
		std::vector<std::array<float, 16>> matrices(count);
		for (uint32_t index = 0; index < count; ++index)
		{
			const float ratio = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.5f;
			std::array<float, 16> matrix = identity_matrix();
			matrix[12] = -BASELINE_SPAN + 2.0f * BASELINE_SPAN * ratio;
			matrix[13] = 0.0f;
			matrix[14] = 0.0f;
			matrices[index] = matrix;
		}
		return matrices;
	}
}

//包围体测试夹具
class Renderer_GPU_Driven_Test : public ::testing::Test
{
};
