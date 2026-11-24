/*
渲染 UI 文本阴影测试（第九百四十轮）
覆盖：阴影参数存取与默认值、开启后文字记录 +1、偏移副本像素出现。
像素用例按回读能力门控。
*/
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
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

	//统计矩形区域内亮度达到阈值的像素数
	uint32_t bright_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0,
		uint32_t x1, uint32_t y1, float threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1; ++y)
		{
			for (uint32_t x = x0; x < x1; ++x)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y, 320)) >= threshold)
					++count;
			}
		}
		return count;
	}
}

//渲染 UI 文本阴影测试夹具
class Renderer_Ui_Text_Shadow_Test : public ::testing::Test
{
};

//阴影参数：默认关闭、写入读回、未知句柄
TEST_F(Renderer_Ui_Text_Shadow_Test, 阴影参数存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, "文本");
	ASSERT_NE(label, 0u);
	//默认：偏移 0、alpha 0（关闭）
	const Ui_Text_Shadow default_shadow = ui.text_shadow_get(label);
	EXPECT_FLOAT_EQ(default_shadow.offset_x, 0.0f);
	EXPECT_FLOAT_EQ(default_shadow.offset_y, 0.0f);
	EXPECT_FLOAT_EQ(default_shadow.color.a, 0.0f);

	//写入读回
	Ui_Text_Shadow shadow{};
	shadow.offset_x = 3.0f;
	shadow.offset_y = -2.0f;
	shadow.color = Ui_Color{ 0.0f, 0.0f, 0.0f, 0.6f };
	ASSERT_TRUE(ui.text_shadow_set(label, shadow));
	const Ui_Text_Shadow read_back = ui.text_shadow_get(label);
	EXPECT_FLOAT_EQ(read_back.offset_x, 3.0f);
	EXPECT_FLOAT_EQ(read_back.offset_y, -2.0f);
	EXPECT_FLOAT_EQ(read_back.color.a, 0.6f);

	//未知句柄
	EXPECT_FALSE(ui.text_shadow_set(label + 100, shadow));
	const Ui_Text_Shadow unknown = ui.text_shadow_get(label + 100);
	EXPECT_FLOAT_EQ(unknown.offset_x, 0.0f);
	EXPECT_FLOAT_EQ(unknown.offset_y, 0.0f);
	EXPECT_FLOAT_EQ(unknown.color.a, 0.0f);
}

//阴影绘制：记录 +1，偏移带出现亮像素，关闭后消失
TEST_F(Renderer_Ui_Text_Shadow_Test, 阴影绘制记录与像素偏移)
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

	//正文取黑色（暗底不可见），阴影取白色并下移 30 像素
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 100.0f }, "HH", Ui_Color{ 0.0f, 0.0f, 0.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 24.0f, Ui_Align::Start));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const uint32_t base_draws = renderer.ui().ui_render_stats().text_draws;
	const uint32_t base_shadow_band = bright_count(pixels, 20, 50, 220, 76, 120.0f);
	EXPECT_EQ(base_shadow_band, 0u) << "未开启阴影时偏移带不应有亮像素";

	//开启白色阴影
	Ui_Text_Shadow shadow{};
	shadow.offset_x = 0.0f;
	shadow.offset_y = 30.0f;
	shadow.color = Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(ui->text_shadow_set(label, shadow));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws + 1u) << "阴影应新增一条文字记录";
	EXPECT_GT(bright_count(pixels, 20, 50, 220, 76, 120.0f), 0u) << "偏移带应出现阴影像素";

	//关闭阴影：记录与像素恢复
	ASSERT_TRUE(ui->text_shadow_set(label, Ui_Text_Shadow{}));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws);
	EXPECT_EQ(bright_count(pixels, 20, 50, 220, 76, 120.0f), base_shadow_band);
}
