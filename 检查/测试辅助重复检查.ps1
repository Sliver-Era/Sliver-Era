# 测试辅助重复检查（2026-11-17）
# 用途：阻止用例文件重新引入按文件复制的窗口配置 / 像素读取 / 标准几何构造。
# 口径：只检查 tests/Renderer/src 下的 *.cpp；公共辅助目录 测试辅助/ 不计入。
$root = "d:\代码仓库\白银纪元\Application\Test\tests\Renderer\src"
$files = Get-ChildItem -LiteralPath $root -Recurse -File -Filter *.cpp |
    Where-Object { $_.DirectoryName -notlike "*\测试辅助*" }
$failures = @()

foreach ($file in $files) {
    $text = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)

    if ($text -match 'engine::Renderer_Config hidden_config\(void\)[\s\S]{0,300}?engine::Renderer_Config config;') {
        $failures += ($file.Name + "：本地 hidden_config 实现")
    }
    if ($text -match 'pixel_get\(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y\)[\s\S]{0,250}?std::size_t offset = \(static_cast<std::size_t>\(y\)') {
        $failures += ($file.Name + "：本地 pixel_get 实现")
    }
    if ($text -match 'uint32_t pixel_bytes\(void\)[\s\S]{0,200}?return [A-Za-z0-9_]+ \* [A-Za-z0-9_]+ \* 4;') {
        $failures += ($file.Name + "：本地 pixel_bytes 实现")
    }
    if ($text -match 'engine::Mesh_Data quad_data\(void\)[\s\S]{0,900}?\{ \{ -1\.0f, -1\.0f, 0\.0f \}, \{ 0\.0f, 1\.0f \} \}') {
        $failures += ($file.Name + "：本地标准 quad_data 实现")
    }
    if ($text -match 'engine::Mesh_Data triangle_data\(void\)[\s\S]{0,700}?\{ \{ 0\.0f, 0\.5f, 0\.0f \}, \{ 0\.5f, 0\.0f \} \}') {
        $failures += ($file.Name + "：本地标准 triangle_data 实现")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "测试辅助重复检查：发现 $($failures.Count) 处" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host ("  " + $_) -ForegroundColor Red }
    exit 1
}
Write-Host "测试辅助重复检查：0 处（窗口 / 像素 / 标准几何均已收敛）" -ForegroundColor Green
