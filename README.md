# 白银纪元 · Game 层（游戏内容分支）

本分支是「白银纪元」工程中的**游戏内容层分支**（远端分支 `Game`），用于承载游戏本体的配置资产、脚本资产与未来的玩法代码。

当前状态：**内容骨架／历史快照**。本分支最后一次实质更新为 2026-10-18，内容停留在「`src/` 为空、只有 `assets/` 配置与脚本样本」的骨架阶段；引擎侧此后已完成分层重构（EngineCore 重命名为 EngineFoundation、渲染层拆分独立仓库、ECS/输入/碰撞重写），但本分支的构建骨架**尚未跟随迁移**。

> 本文档如实记录本分支自身状态。若需了解引擎各层的最新架构，请查看 `EngineFoundation`、`EngineSystem`、`Test` 分支的 README，或顶层 `main` 的总览。

---

## 一、分支与仓库信息

| 项目 | 取值 |
| --- | --- |
| 仓库地址（现行） | `https://github.com/Sliver-Era/Sliver-Era.git` |
| 旧地址 | `https://github.com/yuxingyuzhong/SilverEra.git`（GitHub 重定向到现行地址） |
| 远端名 | `sliverera` |
| 本地分支名 | `main` |
| 远端目标分支 | `Game` |
| 上游跟踪 | `sliverera/Game` |
| 推送命令 | `git push sliverera main:Game` |

同一远端还承载：

| 分支 | 内容 |
| --- | --- |
| `EngineFoundation` | EngineFoundation 层（引擎核心层，活跃） |
| `EngineSystem` | EngineSystem 层（引擎系统层，活跃） |
| `Test` | Test 层（测试与全项目唯一可执行入口，活跃） |
| `main` | 顶层总览＋全仓聚合快照 |
| `Doc` | 工作站文档（任务报告、项目导航） |
| `Engine` | Engine 聚合快照 |
| `EngineRender` | 渲染层快照，由渲染层维护者负责 |

渲染层的活跃源码位于独立仓库 `SilverEra-Renderer`（`main` 分支），不在本远端维护。

---

## 二、本层在依赖链中的位置

```text
EngineFoundation ──▶ EngineSystem ──▶ EngineRenderer ──▶ Application/{Test, Game}
 EngineFoundation.lib   EngineSystem.lib   EngineRenderer.lib
```

| 项目 | 内容 |
| --- | --- |
| 依赖链位置 | 依赖链顶端的游戏内容层 |
| 下层依赖 | Engine 聚合面（EngineSystem.lib ＋ EngineFoundation.lib）；本分支构建骨架仍写旧名 `EngineCore` |
| 产物 | 无。`src/` 为空，`GameCore` 为条件目标，当前不产出 |
| 可执行文件 | 本层不产出；全项目唯一入口为 Test 层 `EngineTests.exe` |

设计约束：

- 依赖方向单向：Game 依赖引擎聚合面，不反向被引擎层依赖。
- 层间只链接**已构建好的**静态库，不通过 `add_subdirectory` 回退编译下层源码。
- 游戏玩法代码未来落在本层 `src/`；当前只有数据资产，没有 C++ 源码。

---

## 三、技术栈（按本分支构建骨架）

| 项目 | 取值 |
| --- | --- |
| 语言 | C++20（骨架声明，当前无源码参与编译） |
| 命名空间 | `engine` |
| 构建系统 | CMake ≥ 3.20、Ninja；`CMakeSettings.json` 提供 x64 配置 |
| 资产格式 | JSON（`nlohmann::json`）＋ Lua 脚本 |
| 第三方库 | 本层不引入；由下层对外面传递 |

---

## 四、目录结构

```text
Application/Game/
├── CMakeLists.txt                 # 层构建入口（骨架，条件目标 GameCore）
├── CMakeSettings.json
├── README.md
├── .gitignore
├── cmake/
│   ├── 对外接口.cmake             # 声明 BYJY_GAME_OUT_LIB = GameCore
│   └── 接入下层.cmake             # byjy_jieru_xiaceng()：接入 Engine 聚合面
└── assets/
    ├── config/
    │   ├── entities/              # 实体配置（如 Slime / Goblin / Minotaur 等样例）
    │   ├── property/              # 属性槽配置（与 entities 一一对应）
    │   ├── format/                # 格式定义（Entity_Manager / Property_Manager）
    │   └── route/                 # 路由表（entity / property）
    └── scripts/
        ├── initialize/            # 初始化 Lua 脚本
        └── behavior/              # 行为（决策）Lua 脚本
```

资产样本仅用于演示「实体配置 → 属性槽 → 行为脚本」的数据驱动链路，不代表最终游戏内容。

---

## 五、资产数据说明

### 5.1 配置四类

| 目录 | 作用 |
| --- | --- |
| `assets/config/entities/` | 每个实体一份 JSON：实体标识、订阅事件、关联属性与行为脚本 |
| `assets/config/property/` | 每个属性槽一份 JSON：初始属性值与分组 |
| `assets/config/format/` | 格式定义：实体管理器与属性管理器的字段约束 |
| `assets/config/route/` | 路由表：把配置文件名映射到实体/属性类型 |

### 5.2 两级脚本

| 目录 | 作用 |
| --- | --- |
| `assets/scripts/initialize/` | 实体创建时的初始化逻辑 |
| `assets/scripts/behavior/` | 实体逐帧/事件驱动的行为决策 |

脚本通过 Sol2 暴露的 `sol::state` 调用；属性槽以键值形式在 Lua 中读写。

### 5.3 与引擎侧的关系

- 配置与脚本的**消费方**在 EngineSystem 层（历史的实体/属性/效应运行时）；该运行时已重构为 ECS 与场景/粒子模块，本分支资产尚未按新结构迁移。
- 本分支构建骨架引用的是旧名 `EngineCore`，与当前 `EngineFoundation` 目录名不一致；恢复本层开发时需先完成改名与包含根迁移。

---

## 六、构建

当前骨架的构建方式（仅用于验证层间契约，不产出 `GameCore`）：

```powershell
cmake -S Application/Game -B Application/Game/out/build/x64-Debug -G Ninja
cmake --build Application/Game/out/build/x64-Debug
```

前置条件：EngineFoundation 与 EngineSystem 已按各自 README 构建完成，Engine 聚合层自检通过。

由于本分支仍引用 `EngineCore` 旧名，直接套用当前工作区目录结构会在配置期找不到下层；**本分支当前不作为可构建目标维护**。

---

## 七、已知差异与后续

| 项 | 现状 | 后续 |
| --- | --- | --- |
| 层名 | 文档与 CMake 仍写 `EngineCore` | 迁移为 `EngineFoundation` |
| 运行时 | 资产面向旧实体/属性/效应模型 | 按 ECS 与场景数据新结构迁移 |
| 渲染 | 无 | 接入 EngineRenderer 独立仓库 |
| 可执行入口 | 无 | 仍由 Test 层统一产出；Game 本体入口另行追加 |

---

## 八、许可

本层不单独维护许可证文件，许可见工程根 `LICENSE`。
