#pragma once
#include <gtest/gtest.h>


//获取渲染器门面与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		return byjy_render_test::hidden_config(320, 180);
	}

	//构造三角形网格（三顶点、三索引）
	inline engine::Mesh_Data triangle_data(void)
	{
		return byjy_render_test::triangle_data();
	}
}

//网格测试夹具
class Renderer_Mesh_Test : public ::testing::Test
{
};

//纹理测试夹具
class Renderer_Texture_Test : public ::testing::Test
{
};
