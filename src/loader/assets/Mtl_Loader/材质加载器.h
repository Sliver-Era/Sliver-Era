#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

/*
MTL 材质库类型与解析器（2026-11-07 自渲染层资产加载下沉本层）
本头 Vulkan-free：只把 .mtl 文本解析为纯字段数据，贴图路径原样保留、不做解码；
渲染层仍负责「字段换算 ＋ 贴图解码 ＋ 材质创建」。
与 Mesh_Loader 平级且互不依赖：MTL 可脱离 OBJ 独立使用（场景清单即显式声明材质库路径）。
*/
namespace engine
{
    /*
    MTL 材质项
    只承载 OBJ 材质库里的原始字段，不直接构造渲染层材质数据。
    */
    struct Mtl_Material
    {
        //材质名（与 OBJ 的 usemtl 对应）
        std::string name;
        //漫反射颜色（Kd；缺省白）
        float diffuse[3] = { 1.0f, 1.0f, 1.0f };
        //镜面颜色（Ks；缺省黑）
        float specular[3] = { 0.0f, 0.0f, 0.0f };
        //自发光颜色（Ke；缺省黑）
        float emissive[3] = { 0.0f, 0.0f, 0.0f };
        //镜面指数（Ns；0..1000，缺省 0）
        float shininess = 0.0f;
        //不透明度（d；缺省 1；Tr 按 1 − Tr 换算）
        float opacity = 1.0f;
        //漫反射贴图（map_Kd；相对 MTL 所在目录，空串表示无）
        std::string diffuse_map;
        //法线 / 凹凸贴图（map_Bump / bump / norm；同上）
        std::string normal_map;
    };

    /*
    材质库加载器（MTL 解析）
    load_file —— 读取整份材质库；失败（路径不可读 / 超限 / 无任何 newmtl）返回假并清空输出。
    支持 newmtl / Kd / Ks / Ke / Ns / d / Tr / map_Kd / map_Bump（bump / norm 视同）；
    其余行（Ka / Ni / illum / 注释等）忽略；贴图路径原样保留，不解析相对目录。
    */
    class Mtl_Loader
    {
    public:
        //加载 MTL 材质库文件
        static bool load_file(const std::string& mtl_path, std::vector<Mtl_Material>& receiver);
    };
}
