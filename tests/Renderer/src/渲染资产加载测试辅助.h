#pragma once
#include <gtest/gtest.h>

//异步用例：轮询等待

#include "Engine/EngineRenderer/src/render/Asset/资产加载.h"
#include "Engine/EngineSystem/src/world/data/scene/Manifest/场景清单.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"
#include "Engine/EngineRenderer/src/render/Asset/资产缓存.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//端到端窗口尺寸
	constexpr uint32_t ASSET_WIDTH = 128;
	constexpr uint32_t ASSET_HEIGHT = 72;
	//端到端覆盖下限（2×2 四边形在默认相机下占屏比例远大于此）
	constexpr uint32_t ASSET_MIN_PIXELS = 800;

	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path asset_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"资产测试");
	}

	//资产文件的 UTF-8 窄路径（供加载器入参；不做任何编码转换）
	std::string asset_utf8(const char* name)
	{
		return std::string("out/资产测试/") + name;
	}

	//写一个 UTF-8 文本文件（目录自动创建）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//整帧平均亮度（Rec.709 加权）
	float average_luma(const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
		return static_cast<float>(sum / static_cast<double>(ASSET_WIDTH * ASSET_HEIGHT));
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

	//第 index 个顶点的法线长度
	float normal_length(const engine::Mesh_Data& data, std::size_t index)
	{
		const engine::Vertex& vertex = data.vertices[index];
		return std::sqrt(vertex.normal[0] * vertex.normal[0]
			+ vertex.normal[1] * vertex.normal[1] + vertex.normal[2] * vertex.normal[2]);
	}

	//写一张 24 位 BMP（测试贴图；stb_image 可直接解码）
	void write_bmp(const std::filesystem::path& path, uint32_t width, uint32_t height)
	{
		//行字节数按 4 字节对齐
		const uint32_t row_bytes = ((width * 3 + 3) / 4) * 4;
		const uint32_t pixel_bytes = row_bytes * height;
		const uint32_t file_bytes = 14 + 40 + pixel_bytes;
		std::vector<uint8_t> bytes(file_bytes, 0);
		//文件头：类型 'BM' ＋ 文件大小 ＋ 保留 ＋ 数据偏移
		bytes[0] = 'B';
		bytes[1] = 'M';
		const uint32_t size = file_bytes;
		bytes[2] = static_cast<uint8_t>(size & 0xFF);
		bytes[3] = static_cast<uint8_t>((size >> 8) & 0xFF);
		bytes[4] = static_cast<uint8_t>((size >> 16) & 0xFF);
		bytes[5] = static_cast<uint8_t>((size >> 24) & 0xFF);
		const uint32_t offset = 54;
		bytes[10] = static_cast<uint8_t>(offset & 0xFF);
		bytes[11] = static_cast<uint8_t>((offset >> 8) & 0xFF);
		//信息头：40 字节 / 宽 / 高 / 平面数 1 / 24 位 / 无压缩
		const uint32_t header_size = 40;
		bytes[14] = static_cast<uint8_t>(header_size & 0xFF);
		bytes[18] = static_cast<uint8_t>(width & 0xFF);
		bytes[19] = static_cast<uint8_t>((width >> 8) & 0xFF);
		bytes[22] = static_cast<uint8_t>(height & 0xFF);
		bytes[23] = static_cast<uint8_t>((height >> 8) & 0xFF);
		bytes[26] = 1;
		bytes[28] = 24;
		//像素：每行 BGR 平铺，行序自下而上（全部填白色 255/255/255）
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				const std::size_t base = 54 + static_cast<std::size_t>(y) * row_bytes + x * 3;
				bytes[base] = 255;
				bytes[base + 1] = 255;
				bytes[base + 2] = 255;
			}
		}
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
}

/*
完整属性 ＋ usemtl 分段（第四百二十二轮）
口径：4 顶点 / 4 纹理坐标 / 1 法线；两个 usemtl 各一个三角形 → 4 个唯一顶点、6 条索引、2 段。
*/
