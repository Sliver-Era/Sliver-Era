/*
渲染 UI 图集扩容上限与退役回收测试（第九百七十八轮）
覆盖：扩容次数计数、退役纹理数量受上限约束、图集高度不超过上限。
*/
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//中文测试字体
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\simhei.ttf";
	//压力字形数量
	constexpr uint32_t GLYPH_COUNT = 1200;

	//测试字体是否存在
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path(FONT_PATH), code);
	}

	//构造隐藏窗口配置
	Renderer_Config hidden_config(uint32_t width, uint32_t height)
	{
		Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}

	//追加一个 UTF-8 码点
	void utf8_append(std::string& out, uint32_t codepoint)
	{
		if (codepoint < 0x80)
			out.push_back(static_cast<char>(codepoint));
		else if (codepoint < 0x800)
		{
			out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
			out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		}
		else
		{
			out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
			out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		}
	}

	//构造连续汉字串
	std::string cjk_stress_text(void)
	{
		std::string text{};
		text.reserve(static_cast<std::size_t>(GLYPH_COUNT) * 3);
		for (uint32_t index = 0; index < GLYPH_COUNT; ++index)
			utf8_append(text, 0x4E00u + index);
		return text;
	}
}

//渲染 UI 图集扩容上限测试夹具
class Renderer_Ui_Atlas_Limit_Test : public ::testing::Test
{
};

//压力补烘：扩容计数递增、退役纹理受上限约束、高度不超上限
TEST_F(Renderer_Ui_Atlas_Limit_Test, 扩容计数与退役回收)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(640, 200)));
	if (!font_available())
		GTEST_SKIP() << "系统中文测试字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	//初始无扩容
	uint32_t grow_count = 0;
	uint32_t retired_count = 0;
	ASSERT_TRUE(renderer.ui().font_atlas_stats(font, grow_count, retired_count));
	EXPECT_EQ(grow_count, 0u);
	EXPECT_EQ(retired_count, 0u);

	//大批汉字压测
	const std::string text = cjk_stress_text();
	ASSERT_NE(ui->label_create(ui->root_get(),
		Ui_Rect{ 0.0f, 0.0f, 640.0f, 40.0f }, text, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }), 0u);
	ASSERT_TRUE(renderer.frame_render());

	ASSERT_TRUE(renderer.ui().font_atlas_stats(font, grow_count, retired_count));
	EXPECT_GE(grow_count, 1u) << "压力补烘应至少扩容一次";
	EXPECT_LE(retired_count, 8u) << "退役纹理应受上限约束";
	EXPECT_EQ(renderer.ui().font_skipped_codepoints(font), 0u);

	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.ui().font_atlas_size(font, width, height));
	EXPECT_LE(height, 8192u) << "图集高度不得超过扩容上限";
}
