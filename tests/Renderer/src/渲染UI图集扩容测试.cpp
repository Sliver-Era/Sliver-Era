/*
渲染 UI 图集扩容测试（第九百七十轮）
覆盖：大量中文字形补烘触发图集高度翻倍、扩容后无跳过码点、图集宽度不变。
像素无关，只读诊断接口与绘制统计。
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
	//中文测试字体（TrueType，覆盖常用汉字）
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\simhei.ttf";
	//压力字形数量（超过初始图集余量，触发扩容）
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

//渲染 UI 图集扩容测试夹具
class Renderer_Ui_Atlas_Grow_Test : public ::testing::Test
{
};

//中文压力补烘：图集高度翻倍且无跳过码点
TEST_F(Renderer_Ui_Atlas_Grow_Test, 图集扩容与压力补烘)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(640, 200)));
	if (!font_available())
		GTEST_SKIP() << "系统中文测试字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//加载并绑定中文测试字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	//初始图集尺寸
	uint32_t initial_width = 0;
	uint32_t initial_height = 0;
	ASSERT_TRUE(renderer.ui().font_atlas_size(font, initial_width, initial_height));
	ASSERT_GT(initial_width, 0u);
	ASSERT_GT(initial_height, 0u);

	//大批汉字单行绘制：补烘过程应触发扩容
	const std::string text = cjk_stress_text();
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 0.0f, 0.0f, 640.0f, 40.0f }, text, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(renderer.frame_render());

	uint32_t grown_width = 0;
	uint32_t grown_height = 0;
	ASSERT_TRUE(renderer.ui().font_atlas_size(font, grown_width, grown_height));
	EXPECT_EQ(grown_width, initial_width) << "扩容只增加高度，宽度不变";
	EXPECT_GT(grown_height, initial_height) << "压力补烘应触发图集扩容";
	EXPECT_EQ(renderer.ui().font_skipped_codepoints(font), 0u) << "扩容后不应跳过码点";
	EXPECT_GT(renderer.ui().ui_render_stats().text_draws, 0u) << "压力文本应有文字绘制";

	//重复绘制同一文本：字形已齐备，图集不再变化
	ASSERT_TRUE(renderer.frame_render());
	uint32_t stable_width = 0;
	uint32_t stable_height = 0;
	ASSERT_TRUE(renderer.ui().font_atlas_size(font, stable_width, stable_height));
	EXPECT_EQ(stable_width, grown_width);
	EXPECT_EQ(stable_height, grown_height) << "字形已齐备时不应重复扩容";
}
