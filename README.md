# 白银纪元 · EngineSystem 层（引擎系统层）

EngineSystem 是「白银纪元」分层工程中的**运行时系统层**，位于 `Engine/EngineFoundation` 之上、`Engine/EngineRenderer` 与 `Application/*` 之下。它把核心层提供的事件、对象池、加载器与工具集，组织成四类与具体游戏无关的运行时能力：

- **碰撞**：碰撞体、碰撞空间、碰撞代理器与碰撞事件；
- **输入**：原始输入 → 映射 → 命令/轴语义，以及焦点管理；
- **世界数据**：ECS（实体、组件存储、命令缓冲）、场景中立数据（关卡/相机/光照/雾/场景清单）与粒子池；
- **运行时公共件**：逻辑帧等。

本层是**独立 git 仓库**，源码通过远端 `EngineSystem` 分支发布；产出静态库 `EngineSystem.lib`。

> 本文档依据 2026-11-20 工作区源码重写，替代此前以 `src/entity`、`src/prop`、`src/effect` 旧目录结构编写的版本。旧版内容仍可在本分支 git 历史中查阅。

---

## 一、分支与仓库信息

| 项目 | 取值 |
| --- | --- |
| 仓库地址（现行） | `https://github.com/Sliver-Era/Sliver-Era.git` |
| 旧地址 | `https://github.com/yuxingyuzhong/SilverEra.git`（GitHub 重定向到现行地址） |
| 远端名 | `sliverera` |
| 本地分支名 | `main` |
| 远端目标分支 | `EngineSystem` |
| 上游跟踪 | `sliverera/EngineSystem` |
| 推送命令 | `git push sliverera main:EngineSystem` |

同一远端还承载 `EngineFoundation`、`EngineRenderer`、`Test`、`Game`、`Doc`、`main` 与 `Engine` 等分支。本层只维护 `EngineSystem` 分支。

---

## 二、本层在依赖链中的位置

```text
EngineFoundation ──▶ EngineSystem ──▶ EngineRenderer ──▶ Application/{Test, Game}
 EngineFoundation.lib   EngineSystem.lib   EngineRenderer.lib
```

| 项目 | 内容 |
| --- | --- |
| 下层依赖 | EngineFoundation（只链接已构建的 `EngineFoundation.lib`） |
| 上层消费者 | EngineRenderer（消费碰撞与世界中立数据）、Application/Test、Application/Game |
| 产物 | `Engine/EngineSystem/out/build/<配置>/lib/EngineSystem.lib` |
| 对外面前缀 | `BYJY_ENGINE_SYSTEM_*` |
| 可执行文件 | 本层不产出；唯一入口为 Test 层 `EngineTests.exe` |

设计约束：

- **单向依赖**：本层引用 EngineFoundation，渲染层引用本层；本层不引用渲染层或应用层的任何类型。
- **中立数据契约**：场景、关卡、粒子等数据类型保持 Vulkan-free、SDL-free，渲染层只消费这些中立数据，不反向把渲染类型带进本层。
- **不自动销毁逻辑对象**：实体、碰撞体、输入对象等生命周期由调用者/玩法层显式管理，本层只提供创建与卸载原语。

---

## 三、技术栈

| 项目 | 取值 |
| --- | --- |
| 语言 | C++20（`CMAKE_CXX_STANDARD 20`，`CMAKE_CXX_EXTENSIONS OFF`） |
| 命名空间 | `engine` |
| 第三方依赖 | `external/bullet3`（LinearMath + BulletCollision）、`external/Lua`、`external/Sol2`；前两者随本层编入静态库，Sol2 为纯头库 |
| 构建系统 | CMake ≥ 3.20（预设要求 ≥ 3.25）、Ninja 或 Visual Studio 多配置生成器 |
| 已验证平台 | MSVC / x64 / Windows |

说明：Lua / Sol2 为早期脚本化能力保留的依赖，当前 `src/` 内没有直接使用；新增脚本能力时可直接复用 `common/external/Sol2/` 的类型别名与注册工具。

---

## 四、目录结构

```text
Engine/EngineSystem/
├── CMakeLists.txt
├── CMakePresets.json
├── CMakeSettings.json
├── README.md
├── .gitignore
├── 源文件列表.cmake
├── cmake/
│   ├── 对外接口.cmake             # 本层对外面（BYJY_ENGINE_SYSTEM_*，并入 EngineFoundation）
│   ├── 接入下层.cmake             # byjy_jieru_xiaceng()：接入已构建的 Foundation 库与头快照
│   └── ...
├── common/
│   ├── 前置头文件包含.h
│   ├── runtime/逻辑帧.h           # 逻辑帧率
│   └── external/Sol2/             # Lua 绑定类型别名与注册
├── external/
│   ├── bullet3/                   # 碰撞后端（仅保留 LinearMath + BulletCollision）
│   ├── Lua/                       # Lua C 运行时
│   └── Sol2/                      # sol2 头库
└── src/
    ├── collision/                 # 碰撞系统
    │   ├── Common/                # 几何类型、运动类型、碰撞事件与数据类型
    │   ├── Collider/              # 碰撞体（含 backend 适配）
    │   ├── Collision_Region/      # 碰撞空间（含 backend 适配）
    │   ├── Collision_Proxy/       # 碰撞代理器（检测入口、运动通道、事件交互）
    │   └── 碰撞系统运行包.h
    ├── input/                     # 输入与焦点
    │   ├── common/                # 输入类型、输入事件
    │   ├── mapping/               # Input_Manager / Input_Mapping / Input_Object
    │   ├── focus/Focus_Manager/   # 焦点系统
    │   ├── 输入映射系统运行包.h
    │   └── 焦点管理系统运行包.h
    └── world/                     # 世界数据
        ├── ecs/                   # 纯数据 ECS
        │   ├── entity/Entity/、Entity_Table/
        │   ├── storage/Store/、Component/
        │   ├── command/Command_Buffer/
        │   └── ECS系统运行包.h
        └── data/
            ├── scene/Level/       # 场景数据、关卡数据、关卡文本、关卡变换
            ├── scene/Manifest/    # 场景清单
            └── particle/          # Particle_Data、Particle_Pool
```

---

## 五、核心架构与运行链路

### 5.1 碰撞

- `Collider`：碰撞体本体，持有形状、位姿与运动状态；后端由 bullet3 适配。
- `Collision_Region`：碰撞空间，容纳碰撞体并执行窄相位检测；空间边界采用静态三角网格（地形直接烘焙为世界坐标网格）。
- `Collision_Proxy`：对外检测入口，负责空间/碰撞体编排、运动通道推进、碰撞响应与事件通知。
- 碰撞结果既可通过公共 API 同步获取详尽信息，也可通过事件发送；默认同时发送简略碰撞事件。
- 碰撞系统**只输出碰撞数据与事件**，不隐含创建或销毁逻辑，碰撞体卸载由外部显式调用。

### 5.2 输入与焦点

```text
原始输入事件 → Input_Manager（映射对象） → Input_Mapping（键位/映射/轴解析）
             → Command / AxisState 事件 → 业务订阅者
焦点侧：Focus_Manager 维护主对象焦点栈，按事件驱动焦点转移与模态闸门
```

- 映射对象由对象池承载，每个对象持有独立事件终端；映射表支持配置文件定义与实时重载。
- 焦点转移统一延后到下一帧开头生效，同帧多次请求后写覆盖前写。
- 焦点系统是输入链路的唯一权威；输入映射系统不直接改写焦点对象，只在焦点系统转发的原始输入下产出语义事件。
- 对象能力位：`focusable`、`enabled`、`text_sink`；禁用的对象保留焦点但丢弃业务输入。
- Enter / Escape 作为特殊语义映射处理，不受宿主键位冲突丢弃规则约束。

### 5.3 ECS 与世界数据

- ECS 是**纯数据层**：`Entity` 为「代际 + 槽位」句柄，`Entity_Table` 管理实体记录，`Store` 按类型保存组件存储，`Command_Buffer` 延迟执行结构变更。
- 场景数据 `场景数据.h` 定义相机、光照、雾、光源快照等纯类型；`关卡数据.h` / `关卡文本.h` / `关卡变换.cpp` 负责关卡文本与数据互转；`Manifest` 解析场景清单。
- 粒子：`Particle_Data` 为 CPU 模拟与 GPU 绘制共用的纯数据布局（由静态断言守住 std430 对齐）；`Particle_Pool` 提供容量、分配回收与 CPU 参考模拟，渲染层只负责上传与绘制。

---

## 六、模块详解

### 6.1 `collision` —— 碰撞

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 公共类型 | `src/collision/Common/` | 几何类型、运动类型、碰撞数据类型、字段读取、数学转换、碰撞事件词汇 |
| 碰撞体 | `src/collision/Collider/碰撞体.h` | 形状配置、位姿与运动推进；后端适配 |
| 碰撞空间 | `src/collision/Collision_Region/碰撞空间.h` | 碰撞体容器、窄相位检测、空间边界与跨越通知 |
| 碰撞代理器 | `src/collision/Collision_Proxy/碰撞代理器.h` | 空间编排、碰撞体编排、运动通道、响应与事件交互 |

### 6.2 `input` —— 输入与焦点

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 输入类型 | `src/input/common/输入类型.h` | 引擎中立键位编码（位域：键类 + 类内编号）与轴类型 |
| 输入事件 | `src/input/common/输入事件.h` | 输入模块事件词汇（category = `Input`） |
| 输入管理器 | `src/input/mapping/Input_Manager/输入管理器.h` | 对象池、接入与分发、原始输入、映射查询、配置加载 |
| 输入映射 | `src/input/mapping/Input_Mapping/输入映射.h` | 映射解析、键位解析、轴解析 |
| 映射对象 | `src/input/mapping/Input_Object/映射对象.h` | 每个映射对象持有事件终端与能力位 |
| 焦点系统 | `src/input/focus/Focus_Manager/焦点系统.h` | 焦点栈、转移请求延迟生效、模态闸门、接入与广播 |

### 6.3 `world/ecs` —— 纯数据 ECS

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 实体 | `src/world/ecs/entity/Entity/实体.h` | 句柄：高 32 位代际、低 32 位槽位，0 为无效 |
| 实体表 | `src/world/ecs/entity/Entity_Table/实体表.h` | 实体记录、槽位分配与代际推进 |
| 组件存储 | `src/world/ecs/storage/Component/组件存储.h` | 基于对象池的按类型组件容器；`Component_Type` 概念约束 |
| ECS 存储 | `src/world/ecs/storage/Store/存储.h` | 以 `type_index` 组织多类组件存储 |
| 命令缓冲 | `src/world/ecs/command/Command_Buffer/命令缓冲.h` | 延迟执行实体/组件的结构变更 |

### 6.4 `world/data` —— 场景与粒子

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| 场景数据 | `src/world/data/scene/Level/场景数据.h` | 相机、光照、雾、光源快照等 Vulkan-free 纯数据 |
| 关卡数据 | `src/world/data/scene/Level/关卡数据.h` | 关卡聚合数据与读写 |
| 关卡文本 | `src/world/data/scene/Level/关卡文本.h` | 关卡文本 ↔ 关卡数据 |
| 关卡变换 | `src/world/data/scene/Level/core/关卡变换.cpp` | 关卡变换构建 |
| 场景清单 | `src/world/data/scene/Manifest/场景清单.h` | `mesh` / `mtl` / `place` 行格式解析 |
| 粒子数据 | `src/world/data/particle/Particle_Data/粒子数据.h` | CPU/GPU 共用布局与常量 |
| 粒子池 | `src/world/data/particle/Particle_Pool/粒子系统.h` | 容量、分配回收与 CPU 参考模拟 |

### 6.5 运行时公共件

`common/runtime/逻辑帧.h`：逻辑帧率的读写（带锁），供碰撞与业务层对齐固定步长。

---

## 七、对外接口契约

`cmake/对外接口.cmake` 先并入 EngineFoundation 对外面，再补充本层自有条目（前缀 `ENGINE_SYSTEM`）：

| 变量 | 取值 | 含义 |
| --- | --- | --- |
| `BYJY_ENGINE_SYSTEM_OUT_LIB` | `EngineSystem` | 本层静态库名 |
| `BYJY_ENGINE_SYSTEM_OUT_LIBDIR` | `Engine/EngineSystem` | 与 `OUT_LIB` 对应的层目录 |
| `BYJY_ENGINE_SYSTEM_OUT_INC` | `external/Sol2/include`、`external/Lua`、`external/bullet3/src` ＋ Foundation 的 `OUT_INC` | 使用本层公共头所需的包含目录 |
| `BYJY_ENGINE_SYSTEM_OUT_DEF` | 继承 Foundation | 编译定义 |
| `BYJY_ENGINE_SYSTEM_OUT_LINK` | 继承 Foundation | 对外传递的第三方库 |
| `BYJY_ENGINE_SYSTEM_OUT_SYS` | 继承 Foundation | 对外传递的系统库 |

上层通过 `byjy_jieru_xiaceng()` 读取本层对外面，按 `OUT_LIB` 定位**已构建好的** `EngineSystem.lib`，并把 Foundation 的快照根与本层快照根一起接入；不回退编译本层源码。

---

## 八、构建

### 8.1 前置条件

先按顺序构建 EngineFoundation 的对应配置，确保 `EngineFoundation.lib` 与头快照已产出。本层配置期会按版本锁解析并定位下层库，找不到即硬失败。

### 8.2 构建步骤

```powershell
cd Engine/EngineSystem

# Debug
cmake --preset x64-Debug
cmake --build --preset build-Debug

# Release
cmake --preset x64-Release
cmake --build --preset build-Release
```

产物：`out/build/<配置>/lib/EngineSystem.lib`；头快照：`out/build/<配置>/include/Engine/EngineSystem/...`。

### 8.3 版本与快照

- 全仓共享 `Engine/版本锁.cmake`：留空 = 跟随 EngineFoundation 最新构建树；填哈希 = 锁定到 `Engine/版本缓存/EngineFoundation/x64-<配置>/<哈希>/`。
- 本层编译时把 Foundation 快照根排在自己的包含目录之前，保证「头与库同版次」。
- 本层构建成功后也会发布自己的版本缓存与头快照，供 Renderer / Test / Game 消费。

---

## 九、实现状态与已知问题

### 9.1 已完成

- 碰撞体 / 碰撞空间 / 碰撞代理器全链路，含运动通道、越界通知、事件化状态查询。
- 输入映射（配置定义、实时重载、轴解析）与焦点管理（延迟转移、模态闸门、能力位）。
- 纯数据 ECS（实体表、组件存储、命令缓冲）。
- 场景/关卡中立数据与场景清单解析；粒子纯数据与 CPU 参考模拟。

### 9.2 已知边界

- 本层不做渲染、不建 GPU 资源；场景与粒子数据类型只描述数据契约。
- Lua / Sol2 依赖仍保留，但当前源码未直接使用；若近期不恢复脚本能力，可在清理窗口评估移除。
- 历史文档中的 `src/entity`、`src/prop`、`src/effect` 目录已不存在，实体/效应语义已由 `world/ecs` 与 `world/data` 承接。

---

## 十、许可

本层不单独维护许可证文件，许可见工程根 `LICENSE`。
