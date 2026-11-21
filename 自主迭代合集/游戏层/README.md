# 游戏层自主迭代

> 主体：《Sliver-Era》本作（工作区 `Application/Game/Sliver-Era`）。
> 轮次从第 1 轮起独立计数，不与渲染核心混排。

## 目录

- `计划分册/`：每 25 轮一份执行计划（`第001-025轮.md` …）。
- `报告分册/`：每 25 轮一份执行报告，与计划同区间。
- `索引/`：`索引_001-500.md` 起分层存放，记录分层清单与轮次索引。
- `里程碑/`：窗口里程碑记录。
- `深研计划合集/`：方向级深研文档（规划 / 预研 / 进度）。
- `下轮迭代方向.md`：下一轮迭代方向。

## 当前状态

前置工作已完成；深研方向「游戏框架搭建」第 1–15 轮已完成（阶段 A 应用骨架；阶段 B 配置、路径、资源缓存、日志；阶段 C 体素/区块/网格/实体/玩家；阶段 D 输入层与相机），Debug / Release 双配置自检全部通过，第 16 轮方向（方块拾取与交互）已写入 `下轮迭代方向.md`。

## 深研进度

- 深研规划：[深研规划-游戏框架搭建.md](深研计划合集/深研规划/深研规划-游戏框架搭建.md)（24 轮，阶段 A–F）
- 第 1 轮预研：[深研预研-游戏框架搭建.md](深研计划合集/深研预研/深研预研-游戏框架搭建.md)
- 第 1 轮进度：[深研进度-游戏框架搭建.md](深研计划合集/深研进度/深研进度-游戏框架搭建.md)

## 工程信息

- 工作区：`Application/Game/Sliver-Era`
- 目录结构：`src/`（源码）｜`external/Engine/{x64-Debug,x64-Release}`（引擎外部依赖）｜`cmake/`（接入下层与版本锁）
- external 内容：每个配置下含 `include/`（三层头快照合并）、`lib/`（EngineFoundation / EngineSystem / EngineRenderer 静态库）、`shaders/`；两份配置合计约 1.05 GB
- 引擎依赖：`external/Engine` 为锁定版本的物理副本；配置期读取副本内版次锚点，与 `cmake/引擎版本锁.cmake` 记录的六条哈希逐项核对，不一致直接报错
- 版本锁：工作区 `cmake/引擎版本锁.cmake` 记录副本来源；`Engine/版本锁.cmake` 漂移不影响本作，只有重新导出 external 副本时才更新。当前值为
  - EngineFoundation：Debug `50e8d22be2f8` / Release `1e5ac2f94d4d`
  - EngineSystem：Debug `7949c936f810` / Release `42a7fce2ae2c`
  - EngineRenderer：Debug `0fc5a3d4197a` / Release `4acda0869c5c`

## 构建前提（本机）

1. `cmake` / `ninja` 不在 PATH：使用 `D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\` 下的可执行文件。
2. 必须先激活 VS 开发环境：`Import-Module D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll; Enter-VsDevShell -VsInstallPath D:\ViusalStudioCommunity2026 -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'`。
3. `TMP` / `TEMP` 必须指向纯 ASCII 目录（如 `D:\agent\tmp`），否则 `cl.exe` 请求调试信息时报 `D8050`。
4. 构建命令：`cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug` 后 `cmake --build out/build/x64-Debug`；Release 同理换目录与配置。

