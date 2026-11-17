# =============================================================================
# 白银纪元 · 双配置构建链
# -----------------------------------------------------------------------------
# 用途：按依赖顺序把四层（EngineFoundation → EngineSystem → EngineRenderer → Test）的
#       Debug / Release 两种配置各构建一遍，替代逐层手敲 8 条 cmake 命令。
#
# 用法：
#   .\构建双配置.ps1                 # 两配置全链
#   .\构建双配置.ps1 -Configs Debug  # 只跑 Debug
#   .\构建双配置.ps1 -SkipTest       # 跳过 Test 层（用例修复期间使用）
#
# 说明：
#   - 自动进入 VS 开发人员环境（cl / ninja 依赖），并调用各层 CMakePresets.json
#     中的 x64-Debug / x64-Release 配置预设与 build-* 构建预设；
#   - TMP / TEMP 强制指向纯 ASCII 目录（cl 带调试信息的硬性要求）；
#   - 任一步失败立即中止，不继续后面的层。
# =============================================================================

param(
    [string]$ProjectRoot = "D:\代码仓库\白银纪元",
    [ValidateSet("Debug", "Release")]
    [string[]]$Configs = @("Debug", "Release"),
    [switch]$SkipTest
)

$ErrorActionPreference = "Stop"

# TMP / TEMP 指向纯 ASCII 目录
$tmp = "D:\agent\tmp"
New-Item -ItemType Directory -Force $tmp | Out-Null
$env:TMP = $tmp
$env:TEMP = $tmp

# 进入 VS 开发人员环境
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "未找到 vswhere：$vswhere"
}
$vsPath = & $vswhere -latest -products * -property installationPath
if (-not $vsPath) {
    throw "未找到 Visual Studio 安装"
}
$devShell = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Import-Module $devShell
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null

# 层顺序即依赖顺序（下层必须先于上层构建）
# Test 层现在只链接三个测试子工程的静态库，故子工程必须排在主层之前。
$layers = @(
    @{ Name = "EngineFoundation"; Dir = Join-Path $ProjectRoot "Engine\EngineFoundation" },
    @{ Name = "EngineSystem";     Dir = Join-Path $ProjectRoot "Engine\EngineSystem" },
    @{ Name = "EngineRenderer";   Dir = Join-Path $ProjectRoot "Engine\EngineRenderer" },
    @{ Name = "TestFoundation";   Dir = Join-Path $ProjectRoot "Application\Test\tests\Foundation" },
    @{ Name = "TestSystem";       Dir = Join-Path $ProjectRoot "Application\Test\tests\System" },
    @{ Name = "TestRenderer";     Dir = Join-Path $ProjectRoot "Application\Test\tests\Renderer" },
    @{ Name = "Test";             Dir = Join-Path $ProjectRoot "Application\Test" }
)

foreach ($cfg in $Configs) {
    foreach ($layer in $layers) {
        # 跳过 Test 层（按需）
        if ($SkipTest -and $layer.Name -eq "Test") {
            continue
        }

        Write-Host ("==== {0} / {1} ====" -f $layer.Name, $cfg) -ForegroundColor Cyan
        Push-Location $layer.Dir
        try {
            # 配置当前层（预设名与配置名不同：x64-Debug / x64-Release）
            & cmake --preset ("x64-" + $cfg)
            if ($LASTEXITCODE -ne 0) {
                throw ("{0} 配置失败（{1}）" -f $layer.Name, $cfg)
            }

            # 构建当前层
            & cmake --build --preset ("build-" + $cfg)
            if ($LASTEXITCODE -ne 0) {
                throw ("{0} 构建失败（{1}）" -f $layer.Name, $cfg)
            }
        }
        finally {
            Pop-Location
        }
    }
}

Write-Host ("双配置构建完成：{0}" -f ($Configs -join ", ")) -ForegroundColor Green
