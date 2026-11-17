# 白银纪元源码行数统计
# 用法：powershell -ExecutionPolicy Bypass -File "D:\agent\working_station\silver_era\检查\源码行数统计.ps1"
# 口径：
#   总行数   = 文件全部行（含空行与纯空白行）
#   有效行   = 去掉空行 / 纯空白行后的行数
#   统计范围 = Engine/EngineFoundation、Engine/EngineSystem、Engine/EngineRenderer、Application/Test、Application/Editor、Application/Game/星屑弹幕（与当前工程树一致，各层按自带的 CMakeLists.txt 划分）
#   排除目录 = external（第三方副本）、out/build（构建产物）、Debug/Release（产物拷贝）、版本缓存（下层头快照）、.git/.vs/.cyrene/.trae 等工具目录
#   源码后缀 = .h .hpp .hxx .c .cpp .cc .cxx .inl .glsl .vert .frag .comp .geom .tesc .tese
#   明细分组 = 层内以 src / source 为源码根，按模块目录汇总；源码根下首级目录若不含直接源码文件则下钻一级，最多两级

param(
    [string]$Root = "d:\代码仓库\白银纪元"
)

$extensions = @('.h', '.hpp', '.hxx', '.c', '.cpp', '.cc', '.cxx', '.inl',
                '.glsl', '.vert', '.frag', '.comp', '.geom', '.tesc', '.tese')
$excludeNames = @('external', 'out', 'build', 'debug', 'release',
                  '.git', '.vs', '.cyrene', '.trae', 'node_modules', '下层依赖', '版本缓存')
$sourceRootNames = @('src', 'source')

# 各层（相对项目根）
$layers = @(
    'Engine\EngineFoundation',
    'Engine\EngineSystem',
    'Engine\EngineRenderer',
    'Application\Test',
    'Application\Editor',
    'Application\Game\星屑弹幕'
)

# 判断路径中是否包含被排除目录
function Test_Excluded([string]$relative_path)
{
    $parts = $relative_path -split '[\\/]'
    foreach ($part in $parts)
    {
        if ($excludeNames -contains $part.ToLower())
        {
            return $true
        }
    }
    return $false
}

# 收集某层下的源码文件（返回相对该层的路径）
function Get_Source_Files([string]$layer_full)
{
    $items = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path $layer_full))
    {
        return ,$items
    }
    $files = Get-ChildItem -Path $layer_full -Recurse -File -ErrorAction SilentlyContinue
    foreach ($file in $files)
    {
        if ($extensions -notcontains $file.Extension.ToLower())
        {
            continue
        }
        $relative = $file.FullName.Substring($layer_full.Length + 1)
        if (Test_Excluded $relative)
        {
            continue
        }
        $items.Add($relative)
    }
    return ,$items
}

# 收集直接容纳源码文件的目录集合（用于判断分组是否下钻一级）
function Get_Direct_Dir_Set($relative_files)
{
    $set = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($relative in $relative_files)
    {
        $parts = $relative -split '[\\/]'
        $dir = (@($parts | Select-Object -First ($parts.Length - 1)) -join '\')
        [void]$set.Add($dir.ToLower())
    }
    return ,$set
}

# 分组长名（相对层路径 → 模块分组）
function Get_Group_Name([string]$relative, $direct_dirs)
{
    $parts = $relative -split '[\\/]'
    $dir_parts = @($parts | Select-Object -First ($parts.Length - 1))
    # 若源码文件直接位于层根
    if ($dir_parts.Length -eq 0)
    {
        return '(根)'
    }
    # 在目录路径中定位源码根
    $root_index = -1
    for ($i = 0; $i -lt $dir_parts.Length; $i += 1)
    {
        if ($sourceRootNames -contains $dir_parts[$i].ToLower())
        {
            $root_index = $i
            break
        }
    }
    # 层内无源码根目录时按首级目录汇总
    if ($root_index -lt 0)
    {
        return $dir_parts[0]
    }
    $depth = 1
    # 若首级目录不含直接源码文件且存在下一级
    if (($root_index + 1) -lt $dir_parts.Length)
    {
        $first_full = (@($dir_parts[0..($root_index + 1)]) -join '\')
        # 下钻一级，最多两级
        if (-not $direct_dirs.Contains($first_full.ToLower()))
        {
            $depth = 2
        }
    }
    $last = [Math]::Min($root_index + $depth, $dir_parts.Length - 1)
    return (@($dir_parts[0..$last]) -join '/')
}

# 统计单个文件
function Get_File_Counts([string]$full_path)
{
    $lines = [System.IO.File]::ReadAllLines($full_path, [System.Text.Encoding]::UTF8)
    $total = $lines.Length
    $effective = 0
    foreach ($line in $lines)
    {
        if ($line.Trim().Length -gt 0)
        {
            $effective += 1
        }
    }
    return @{ Total = $total; Effective = $effective }
}

$overall_files = 0
$overall_total = 0
$overall_effective = 0
$layer_rows = New-Object System.Collections.Generic.List[object]
$group_rows = New-Object System.Collections.Generic.List[object]

foreach ($layer in $layers)
{
    $layer_full = Join-Path $Root $layer
    $relative_files = Get_Source_Files $layer_full
    $direct_dirs = Get_Direct_Dir_Set $relative_files
    $layer_files = 0
    $layer_total = 0
    $layer_effective = 0

    $groups = @{}
    foreach ($relative in $relative_files)
    {
        $counts = Get_File_Counts (Join-Path $layer_full $relative)
        $layer_files += 1
        $layer_total += $counts.Total
        $layer_effective += $counts.Effective

        $group = Get_Group_Name $relative $direct_dirs
        if (-not $groups.ContainsKey($group))
        {
            $groups[$group] = @{ Files = 0; Total = 0; Effective = 0 }
        }
        $groups[$group].Files += 1
        $groups[$group].Total += $counts.Total
        $groups[$group].Effective += $counts.Effective
    }

    $overall_files += $layer_files
    $overall_total += $layer_total
    $overall_effective += $layer_effective

    $layer_rows.Add([pscustomobject]@{
        Layer = $layer
        Files = $layer_files
        Total = $layer_total
        Effective = $layer_effective
    })

    foreach ($key in ($groups.Keys | Sort-Object))
    {
        $group_rows.Add([pscustomobject]@{
            Layer = $layer
            Group = $key
            Files = $groups[$key].Files
            Total = $groups[$key].Total
            Effective = $groups[$key].Effective
        })
    }
}

Write-Output "===== 白银纪元 · 源码行数统计 ====="
Write-Output ("项目根：{0}" -f $Root)
Write-Output "口径：总行数含空行；有效行 = 非空非纯空白行；已排除 external / out / build / Debug / Release / 版本缓存 / 工具目录"
Write-Output ""

Write-Output "【总计】"
Write-Output ("  文件数   : {0}" -f $overall_files)
Write-Output ("  总行数   : {0}" -f $overall_total)
Write-Output ("  有效行   : {0}" -f $overall_effective)
Write-Output ""

Write-Output "【各层汇总】"
$layer_rows | Format-Table -AutoSize Layer, Files, Total, Effective
Write-Output ""

Write-Output "【各模块明细】"
$group_rows | Sort-Object Effective -Descending | Format-Table -AutoSize Layer, Group, Files, Total, Effective
