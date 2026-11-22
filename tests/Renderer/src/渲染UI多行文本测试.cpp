/*
渲染 UI 多行文本测试（第八百九十四轮）
覆盖：多行布局参数存取与默认值、显式换行按行高分行绘制、
      自动换行在区域宽度内折行绘制。
像素用例按回读能力门控。
*/
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//测试字体（与既有 UI 文本用例一致）
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

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

	//统计矩形区域内亮度超过阈值的像素数
	uint32_t bright_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0,
		uint32_t x1, uint32_t y1, uint32_t row_width, float threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1; ++y)
		{
			for (uint32_t x = x0; x < x1; ++x)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y, row_width)) >= threshold)
					++count;
			}
		}
		return count;
	}
}

//渲染 UI 多行文本测试夹具
class Renderer_Ui_Text_Block_Test : public ::testing::Test
{
};

//多行布局参数：默认值、写入读回、负行高归零、未知句柄
TEST_F(Renderer_Ui_Text_Block_Test, 多行布局存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 60.0f }, "文本");
	ASSERT_NE(label, 0u);
	//默认：不换行、行高取字体、垂直居中
	bool wrap = true;
	float line_height = -1.0f;
	Ui_Align valign = Ui_Align::Start;
	ui.text_layout_get(label, wrap, line_height, valign);
	EXPECT_FALSE(wrap);
	EXPECT_FLOAT_EQ(line_height, 0.0f);
	EXPECT_EQ(valign, Ui_Align::Center);
	//写入读回
	ASSERT_TRUE(ui.text_layout_set(label, true, 24.0f, Ui_Align::End));
	ui.text_layout_get(label, wrap, line_height, valign);
	EXPECT_TRUE(wrap);
	EXPECT_FLOAT_EQ(line_height, 24.0f);
	EXPECT_EQ(valign, Ui_Align::End);
	//负行高按字体高度
	ASSERT_TRUE(ui.text_layout_set(label, false, -8.0f, Ui_Align::Start));
	ui.text_layout_get(label, wrap, line_height, valign);
	EXPECT_FLOAT_EQ(line_height, 0.0f);
	EXPECT_EQ(valign, Ui_Align::Start);
	//未知句柄
	EXPECT_FALSE(ui.text_layout_set(label + 100, true, 20.0f, Ui_Align::Start));
	ui.text_layout_get(label + 100, wrap, line_height, valign);
	EXPECT_FALSE(wrap);
	EXPECT_EQ(valign, Ui_Align::Center);
}

//显式换行：两行分别落在各自行高带内，第三带为空
TEST_F(Renderer_Ui_Text_Block_Test, 显式换行分行绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//加载并绑定测试字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 100.0f }, "HH\nHH");
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 24.0f, Ui_Align::Start));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//第一行与第二行均有亮像素
	const uint32_t first_line = bright_count(pixels, 20, 20, 220, 44, 320, 120.0f);
	const uint32_t second_line = bright_count(pixels, 20, 44, 220, 68, 320, 120.0f);
	const uint32_t third_band = bright_count(pixels, 20, 68, 220, 100, 320, 120.0f);
	EXPECT_GT(first_line, 0u) << "第一行应绘制文本";
	EXPECT_GT(second_line, 0u) << "换行后的第二行应绘制文本";
	EXPECT_EQ(third_band, 0u) << "两行以外区域不应有文本";
}

//自动换行：窄区域内的长串折成多行
TEST_F(Renderer_Ui_Text_Block_Test, 自动换行绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//加载并绑定测试字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 40.0f, 120.0f }, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, true, 20.0f, Ui_Align::Start));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//至少前两行有内容（说明发生了折行）
	const uint32_t first_line = bright_count(pixels, 20, 20, 60, 40, 320, 120.0f);
	const uint32_t second_line = bright_count(pixels, 20, 40, 60, 60, 320, 120.0f);
	EXPECT_GT(first_line, 0u) << "第一行应绘制文本";
	EXPECT_GT(second_line, 0u) << "窄区域应触发生成第二行";
}
