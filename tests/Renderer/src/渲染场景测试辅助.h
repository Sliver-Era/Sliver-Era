#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
/*
渲染场景测试辅助（2026-11-17 测试层改造 A：公共部分迁入 测试辅助/渲染测试辅助.h）
本文件只保留场景套件的夹具；窗口配置、像素读取、几何 / 纹理 / 矩阵构造经 using 引入，
不再按文件复制实现。
*/
#include <gtest/gtest.h>

//获取渲染器门面（含场景描述）与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
//公共测试辅助
#include "测试辅助/渲染测试辅助.h"
//契约常量（实现常量的唯一来源）
#include "测试辅助/渲染契约常量.h"

namespace
{
	using byjy_render_test::WINDOW_WIDTH;
	using byjy_render_test::WINDOW_HEIGHT;
	using byjy_render_test::hidden_config;
	using byjy_render_test::pixel_bytes;
	using byjy_render_test::pixel_get;
	using byjy_render_test::pixel_text;
	using byjy_render_test::outer_max;
	using byjy_render_test::center_brightness;
	using byjy_render_test::quad_data;
	using byjy_render_test::quad_data_with_normal;
	using byjy_render_test::triangle_data;
	using byjy_render_test::cube_data;
	using byjy_render_test::solid_red;
	using byjy_render_test::solid_white;
	using byjy_render_test::solid_gray;
	using byjy_render_test::rotation_y_matrix;
	using byjy_render_test::full_screen_matrix;
	using byjy_render_test::SCENE_UBO_BYTES;
	using byjy_render_test::INSTANCE_STRIDE_BYTES;
	using byjy_render_test::DEFAULT_TEXTURE_COUNT;
	using byjy_render_test::DEFAULT_PIPELINE_COUNT;
	using byjy_render_test::DEFAULT_RENDER_TARGET_COUNT;
	using byjy_render_test::FORWARD_LIGHT_SLOTS;
}

//场景数据测试夹具
class Renderer_Scene_Test : public ::testing::Test
{
};
