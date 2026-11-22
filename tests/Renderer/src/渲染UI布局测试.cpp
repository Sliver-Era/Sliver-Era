/*
渲染 UI 锚点布局测试（第八百七十六轮）
覆盖：九宫格锚点解析、偏移叠加、非正父矩形退化、窗口尺寸变化后的矩形重算、
      嵌套布局的父子传播、rect_set 取消布局。
纯数据用例不依赖 GPU；集成用例走隐藏窗口渲染器的一帧驱动。
*/
#include <gtest/gtest.h>

//获取渲染器门面（公共头，Vulkan / SDL-free）
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
//获取原生 UI 控件树
#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
//获取帧输入聚合器（公共扩展头，含 SDL3；用于命中随布局）
#include "Engine/EngineRenderer/src/render/Input/输入聚合.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//构造指定尺寸的隐藏窗口配置
	Renderer_Config hidden_config(uint32_t width, uint32_t height)
	{
		Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}

	//构造布局描述
	Ui_Layout make_layout(Ui_Anchor anchor, float x, float y, float width, float height)
	{
		Ui_Layout layout;
		layout.anchor = anchor;
		layout.offset_x = x;
		layout.offset_y = y;
		layout.width = width;
		layout.height = height;
		layout.active = true;
		return layout;
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

	//构造鼠标左键事件
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

//渲染 UI 布局测试夹具
class Renderer_Ui_Layout_Test : public ::testing::Test
{
};

//九宫格锚点：父 (10,20,200,100)，子 40×20
TEST_F(Renderer_Ui_Layout_Test, 九宫格锚点解析)
{
	const Ui_Rect parent{ 10.0f, 20.0f, 200.0f, 100.0f };
	const struct
	{
		Ui_Anchor anchor;
		float x;
		float y;
	} cases[] = {
		{ Ui_Anchor::Top_Left, 10.0f, 20.0f },
		{ Ui_Anchor::Top, 90.0f, 20.0f },
		{ Ui_Anchor::Top_Right, 170.0f, 20.0f },
		{ Ui_Anchor::Left, 10.0f, 60.0f },
		{ Ui_Anchor::Center, 90.0f, 60.0f },
		{ Ui_Anchor::Right, 170.0f, 60.0f },
		{ Ui_Anchor::Bottom_Left, 10.0f, 100.0f },
		{ Ui_Anchor::Bottom, 90.0f, 100.0f },
		{ Ui_Anchor::Bottom_Right, 170.0f, 100.0f },
	};
	for (const auto& item : cases)
	{
		const Ui_Rect rect = ui_layout_rect_resolve(parent, make_layout(item.anchor, 0.0f, 0.0f, 40.0f, 20.0f));
		EXPECT_FLOAT_EQ(rect.x, item.x);
		EXPECT_FLOAT_EQ(rect.y, item.y);
		EXPECT_FLOAT_EQ(rect.w, 40.0f);
		EXPECT_FLOAT_EQ(rect.h, 20.0f);
	}
}

//偏移叠加：左上与右下锚点各加一次偏移
TEST_F(Renderer_Ui_Layout_Test, 偏移叠加)
{
	const Ui_Rect parent{ 10.0f, 20.0f, 200.0f, 100.0f };
	const Ui_Rect top_left = ui_layout_rect_resolve(parent,
		make_layout(Ui_Anchor::Top_Left, 5.0f, -3.0f, 40.0f, 20.0f));
	EXPECT_FLOAT_EQ(top_left.x, 15.0f);
	EXPECT_FLOAT_EQ(top_left.y, 17.0f);
	const Ui_Rect bottom_right = ui_layout_rect_resolve(parent,
		make_layout(Ui_Anchor::Bottom_Right, -5.0f, 3.0f, 40.0f, 20.0f));
	EXPECT_FLOAT_EQ(bottom_right.x, 165.0f);
	EXPECT_FLOAT_EQ(bottom_right.y, 103.0f);
}

//非正父矩形：宽或高为零时退化为零矩形
TEST_F(Renderer_Ui_Layout_Test, 非正父矩形退化)
{
	const Ui_Rect zero_parent{ 10.0f, 20.0f, 0.0f, 100.0f };
	const Ui_Rect zero = ui_layout_rect_resolve(zero_parent,
		make_layout(Ui_Anchor::Center, 0.0f, 0.0f, 40.0f, 20.0f));
	EXPECT_FLOAT_EQ(zero.x, 0.0f);
	EXPECT_FLOAT_EQ(zero.y, 0.0f);
	EXPECT_FLOAT_EQ(zero.w, 0.0f);
	EXPECT_FLOAT_EQ(zero.h, 0.0f);
	//期望尺寸非正按零尺寸返回
	const Ui_Rect negative = ui_layout_rect_resolve(Ui_Rect{ 0.0f, 0.0f, 100.0f, 100.0f },
		make_layout(Ui_Anchor::Top_Left, 0.0f, 0.0f, -1.0f, 0.0f));
	EXPECT_FLOAT_EQ(negative.w, 0.0f);
	EXPECT_FLOAT_EQ(negative.h, 0.0f);
}

//窗口尺寸变化：右上锚点随根尺寸重算
TEST_F(Renderer_Ui_Layout_Test, 窗口尺寸变化重算)
{
	//小窗口 320×180
	{
		Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
		Ui_Context* ui = renderer.ui().ui_context();
		ASSERT_NE(ui, nullptr);
		const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{}, Ui_Color{});
		ASSERT_NE(panel, 0u);
		ASSERT_TRUE(ui->layout_set(panel, make_layout(Ui_Anchor::Top_Right, -10.0f, 10.0f, 100.0f, 50.0f)));
		ASSERT_TRUE(renderer.frame_render());
		Ui_Rect rect{};
		ASSERT_TRUE(ui->rect_get(panel, rect));
		EXPECT_FLOAT_EQ(rect.x, 210.0f);
		EXPECT_FLOAT_EQ(rect.y, 10.0f);
		EXPECT_FLOAT_EQ(rect.w, 100.0f);
		EXPECT_FLOAT_EQ(rect.h, 50.0f);
	}
	//大窗口 640×360：同一布局自动右移
	{
		Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config(640, 360)));
		Ui_Context* ui = renderer.ui().ui_context();
		ASSERT_NE(ui, nullptr);
		const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{}, Ui_Color{});
		ASSERT_NE(panel, 0u);
		ASSERT_TRUE(ui->layout_set(panel, make_layout(Ui_Anchor::Top_Right, -10.0f, 10.0f, 100.0f, 50.0f)));
		ASSERT_TRUE(renderer.frame_render());
		Ui_Rect rect{};
		ASSERT_TRUE(ui->rect_get(panel, rect));
		EXPECT_FLOAT_EQ(rect.x, 530.0f);
		EXPECT_FLOAT_EQ(rect.y, 10.0f);
	}
}

//嵌套布局：子控件以父控件布局结果为基准
TEST_F(Renderer_Ui_Layout_Test, 嵌套布局按父传播)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{}, Ui_Color{});
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->layout_set(panel, make_layout(Ui_Anchor::Center, 0.0f, 0.0f, 200.0f, 100.0f)));
	const Ui_Handle button = ui->button_create(panel, Ui_Rect{}, "OK");
	ASSERT_NE(button, 0u);
	ASSERT_TRUE(ui->layout_set(button, make_layout(Ui_Anchor::Bottom_Right, -5.0f, -5.0f, 40.0f, 20.0f)));
	ASSERT_TRUE(renderer.frame_render());

	Ui_Rect panel_rect{};
	ASSERT_TRUE(ui->rect_get(panel, panel_rect));
	EXPECT_FLOAT_EQ(panel_rect.x, 60.0f);
	EXPECT_FLOAT_EQ(panel_rect.y, 40.0f);
	Ui_Rect button_rect{};
	ASSERT_TRUE(ui->rect_get(button, button_rect));
	//面板右下角 (260,140)，按钮 40×20、内缩 5
	EXPECT_FLOAT_EQ(button_rect.x, 215.0f);
	EXPECT_FLOAT_EQ(button_rect.y, 115.0f);
}

//rect_set 取消布局：显式矩形优先且后续帧不被覆盖
TEST_F(Renderer_Ui_Layout_Test, 显式矩形取消布局)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{}, Ui_Color{});
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->layout_set(panel, make_layout(Ui_Anchor::Top_Left, 0.0f, 0.0f, 100.0f, 50.0f)));
	const Ui_Rect fixed{ 30.0f, 40.0f, 10.0f, 10.0f };
	ASSERT_TRUE(ui->rect_set(panel, fixed));
	ASSERT_TRUE(renderer.frame_render());
	Ui_Rect rect{};
	ASSERT_TRUE(ui->rect_get(panel, rect));
	EXPECT_FLOAT_EQ(rect.x, 30.0f);
	EXPECT_FLOAT_EQ(rect.y, 40.0f);
	EXPECT_FLOAT_EQ(rect.w, 10.0f);
	EXPECT_FLOAT_EQ(rect.h, 10.0f);
	//布局已被取消
	Ui_Layout layout{};
	ASSERT_TRUE(ui->layout_get(panel, layout));
	EXPECT_FALSE(layout.active);
}

//百分比尺寸：按父宽高换算，超出 1 按 1 处理
TEST_F(Renderer_Ui_Layout_Test, 百分比尺寸)
{
	const Ui_Rect parent{ 10.0f, 20.0f, 200.0f, 100.0f };
	Ui_Layout layout = make_layout(Ui_Anchor::Top_Left, 0.0f, 0.0f, 0.0f, 0.0f);
	layout.width_percent = 0.25f;
	layout.height_percent = 0.5f;
	const Ui_Rect rect = ui_layout_rect_resolve(parent, layout);
	EXPECT_FLOAT_EQ(rect.w, 50.0f);
	EXPECT_FLOAT_EQ(rect.h, 50.0f);
	//百分比超过 1 按 1 处理
	layout.width_percent = 2.0f;
	layout.height_percent = 2.0f;
	const Ui_Rect full = ui_layout_rect_resolve(parent, layout);
	EXPECT_FLOAT_EQ(full.w, 200.0f);
	EXPECT_FLOAT_EQ(full.h, 100.0f);
}

//最小 / 最大尺寸夹紧
TEST_F(Renderer_Ui_Layout_Test, 尺寸夹紧)
{
	const Ui_Rect parent{ 10.0f, 20.0f, 200.0f, 100.0f };
	Ui_Layout layout = make_layout(Ui_Anchor::Top_Left, 0.0f, 0.0f, 10.0f, 10.0f);
	layout.min_width = 30.0f;
	layout.min_height = 25.0f;
	const Ui_Rect minimum = ui_layout_rect_resolve(parent, layout);
	EXPECT_FLOAT_EQ(minimum.w, 30.0f);
	EXPECT_FLOAT_EQ(minimum.h, 25.0f);
	//最大尺寸优先于最小尺寸之后的放大
	layout.width = 80.0f;
	layout.height = 90.0f;
	layout.max_width = 60.0f;
	layout.max_height = 40.0f;
	const Ui_Rect maximum = ui_layout_rect_resolve(parent, layout);
	EXPECT_FLOAT_EQ(maximum.w, 60.0f);
	EXPECT_FLOAT_EQ(maximum.h, 40.0f);
	//百分比结果同样受最大尺寸约束
	layout.width = 0.0f;
	layout.height = 0.0f;
	layout.width_percent = 0.5f;
	layout.height_percent = 0.5f;
	layout.max_width = 70.0f;
	layout.max_height = 45.0f;
	const Ui_Rect percent_max = ui_layout_rect_resolve(parent, layout);
	EXPECT_FLOAT_EQ(percent_max.w, 70.0f);
	EXPECT_FLOAT_EQ(percent_max.h, 45.0f);
}

//百分比尺寸参与锚点居中
TEST_F(Renderer_Ui_Layout_Test, 百分比居中)
{
	const Ui_Rect parent{ 10.0f, 20.0f, 200.0f, 100.0f };
	Ui_Layout layout = make_layout(Ui_Anchor::Center, 0.0f, 0.0f, 0.0f, 0.0f);
	layout.width_percent = 0.5f;
	layout.height_percent = 0.5f;
	const Ui_Rect rect = ui_layout_rect_resolve(parent, layout);
	EXPECT_FLOAT_EQ(rect.x, 60.0f);
	EXPECT_FLOAT_EQ(rect.y, 45.0f);
	EXPECT_FLOAT_EQ(rect.w, 100.0f);
	EXPECT_FLOAT_EQ(rect.h, 50.0f);
}

//纵向排列：内边距 10、间距 5，子节点依次下移
TEST_F(Renderer_Ui_Layout_Test, 纵向排列)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{ 0.0f, 0.0f, 200.0f, 100.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->flow_set(panel, Ui_Flow::Vertical, 5.0f, 10.0f));
	const Ui_Handle first = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "a");
	const Ui_Handle second = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "b");
	const Ui_Handle third = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "c");
	ASSERT_TRUE(renderer.frame_render());

	Ui_Rect rect{};
	ASSERT_TRUE(ui->rect_get(first, rect));
	EXPECT_FLOAT_EQ(rect.x, 10.0f);
	EXPECT_FLOAT_EQ(rect.y, 10.0f);
	ASSERT_TRUE(ui->rect_get(second, rect));
	EXPECT_FLOAT_EQ(rect.y, 35.0f);
	ASSERT_TRUE(ui->rect_get(third, rect));
	EXPECT_FLOAT_EQ(rect.y, 60.0f);
}

//横向排列：子节点依次右移
TEST_F(Renderer_Ui_Layout_Test, 横向排列)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{ 0.0f, 0.0f, 200.0f, 100.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->flow_set(panel, Ui_Flow::Horizontal, 5.0f, 10.0f));
	const Ui_Handle first = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "a");
	const Ui_Handle second = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "b");
	const Ui_Handle third = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "c");
	ASSERT_TRUE(renderer.frame_render());

	Ui_Rect rect{};
	ASSERT_TRUE(ui->rect_get(first, rect));
	EXPECT_FLOAT_EQ(rect.x, 10.0f);
	EXPECT_FLOAT_EQ(rect.y, 10.0f);
	ASSERT_TRUE(ui->rect_get(second, rect));
	EXPECT_FLOAT_EQ(rect.x, 55.0f);
	ASSERT_TRUE(ui->rect_get(third, rect));
	EXPECT_FLOAT_EQ(rect.x, 100.0f);
}

//不可见子节点不占排列位置
TEST_F(Renderer_Ui_Layout_Test, 排列跳过不可见)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(), Ui_Rect{ 0.0f, 0.0f, 200.0f, 100.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(ui->flow_set(panel, Ui_Flow::Vertical, 5.0f, 10.0f));
	const Ui_Handle first = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "a");
	const Ui_Handle second = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "b");
	const Ui_Handle third = ui->label_create(panel, Ui_Rect{ 0.0f, 0.0f, 40.0f, 20.0f }, "c");
	ASSERT_TRUE(ui->visible_set(second, false));
	ASSERT_TRUE(renderer.frame_render());

	Ui_Rect rect{};
	ASSERT_TRUE(ui->rect_get(first, rect));
	EXPECT_FLOAT_EQ(rect.y, 10.0f);
	ASSERT_TRUE(ui->rect_get(third, rect));
	//第二个不占位：第三个紧随第一个（10 + 20 + 5）
	EXPECT_FLOAT_EQ(rect.y, 35.0f);
}

//命中测试跟随布局：按布局位置合成的点击可命中按钮
TEST_F(Renderer_Ui_Layout_Test, 命中随布局)
{
	Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);
	const Ui_Handle button = ui->button_create(ui->root_get(), Ui_Rect{}, "OK");
	ASSERT_NE(button, 0u);
	//右上角内缩 10：矩形 (210,10,100,30)，中心 (260,25)
	ASSERT_TRUE(ui->layout_set(button, make_layout(Ui_Anchor::Top_Right, -10.0f, 10.0f, 100.0f, 30.0f)));
	ASSERT_TRUE(renderer.frame_render());

	Ui_Rect rect{};
	ASSERT_TRUE(ui->rect_get(button, rect));
	ASSERT_FLOAT_EQ(rect.x, 210.0f);
	ASSERT_FLOAT_EQ(rect.y, 10.0f);

	//指针移到布局结果中心并推进一帧
	SDL_Event motion = mouse_motion(260.0f, 25.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->hovered_get(button)) << "布局后的矩形应参与命中";

	//按下与抬起完成一次单击
	SDL_Event down = mouse_button(true, 260.0f, 25.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	SDL_Event up = mouse_button(false, 260.0f, 25.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->clicked_consume(button));
}
