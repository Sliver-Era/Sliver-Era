#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

/*
tinygltf 配置与引入（模块私有）
禁用 tinygltf 自带的 json 与图像 I/O：json 复用本层 external/Json，图像解码归渲染层。
唯一实现单元为 `tinygltf实现.cpp`；其余编译单元只拿声明。
*/
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE

//复用本层 json
#include "external/Json/nlohmann/json.hpp"

//第三方头自身的告警不属于本层代码问题
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4245 4267 4456 4457 4996)
#endif
#include "external/tinygltf/tiny_gltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
