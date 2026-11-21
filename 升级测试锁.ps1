# =============================================================================
# 白银纪元 · 测试锁成套升级
# -----------------------------------------------------------------------------
# 用途：把 Engine/版本锁.cmake 的六条锁（三层 × Debug/Release）一次性升级为
#       「各层当前构建树的版本哈希」，并打印每条的旧值 → 新值。
#
# 为什么必须成套
#     锁是全链共享的：只改 Foundation 而 System / Renderer 仍按旧 Foundation 构建，
#     链接期必然锚点失配（LNK2019）；版本工具还会做「被锁层构建时用的下层版本」
#     一致性校验，不成套会直接 FATAL_ERROR。
#
# 用法：
#   .\升级测试锁.ps1            # 读取构建树当前版本，写入版本锁
#   .\升级测试锁.ps1 -DryRun    # 只打印将要写入的内容，不改文件
#
# 前置：三层各自的 Debug / Release 构建树都已存在（out/build/x64-<配置>/include）。
#       任一缺失即中止 —— 不做「部分升级」。
# =============================================================================

param(
    [string]$ProjectRoot = "D:\代码仓库\白银纪元",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$lockFile = Join-Path $ProjectRoot "Engine\版本锁.cmake"
$layers = @("EngineFoundation", "EngineSystem", "EngineRenderer")
$configs = @("Debug", "Release")

# 读某层某配置的版本哈希（来自构建树导出的版本锚点头）
function Get-VersionHash([string]$layer, [string]$cfg) {
    $header = Join-Path $ProjectRoot ("Engine\{0}\out\build\x64-{1}\include\Engine\{0}\版本锚点.h" -f $layer, $cfg)
    if (-not (Test-Path $header)) {
        return $null
    }
    $match = Select-String -Path $header -Pattern "byjy_stamp_.*_([0-9a-f]+)" | Select-Object -First 1
    if (-not $match) {
        return $null
    }
    return $match.Matches[0].Groups[1].Value
}

# 先取齐六条，缺任何一条即中止
$resolved = @{}
$missing = @()
foreach ($layer in $layers) {
    foreach ($cfg in $configs) {
        $hash = Get-VersionHash $layer $cfg
        if (-not $hash) {
            $missing += ("{0} / {1}" -f $layer, $cfg)
        }
        else {
            $resolved["${layer}_${cfg}"] = $hash
        }
    }
}
if ($missing.Count -gt 0) {
    throw ("以下层 / 配置没有构建树，无法成套升级：`n  {0}`n请先构建对应层后再运行本脚本。" -f ($missing -join "`n  "))
}

if (-not (Test-Path $lockFile)) {
    throw ("未找到版本锁文件：{0}" -f $lockFile)
}

$text = Get-Content $lockFile -Raw -Encoding UTF8

# 逐条替换；文件中不存在的条目追加到末尾
foreach ($layer in $layers) {
    foreach ($cfg in $configs) {
        $new = $resolved["${layer}_${cfg}"]
        $pattern = '(?m)^set\(BYJY_LOCK_{0}_{1}\s+"[^"]*"\)' -f [regex]::Escape($layer), $cfg
        $found = [regex]::Match($text, $pattern)
        if ($found.Success) {
            $old = [regex]::Match($found.Value, '"([^"]*)"').Groups[1].Value
            Write-Host ("{0,-18} {1,-8} {2} -> {3}" -f $layer, $cfg, $(if ($old) { $old } else { "<空>" }), $new)
            $replacement = 'set(BYJY_LOCK_{0}_{1} "{2}")' -f $layer, $cfg, $new
            $text = $text.Substring(0, $found.Index) + $replacement + $text.Substring($found.Index + $found.Length)
        }
        else {
            Write-Host ("{0,-18} {1,-8} <缺失> -> {2}" -f $layer, $cfg, $new)
            $text = $text.TrimEnd() + "`r`n" + ('set(BYJY_LOCK_{0}_{1} "{2}")' -f $layer, $cfg, $new) + "`r`n"
        }
    }
}

if ($DryRun) {
    Write-Host "DryRun：未写入文件。" -ForegroundColor Yellow
}
else {
    Set-Content -Path $lockFile -Value $text -Encoding UTF8 -NoNewline
    Write-Host ("版本锁已成套升级：{0}" -f $lockFile) -ForegroundColor Green
}
