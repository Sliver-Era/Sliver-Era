# 白银纪元

一个用 **C++20** 从零编写的游戏引擎工程。源码按依赖方向分成若干层静态库，**每一层都是一个独立的 git 仓库**，各自向本仓库推送不同分支；本仓库是多分支聚合远端，`main` 是总览线与全仓聚合快照。

本文件是 `main` 分支的 README，用于交代**整个仓库的分支结构、分层架构、构建链与当前状态**。某一层的细节请直接看该层分支根目录的 `README.md`。

---

## 一、仓库形态

- 各层源码位于独立 git 仓库（目录内含自身 `.git`），推送目标是本仓库的不同分支。
- `main` 分支自身同时承担两件事：
  1. **总览线**：本文件与分支说明；
  2. **全仓聚合快照**：2026-11-15 把 Engine（Foundation ＋ System）与 Test 层源码合入本分支，便于一次性浏览。
- 顶层不再使用 `add_subdirectory` 把各层编进同一构建树；构建一律**逐层独立进行**，层间只链接**已构建好的静态库**。
- 渲染层（EngineRenderer）的活跃源码位于独立仓库 `SilverEra-Renderer`（`main` 分支），不在本远端维护；本远端的 `EngineRender` 分支为渲染层快照，由渲染层维护者负责。

---

## 二、分支总览

| 分支 | 推送源 | 本地分支 | 内容 | 状态 |
| --- | --- | --- | --- | --- |
| `main` | 顶层仓库（本目录） | `main` | 总览 ＋ 全仓聚合快照 | 总览线；快照截至 2026-11-15 |
| `engine` | `Engine/EngineFoundation/` | `main` | 引擎核心层：事件 / 对象 / 空间分区 / 配置与资产加载 / 日志 / 工具集 | 活跃 |
| `system` | `Engine/EngineSystem/` | `main` | 引擎系统层：碰撞 / 输入与焦点 / ECS / 场景与粒子 | 活跃 |
| `test` | `Application/Test/` | `test` | 测试体系与测试模块选择器（全项目唯一可执行文件产地） | 活跃 |
| `game` | `Application/Game/` | `main` | 游戏内容资产与构建骨架 | 内容骨架（2026-10-18 后未迁移） |
| `doc` | 工作站（工作区之外） | `doc` | 任务报告、项目导航、自主迭代文档 | 活跃 |
| `Engine` | 顶层仓库 | — | Engine 聚合层快照（EngineFoundation ＋ EngineSystem） | 历史快照（2026-11-15） |
| `EngineFoundation` | EngineFoundation 仓 | — | 早于 `engine` 的核心层版次 | 历史快照（2026-11-07） |
| `EngineSystem` | EngineSystem 仓 | — | 早于 `system` 的系统层版次 | 历史快照（2026-11-08） |
| `EngineRender` | 渲染层仓 | — | 渲染层快照 | 由渲染层维护者负责，本仓库不更新 |

说明：

- 层仓库的远端名统一为 `sliverera`；顶层仓库为 `origin`。两者指向同一个 GitHub 仓库（现行地址 `https://github.com/Sliver-Era/Sliver-Era.git`，旧地址 `yuxingyuzhong/SilverEra` 由 GitHub 重定向）。
- `EngineFoundation` / `EngineSystem` / `Game` 的本地分支名都是 `main`，与远端分支名不一致，推送必须写显式 refspec：
  - `git push sliverera main:engine`
  - `git push sliverera main:system`
  - `git push sliverera main:game`
- `Engine` / `EngineFoundation` / `EngineSystem` 三个大写分支是聚合或历史快照，不是活跃开发分支；当前开发以 `engine` / `system` / `test` 为准。
- 渲染层活跃仓库：`https://github.com/Sliver-Era/SilverEra-Renderer`（`main`）。

---

## 三、分层架构

```text
EngineFoundation ──▶ EngineSystem ──▶ EngineRenderer ──▶ Application/{Test, Game}
 EngineFoundation.lib   EngineSystem.lib   EngineRenderer.lib
```

| 层 | 目录（活跃仓库） | 产物 | 职责 |
| --- | --- | --- | --- |
| EngineFoundation | `Engine/EngineFoundation/` | `EngineFoundation.lib` | 事件总线、对象池、四叉树、配置/资产加载（OBJ/MTL/glTF）、日志、工具集 |
| EngineSystem | `Engine/EngineSystem/` | `EngineSystem.lib` | 碰撞、输入映射与焦点、纯数据 ECS、场景/关卡中立数据、粒子池 |
| EngineRenderer | `Engine/EngineRenderer/`（独立仓库） | `EngineRenderer.lib` | Vulkan 渲染：设备/交换链/帧/管线/网格/材质/纹理/渲染目标/计算/粒子/文字/UI 等 |
| Test | `Application/Test/` | `EngineTests.exe`（唯一可执行） | googletest 用例、测试选择器、图形/控制台运行界面 |
| Game | `Application/Game/` | 无（`GameCore` 条件目标） | 游戏内容资产与玩法代码（当前骨架） |

`Engine/` 目录本身是 **Engine 聚合层**：不编源码、不产出库，只把 Foundation 与 System 两层对外面合并成 `BYJY_ENGINE_*`，便于应用层只接一次。渲染层**不并入聚合面**，由应用层单独链接 `EngineRenderer.lib`。

---

## 四、目录结构（顶层）

```text
白银纪元/
├── README.md
├── LICENSE
├── .gitignore
├── Engine/
│   ├── CMakeLists.txt             # 聚合层构建脚本（自检两个子层库齐备）
│   ├── CMakePresets.json
│   ├── 版本锁.cmake               # 全链版本锁（留空 = 跟随最新构建树）
│   ├── 版本缓存/                  # 各层构建后发布：头快照 + 静态库 + 依赖记录
│   ├── EngineFoundation/          # 独立 git 仓库（engine）
│   ├── EngineSystem/              # 独立 git 仓库（system）
│   └── EngineRenderer/            # 独立 git 仓库（SilverEra-Renderer）
└── Application/
    ├── Test/                      # 独立 git 仓库（test）
    ├── Game/                      # 独立 git 仓库（game）
    └── Editor/                    # 占位
```

> 上图是完整工作区的目录形态。`main` 分支的全仓聚合快照只收录 Engine（Foundation ＋ System）与 Test 层源码；`EngineRenderer/` 的源码位于独立仓库 `SilverEra-Renderer`，不在本分支快照内。

---

## 五、层间契约

### 5.1 对外面自描述

每层维护 `cmake/对外接口.cmake`，声明「本层对外面 = 本层自有面 ＋ 下层对外面」，导出六个变量：

| 变量 | 含义 |
| --- | --- |
| `OUT_LIB` | 本层对外传递的静态库名列表（分号分隔）；无产物留空 |
| `OUT_LIBDIR` | 与 `OUT_LIB` 一一对应的库所在层目录 |
| `OUT_INC` | 使用本层公共头所需的包含目录 |
| `OUT_DEF` | 编译定义 |
| `OUT_LINK` | 对外传递的第三方库 |
| `OUT_SYS` | 对外传递的系统库 |

上层通过 `cmake/接入下层.cmake` 的 `byjy_jieru_xiaceng()` 读取下层对外面，按 `OUT_LIB` 逐库定位**已构建**的静态库，建立 `IMPORTED STATIC GLOBAL` 目标；任何情况下都不回退编译下层源码。

### 5.2 版本锁、版本缓存与版次锚点

- 各层构建成功后，把「头快照 ＋ 静态库 ＋ 依赖版本记录」按版次锚点哈希发布到 `Engine/版本缓存/<层>/x64-<配置>/<哈希>/`。
- `Engine/版本锁.cmake` 是唯一真源：**留空 = 跟随下层最新构建树**；填哈希 = 锁定到版本缓存中的冻结副本。锁值落后于构建树时，配置期打印 WARNING。
- 每个产物都带**版次锚点**符号：头与库版次不一致时链接期直接报错，避免「新头配旧库」。
- 上层编译时把下层头快照根排在项目根之前，确保头与所链接的库同版次。

### 5.3 包含路径

工程内部一律写**项目根相对全路径**：

```cpp
#include "Engine/EngineFoundation/src/event/Event/事件.h"
#include "Engine/EngineSystem/src/world/ecs/entity/Entity/实体.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
```

第三方头保持原样（`<nlohmann/json.hpp>`、`<vulkan/vulkan.h>`、`<SDL3/SDL.h>` 等）。

---

## 六、构建

### 6.1 工具链

- CMake ≥ 3.24（各层预设要求 ≥ 3.25）、Ninja 或 Visual Studio 多配置生成器；
- 支持 C++20 的 MSVC；当前只在 MSVC / x64 / Windows 验证；
- Vulkan SDK、SDL3 由渲染层构建脚本探测/就地编入。

### 6.2 构建顺序

必须逐层构建，顺序不能颠倒——每层在配置期定位下层**已构建**的静态库，找不到直接硬失败。

```powershell
# 1. EngineFoundation
cd Engine/EngineFoundation
cmake --preset x64-Debug;  cmake --build --preset build-Debug
cmake --preset x64-Release; cmake --build --preset build-Release

# 2. EngineSystem
cd ../EngineSystem
cmake --preset x64-Debug;  cmake --build --preset build-Debug
cmake --preset x64-Release; cmake --build --preset build-Release

# 3. Engine 聚合层（自检两个子层库齐备）
cd ..
cmake --preset x64-Debug;  cmake --build --preset build-Debug

# 4. EngineRenderer（独立仓库，链接 Foundation + System）
cmake --preset x64-Debug;  cmake --build --preset build-Debug

# 5. Application/Test
cd ../../Application/Test
cmake --preset x64-Debug;  cmake --build --preset build-Debug
```

也可以用生成器命令直接配置：`cmake -S <层目录> -B <层目录>/out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`。

### 6.3 本机环境注意事项

1. 配置期必须处于 UTF-8 代码页（`chcp 65001`），且必须在进入 VS 开发人员环境**之后**执行；否则 `/showIncludes` 前缀编码不一致，Ninja 记录不到头文件依赖。
2. `TMP` / `TEMP` 必须指向纯 ASCII 路径，否则 MSVC 请求调试信息时报 `D8050`。
3. 中文源码文件名的归档/链接响应文件由各层的 `ar_rsp_bom.ps1` / `link_rsp_bom.ps1` 补 UTF-8 BOM，避免 `lib.exe` / `link.exe` 按 ANSI 代码页误读。

---

## 七、测试

测试层 `Application/Test` 产出全项目唯一可执行文件 `EngineTests.exe`，并按被测层提供三个薄入口：

| 入口 | 内容 |
| --- | --- |
| `EngineTests.exe` | 全量入口；图形窗口 / 控制台 / 关闭选择器三种模式 |
| `TestFoundation.exe` | 只跑 EngineFoundation 用例：23 套件 / 561 例 |
| `TestSystem.exe` | 只跑 EngineSystem 用例：13 套件 / 126 例 |
| `TestRenderer.exe` | 只跑渲染层用例：57 套件 / 909 例 |

当前注册规模：**96 套件 / 1643 用例**（2026-11-20 构建口径）。常用运行方式：

```powershell
EngineTests.exe                       # 默认 --selector=window：图形窗口勾选模块
EngineTests.exe --selector=console    # 控制台编号菜单
EngineTests.exe --selector=off        # 不开窗，全量运行（ctest 走这条）
EngineTests.exe --gtest_filter=...    # 已带 gtest 参数时直接透传
```

无人值守环境可设 `BYJY_RUN_L3=0` 跳过可见窗口用例。

---

## 八、文档索引

| 文档 | 位置 |
| --- | --- |
| 引擎核心层 | `engine` 分支根目录 `README.md` |
| 引擎系统层 | `system` 分支根目录 `README.md` |
| 测试层 | `test` 分支根目录 `README.md` |
| 游戏层 | `game` 分支根目录 `README.md` |
| 工作站文档 | `doc` 分支根目录 `README.md`（任务报告、项目导航、自主迭代） |
| 渲染层 | 独立仓库 `SilverEra-Renderer` |

---

## 九、许可

见 [LICENSE](LICENSE)（MIT）。各层目录下如有独立 `LICENSE` / `LICENSE.txt`，以各层自身文件为准。
