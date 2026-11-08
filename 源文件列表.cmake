#
# =============================================================================
# Engine/EngineFoundation 源文件列表（生成物，勿手改）
# -----------------------------------------------------------------------------
# 生成方式
#     由 Engine/EngineFoundation/cmake/生成源文件列表.cmake 在每轮配置时自动维护：把
#     file(GLOB_RECURSE) 的扫描结果与本文件比对（按路径集合），仅在集合有差异时
#     整体写回。
#
# 为什么要这个文件
#     CMakeLists 用 GLOB 收集源文件时，构建脚本里没有任何「具体文件名」。
#     Visual Studio 的文件视图改名 / 迁移操作，靠 File API 的 backtrace 找
#     「可改写的引用」；backtrace 只落在「把源文件直接加入 target 的那条命令」
#     上。本文件用 target_sources 直接列出文件名的字面量，于是 VS 的改写就会落到
#     本文件上；而本文件被 CMakeLists include，属配置依赖，被改写即触发重新配置。
#
# 维护
#     - 不要手工编辑。新增 / 删除 / 改名文件后，下一轮配置自动刷新。
#     - 路径一律相对本层根目录（Engine/EngineFoundation），与 VS 改写后的写法一致。
#     - 这里是这些文件的「唯一」加入点：CMakeLists 的 add_library 不再传这些源文件，
#       否则 backtrace 会归到 add_library 上，VS 就又无法改写了。
# =============================================================================
#
target_sources(EngineFoundation PRIVATE
    "src/event/Event_Broker/core/事件中转器.cpp"
    "src/event/Event_Terminal/core/事件终端.cpp"
    "src/loader/assets/Gltf_Loader/core/gltf加载器.cpp"
    "src/loader/assets/Gltf_Loader/core/gltf转换.cpp"
    "src/loader/assets/Gltf_Loader/core/tinygltf实现.cpp"
    "src/loader/assets/Mesh_Loader/core/网格加载器.cpp"
    "src/loader/assets/Mtl_Loader/core/材质加载器.cpp"
    "src/loader/config/Config_Loader/core/事件处理.cpp"
    "src/loader/config/Config_Loader/core/缓存清除.cpp"
    "src/loader/config/Config_Loader/core/脏标记.cpp"
    "src/loader/config/Config_Loader/core/路径与文件.cpp"
    "src/loader/config/Config_Loader/core/路由文件操作.cpp"
    "src/loader/config/Config_Loader/core/配置加载.cpp"
    "src/log/Logging_Config_Bridge/core/日志配置桥接.cpp"
    "src/object/Object_Pool/core/对象池实例化.cpp"
    "src/spatial/partition/Quadtree/core/四叉树实例化.cpp"
    "src/spatial/partition/Quadtree_Manager/core/四叉树管理器实例化.cpp"
    "src/time/Timer/core/计时器.cpp"
)

# 头文件不参与编译，只为让 IDE 建立文件索引；同样经 target_sources 列出，
# 使头文件改名 / 迁移时 VS 也能改写引用
target_sources(EngineFoundation PRIVATE
    "common/前置头文件包含.h"
    "src/detail/class/Number_Pool/数值分配器.h"
    "src/detail/class/Random_Generator/随机数生成器.h"
    "src/detail/methods/json字段可用性校验.h"
    "src/detail/methods/package/数据校验工具.h"
    "src/detail/methods/package/文本处理工具.h"
    "src/detail/methods/package/路径操作工具.h"
    "src/detail/methods/二分查找.h"
    "src/detail/methods/哈希混合.h"
    "src/detail/methods/数值校验.h"
    "src/detail/methods/文件路径可用性校验.h"
    "src/detail/methods/文件路径获取.h"
    "src/detail/methods/文件路径规范化.h"
    "src/detail/methods/文本行解析.h"
    "src/detail/methods/标准库format特化.h"
    "src/detail/methods/路径字符串转换.h"
    "src/event/Common/事件协议.h"
    "src/event/Event/事件.h"
    "src/event/Event_Broker/事件中转器.h"
    "src/event/Event_Broker/局部命名空间使用.h"
    "src/event/Event_Terminal/事件终端.h"
    "src/event/Event_Terminal/局部命名空间使用.h"
    "src/event/Event_Terminal/终端端口.h"
    "src/event/事件系统运行包.h"
    "src/loader/assets/Gltf_Loader/core/gltf转换.h"
    "src/loader/assets/Gltf_Loader/core/gltf配置.h"
    "src/loader/assets/Gltf_Loader/core/tinygltf包含.h"
    "src/loader/assets/Gltf_Loader/gltf加载器.h"
    "src/loader/assets/Gltf_Loader/局部命名空间使用.h"
    "src/loader/assets/Mesh_Loader/局部命名空间使用.h"
    "src/loader/assets/Mesh_Loader/网格加载器.h"
    "src/loader/assets/Mtl_Loader/局部命名空间使用.h"
    "src/loader/assets/Mtl_Loader/材质加载器.h"
    "src/loader/config/Config_Loader/Common/配置事件.h"
    "src/loader/config/Config_Loader/局部命名空间使用.h"
    "src/loader/config/Config_Loader/配置加载器.h"
    "src/log/Log/日志系统.h"
    "src/log/Logging_Config_Bridge/Common/日志事件.h"
    "src/log/Logging_Config_Bridge/局部命名空间使用.h"
    "src/log/Logging_Config_Bridge/日志配置桥接.h"
    "src/log/Node_Cache/节点缓存.h"
    "src/log/Rule/日志规则.h"
    "src/log/Stream_Sink/输出目标.h"
    "src/log/Stream_Tree/流节点树.h"
    "src/log/日志系统运行包.h"
    "src/object/Object/对象.h"
    "src/object/Object_Pool/对象池.h"
    "src/object/对象系统运行包.h"
    "src/spatial/math/坐标类型.h"
    "src/spatial/partition/Quadtree/core/区块信息检索.hpp"
    "src/spatial/partition/Quadtree/core/导航与范围计算.hpp"
    "src/spatial/partition/Quadtree/core/节点创建与树扩大.hpp"
    "src/spatial/partition/Quadtree/core/设置与交互.hpp"
    "src/spatial/partition/Quadtree/函数预声明.h"
    "src/spatial/partition/Quadtree/四叉树.h"
    "src/spatial/partition/Quadtree/数据结构.h"
    "src/spatial/partition/Quadtree_Manager/core/区块信息检索.hpp"
    "src/spatial/partition/Quadtree_Manager/core/四叉树合并.hpp"
    "src/spatial/partition/Quadtree_Manager/core/四叉树扩大回调管理.hpp"
    "src/spatial/partition/Quadtree_Manager/core/四叉树智能创建.hpp"
    "src/spatial/partition/Quadtree_Manager/core/基础操作.hpp"
    "src/spatial/partition/Quadtree_Manager/core/相邻四叉树查找.hpp"
    "src/spatial/partition/Quadtree_Manager/core/设置与交互.hpp"
    "src/spatial/partition/Quadtree_Manager/函数预声明.h"
    "src/spatial/partition/Quadtree_Manager/四叉树管理器.h"
    "src/spatial/partition/Quadtree_Manager/数据结构.h"
    "src/time/Timer/局部命名空间使用.h"
    "src/time/Timer/计时器.h"
)
