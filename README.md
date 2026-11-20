# 白银纪元 · Engine 聚合层（快照分支）

本分支（远端 `Engine`）是「白银纪元」引擎侧的**聚合快照**，创建于 2026-11-15：把 `EngineFoundation` 与 `EngineSystem` 两个子层合入 `Engine/` 目录，并提供聚合层构建脚本，便于在一个分支内查看完整的引擎侧源码。

> 本分支**不是活跃开发分支**。当前开发以 `EngineFoundation` 与 `EngineSystem` 两个分支为准；渲染层位于独立仓库 `SilverEra-Renderer`。

---

## 目录内容

```text
Engine/
├── CMakeLists.txt        # 聚合层入口：自检两个子层静态库齐备
├── CMakePresets.json     # x64-Debug / x64-Release
├── 版本锁.cmake          # 全链版本锁
├── cmake/
│   ├── 接入下层.cmake
│   └── 对外接口.cmake    # 聚合面：BYJY_ENGINE_*，转发 EngineSystem + EngineFoundation
├── EngineFoundation/     # 引擎核心层源码快照
└── EngineSystem/         # 引擎系统层源码快照
```

聚合层不编源码、不产出静态库：`BYJY_ENGINE_OUT_LIB` 转发 `EngineSystem;EngineFoundation`，让应用层只接一次。

---

## 依赖方向

```text
EngineFoundation ──▶ EngineSystem ──▶ Engine（聚合，无产物） ──▶ Application/{Test, Game}
```

渲染层（EngineRenderer）不并入聚合面：它依赖 EngineSystem，由应用层在聚合面之外单独链接。

---

## 构建

前置条件：先按顺序构建 `Engine/EngineFoundation` 与 `Engine/EngineSystem` 的对应配置（版本锁留空时跟随最新构建树）。

```powershell
cd Engine
cmake --preset x64-Debug
cmake --build --preset build-Debug

cmake --preset x64-Release
cmake --build --preset build-Release
```

聚合层构建只做自检：任一子层静态库缺失即配置期硬失败。

---

## 与活跃分支的关系

| 内容 | 活跃分支／仓库 |
| --- | --- |
| EngineFoundation | `EngineFoundation` 分支（`Engine/EngineFoundation/` 独立仓库） |
| EngineSystem | `EngineSystem` 分支（`Engine/EngineSystem/` 独立仓库） |
| 渲染层 | 独立仓库 `SilverEra-Renderer`（`main`） |
| 测试层 | `Test` 分支 |
| 游戏层 | `Game` 分支 |
| 工作站文档 | `Doc` 分支 |

各层 README 见对应分支根目录；顶层总览见 `main` 分支的 `README.md`。
