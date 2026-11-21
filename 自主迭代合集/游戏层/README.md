# 游戏层自主迭代

> 主体：《Sliver-Era》本作（工作区 `Application/Game/Sliver-Era`）。
> 轮次从第 1 轮起独立计数，不与渲染核心混排。

## 目录

- `计划分册/`：每 25 轮一份执行计划（`第001-025轮.md` …）。
- `报告分册/`：每 25 轮一份执行报告，与计划同区间。
- `索引/`：`索引_001-500.md` 起分层存放，记录分层清单与轮次索引。
- `里程碑/`：窗口里程碑记录。
- `下轮迭代方向.md`：下一轮迭代方向（进入自主迭代后书写）。

## 当前状态

前置工作已完成：工作区本地仓库、引擎分层构建系统与六条版本锁已就位，Debug / Release 双配置构建与运行验证通过；尚未开始第 1 轮。

## 工程信息

- 工作区：`Application/Game/Sliver-Era`
- 引擎依赖：`Engine/cmake/对外接口.cmake`（聚合面）＋ `Engine/EngineRenderer/cmake/对外接口.cmake`（渲染层）
- 版本锁：工作区 `cmake/引擎版本锁.cmake` 固定六条哈希，构建期直接解析到 `Engine/版本缓存/<层>/x64-<配置>/<哈希>/` 的冻结副本；`Engine/版本锁.cmake` 漂移只输出提示，不影响本作构建。当前值为
  - EngineFoundation：Debug `50e8d22be2f8` / Release `1e5ac2f94d4d`
  - EngineSystem：Debug `7949c936f810` / Release `42a7fce2ae2c`
  - EngineRenderer：Debug `0fc5a3d4197a` / Release `4acda0869c5c`

## 构建前提（本机）

1. `cmake` / `ninja` 不在 PATH：使用 `D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\` 下的可执行文件。
2. 必须先激活 VS 开发环境：`Import-Module D:\ViusalStudioCommunity2026\Common7\Tools\Microsoft.VisualStudio.DevShell.dll; Enter-VsDevShell -VsInstallPath D:\ViusalStudioCommunity2026 -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'`。
3. `TMP` / `TEMP` 必须指向纯 ASCII 目录（如 `D:\agent\tmp`），否则 `cl.exe` 请求调试信息时报 `D8050`。
4. 构建命令：`cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug` 后 `cmake --build out/build/x64-Debug`；Release 同理换目录与配置。

