#pragma once
#include <gtest/gtest.h>


#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "Engine/EngineRenderer/src/render/Particle/粒子宿主.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		return byjy_render_test::hidden_config(64, 36);
	}

	//点发射器（数量 32、寿命 0.5..1.0）
	engine::Particle_Emitter point_emitter(void)
	{
		engine::Particle_Emitter emitter{};
		emitter.position[0] = 0.0f;
		emitter.position[1] = 1.0f;
		emitter.position[2] = 0.0f;
		emitter.direction[1] = 1.0f;
		emitter.speed_min = 1.0f;
		emitter.speed_max = 2.0f;
		emitter.lifetime_min = 0.5f;
		emitter.lifetime_max = 1.0f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
		emitter.count = 32;
		return emitter;
	}
}

//未初始化：路径不可用、开关与容量设置被拒
