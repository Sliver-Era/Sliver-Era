/*
渲染 UI 文本省略测试（第八百九十五轮）
覆盖：省略开关存取与未知句柄、超宽单行开启省略后绘制结果变化。
像素用例按回读能力与字体可用性门控。
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
}

//渲染 UI 文本省略测试夹具
class Renderer_Ui_Text_Ellipsis_Test : public ::testing::Test
{
};

//省略开关默认关闭、可设置读回、未知句柄拒绝
TEST_F(Renderer_Ui_Text_Ellipsis_Test, 省略开关存取)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 60.0f, 20.0f }, "文本");
	ASSERT_NE(label, 0u);
	//默认不省略
	EXPECT_FALSE(ui.text_ellipsis_get(label));
	ASSERT_TRUE(ui.text_ellipsis_set(label, true));
	EXPECT_TRUE(ui.text_ellipsis_get(label));
	ASSERT_TRUE(ui.text_ellipsis_set(label, false));
	EXPECT_FALSE(ui.text_ellipsis_get(label));
	//未知句柄
	EXPECT_FALSE(ui.text_ellipsis_set(label + 100, true));
	EXPECT_FALSE(ui.text_ellipsis_get(label + 100));
}

//超宽单行：开启省略后尾部改为省略号，绘制结果发生变化
TEST_F(Renderer_Ui_Text_Ellipsis_Test, 省略改变绘制)
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

	//矩形宽度远小于文本宽度
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 60.0f, 24.0f }, "ABCDEFGHIJKLMNOPQRST");
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 0.0f, Ui_Align::Center));

	//未开启省略
	std::vector<uint8_t> without(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(without.data(), static_cast<uint32_t>(without.size())));
	//开启省略
	ASSERT_TRUE(ui->text_ellipsis_set(label, true));
	std::vector<uint8_t> with(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(with.data(), static_cast<uint32_t>(with.size())));
	//两帧应有可见差异（尾部字符被省略号替换）
	EXPECT_GT(byjy_render_test::max_channel_diff(without, with), 0) << "开启省略后绘制结果应变化";
}
