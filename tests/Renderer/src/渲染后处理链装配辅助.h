#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
/*
渲染后处理链装配辅助（2026-11-21 测试用例重构）
集中跨文件复用的两组用例装配：
1. Explicit_Bloom_Chain / explicit_bloom_chain_build / explicit_bloom_chain_destroy —— 显式泛光链；
2. Intermediate_Tone_Spans / run_intermediate_tone_case —— 单源调色中间级 → 末级逐材质色调。
*/
#include <vulkan/vulkan.h>

#include "渲染后处理链测试辅助.h"
//场景几何与纹理构造（强光照压力场景用例）
#include "渲染场景测试辅助.h"

namespace
{
	//显式泛光链装配件（第 778 轮）：16F 场景目标 ＋ 场景管线 ＋ 泛光预设 ＋ 末级色调映射管线
	struct Explicit_Bloom_Chain
	{
		uint64_t scene_target = 0;
		uint64_t scene_pipeline = 0;
		uint64_t tone_pipeline = 0;
		engine::Bloom_Preset preset;
	};

	//按描述建立一套显式泛光链（场景目标与场景管线恒为 16F；调用方负责最后释放）
	//samples = 场景目标采样数；tone_operator 决定末级色调映射片元（0 ACES / 1 胶片 / 2 指数 / 3 扩展 Reinhard / 4 AgX）
	bool explicit_bloom_chain_build(engine::Renderer& renderer, Explicit_Bloom_Chain& chain,
		const engine::Bloom_Preset_Desc& desc, uint32_t samples = 1, uint32_t tone_operator = 0)
	{
		const uint32_t width = renderer.device().extent_width();
		const uint32_t height = renderer.device().extent_height();
		const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
		const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
		if (!renderer.resource().render_target_create_format(chain.scene_target, width, height,
			hdr_format, depth_format, samples))
			return false;
		if (!renderer.resource().pipeline_load_ex(chain.scene_pipeline, "场景.vert.spv", "场景.frag.spv",
			engine::Blend_Mode::Opaque, false, false, true, false, true, true, depth_format, samples, hdr_format))
			return false;
		//末级色调映射片元（与默认路径的算子选择口径一致）
		const char* tone_fragments[5] = { "色调映射.frag.spv", "胶片色调映射.frag.spv", "指数色调映射.frag.spv",
			"Reinhard扩展色调映射.frag.spv", "AgX色调映射.frag.spv" };
		const char* tone_fragment = tone_operator < 5 ? tone_fragments[tone_operator] : tone_fragments[0];
		if (!renderer.resource().pipeline_load(chain.tone_pipeline, "后处理.vert.spv", tone_fragment))
			return false;
		if (!renderer.device().post_bloom_preset_build(chain.preset, desc))
			return false;
		//订阅四级链（提取 → 模糊 → 叠加 → 色调映射）与「场景 ＋ 三个中间目标」
		uint64_t stages[4] = { chain.preset.pipelines[0], chain.preset.pipelines[1], chain.preset.pipelines[2],
			chain.tone_pipeline };
		if (!renderer.device().post_chain_set(stages, 4))
			return false;
		uint64_t targets[4] = { chain.scene_target, chain.preset.targets[0], chain.preset.targets[1],
			chain.preset.targets[2] };
		if (!renderer.device().render_target_present_chain_set(targets, 4))
			return false;
		return renderer.device().post_bloom_preset_params(chain.preset, desc);
	}

	//释放显式泛光链（先摘链、再释放资源）
	void explicit_bloom_chain_destroy(engine::Renderer& renderer, Explicit_Bloom_Chain& chain)
	{
		renderer.device().post_chain_set(nullptr, 0);
		renderer.device().render_target_present_chain_set(nullptr, 0);
		renderer.device().post_bloom_preset_release(chain.preset);
		if (chain.tone_pipeline != 0)
			renderer.resource().pipeline_unload(chain.tone_pipeline);
		if (chain.scene_pipeline != 0)
			renderer.resource().pipeline_unload(chain.scene_pipeline);
		if (chain.scene_target != 0)
			renderer.resource().render_target_destroy(chain.scene_target);
		chain.tone_pipeline = 0;
		chain.scene_pipeline = 0;
		chain.scene_target = 0;
	}
}

namespace
{
	//「单源调色中间级 → 末级逐材质色调」用例的左右半通道差
	struct Intermediate_Tone_Spans
	{
		//左半（AgX 材质）最大通道差
		int left_span = 0;
		//右半（ACES 材质）最大通道差
		int right_span = 0;
		//本帧实际执行的后处理级数
		uint32_t stages = 0;
	};

	/*
	跑两级链「单源调色中间级 → 材质色调映射」（第 791 轮抽取）
	左材质 AgX、右材质 ACES，纯蓝纹理；中间级参数按需置恒等（分级调色用）。
	断言失败以 ADD_FAILURE 记录并提前返回（用例侧据 spans 判定）。
	*/
	Intermediate_Tone_Spans run_intermediate_tone_case(const char* middle_fragment, bool identity_grade)
	{
		//用例结果（未填即视为失败）
		Intermediate_Tone_Spans result;
		//纯蓝纹理（2×2）
		std::vector<uint8_t> texels(2 * 2 * 4, 0);
		for (std::size_t index = 0; index < texels.size(); index += 4)
		{
			texels[index + 0] = 255;
			texels[index + 3] = 255;
		}
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.vsync = false;
		if (!renderer.init(config))
		{
			ADD_FAILURE() << "渲染器初始化失败";
			return result;
		}
		renderer.resource().texture_bind_set(0);
		if (!renderer.scene().exposure_set(2.0f))
			ADD_FAILURE() << "曝光设置失败";
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.scene().light_set(direction, color, 1.0f, 0.0f);
		engine::Mesh_Handle quad = 0;
		if (!renderer.resource().mesh_load(quad, quad_data()))
		{
			ADD_FAILURE() << "四边形加载失败";
			return result;
		}
		const uint32_t width = renderer.device().extent_width();
		const uint32_t height = renderer.device().extent_height();
		const uint32_t hdr_format = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
		const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
		uint64_t scene_target = 0;
		uint64_t mid_target = 0;
		uint64_t scene_pipeline = 0;
		//场景目标与场景管线（16F 附件：alpha 承载逐材质色调编号）
		if (!renderer.resource().render_target_create_format(scene_target, width, height,
			hdr_format, depth_format, 1)
			|| !renderer.resource().render_target_create_format(mid_target, width, height,
				hdr_format, depth_format, 1)
			|| !renderer.resource().scene_pipeline_variant_load(scene_pipeline, hdr_format, 1, depth_format))
		{
			ADD_FAILURE() << "场景目标 / 管线创建失败";
			return result;
		}
		/*
		中间级管线按 16F 目标格式登记（第 829 轮）
		中间级写入 mid_target（16F），若沿用交换链格式烘焙会触发动态渲染格式不一致（VUID-08910）。
		*/
		uint64_t middle_pipeline = 0;
		if (!renderer.resource().pipeline_load_ex(middle_pipeline, "后处理.vert.spv", middle_fragment,
			engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, hdr_format))
		{
			ADD_FAILURE() << "中间级管线登记失败：" << middle_fragment;
			return result;
		}
		uint64_t tone_pipeline = post_pipeline_register(renderer, "材质色调映射.frag.spv");
		//中间级与末级管线均须登记成功
		if (middle_pipeline == 0 || tone_pipeline == 0)
		{
			ADD_FAILURE() << "中间级 / 末级管线登记失败：" << middle_fragment;
			return result;
		}
		uint64_t stages[2] = { middle_pipeline, tone_pipeline };
		uint64_t targets[2] = { scene_target, mid_target };
		//两级链 ＋ 成对目标（场景 → 中间 → 交换链）
		if (!renderer.device().post_chain_set(stages, 2)
			|| !renderer.device().render_target_present_chain_set(targets, 2))
		{
			ADD_FAILURE() << "两级链装配失败";
			return result;
		}
		//分级调色用恒等参数（lift 0 / gamma 1 / gain 1 / saturation 1）
		if (identity_grade)
			renderer.device().post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
		//末级对未覆盖像素回落 ACES（0）
		renderer.device().post_stage_tone_operator_set(1, 0);
		uint64_t texture = 0;
		if (!renderer.resource().texture_load(texture, texels.data(), 2, 2))
		{
			ADD_FAILURE() << "纹理加载失败";
			return result;
		}
		engine::Material_Data base;
		base.texture = texture;
		//场景管线变体：材质引用它后仍可携带逐材质色调编号
		base.pipeline = scene_pipeline;
		base.pbr_override = true;
		base.pbr_roughness = 0.4f;
		base.pbr_metallic = 0.0f;
		engine::Material_Data agx_data = base;
		agx_data.tone_operator = 4;
		engine::Material_Data aces_data = base;
		aces_data.tone_operator = 0;
		engine::Material_Handle agx_handle = 0;
		engine::Material_Handle aces_handle = 0;
		if (!renderer.resource().material_load(agx_handle, agx_data)
			|| !renderer.resource().material_load(aces_handle, aces_data))
		{
			ADD_FAILURE() << "材质加载失败";
			return result;
		}
		//左球 / 右球各平移 1.1（模型矩阵第 12 列为 x 平移）
		std::array<float, 16> left_matrix = {};
		left_matrix[0] = 1.0f; left_matrix[5] = 1.0f; left_matrix[10] = 1.0f; left_matrix[12] = -1.1f; left_matrix[15] = 1.0f;
		std::array<float, 16> right_matrix = {};
		right_matrix[0] = 1.0f; right_matrix[5] = 1.0f; right_matrix[10] = 1.0f; right_matrix[12] = 1.1f; right_matrix[15] = 1.0f;
		renderer.resource().pipeline_bind_set(scene_pipeline);
		renderer.resource().material_bind_set(agx_handle);
		if (!renderer.resource().mesh_draw(quad, left_matrix.data()))
			ADD_FAILURE() << "左半绘制失败";
		renderer.resource().material_bind_set(aces_handle);
		if (!renderer.resource().mesh_draw(quad, right_matrix.data()))
			ADD_FAILURE() << "右半绘制失败";
		std::vector<uint8_t> pixels(pixel_bytes());
		if (!renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
		{
			ADD_FAILURE() << "出帧失败";
			return result;
		}
		result.stages = renderer.frame_stats_get().post_stages;
		//按左右半统计最大通道差（AgX 会把纯色压灰，ACES 保持饱和）
		auto half_span = [](const std::vector<uint8_t>& image, uint32_t half) -> int
		{
			int span = 0;
			uint32_t begin = half == 0 ? 0 : WINDOW_WIDTH / 2;
			uint32_t end = half == 0 ? WINDOW_WIDTH / 2 : WINDOW_WIDTH;
			for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			{
				for (uint32_t x = begin; x < end; ++x)
				{
					std::array<uint8_t, 4> pixel = pixel_get(image, x, y);
					int high = static_cast<int>(*std::max_element(pixel.begin(), pixel.begin() + 3));
					int low = static_cast<int>(*std::min_element(pixel.begin(), pixel.begin() + 3));
					if (high - low > span)
						span = high - low;
				}
			}
			return span;
		};
		result.left_span = half_span(pixels, 0);
		result.right_span = half_span(pixels, 1);
		//拆链与释放（顺序：链 → 材质 → 纹理 → 管线 → 目标 → 网格）
		renderer.device().post_chain_set(nullptr, 0);
		renderer.device().render_target_present_chain_set(nullptr, 0);
		renderer.resource().material_unload(aces_handle);
		renderer.resource().material_unload(agx_handle);
		renderer.resource().texture_unload(texture);
		renderer.resource().pipeline_unload(tone_pipeline);
		renderer.resource().pipeline_unload(middle_pipeline);
		renderer.resource().pipeline_unload(scene_pipeline);
		renderer.resource().render_target_destroy(mid_target);
		renderer.resource().render_target_destroy(scene_target);
		renderer.resource().mesh_unload(quad);
		return result;
	}
}
