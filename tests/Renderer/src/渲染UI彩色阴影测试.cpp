/*
渲染 UI 彩色阴影测试（第九百六十八轮）
覆盖：阴影副本继承行内区间色后按段切分，统计记录数随区间变化。
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
}

//渲染 UI 彩色阴影测试夹具
class Renderer_Ui_Shadow_Color_Test : public ::testing::Test
{
};

//阴影副本按区间切分：区间使阴影与正文各多一条文字记录
TEST_F(Renderer_Ui_Shadow_Color_Test, 阴影继承区间色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//加载并绑定测试字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 40.0f }, "AA", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 0.0f, Ui_Align::Start));

	//基线：无区间无阴影，整行一条记录
	ASSERT_TRUE(renderer.frame_render());
	const uint32_t base_draws = renderer.ui().ui_render_stats().text_draws;
	EXPECT_EQ(base_draws, 1u);

	//区间：正文按段切分
	ASSERT_TRUE(ui->text_span_add(label, 0, 1, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws + 1u) << "区间切分应新增一条正文记录";

	//阴影：偏移副本同样按区间切分，记录再 +2
	ASSERT_TRUE(ui->text_shadow_set(label, Ui_Text_Shadow{ 2.0f, 2.0f, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f } }));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws + 3u) << "阴影应按区间色分两段绘制";

	//清空区间：阴影与正文各回一条记录
	ASSERT_TRUE(ui->text_span_clear(label));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws + 1u) << "无区间时阴影整行一条记录";

	//关闭阴影：回到单条记录
	ASSERT_TRUE(ui->text_shadow_set(label, Ui_Text_Shadow{}));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws);
}
