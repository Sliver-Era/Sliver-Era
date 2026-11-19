# 后处理 alpha 守卫（第 798 轮）
# 目的：后处理链的中间级不得把输出 alpha 写成常量 1，否则场景 alpha 携带的逐材质色调编号会被冲掉
#       （详见 自主迭代规则 §0.1 判据① 与 790–793 轮报告）。
# 口径：扫描 EngineRenderer/assets/shaders/*.frag 的 out_color 赋值，命中「末尾常量 1.0」即报违规；
#       白名单为「色调映射家族（解码后清编号）」「几何 / 全屏直写片元」「UI / 场景 / 粒子等非后处理链片元」。
param(
    [string]$Root = "d:\代码仓库\白银纪元"
)
$ErrorActionPreference = 'Stop'
$root = $Root
$shaderDir = Join-Path $root 'Engine\EngineRenderer\assets\shaders'
if (-not (Test-Path $shaderDir))
{
    Write-Output "后处理 alpha 守卫：未找到着色器目录 $shaderDir"
    exit 2
}
# 白名单：不以「保留逐材质色调编号」为职责的片元
$allow = @(
    'AgX色调映射', 'Reinhard扩展色调映射', 'Reinhard色调映射', '指数色调映射', '胶片色调映射',
    '色调映射', '双轨色调映射', '材质色调映射',
    '场景', '天空盒', '粒子', '文字', 'UI文字', 'UI纯色', '纯色', '阴影',
    '反向外扩', '亮部提取'
)
# 已知合规但不在白名单的片元（若有，报违规以便复核）
$violations = @()
Get-ChildItem $shaderDir -Filter *.frag | ForEach-Object {
    $name = $_.BaseName
    if ($allow -contains $name) { return }
    $text = Get-Content $_.FullName -Raw
    # 匹配 out_color = vec4(..., 1.0);（含换行与空格差异）
    $pattern = 'out_color\s*=\s*vec4\([^;]*?,\s*1\.0\s*\)\s*;'
    foreach ($match in [regex]::Matches($text, $pattern))
    {
        # 归一化空白后判断是否为「末位常量 1.0」
        $line = ($match.Value -replace '\s+', ' ')
        if ($line -match ',\s*1\.0\s*\)\s*;$')
        {
            $violations += "$name : $line"
        }
    }
}
# 上一轮的 793 复核项（亮度阈值 / 深度描边）已改为透传，故不再白名单豁免；
# 若仍有残留，直接输出违规清单。
if ($violations.Count -eq 0)
{
    Write-Output '后处理 alpha 守卫：0 处（无中间级把 alpha 写死为 1）'
    exit 0
}
Write-Output "后处理 alpha 守卫：$($violations.Count) 处违规"
$violations | ForEach-Object { Write-Output "  $_" }
exit 1
