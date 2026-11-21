param([switch]$Configure)
# 构建 Renderer 测试子工程（Debug）。-Configure 时先重新配置。
# 说明：MSVC lib.exe 对中文对象名的 @响应文件按 ANSI 解析会失败，
#       因此在每次 ninja 规则生成后把归档器替换为补 BOM 的包装脚本。
$ErrorActionPreference = 'Stop'
chcp 65001 | Out-Null
$env:TMP = 'C:\Temp'
$env:TEMP = 'C:\Temp'
& 'D:\ViusalStudioCommunity2026\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$cmake = 'D:\ViusalStudioCommunity2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$src = 'd:\代码仓库\白银纪元\Application\Test\tests\Renderer'
$bin = Join-Path $src 'out\build\x64-Debug'
if ($Configure) {
    & $cmake -S $src -B $bin 2>&1 | Select-Object -Last 2
}
$rules = Join-Path $bin 'CMakeFiles\rules.ninja'
$text = [System.IO.File]::ReadAllText($rules, [System.Text.Encoding]::UTF8)
$text = $text.Replace('D:\ViusalStudioCommunity2026\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\lib.exe /nologo', 'D:/agent/working_station/silver_era/工具/lib_utf8_bom.bat /nologo')
[System.IO.File]::WriteAllText($rules, $text, (New-Object System.Text.UTF8Encoding($false)))
& $cmake --build $bin --config Debug 2>&1 | Select-Object -Last 12
