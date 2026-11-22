/*
渲染 UI 滚轮测试（第八百九十一轮）
覆盖：指针下滚动容器响应滚轮、滚轮方向与步长、滚出上界夹紧、
      指针不在滚动容器内时滚轮不生效。
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

	//构造滚轮事件（y 为正向上、为负向下）
	SDL_Event mouse_wheel(float x, float y)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_WHEEL;
		event.wheel.x = 0.0f;
		event.wheel.y = y;
		event.wheel.mouse_x = x;
		event.wheel.mouse_y = y;
		return event;
	}
}

//渲染 UI 滚轮测试夹具
class Renderer_Ui_Wheel_Test : public ::testing::Test
{
};

//滚轮按步长滚动指针所在容器，向上滚回并夹紧到零
TEST_F(Renderer_Ui_Wheel_Test, 滚轮驱动容器滚动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//容器视口 100×60，内容高 200
	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.15f, 0.15f, 0.15f, 1.0f });
	const Ui_Handle content = ui->panel_create(container,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 200.0f }, Ui_Color{ 0.75f, 0.75f, 0.75f, 1.0f });
	ASSERT_NE(container, 0u);
	ASSERT_NE(content, 0u);
	ASSERT_TRUE(ui->clip_children_set(container, true));

	//指针进入容器内容区
	SDL_Event motion = mouse_motion(60.0f, 40.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());

	//向下滚一格：偏移 24
	SDL_Event down = mouse_wheel(60.0f, -1.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_FLOAT_EQ(scroll_y, 24.0f);
	//内容矩形随之上移
	Ui_Rect content_rect{};
	ASSERT_TRUE(ui->rect_get(content, content_rect));
	EXPECT_FLOAT_EQ(content_rect.y, -4.0f);

	//再向下滚一格：偏移 48
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_FLOAT_EQ(scroll_y, 48.0f);

	//向上滚两格：回到 0
	SDL_Event up = mouse_wheel(60.0f, 2.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_FLOAT_EQ(scroll_y, 0.0f);

	//继续向上滚：夹紧在 0
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_FLOAT_EQ(scroll_y, 0.0f);
}

//指针不在滚动容器内时滚轮不生效
TEST_F(Renderer_Ui_Wheel_Test, 容器外滚轮不生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.15f, 0.15f, 0.15f, 1.0f });
	const Ui_Handle content = ui->panel_create(container,
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 200.0f }, Ui_Color{ 0.75f, 0.75f, 0.75f, 1.0f });
	ASSERT_NE(container, 0u);
	ASSERT_NE(content, 0u);
	ASSERT_TRUE(ui->clip_children_set(container, true));

	//指针在容器外空白处
	SDL_Event motion = mouse_motion(280.0f, 160.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	SDL_Event down = mouse_wheel(280.0f, -3.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());

	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	ASSERT_TRUE(ui->scroll_get(container, scroll_x, scroll_y));
	EXPECT_FLOAT_EQ(scroll_y, 0.0f) << "指针不在滚动容器内时滚轮不应改变偏移";
}
