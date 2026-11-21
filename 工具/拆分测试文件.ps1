param(
    [Parameter(Mandatory=$true)][string]$Src,
    [string]$AuxName = '',
    [int]$AuxStart = 1,
    [int]$AuxEnd = 1,
    [Parameter(Mandatory=$true)][string]$Spec
)
# Spec 形如：文件名.cpp|头部说明文件.txt|405;文件名2.cpp|头部2.txt|1121
# 每个文件的第三段可含多个锚点（TEST 行号）并用逗号分隔，表示合并多个不连续区间。
$ErrorActionPreference = 'Stop'
$srcFull = [System.IO.Path]::GetFullPath($Src)
$srcDir = [System.IO.Path]::GetDirectoryName($srcFull)
$L = [System.IO.File]::ReadAllLines($srcFull, [System.Text.Encoding]::UTF8)
$utf8 = New-Object System.Text.UTF8Encoding($true)

# 求某 TEST 行号所属「用例注释块」起始行（无注释块则取 TEST 行本身）
function Section-Start([int]$testLine) {
    $i = $testLine - 1
    while ($i -ge 1 -and $L[$i - 1].Trim() -eq '') { $i-- }
    if ($i -ge 1 -and $L[$i - 1].Trim() -eq '*/') {
        while ($i -ge 1 -and -not $L[$i - 1].Trim().StartsWith('/*')) { $i-- }
    } else {
        $i++
    }
    return $i
}
function Slice([int]$a, [int]$b) {
    if ($b -lt $a) { return @() }
    return $L[($a - 1)..($b - 1)]
}

# 生成共享辅助头（AuxName 为空表示各文件自带完整头部，不抽公共头）
if ($AuxName) {
    $aux = @('#pragma once')
    $aux += (Slice $AuxStart $AuxEnd)
    [System.IO.File]::WriteAllText((Join-Path $srcDir $AuxName), ($aux -join "`r`n") + "`r`n", $utf8)
    Write-Output ("{0}`t{1}" -f $AuxName, $aux.Count)
}

# 解析分组与锚点
$items = @()
foreach ($part in $Spec.Split(';')) {
    $f = $part.Split('|')
    $items += [pscustomobject]@{ Name = $f[0]; Head = $f[1]; Anchors = ($f[2].Split(',') | ForEach-Object { [int]$_ }); Text = @() }
}
$flat = @()
for ($i = 0; $i -lt $items.Count; $i++) {
    foreach ($a in $items[$i].Anchors) { $flat += [pscustomobject]@{ File = $i; Anchor = $a; Start = (Section-Start $a) } }
}
$flat = $flat | Sort-Object Start
for ($i = 0; $i -lt $flat.Count; $i++) {
    $end = if ($i -lt $flat.Count - 1) { $flat[$i + 1].Start - 1 } else { $L.Count }
    $chunk = Slice $flat[$i].Start $end
    $items[$flat[$i].File].Text += $chunk
    $items[$flat[$i].File].Text += ''
}

# 逐文件写出
for ($i = 0; $i -lt $items.Count; $i++) {
    $out = @([System.IO.File]::ReadAllLines($items[$i].Head, [System.Text.Encoding]::UTF8))
    $out += ''
    if ($AuxName) { $out += ('#include "' + $AuxName + '"') }
    $out += ''
    $out += $items[$i].Text
    while ($out.Count -gt 0 -and $out[$out.Count - 1].Trim() -eq '') {
        if ($out.Count -eq 1) { $out = @() } else { $out = $out[0..($out.Count - 2)] }
    }
    [System.IO.File]::WriteAllText((Join-Path $srcDir $items[$i].Name), ($out -join "`r`n") + "`r`n", $utf8)
    Write-Output ("{0}`t{1}" -f $items[$i].Name, $out.Count)
}
