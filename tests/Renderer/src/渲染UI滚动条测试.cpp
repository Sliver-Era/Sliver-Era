/*
渲染 UI 滚动条测试（第八百九十轮）
覆盖：滚动条绑定校验与解绑、纵向拖拽驱动内容滚动、
      横向拖拽驱动横向滚动、内容不超过视口时偏移保持零。
闭环用例使用隐藏窗口渲染器与输入聚合器。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "Engine/EngineRenderer/src/render/Input/输入聚合.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造隐藏窗口配置
	Renderer_Config hidden_config(uint32_t width, uint32_t height)
	{
		Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}

	//构造指针移动事件
	SDL_Event mouse_motion(float x, float y)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.x = x;
		event.motion.y = y;
		return event;
	}

	//构造鼠标按键事件
	SDL_Event mouse_button(bool down, float x, float y)
	{
		SDL_Event event{};
		event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
		event.button.button = SDL_BUTTON_LEFT;
		event.button.down = down;
		event.button.x = x;
		event.button.y = y;
		return event;
	}
}

//渲染 UI 滚动条测试夹具
class Renderer_Ui_Scroll_Bar_Test : public ::testing::Test
{
};

//绑定参数校验与解绑
TEST_F(Renderer_Ui_Scroll_Bar_Test, 绑定校验与解绑)
{
	Ui_Context ui;
	const Ui_Handle container = ui.panel_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 100.0f, 60.0f }, Ui_Color{});
	const Ui_Handle bar = ui.slider_create(ui.root_get(),
		Ui_Rect{ 110.0f, 0.0f, 20.0f, 60.0f }, 0.0f);
	ASSERT_NE(container, 0u);
	ASSERT_NE(bar, 0u);
	//正常绑定
	EXPECT_TRUE(ui.scroll_bar_bind(bar, container, 200.0f, true));
	//未知滚动条句柄
	EXPECT_FALSE(ui.scroll_bar_bind(bar + 100, container, 200.0f, true));
	//未知目标句柄
	EXPECT_FALSE(ui.scroll_bar_bind(bar, container + 100, 200.0f, true));
	//解绑（目标 0）
	EXPECT_TRUE(ui.scroll_bar_bind(bar, 0, 0.0f, true));
}

//纵向拖拽：滑块取值映射为容器纵向滚动偏移
TEST_F(Renderer_Ui_Scroll_Bar_Test, 纵向拖拽驱动滚动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//容器视口 100×60，内容高 200，最大滚动 140
	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.15f, 0.15f, 0.15f, 1.0f });
	const Ui_Handle content = ui->panel_create(container,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 200.0f }, Ui_Color{ 0.75f, 0.75f, 0.75f, 1.0f });
	//纵向滚动条位于容器右侧
	const Ui_Handle bar = ui->slider_create(ui->root_get(),
		Ui_Rect{ 130.0f, 20.0f, 20.0f, 60.0f }, 0.0f);
	ASSERT_NE(container, 0u);
	ASSERT_NE(content, 0u);
	ASSERT_NE(bar, 0u);
	ASSERT_TRUE(ui->clip_children_set(container, true));
	ASSERT_TRUE(ui->scroll_bar_bind(bar, container, 200.0f, true));
	ASSERT_TRUE(renderer.frame_render());

	//按下滚动条 91.67% 处：偏移应为 140 × 0.9167 ≈ 128.33
	SDL_Event down = mouse_button(true, 140.0f, 75.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_NEAR(scroll_y, 128.333f, 0.5f);
	//内容矩形随之上移
	Ui_Rect content_rect{};
	ASSERT_TRUE(ui->rect_get(content, content_rect));
	EXPECT_NEAR(content_rect.y, 20.0f - scroll_y, 0.5f);

	//拖拽到滚动条中点：偏移应为 70
	SDL_Event motion = mouse_motion(140.0f, 50.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_NEAR(scroll_y, 70.0f, 0.5f);
	ASSERT_TRUE(ui->rect_get(content, content_rect));
	EXPECT_NEAR(content_rect.y, -50.0f, 0.5f);

	//抬起结束拖拽
	SDL_Event up = mouse_button(false, 140.0f, 50.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_NEAR(scroll_y, 70.0f, 0.5f);
}

//横向拖拽：滑块按横坐标取值，驱动容器横向滚动
TEST_F(Renderer_Ui_Scroll_Bar_Test, 横向拖拽驱动横向滚动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//容器视口宽 100，内容宽 200，最大横向滚动 100
	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.15f, 0.15f, 0.15f, 1.0f });
	const Ui_Handle content = ui->panel_create(container,
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 60.0f }, Ui_Color{ 0.75f, 0.75f, 0.75f, 1.0f });
	//横向滚动条位于容器下方
	const Ui_Handle bar = ui->slider_create(ui->root_get(),
		Ui_Rect{ 20.0f, 100.0f, 60.0f, 10.0f }, 0.0f);
	ASSERT_NE(container, 0u);
	ASSERT_NE(content, 0u);
	ASSERT_NE(bar, 0u);
	ASSERT_TRUE(ui->clip_children_set(container, true));
	ASSERT_TRUE(ui->scroll_bar_bind(bar, container, 200.0f, false));
	ASSERT_TRUE(renderer.frame_render());

	//按下滚动条中点：横向偏移应为 50
	SDL_Event down = mouse_button(true, 50.0f, 105.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_NEAR(scroll_x, 50.0f, 0.5f);
	EXPECT_FLOAT_EQ(scroll_y, 0.0f);
	//内容矩形随之上移（横向左移）
	Ui_Rect content_rect{};
	ASSERT_TRUE(ui->rect_get(content, content_rect));
	EXPECT_NEAR(content_rect.x, 20.0f - 50.0f, 0.5f);
}

//内容不超过视口时最大偏移为零
TEST_F(Renderer_Ui_Scroll_Bar_Test, 内容适配不滚动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//视口高 60，内容高 50，无滚动空间
	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.15f, 0.15f, 0.15f, 1.0f });
	const Ui_Handle bar = ui->slider_create(ui->root_get(),
		Ui_Rect{ 130.0f, 20.0f, 20.0f, 60.0f }, 0.0f);
	ASSERT_NE(container, 0u);
	ASSERT_NE(bar, 0u);
	ASSERT_TRUE(ui->scroll_bar_bind(bar, container, 50.0f, true));
	ASSERT_TRUE(renderer.frame_render());

	//拖到最底部
	SDL_Event down = mouse_button(true, 140.0f, 79.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_FLOAT_EQ(scroll_y, 0.0f) << "内容不超过视口时不应产生滚动";
}
