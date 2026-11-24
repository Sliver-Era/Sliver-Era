/*
渲染 UI 多行文本测试（第八百九十四轮）
覆盖：多行布局参数存取与默认值、显式换行按行高分行绘制、
      自动换行在区域宽度内折行绘制；第九百三十九轮补首行缩进存取与绘制。
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

	//扫描区域内最左侧亮像素横坐标（无亮像素返回 -1）
	int leftmost_lit(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t x1,
		uint32_t y0, uint32_t y1, float threshold)
	{
		for (uint32_t x = x0; x < x1; ++x)
		{
			for (uint32_t y = y0; y < y1; ++y)
			{
				//亮度达到阈值的像素视为文本墨迹
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y, 320)) >= threshold)
					return static_cast<int>(x);
			}
		}
		return -1;
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

//首行缩进：默认 0、读写负值、未知句柄（第九百三十九轮）
TEST_F(Renderer_Ui_Text_Block_Test, 首行缩进存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 60.0f }, "文本");
	ASSERT_NE(label, 0u);
	EXPECT_FLOAT_EQ(ui.text_indent_get(label), 0.0f);

	//正值右移、负值外突
	ASSERT_TRUE(ui.text_indent_set(label, 12.0f));
	EXPECT_FLOAT_EQ(ui.text_indent_get(label), 12.0f);
	ASSERT_TRUE(ui.text_indent_set(label, -4.0f));
	EXPECT_FLOAT_EQ(ui.text_indent_get(label), -4.0f);

	//未知句柄
	EXPECT_FALSE(ui.text_indent_set(label + 100, 8.0f));
	EXPECT_FLOAT_EQ(ui.text_indent_get(label + 100), 0.0f);
}

//首行缩进：首行起点右移、第二行不动，End 对齐同样叠加（第九百三十九轮）
TEST_F(Renderer_Ui_Text_Block_Test, 首行缩进绘制)
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
	const int base_first = leftmost_lit(pixels, 20, 220, 20, 44, 120.0f);
	const int base_second = leftmost_lit(pixels, 20, 220, 44, 68, 120.0f);
	ASSERT_GE(base_first, 20);
	EXPECT_EQ(base_second, base_first) << "未缩进时两行同起点";

	//缩进 12：仅首行右移
	ASSERT_TRUE(ui->text_indent_set(label, 12.0f));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const int indented_first = leftmost_lit(pixels, 20, 220, 20, 44, 120.0f);
	const int indented_second = leftmost_lit(pixels, 20, 220, 44, 68, 120.0f);
	EXPECT_EQ(indented_first, base_first + 12) << "首行应右移缩进量";
	EXPECT_EQ(indented_second, base_second) << "第二行不受缩进影响";

	//End 对齐：先取无缩进基线，再加缩进应同样右移 12
	ASSERT_TRUE(ui->text_indent_set(label, 0.0f));
	ASSERT_TRUE(ui->text_halign_set(label, Ui_Align::End));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const int end_first = leftmost_lit(pixels, 20, 220, 20, 44, 120.0f);
	const int end_second = leftmost_lit(pixels, 20, 220, 44, 68, 120.0f);
	ASSERT_TRUE(ui->text_indent_set(label, 12.0f));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(leftmost_lit(pixels, 20, 220, 20, 44, 120.0f), end_first + 12) << "End 对齐首行应再右移缩进量";
	EXPECT_EQ(leftmost_lit(pixels, 20, 220, 44, 68, 120.0f), end_second) << "End 对齐第二行不受缩进影响";
}
