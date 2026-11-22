/*
渲染 UI 滚动测试（第八百八十九轮）
覆盖：滚动偏移存取与夹紧、内容偏移改变子节点矩形、
      命中随滚动偏移、像素上内容随偏移移动并被父裁剪。
纯数据用例直接构造 Ui_Context；闭环与像素用例使用隐藏窗口渲染器。
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

//渲染 UI 滚动测试夹具
class Renderer_Ui_Scroll_Test : public ::testing::Test
{
};

//滚动偏移默认零、可设置读回、负值夹紧、未知句柄拒绝
TEST_F(Renderer_Ui_Scroll_Test, 滚动偏移存取与夹紧)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 60.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	float x = -1.0f;
	float y = -1.0f;
	ASSERT_TRUE(ui.scroll_get(panel, x, y));
	EXPECT_FLOAT_EQ(x, 0.0f);
	EXPECT_FLOAT_EQ(y, 0.0f);
	//正常写入与读回
	ASSERT_TRUE(ui.scroll_set(panel, 12.0f, 30.0f));
	ASSERT_TRUE(ui.scroll_get(panel, x, y));
	EXPECT_FLOAT_EQ(x, 12.0f);
	EXPECT_FLOAT_EQ(y, 30.0f);
	//负值夹紧到 0
	ASSERT_TRUE(ui.scroll_set(panel, -4.0f, -5.0f));
	ASSERT_TRUE(ui.scroll_get(panel, x, y));
	EXPECT_FLOAT_EQ(x, 0.0f);
	EXPECT_FLOAT_EQ(y, 0.0f);
	//未知句柄
	EXPECT_FALSE(ui.scroll_set(panel + 100, 5.0f, 5.0f));
	EXPECT_FALSE(ui.scroll_get(panel + 100, x, y));
	EXPECT_FLOAT_EQ(x, 0.0f);
	EXPECT_FLOAT_EQ(y, 0.0f);
}

//滚动偏移平移直接子节点矩形（基准矩形保持不变，逐帧幂等）
TEST_F(Renderer_Ui_Scroll_Test, 内容偏移改变矩形)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	const Ui_Handle child = ui->panel_create(container,
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 40.0f }, Ui_Color{ 0.8f, 0.8f, 0.8f, 1.0f });
	ASSERT_NE(container, 0u);
	ASSERT_NE(child, 0u);
	ASSERT_TRUE(ui->clip_children_set(container, true));
	ASSERT_TRUE(renderer.frame_render());

	//未滚动时子节点矩形原样
	Ui_Rect rect{};
	ASSERT_TRUE(ui->rect_get(child, rect));
	EXPECT_FLOAT_EQ(rect.x, 20.0f);
	EXPECT_FLOAT_EQ(rect.y, 20.0f);
	//纵向滚动 20 后子节点上移 20，横向不变
	ASSERT_TRUE(ui->scroll_set(container, 0.0f, 20.0f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->rect_get(child, rect));
	EXPECT_FLOAT_EQ(rect.x, 20.0f);
	EXPECT_FLOAT_EQ(rect.y, 0.0f);
	EXPECT_FLOAT_EQ(rect.w, 80.0f);
	EXPECT_FLOAT_EQ(rect.h, 40.0f);
	//重复帧保持幂等（不累积偏移）
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(ui->rect_get(child, rect));
	EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

//命中测试使用滚动后的矩形
TEST_F(Renderer_Ui_Scroll_Test, 命中随滚动偏移)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	const Ui_Handle container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	const Ui_Handle button = ui->button_create(container,
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 40.0f }, "OK");
	ASSERT_NE(container, 0u);
	ASSERT_NE(button, 0u);
	ASSERT_TRUE(ui->clip_children_set(container, true));
	//上滚 30：按钮矩形变为 y = -10 .. 30
	ASSERT_TRUE(ui->scroll_set(container, 0.0f, 30.0f));
	ASSERT_TRUE(renderer.frame_render());

	//指针落在滚动后的按钮可视区
	SDL_Event motion = mouse_motion(40.0f, 25.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->hovered_get(button)) << "滚动后的矩形应参与命中";
	//按下与抬起完成单击
	SDL_Event down = mouse_button(true, 40.0f, 25.0f);
	input->event_feed(&down);
	ASSERT_TRUE(renderer.frame_render());
	SDL_Event up = mouse_button(false, 40.0f, 25.0f);
	input->event_feed(&up);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_TRUE(ui->clicked_consume(button));
	//原矩形下缘之外（y = 45）不再命中
	SDL_Event outside = mouse_motion(40.0f, 45.0f);
	input->event_feed(&outside);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_FALSE(ui->hovered_get(button)) << "滚动后超出按钮矩形的指针不应命中";
}

//像素：滚动后的内容在父裁剪内移动
TEST_F(Renderer_Ui_Scroll_Test, 内容随偏移移动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color child_color{ 0.85f, 0.85f, 0.85f, 1.0f };
	//左：不滚动；右：上滚 40
	const Ui_Handle left_container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 80.0f }, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	const Ui_Handle left_child = ui->panel_create(left_container,
		Ui_Rect{ 20.0f, 60.0f, 80.0f, 40.0f }, child_color);
	const Ui_Handle right_container = ui->panel_create(ui->root_get(),
		Ui_Rect{ 140.0f, 20.0f, 80.0f, 80.0f }, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f });
	const Ui_Handle right_child = ui->panel_create(right_container,
		Ui_Rect{ 140.0f, 60.0f, 80.0f, 40.0f }, child_color);
	ASSERT_NE(left_container, 0u);
	ASSERT_NE(left_child, 0u);
	ASSERT_NE(right_container, 0u);
	ASSERT_NE(right_child, 0u);
	ASSERT_TRUE(ui->clip_children_set(left_container, true));
	ASSERT_TRUE(ui->clip_children_set(right_container, true));
	ASSERT_TRUE(ui->scroll_set(right_container, 0.0f, 40.0f));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//上部：左侧仍为容器底色，右侧内容已上移到该区域
	const std::array<uint8_t, 4> left_top = byjy_render_test::pixel_get(pixels, 60, 30, 320);
	const std::array<uint8_t, 4> right_top = byjy_render_test::pixel_get(pixels, 180, 30, 320);
	EXPECT_GT(byjy_render_test::luma_of(right_top), byjy_render_test::luma_of(left_top) + 40.0f)
		<< "滚动后内容应出现在上部";
	//下部：左侧内容仍在，右侧已被滚走
	const std::array<uint8_t, 4> left_bottom = byjy_render_test::pixel_get(pixels, 60, 70, 320);
	const std::array<uint8_t, 4> right_bottom = byjy_render_test::pixel_get(pixels, 180, 70, 320);
	EXPECT_GT(byjy_render_test::luma_of(left_bottom), byjy_render_test::luma_of(right_bottom) + 40.0f)
		<< "滚动后内容应离开下部";
}
