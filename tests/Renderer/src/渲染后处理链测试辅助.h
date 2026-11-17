#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
/*
渲染后处理链测试辅助（2026-11-17 测试层改造 A：公共部分迁入 测试辅助/渲染测试辅助.h）
本文件只保留后处理链特有的常量、场景登记与管线登记；窗口配置、像素读取、几何构造、
亮度 / 差异统计经 using 引入，不再按文件复制实现。
*/
#include <gtest/gtest.h>

//获取渲染器门面（含管线登记与后处理链接口）与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
//公共测试辅助
#include "测试辅助/渲染测试辅助.h"

namespace
{
	using byjy_render_test::WINDOW_WIDTH;
	using byjy_render_test::WINDOW_HEIGHT;
	using byjy_render_test::hidden_config;
	using byjy_render_test::msaa_config;
	using byjy_render_test::pixel_bytes;
	using byjy_render_test::pixel_get;
	using byjy_render_test::pixel_text;
	using byjy_render_test::quad_data;
	using byjy_render_test::max_channel_diff;
	using byjy_render_test::luma_of;
	using byjy_render_test::darkened_count;
	using byjy_render_test::brightened_count;

	//系统字体路径（存在才跑文字兼容用例；字号 16）
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;

	/*
	两级链画面与参考画面的最大允许逐通道差
	「反色 → 反色」的每一级都要经历一次「附件编码 → 采样解码」的取整，两级累积故容差取 4；
	若只执行了一级则画面整体反色（中心与角落的通道差均达 200 以上），故该容差仍具决定性。
	*/
	constexpr int CHAIN_TOLERANCE = 4;
	//「纯黑 / 纯白」判定的通道阈值（阈值化后应饱和到 0 / 255）
	constexpr int SATURATION_THRESHOLD = 250;
	//「近黑」判定的通道上限
	constexpr int DARK_THRESHOLD = 4;

	//登记「白纹理四边形」场景（供各路径复用，保证输入一致）
	inline void scene_register(engine::Renderer& renderer, engine::Mesh_Handle& handle_out)
	{
		//回落内置默认白纹理
		renderer.resource().texture_bind_set(0);
		//加载四边形并登记绘制
		ASSERT_TRUE(renderer.resource().mesh_load(handle_out, quad_data()));
		ASSERT_TRUE(renderer.resource().mesh_draw(handle_out, nullptr));
	}

	//登记一条后处理管线（顶点复用 后处理.vert，片元取给定文件名）
	inline uint64_t post_pipeline_register(engine::Renderer& renderer, const std::string& frag_shader)
	{
		uint64_t handle = 0;
		EXPECT_TRUE(renderer.resource().pipeline_load(handle, "后处理.vert.spv", frag_shader));
		return handle;
	}
}

//后处理链测试夹具
class Renderer_Post_Chain_Test : public ::testing::Test
{
};
