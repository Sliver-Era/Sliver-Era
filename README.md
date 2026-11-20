# 白银纪元 · EngineFoundation 层（引擎核心层）

EngineFoundation 是「白银纪元」分层工程中**拓扑最底层的通用基础层**：向上提供事件总线、对象与对象池、空间分区、配置/资产加载、日志与工具集。本层**不包含任何具体游戏逻辑**，不感知上层存在，也不依赖任何窗口或图形 API。

本层是一个**独立 git 仓库**（目录内含自身 `.git`），源码通过远端 `engine` 分支发布。

> 本文档依据 2026-11-20 工作区源码重写，替代此前以 `src/core/...` 旧目录结构编写的版本。旧版内容仍可在本分支 git 历史中查阅。

---

## 一、分支与仓库信息

| 项目 | 取值 |
| --- | --- |
| 仓库地址（现行） | `https://github.com/Sliver-Era/Sliver-Era.git` |
| 旧地址 | `https://github.com/yuxingyuzhong/SilverEra.git`（GitHub 重定向到现行地址） |
| 远端名 | `sliverera` |
| 本地分支名 | `main` |
| 远端目标分支 | `engine` |
| 上游跟踪 | `sliverera/engine` |
| 推送命令 | `git push sliverera main:engine` |

同一远端还承载其余层的分支：`system`（EngineSystem）、`test`（Test）、`game`（Game）、`doc`（工作站文档）、`main`（顶层总览与聚合快照），以及 `Engine` / `EngineFoundation` / `EngineSystem` / `EngineRender` 四个聚合或历史快照分支。本层只维护 `engine` 分支。

---

## 二、本层在依赖链中的位置

```text
EngineFoundation ──▶ EngineSystem ──▶ EngineRenderer ──▶ Application/{Test, Game}
 EngineFoundation.lib   EngineSystem.lib   EngineRenderer.lib
```

| 项目 | 内容 |
| --- | --- |
| 依赖链位置 | 拓扑最底层，**无下层依赖**，不感知任何上层 |
| 产物 | `Engine/EngineFoundation/out/build/<配置>/lib/EngineFoundation.lib` |
| 对外面前缀 | `BYJY_ENGINE_FOUNDATION_*` |
| 可执行文件 | 本层不产出；全项目唯一可执行入口为 Test 层的 `EngineTests.exe` |

本层的职责边界：

- **不含**实体、属性、效应、场景、粒子等玩法或世界语义（这些位于 EngineSystem）。
- **不含**碰撞检测（碰撞体、碰撞空间、碰撞代理器已于后续版次整体迁至 `Engine/EngineSystem/src/collision/`）。
- **不含**窗口、输入采集、Vulkan、SDL3 等平台/图形内容（这些位于 EngineRenderer）。
- 只保留与具体游戏、具体平台无关的基础设施。

---

## 三、技术栈

| 项目 | 取值 |
| --- | --- |
| 语言 | C++20（`CMAKE_CXX_STANDARD 20`，`CMAKE_CXX_EXTENSIONS OFF`） |
| 命名空间 | `engine`（辅助设施位于 `engine::detail`） |
| 第三方依赖 | `external/Json`（nlohmann/json）、`external/tinygltf`；均为源码副本，随本层直接编入静态库 |
| 构建系统 | CMake ≥ 3.20（预设文件要求 ≥ 3.25）、Ninja 或 Visual Studio 多配置生成器 |
| 已验证平台 | MSVC / x64 / Windows |

---

## 四、目录结构

以本层根为基准（`out/`、`.vs/` 为构建附属物，不纳入版本控制）：

```text
Engine/EngineFoundation/
├── CMakeLists.txt
├── CMakePresets.json              # x64-Debug / x64-Release（Ninja 双预设）
├── CMakeSettings.json             # Visual Studio 打开层目录时使用
├── LICENSE.txt
├── README.md
├── .gitignore
├── 源文件列表.cmake               # 由 GLOB 生成的显式源文件清单（可改写以触发重配）
├── cmake/
│   ├── 对外接口.cmake             # 本层对外面（BYJY_ENGINE_FOUNDATION_OUT_*）
│   ├── 导出头快照.cmake           # 把本层自有头镜像到构建树，供上层同版次编译
│   ├── 生成版本锚点.cmake         # 版次锚点（头/库错配在链接期报错）
│   ├── 发布版本缓存.cmake         # 构建成功后发布「头快照＋库＋依赖记录」
│   ├── 版本工具.cmake             # 版本锁解析与全链一致性校验
│   ├── 生成源文件列表.cmake       # GLOB → 显式清单
│   ├── ar_rsp_bom.ps1             # 归档响应文件补 UTF-8 BOM（MSVC + Ninja）
│   └── link_rsp_bom.ps1           # 链接响应文件补 UTF-8 BOM
├── common/
│   └── 前置头文件包含.h           # 统一预编译头（标准库与第三方库集中引入）
├── external/
│   ├── Json/nlohmann/json.hpp
│   └── tinygltf/                  # glTF 解析（随本层编入）
└── src/
    ├── detail/                    # 细粒度工具
    │   ├── class/Number_Pool/数值分配器.h
    │   ├── class/Random_Generator/随机数生成器.h
    │   └── methods/               # 二分查找、哈希混合、路径与文本、数据校验、format 特化等
    ├── event/                     # 事件系统
    │   ├── Common/事件协议.h
    │   ├── Event/事件.h
    │   ├── Event_Broker/          # 事件中转器
    │   ├── Event_Terminal/        # 事件终端 + 终端端口
    │   └── 事件系统运行包.h
    ├── loader/                    # 加载器
    │   ├── assets/Gltf_Loader/    # glTF 静态子集
    │   ├── assets/Mesh_Loader/    # Mesh_Data + OBJ
    │   ├── assets/Mtl_Loader/     # MTL 材质库
    │   └── config/Config_Loader/  # 配置目录加载、脏标记缓存、Config/Load 事件
    ├── log/                       # 日志系统
    │   ├── Log/日志系统.h
    │   ├── Logging_Config_Bridge/
    │   ├── Node_Cache/、Rule/、Stream_Sink/、Stream_Tree/
    │   └── 日志系统运行包.h
    ├── object/                    # 对象与对象池
    │   ├── Object/对象.h
    │   ├── Object_Pool/对象池.h
    │   └── 对象系统运行包.h
    ├── spatial/                   # 空间
    │   ├── math/坐标类型.h
    │   └── partition/
    │       ├── Quadtree/          # 四叉树
    │       └── Quadtree_Manager/  # 多树组织、智能创建、合并与扩大回调
    └── time/Timer/计时器.h
```

---

## 五、核心架构与运行链路

### 5.1 事件驱动

本层所有跨模块通知统一走事件总线：

- `Event`：事件本体，含类别、名称与 JSON 载荷。
- `Event_Terminal`：事件终端，负责订阅登记、权限密钥与分发；对外通过 `终端端口.h` 暴露接口。
- `Event_Broker`：事件中转器，按协议词汇完成订阅查询与定向投递。
- `Common/事件协议.h`：集中定义订阅登记与配置投递的协议字符串，避免发送侧与接收侧各写一份而漂移。

### 5.2 层内依赖方向

```text
detail（无依赖）
   ▲
   ├── log
   ├── event ──▶ detail
   ├── object ──▶ detail（数值分配器、二分查找）
   ├── loader ──▶ event（配置/资产事件）
   ├── spatial ──▶ detail
   └── time
```

`detail` 是最底层工具集合，其余模块只向下依赖，不反向引用事件或日志实现，避免形成包含环。

### 5.3 典型链路

1. **配置加载**：`Config_Loader` 扫描可信根与路由目录，读取配置并维护脏标记缓存；变更后广播 `Config/Load` 事件，订阅者按需刷新。
2. **对象池**：`Object_Pool` 通过 `Number_Pool` 分配/回收编号，支持 `Unorder` 与 `Sorted` 两种排列；排序键投影与比较器在运行期以 `std::function` 提供。
3. **空间检索**：`Quadtree` 负责单树建树、插入、区域检索与合并；`Quadtree_Manager` 管理多棵树、按范围智能创建并处理扩大回调。
4. **资产解析**：`Mesh_Loader` / `Mtl_Loader` / `Gltf_Loader` 把 OBJ / MTL / glTF 解析为纯 CPU 数据（`Mesh_Data`、材质字段、节点层级），**不创建任何 GPU 资源**，由渲染层消费。

---

## 六、模块详解

### 6.1 `detail` —— 细粒度工具

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 数值分配器 | `src/detail/class/Number_Pool/数值分配器.h` | 整数编号分配与回收，支持 LIFO / FIFO 复用 |
| 随机数生成器 | `src/detail/class/Random_Generator/随机数生成器.h` | 基于 PCG32，按整数类型生成对应范围随机数 |
| 查找与哈希 | `src/detail/methods/二分查找.h`、`哈希混合.h` | 容器二分/区间查找；多值哈希混合 |
| 路径工具 | `路径字符串转换.h`、`文件路径获取.h`、`文件路径规范化.h`、`文件路径可用性校验.h`、`package/路径操作工具.h` | 中文路径与 UTF-8 字符串互转、绝对路径拼接、路径规范化与可用性校验 |
| 文本与校验 | `json字段可用性校验.h`、`数值校验.h`、`文本行解析.h`、`package/文本处理工具.h`、`package/数据校验工具.h` | JSON 字段校验、数值合法性校验、按行文本解析 |
| 格式化 | `标准库format特化.h` | 错误码、错误条件、文件系统路径的 `std::format` 特化 |

### 6.2 `event` —— 事件系统

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 事件 | `src/event/Event/事件.h` | `Event` 结构体：类别、名称、载荷 |
| 协议词汇 | `src/event/Common/事件协议.h` | 订阅登记、订阅者查询、配置投递的协议常量 |
| 事件终端 | `src/event/Event_Terminal/事件终端.h`、`终端端口.h` | 订阅、权限密钥、事件分发与终端接口 |
| 事件中转器 | `src/event/Event_Broker/事件中转器.h` | 全局事件路由与定向投递 |
| 运行包 | `src/event/事件系统运行包.h` | 一次性引入事件系统公共头 |

### 6.3 `loader` —— 配置与资产加载

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 配置加载器 | `src/loader/config/Config_Loader/配置加载器.h` | 可信根/路由目录扫描、脏标记缓存回写、`Config/Load` 事件广播 |
| 网格加载器 | `src/loader/assets/Mesh_Loader/网格加载器.h` | 引擎唯一的 `Mesh_Data` 定义与 OBJ 解析（顶点步长 32 字节，与渲染层着色器一致） |
| 材质加载器 | `src/loader/assets/Mtl_Loader/材质加载器.h` | MTL 文本解析为纯字段，贴图路径原样保留 |
| glTF 加载器 | `src/loader/assets/Gltf_Loader/gltf加载器.h` | glTF 静态子集（节点、primitive、PBR 材质）；不解析动画/蒙皮/形态目标 |

三个资产加载器均为 **Vulkan-free**，彼此平级且互不依赖。

### 6.4 `log` —— 日志系统

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 日志门面 | `src/log/Log/日志系统.h` | `Log` 的日志方法与流设置接口 |
| 流节点树 | `src/log/Stream_Tree/流节点树.h` | 按路径公共前缀分流 |
| 输出目标 | `src/log/Stream_Sink/输出目标.h` | 文件/控制台等输出端 |
| 日志规则 | `src/log/Rule/日志规则.h` | 根目录作用域与输出许可 |
| 节点缓存 | `src/log/Node_Cache/节点缓存.h` | 流节点缓存 |
| 配置桥接 | `src/log/Logging_Config_Bridge/日志配置桥接.h` | 把配置系统的日志设置接入日志系统 |

日志按 `logger.` 实例语义调用；调用方只需引用运行包 `src/log/日志系统运行包.h`。

### 6.5 `object` —— 对象与对象池

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 对象 | `src/object/Object/对象.h` | 仅持有 ID 的基类，无虚表、无带参构造；ID 与有效性由对象池内部读写 |
| 对象池 | `src/object/Object_Pool/对象池.h` | 运行期键投影与比较器；`Unorder` / `Sorted` 两种排列；查找与排序共用同一比较口径 |

### 6.6 `spatial` —— 空间

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 坐标类型 | `src/spatial/math/坐标类型.h` | `Point2i/2d/2l`、`Rect2i/2d/2l` 等模板类型与 ULP 精度比较 |
| 四叉树 | `src/spatial/partition/Quadtree/四叉树.h` | 单树建树、插入、区域检索、合并与范围计算 |
| 四叉树管理器 | `src/spatial/partition/Quadtree_Manager/四叉树管理器.h` | 多树组织、智能创建、相邻查找、扩大回调与合并 |

### 6.7 `time` —— 计时器

`src/time/Timer/计时器.h`：基于 `std::chrono::steady_clock` 的多任务命名计时。

---

## 七、对外接口契约

本层是拓扑最底层，没有下层可并入。`cmake/对外接口.cmake` 只描述本层自身，按约定导出六个变量（前缀 `ENGINE_FOUNDATION`）：

| 变量 | 取值 | 含义 |
| --- | --- | --- |
| `BYJY_ENGINE_FOUNDATION_OUT_LIB` | `EngineFoundation` | 本层静态库名 |
| `BYJY_ENGINE_FOUNDATION_OUT_LIBDIR` | `Engine/EngineFoundation` | 与 `OUT_LIB` 一一对应的层目录，供上层自动探测 |
| `BYJY_ENGINE_FOUNDATION_OUT_INC` | 项目根、`external/Json` | 使用本层公共头所需的包含目录 |
| `BYJY_ENGINE_FOUNDATION_OUT_DEF` | 空 | 无编译定义 |
| `BYJY_ENGINE_FOUNDATION_OUT_LINK` | 空 | 不对外传递第三方静态库 |
| `BYJY_ENGINE_FOUNDATION_OUT_SYS` | 空 | 不对外传递系统库 |

工程内部一律写**项目根相对全路径**包含：`#include "Engine/EngineFoundation/src/event/Event/事件.h"`。

上层通过 `cmake/接入下层.cmake` 的 `byjy_jieru_xiaceng()` 读取本层对外面，按 `OUT_LIB` 逐库定位**已构建好的**静态库，建立 `IMPORTED STATIC GLOBAL` 目标；任何情况下都不回退编译本层源码。

---

## 八、构建

### 8.1 前置条件

- CMake ≥ 3.20（`CMakePresets.json` 要求 ≥ 3.25）、Ninja 或 Visual Studio 多配置生成器；
- 支持 C++20 的 MSVC；
- 本层无下层依赖，可独立构建。

### 8.2 构建步骤

```powershell
cd Engine/EngineFoundation

# Debug
cmake --preset x64-Debug
cmake --build --preset build-Debug

# Release
cmake --preset x64-Release
cmake --build --preset build-Release
```

也可以直接用生成器命令：

```powershell
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-Debug
```

产物：`out/build/<配置>/lib/EngineFoundation.lib`；头快照：`out/build/<配置>/include/Engine/EngineFoundation/...`。

### 8.3 版本锁、版次锚点与头快照

- 本层构建成功后把「头快照 ＋ 静态库 ＋ 依赖版本记录」发布到 `Engine/版本缓存/EngineFoundation/x64-<配置>/<版本哈希>/`。
- 上层读取全仓共享的 `Engine/版本锁.cmake`：留空 = 跟随本层最新构建树；填哈希 = 锁定到版本缓存中的冻结副本。锁值落后于当前构建树时，配置期打印 WARNING。
- 每个构建产出都带**版次锚点**符号：头与库不同版次时，链接期直接报错，避免「新头配旧库」。
- 上层把本层头快照根排在项目根之前编译，确保引用的头与所链接的库同版次。

### 8.4 本机环境注意事项

1. 配置期必须处于 UTF-8 代码页（`chcp 65001`），且必须在进入 VS 开发人员环境**之后**执行；否则 `/showIncludes` 前缀编码不一致，Ninja 记录不到头文件依赖。
2. `TMP` / `TEMP` 必须指向纯 ASCII 路径，否则 MSVC 请求调试信息时会报 `D8050`。
3. 中文源码文件名经 `ar_rsp_bom.ps1` / `link_rsp_bom.ps1` 为响应文件补 UTF-8 BOM，避免 `lib.exe` / `link.exe` 按 ANSI 代码页误读。

---

## 九、实现状态与已知问题

### 9.1 已完成

- 事件总线、对象池、四叉树族、配置加载器、日志系统、计时器、数值分配器、随机数生成器、路径与校验工具。
- OBJ / MTL / glTF 三类资产解析（Vulkan-free）。
- 版本锁、版本缓存、版次锚点、头快照、双配置（Debug / Release）与 Ninja / VS 双生成器支持。

### 9.2 已知边界

- 碰撞检测已整体迁至 EngineSystem，本层不再提供碰撞体 / 碰撞空间 / 碰撞代理器。
- 窗口、输入采集、图形 API 由 EngineRenderer 负责；本层不引用任何窗口或图形库。
- 中文路径在 `std::filesystem::path` 下需使用 `src/detail/methods/` 提供的路径转换工具，不要直接把 UTF-8 字符串交给系统本地编码接口。

---

## 十、许可

见本层 [LICENSE.txt](LICENSE.txt)。
