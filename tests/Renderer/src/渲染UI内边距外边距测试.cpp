/*
渲染 UI 内边距 / 外边距测试（第九百四十七轮）
覆盖：参数存取与负值归零、标签文本按内边距内缩、流式排列按外边距留空。
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
	//测试字体
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

	//构造零输入帧（窗口宽度可变，用于触发布局重算）
	Input_Frame layout_frame(uint32_t window_width, uint32_t window_height)
	{
		Input_Frame input{};
		input.window_width = window_width;
		input.window_height = window_height;
		return input;
	}

	//扫描区域内最左侧亮像素横坐标（无亮像素返回 -1）
	int leftmost_lit(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t x1,
		uint32_t y0, uint32_t y1, float threshold)
	{
		for (uint32_t x = x0; x < x1; ++x)
		{
			for (uint32_t y = y0; y < y1; ++y)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y, 320)) >= threshold)
					return static_cast<int>(x);
			}
		}
		return -1;
	}
}

//渲染 UI 内边距 / 外边距测试夹具
class Renderer_Ui_Spacing_Test : public ::testing::Test
{
};

//参数存取：默认 0、负值归零、未知句柄
TEST_F(Renderer_Ui_Spacing_Test, 内边距外边距存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, "文本");
	ASSERT_NE(label, 0u);
	EXPECT_FLOAT_EQ(ui.padding_get(label), 0.0f);
	EXPECT_FLOAT_EQ(ui.margin_get(label), 0.0f);

	//正值写入、负值按 0
	ASSERT_TRUE(ui.padding_set(label, 8.0f));
	EXPECT_FLOAT_EQ(ui.padding_get(label), 8.0f);
	ASSERT_TRUE(ui.padding_set(label, -3.0f));
	EXPECT_FLOAT_EQ(ui.padding_get(label), 0.0f);
	ASSERT_TRUE(ui.margin_set(label, 5.0f));
	EXPECT_FLOAT_EQ(ui.margin_get(label), 5.0f);
	ASSERT_TRUE(ui.margin_set(label, -2.0f));
	EXPECT_FLOAT_EQ(ui.margin_get(label), 0.0f);

	//未知句柄
	EXPECT_FALSE(ui.padding_set(label + 100, 4.0f));
	EXPECT_FLOAT_EQ(ui.padding_get(label + 100), 0.0f);
	EXPECT_FALSE(ui.margin_set(label + 100, 4.0f));
	EXPECT_FLOAT_EQ(ui.margin_get(label + 100), 0.0f);
}

//内边距：标签文本起点右移缩进量
TEST_F(Renderer_Ui_Spacing_Test, 内边距文本内缩)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 40.0f }, "AA", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 20.0f, Ui_Align::Start));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const int base_left = leftmost_lit(pixels, 20, 220, 20, 60, 120.0f);
	ASSERT_GE(base_left, 20);

	//内边距 10：文本起点右移 10
	ASSERT_TRUE(ui->padding_set(label, 10.0f));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(leftmost_lit(pixels, 20, 220, 20, 60, 120.0f), base_left + 10) << "文本应内缩";

	//归零：恢复
	ASSERT_TRUE(ui->padding_set(label, 0.0f));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(leftmost_lit(pixels, 20, 220, 20, 60, 120.0f), base_left);
}

//外边距：流式横向排列相邻子项按外边距留空
TEST_F(Renderer_Ui_Spacing_Test, 外边距排列留空)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 200.0f, 50.0f },
		Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui.flow_set(panel, Ui_Flow::Horizontal, 0.0f, 0.0f));
	const Ui_Handle first = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 30.0f, 20.0f }, "A");
	const Ui_Handle second = ui.label_create(panel, Ui_Rect{ 0.0f, 0.0f, 30.0f, 20.0f }, "B");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);

	//首帧触发布局
	ui.frame_begin(layout_frame(320, 180));
	Ui_Rect first_rect{};
	Ui_Rect second_rect{};
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 0.0f);
	EXPECT_FLOAT_EQ(second_rect.x, 30.0f);

	//首项外边距 5：首项右移 5，次项再顺延 5
	ASSERT_TRUE(ui.margin_set(first, 5.0f));
	ui.frame_begin(layout_frame(321, 180));
	ASSERT_TRUE(ui.rect_get(first, first_rect));
	ASSERT_TRUE(ui.rect_get(second, second_rect));
	EXPECT_FLOAT_EQ(first_rect.x, 5.0f) << "首项应右移外边距";
	EXPECT_FLOAT_EQ(second_rect.x, 40.0f) << "次项应顺延两段外边距";
}
