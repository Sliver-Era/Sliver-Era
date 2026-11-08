#pragma once
//本层公开数据
#include "../gltf加载器.h"
//第三方模型结构（仅声明）
#include "tinygltf包含.h"

//模块内部实现
namespace engine
{
    //模块内部实现
    namespace gltf_detail
    {
        //第三方模型 → 引擎数据（成功才写出 receiver）
        bool model_convert(const tinygltf::Model& model, Gltf_Asset& receiver);
    }
}
