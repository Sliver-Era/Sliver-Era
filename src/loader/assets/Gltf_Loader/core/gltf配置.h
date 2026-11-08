#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//glTF 解析限制（模块私有；与 Mesh_Loader 同口径）
namespace engine
{
    //模块内部实现
    namespace gltf_detail
    {
        //glTF 文件体量上限（64 MiB）
        constexpr uint64_t max_file_bytes = 64ull * 1024 * 1024;
        //单图元顶点上限
        constexpr uint32_t max_attributes = 4000000u;
        //单图元索引上限
        constexpr uint32_t max_indices = 12000000u;
        //节点 / 网格 / 材质 / 纹理 / 图像 / 采样器数量上限
        constexpr uint32_t max_objects = 65536u;
    }
}
