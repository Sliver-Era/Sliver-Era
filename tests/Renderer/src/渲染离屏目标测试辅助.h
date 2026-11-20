#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
/*
渲染离屏目标测试辅助（2026-11-20 测试用例重构）
集中本套件的公共前提：窗口尺寸 / 配置、像素读取与诊断文本、纯绿纹理与绿判定、几何与全屏矩阵。
拆分说明：用例本体分布在「渲染离屏目标测试.cpp」（目标与回读）与「渲染离屏编号通道测试.cpp」（编号通道）。
*/
#include <gtest/gtest.h>

//获取渲染器门面与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
//公共测试辅助
#include "测试辅助/渲染测试辅助.h"

namespace
{
	using byjy_render_test::WINDOW_WIDTH;
	using byjy_render_test::WINDOW_HEIGHT;
	using byjy_render_test::hidden_config;
	using byjy_render_test::pixel_bytes;
	using byjy_render_test::pixel_get;
	using byjy_render_test::pixel_text;
	using byjy_render_test::outer_max;
	using byjy_render_test::pixel_is_green;
	using byjy_render_test::solid_green;
	using byjy_render_test::quad_data;
	using byjy_render_test::full_screen_matrix;
}

//渲染离屏目标测试夹具
class Renderer_Offscreen_Test : public ::testing::Test
{
};
