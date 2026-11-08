#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//网格数据类型（Vertex / Mesh_Data）
#include "Engine/EngineFoundation/src/loader/assets/Mesh_Loader/网格加载器.h"

/*
glTF 静态子集类型与解析器（2026-11-07 新增）
本头 Vulkan-free：只把 .gltf / .glb 解析为引擎自有的纯 CPU 数据；
不创建 GPU 资源、不解码贴图，TANGENT / 动画 / 蒙皮 / 形态目标均不参与解析。

与 Mesh_Loader、Mtl_Loader 平级且互不依赖：
  Mesh_Loader 负责 OBJ 网格，Mtl_Loader 负责 MTL 材质库，
  本模块负责 glTF 的节点层级、网格 primitive 与 PBR 材质描述。
*/
namespace engine
{
    /*
    glTF 节点（静态子集）
    局部变换统一以列主序 4x4 矩阵表达：节点写 matrix 时原样保留，
    写 TRS 时由解析器合成为矩阵；parent 为 -1 表示场景根节点。
    */
    struct Gltf_Node
    {
        //节点名
        std::string name;
        //父节点下标（-1 表示根）
        int parent = -1;
        //子节点下标
        std::vector<int> children;
        //局部变换矩阵（列主序）
        float matrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f };
        //引用的网格下标（-1 表示无）
        int mesh = -1;
    };

    /*
    glTF 网格图元
    只承载 TRIANGLES 模式的顶点与索引；material 为材质下标（-1 表示无）。
    */
    struct Gltf_Primitive
    {
        //顶点与索引（复用引擎唯一网格类型）
        Mesh_Data data;
        //材质下标（-1 表示无）
        int material = -1;
    };

    //glTF 网格
    struct Gltf_Mesh
    {
        //网格名
        std::string name;
        //图元集合（非 TRIANGLES 图元在解析时被跳过）
        std::vector<Gltf_Primitive> primitives;
    };

    /*
    glTF PBR 材质（静态子集）
    贴图字段保存的是 texture 下标（-1 表示无）；texCoord 只支持 0 号 UV 集，
    故不单独记录 texCoord。
    */
    struct Gltf_Material
    {
        //材质名
        std::string name;
        //基础颜色（baseColorFactor；RGBA）
        float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        //金属度（metallicFactor；缺省 1）
        float metallic = 1.0f;
        //粗糙度（roughnessFactor；缺省 1）
        float roughness = 1.0f;
        //自发光颜色（emissiveFactor）
        float emissive[3] = { 0.0f, 0.0f, 0.0f };
        //透明模式（0 不透明 / 1 遮罩 / 2 混合）
        int alpha_mode = 0;
        //遮罩阈值（alphaCutoff；仅 alpha_mode = 1 时有效）
        float alpha_cutoff = 0.5f;
        //是否双面
        bool double_sided = false;
        //基础颜色贴图下标（-1 表示无）
        int base_color_texture = -1;
        //金属度 / 粗糙度贴图下标（-1 表示无）
        int metallic_roughness_texture = -1;
        //法线贴图下标（-1 表示无）
        int normal_texture = -1;
        //法线贴图强度（normalTexture.scale）
        float normal_scale = 1.0f;
        //环境光遮蔽贴图下标（-1 表示无）
        int occlusion_texture = -1;
        //环境光遮蔽强度（occlusionTexture.strength）
        float occlusion_strength = 1.0f;
        //自发光贴图下标（-1 表示无）
        int emissive_texture = -1;
    };

    /*
    glTF 纹理
    source 为图像下标（-1 表示无），sampler 为采样器下标（-1 表示默认）。
    */
    struct Gltf_Texture
    {
        //图像下标
        int source = -1;
        //采样器下标
        int sampler = -1;
    };

    /*
    glTF 图像引用
    外部图像只记录 uri；内嵌图像只记录 bufferView 与 mimeType。
    图像字节不解码，由调用方按需交给渲染层。
    */
    struct Gltf_Image
    {
        //图像名
        std::string name;
        //外部相对路径（空串表示内嵌）
        std::string uri;
        //MIME 类型（内嵌时必填）
        std::string mime_type;
        //内嵌图像所在 bufferView 下标（-1 表示外部）
        int buffer_view = -1;
    };

    /*
    glTF 采样器
    保留 glTF 原始枚举值；-1 表示文件未指定（过滤器），wrap 缺省 10497 = REPEAT。
    */
    struct Gltf_Sampler
    {
        //放大过滤
        int mag_filter = -1;
        //缩小过滤
        int min_filter = -1;
        //S 轴环绕
        int wrap_s = 10497;
        //T 轴环绕
        int wrap_t = 10497;
    };

    //glTF 场景（根节点集合）
    struct Gltf_Scene
    {
        //场景名
        std::string name;
        //根节点下标
        std::vector<int> nodes;
    };

    /*
    glTF 资产（解析结果）
    失败时整体清空；节点矩阵为局部变换，世界变换由调用方按 parent 链自行累乘。
    */
    struct Gltf_Asset
    {
        //节点集合
        std::vector<Gltf_Node> nodes;
        //网格集合
        std::vector<Gltf_Mesh> meshes;
        //材质集合
        std::vector<Gltf_Material> materials;
        //纹理集合
        std::vector<Gltf_Texture> textures;
        //图像引用集合
        std::vector<Gltf_Image> images;
        //采样器集合
        std::vector<Gltf_Sampler> samplers;
        //场景集合
        std::vector<Gltf_Scene> scenes;
        //默认场景下标（-1 表示未指定）
        int default_scene = -1;
    };

    /*
    glTF 加载器
    load_file —— 按文件魔数分流 .gltf / .glb，解析为 Gltf_Asset；
    失败（路径不可读 / 体量超限 / JSON 非法 / accessor 越界）返回假并清空输出。
    */
    class Gltf_Loader
    {
    public:
        //加载 glTF 文件（静态子集）
        static bool load_file(const std::string& gltf_path, Gltf_Asset& receiver);
    };
}
