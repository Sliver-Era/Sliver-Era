#pragma once
#include <gtest/gtest.h>


#include "Engine/EngineSystem/src/world/data/scene/Level/关卡文本.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{

	//端到端窗口尺寸
	constexpr uint32_t LEVEL_WIDTH = 128;
	constexpr uint32_t LEVEL_HEIGHT = 72;

	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path level_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"关卡测试");
	}

	//关卡文件的 UTF-8 窄路径（供读写入参）
	std::string level_utf8(const char* name)
	{
		return std::string("out/关卡测试/") + name;
	}

	//写一个 UTF-8 文本文件（目录自动创建；宽路径写盘）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//两帧之间逐通道差 ≥ 6 的像素数
	uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				if (delta < 0)
					delta = -delta;
				if (maximum < delta)
					maximum = delta;
			}
			if (maximum >= 6)
				++count;
		}
		return count;
	}

	/*
	写立方体几何 ＋ 单材质（第四百七十二轮；卡通用例需要多变法线）
	六个面各自独立法线，供逐物体卡通色阶产生可见差异；正面 / 右侧 / 顶面按外法线逆时针绕序。
	*/
	void write_cube_assets(void)
	{
		write_text(level_dir() / L"cube.obj",
			"v -1 -1 -1\n"
			"v 1 -1 -1\n"
			"v 1 1 -1\n"
			"v -1 1 -1\n"
			"v -1 -1 1\n"
			"v 1 -1 1\n"
			"v 1 1 1\n"
			"v -1 1 1\n"
			"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
			"vn 0 0 -1\nvn 0 0 1\nvn -1 0 0\nvn 1 0 0\nvn 0 -1 0\nvn 0 1 0\n"
			"f 5/1/2 6/2/2 7/3/2\n"
			"f 5/1/2 7/3/2 8/4/2\n"
			"f 6/1/4 7/2/4 3/3/4\n"
			"f 6/1/4 3/3/4 2/4/4\n"
			"f 8/1/6 7/2/6 3/3/6\n"
			"f 8/1/6 3/3/6 4/4/6\n"
			"f 2/1/1 1/2/1 4/3/1\n"
			"f 2/1/1 4/3/1 3/4/1\n"
			"f 5/1/3 1/2/3 4/3/3\n"
			"f 5/1/3 4/3/3 8/4/3\n"
			"f 1/1/5 2/2/5 6/3/5\n"
			"f 1/1/5 6/3/5 5/4/5\n");
		write_text(level_dir() / L"cube.mtl", "newmtl 甲\nKd 0.7 0.5 0.4\nNs 64\n");
	}

	/*
	八盏聚光关卡（第五百一十五轮抽出）
	八盏聚光各照一块地面与遮挡体；无其它前向灯，确保八盏全部登记（前向光源上限 8 盏）。
	*/
	void write_eight_spot_level(void)
	{
		write_text(level_dir() / L"八盏聚光.level",
			"level 八盏聚光\n"
			"camera position 0 8 10\n"
			"camera target 0 0 0\n"
			"light direction 0 -1 0\n"
			"light color 0.1 0.1 0.1\n"
			"light intensity 0.1\n"
			"light spot -7 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -5 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -1 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 1 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 5 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 7 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"mesh 方块 cube.obj\n"
			"mtl 方块材质 cube.mtl\n"
			"place 方块 方块材质 0 -1.5 0 24 0.2 10\n"
			"place 方块 方块材质 -7 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -5 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -1 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 1 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 5 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 7 0.6 0 1.2 1.2 1.2\n");
	}
}

/*
Cluster 路径八盏聚光阴影（第五百二十五轮）
口径：八盏关卡在 Cluster 开启下，关闭 / 开启八盏阴影差异 > 100 像素且瓦片数为 8；
随后关闭 Cluster 再出图，应与此前 Cluster 开启的有影图逐字节一致（两条路径同结果）。
*/
