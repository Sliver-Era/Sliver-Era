# 下层与测试层同步检查（2026-11-17 修正版）
# 用法: powershell -ExecutionPolicy Bypass -File <本文件>
# 返回: 0 = 同步; 1 = 不同步（日志见 检查\同步检查日志.txt）

$env:TMP = 'D:\agent\_byjy_tmp'
$env:TEMP = 'D:\agent\_byjy_tmp'
New-Item -ItemType Directory -Force -Path 'D:\agent\_byjy_tmp' | Out-Null
New-Item -ItemType Directory -Force -Path 'D:\agent\working_station\silver_era\检查' | Out-Null

& 'D:\ViusalStudioCommunity2026\Common7\Tools\Launch-VsDevShell.ps1' -VsInstallationPath 'D:\ViusalStudioCommunity2026' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
chcp 65001 | Out-Null

$cmake = 'D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$root = 'd:\代码仓库\白银纪元'
$test = Join-Path $root 'Application\Test'
$log = 'D:\agent\working_station\silver_era\检查\同步检查日志.txt'

"==== 下层与测试层同步检查：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ====" | Out-File -Encoding utf8 $log

$codes = @()
$layers = @(
    @{ Name = 'EngineFoundation'; Dir = 'Engine\EngineFoundation'; Lib = 'EngineFoundation.lib' },
    @{ Name = 'EngineSystem'; Dir = 'Engine\EngineSystem'; Lib = 'EngineSystem.lib' },
    @{ Name = 'EngineRenderer'; Dir = 'Engine\EngineRenderer'; Lib = 'EngineRenderer.lib' }
)

foreach ($layer in $layers)
{
    $dir = Join-Path $root $layer.Dir
    $build = Join-Path $dir 'out\build\x64-Debug'
    "---- 配置下层 $($layer.Name) ----" | Out-File -Encoding utf8 -Append $log
    & $cmake -S $dir -B $build 2>&1 | Out-File -Encoding utf8 -Append $log
    $codes += $LASTEXITCODE
    "---- 构建下层 $($layer.Name) ----" | Out-File -Encoding utf8 -Append $log
    & $cmake --build $build 2>&1 | Out-File -Encoding utf8 -Append $log
    $codes += $LASTEXITCODE
    $lib = Join-Path $build ('lib\' + $layer.Lib)
    $lib_exists = Test-Path $lib
    if (-not $lib_exists)
    {
        "结论：不同步（$($layer.Name) Debug 静态库缺失：$lib）" | Out-File -Encoding utf8 -Append $log
        "结论：不同步 —— $($layer.Name) Debug 静态库缺失，请检查下层构建：$lib"
        exit 1
    }
}

foreach ($name in @('Foundation', 'System', 'Renderer'))
{
    $subBuild = Join-Path $test ("tests\$name\out\build\x64-Debug")
    $sub_exists = Test-Path $subBuild
    if (-not $sub_exists)
    {
        continue
    }
    "---- 构建测试子工程 $name ----" | Out-File -Encoding utf8 -Append $log
    & $cmake --build $subBuild 2>&1 | Out-File -Encoding utf8 -Append $log
    $codes += $LASTEXITCODE
}

"---- 配置测试层 ----" | Out-File -Encoding utf8 -Append $log
& $cmake -S $test -B (Join-Path $test 'out\build\x64-Debug') 2>&1 | Out-File -Encoding utf8 -Append $log
$codes += $LASTEXITCODE
"---- 构建测试层 ----" | Out-File -Encoding utf8 -Append $log
& $cmake --build (Join-Path $test 'out\build\x64-Debug') 2>&1 | Out-File -Encoding utf8 -Append $log
$codes += $LASTEXITCODE

$bad = $codes | Where-Object { $_ -ne 0 }
$bad_count = @($bad).Count
if ($bad_count -gt 0)
{
    "结论：不同步（返回码：$($codes -join ' / ')）" | Out-File -Encoding utf8 -Append $log
    "结论：不同步 —— 引擎层 API 与测试层不一致，请先迁移测试层；详见 $log"
    exit 1
}
"结论：同步（三层与测试层均构建通过）" | Out-File -Encoding utf8 -Append $log
"结论：同步 —— 三层与测试层均构建通过；日志 $log"
exit 0
