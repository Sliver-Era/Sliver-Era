/*
渲染 UI 滑动条测试（第九百二十四轮）
覆盖：步进量化与回调取值、步进存取边界、程序赋值不受步进约束。
闭环用例使用隐藏窗口渲染器与输入聚合器。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "Engine/EngineRenderer/src/render/Input/输入聚合.h"

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

	//构造指针移动事件
	SDL_Event mouse_motion(float x, float y)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.x = x;
		event.motion.y = y;
		return event;
	}

	//滑动条回调记录
	struct Slider_Record
	{
		//回调次数
		int count = 0;
		//最近一次取值
		float last = -1.0f;
	};

	//滑动条取值回调
	void slider_record(Ui_Handle handle, float value, void* user)
	{
		Slider_Record* record = static_cast<Slider_Record*>(user);
		if (record == nullptr)
			return;
		++record->count;
		record->last = value;
	}
}

//渲染 UI 滑动条测试夹具
class Renderer_Ui_Slider_Test : public ::testing::Test
{
};

//步进量化：点击 60% 落在 0.5 档，拖到 90% 落在 1.0 档
TEST_F(Renderer_Ui_Slider_Test, 步进量化与回调)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//滑块横跨 20..220，档距 0.25
	Slider_Record record{};
	const Ui_Handle slider = ui->slider_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 20.0f }, 0.0f, slider_record, &record);
	ASSERT_NE(slider, 0u);
	ASSERT_TRUE(ui->slider_step_set(slider, 0.25f));
	ASSERT_TRUE(renderer.frame_render());

	//点击 60% 处：量化到 0.5
	SDL_Event down = mouse_button(true, 140.0f, 30.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float value = -1.0f;
	ASSERT_TRUE(ui->value_get(slider, value));
	EXPECT_FLOAT_EQ(value, 0.5f);
	EXPECT_FLOAT_EQ(record.last, 0.5f) << "回调应收到量化后的取值";
	EXPECT_EQ(record.count, 1);

	//拖到 90% 处：量化到 1.0
	SDL_Event motion = mouse_motion(200.0f, 30.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->value_get(slider, value));
	EXPECT_FLOAT_EQ(value, 1.0f);
	EXPECT_FLOAT_EQ(record.last, 1.0f);

	//抬起结束拖拽
	SDL_Event up = mouse_button(false, 200.0f, 30.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
}

//纵向滑动条：自下而上取值，点击下部得小值、上部得大值
TEST_F(Renderer_Ui_Slider_Test, 纵向自下而上取值)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	//纵向滑块横跨 20..180（高 160）
	const Ui_Handle slider = ui->slider_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 20.0f, 160.0f }, 0.0f);
	ASSERT_NE(slider, 0u);
	//默认横向
	EXPECT_FALSE(ui->slider_vertical_get(slider));
	ASSERT_TRUE(ui->slider_vertical_set(slider, true));
	EXPECT_TRUE(ui->slider_vertical_get(slider));
	ASSERT_TRUE(renderer.frame_render());

	//点击 80% 高度处（y = 148）：自下而上应为 0.2
	SDL_Event down = mouse_button(true, 30.0f, 148.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	float value = -1.0f;
	ASSERT_TRUE(ui->value_get(slider, value));
	EXPECT_NEAR(value, 0.2f, 0.01f);

	//拖到 10% 高度处（y = 36）：自下而上应为 0.9
	SDL_Event motion = mouse_motion(30.0f, 36.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->value_get(slider, value));
	EXPECT_NEAR(value, 0.9f, 0.01f);

	//抬起结束拖拽
	SDL_Event up = mouse_button(false, 30.0f, 36.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
}

//步进存取：默认连续、负值按 0、未知句柄；程序赋值不受步进约束
TEST_F(Renderer_Ui_Slider_Test, 步进存取与程序赋值)
{
	Ui_Context ui;
	const Ui_Handle slider = ui.slider_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, 0.0f);
	ASSERT_NE(slider, 0u);
	//默认连续
	EXPECT_FLOAT_EQ(ui.slider_step_get(slider), 0.0f);
	ASSERT_TRUE(ui.slider_step_set(slider, 0.25f));
	EXPECT_FLOAT_EQ(ui.slider_step_get(slider), 0.25f);
	//负值按连续
	ASSERT_TRUE(ui.slider_step_set(slider, -0.5f));
	EXPECT_FLOAT_EQ(ui.slider_step_get(slider), 0.0f);
	//未知句柄
	EXPECT_FALSE(ui.slider_step_set(999999u, 0.5f));
	EXPECT_FLOAT_EQ(ui.slider_step_get(999999u), 0.0f);

	//程序赋值保留原值（不量化）
	ASSERT_TRUE(ui.slider_step_set(slider, 0.25f));
	ASSERT_TRUE(ui.value_set(slider, 0.37f));
	float value = 0.0f;
	ASSERT_TRUE(ui.value_get(slider, value));
	EXPECT_FLOAT_EQ(value, 0.37f);
}
