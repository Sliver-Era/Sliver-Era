#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
/*
渲染契约常量（2026-11-17 测试层改造 B）
测试层对「实现常量」的唯一来源。功能用例只断言行为；新增场景字段 / 实例字段 / 默认资源时，
只需修改本文件与 渲染契约探针测试.cpp，不再逐个功能用例同步数字。
*/
#include <cstdint>

namespace byjy_render_test
{
	//场景 UBO 整块字节数（第 713 轮 984 浮点 = 3936 字节；第 718 轮追加 pbr_extra 4 浮点 = 988 浮点 / 3952 字节）
	inline constexpr uint64_t SCENE_UBO_BYTES = 3952;

	//每实例上传步长（模型矩阵 16 ＋ 材质 4 ＋ 卡通 4 ＋ 暗部色调 4 浮点 = 112 字节）
	inline constexpr uint64_t INSTANCE_STRIDE_BYTES = 112;

	//初始化后的默认资源计数（默认白纹理 ＋ 默认天空盒 = 2；默认场景 ＋ 外扩描边 ＋ 两条原生 UI 管线 = 4；方向光与聚光阴影目标 = 2）
	inline constexpr uint32_t DEFAULT_TEXTURE_COUNT = 2;
	inline constexpr uint32_t DEFAULT_PIPELINE_COUNT = 4;
	inline constexpr uint32_t DEFAULT_RENDER_TARGET_COUNT = 2;

	//前向光照槽上限（点光与聚光共用）
	inline constexpr uint32_t FORWARD_LIGHT_SLOTS = 32;
}
