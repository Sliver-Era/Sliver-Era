/*
UI 域内总线测试（2026-11-21 · UI 局域网改造 批次 1）

覆盖：域内事件先入队、派发时送达订阅者；同名订阅覆盖；取消订阅；
      派发期间新事件留到下一轮；外部终端经 attach_node 级联到中心终端；
      非域内分类被拒收。
*/
#include <gtest/gtest.h>

//UI 域内总线与协议常量
#include "Engine/EngineRenderer/src/render/UI/core/UI本地总线.h"
#include "Engine/EngineRenderer/src/render/UI/common/UI事件协议.h"
//UI 控件树
#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//UI 域内总线测试夹具
class Ui_Local_Bus_Test : public ::testing::Test
{
};

//构造一帧纯鼠标输入
engine::Input_Frame ui_frame_make(float x, float y)
{
	engine::Input_Frame frame{};
	frame.window_width = 640;
	frame.window_height = 360;
	frame.window_focused = true;
	frame.pointer.x = x;
	frame.pointer.y = y;
	return frame;
}

//发布后未派发前只入队，派发时送达订阅者
TEST_F(Ui_Local_Bus_Test, 派发时送达订阅者)
{
	engine::Ui_Local_Bus bus;
	//记录收到的事件
	std::vector<std::string> tags{};
	bus.subscribe("观察者", [&tags](const std::string& tag, const nlohmann::json&) { tags.push_back(tag); });
	//发布两条事件
	ASSERT_TRUE(bus.publish(engine::ui_event::click, nlohmann::json::object()));
	ASSERT_TRUE(bus.publish(engine::ui_event::toggle, nlohmann::json::object()));
	//派发前只入队
	EXPECT_EQ(tags.size(), std::size_t{0});
	EXPECT_EQ(bus.pending_count(), std::size_t{2});
	//派发后按顺序送达
	bus.frame_flush();
	ASSERT_EQ(tags.size(), std::size_t{2});
	EXPECT_EQ(tags[0], engine::ui_event::click);
	EXPECT_EQ(tags[1], engine::ui_event::toggle);
	EXPECT_EQ(bus.pending_count(), std::size_t{0});
}

//同名订阅覆盖旧回调
TEST_F(Ui_Local_Bus_Test, 同名订阅覆盖)
{
	engine::Ui_Local_Bus bus;
	//两次同名订阅
	int first = 0;
	int second = 0;
	bus.subscribe("观察者", [&first](const std::string&, const nlohmann::json&) { ++first; });
	bus.subscribe("观察者", [&second](const std::string&, const nlohmann::json&) { ++second; });
	EXPECT_EQ(bus.subscriber_count(), std::size_t{1});
	//发布并派发
	ASSERT_TRUE(bus.publish(engine::ui_event::click, nlohmann::json::object()));
	bus.frame_flush();
	EXPECT_EQ(first, 0);
	EXPECT_EQ(second, 1);
}

//取消订阅后不再送达
TEST_F(Ui_Local_Bus_Test, 取消订阅后不再送达)
{
	engine::Ui_Local_Bus bus;
	int count = 0;
	bus.subscribe("观察者", [&count](const std::string&, const nlohmann::json&) { ++count; });
	bus.unsubscribe("观察者");
	EXPECT_EQ(bus.subscriber_count(), std::size_t{0});
	ASSERT_TRUE(bus.publish(engine::ui_event::click, nlohmann::json::object()));
	bus.frame_flush();
	EXPECT_EQ(count, 0);
}

//派发期间新发布的事件留到下一轮
TEST_F(Ui_Local_Bus_Test, 派发期间新事件留到下一轮)
{
	engine::Ui_Local_Bus bus;
	int count = 0;
	//首个回调在派发中再发布一条事件
	bus.subscribe("再入", [&bus, &count](const std::string& tag, const nlohmann::json&)
		{
			++count;
			if (tag == engine::ui_event::click)
				bus.publish(engine::ui_event::toggle, nlohmann::json::object());
		});
	ASSERT_TRUE(bus.publish(engine::ui_event::click, nlohmann::json::object()));
	//首轮只派发 click
	bus.frame_flush();
	EXPECT_EQ(count, 1);
	EXPECT_EQ(bus.pending_count(), std::size_t{1});
	//第二轮派发 toggle
	bus.frame_flush();
	EXPECT_EQ(count, 2);
	EXPECT_EQ(bus.pending_count(), std::size_t{0});
}

//外部终端经 attach_node 级联到中心终端
TEST_F(Ui_Local_Bus_Test, 外部终端接入后可发送)
{
	engine::Ui_Local_Bus bus;
	//内部节点终端
	engine::Event_Terminal node;
	int received = 0;
	//订阅并接入节点
	bus.subscribe("观察者", [&received](const std::string&, const nlohmann::json&) { ++received; });
	//节点先以自己的密钥完成初始化，再接入总线
	auto key = node.key();
	ASSERT_TRUE(key.has_value());
	bus.attach_node(node);
	//节点发送一条域内事件
	auto evt = node.build("节点", "", engine::ui_event::category, engine::ui_event::toggle);
	ASSERT_TRUE(node.send(evt, key.value()));
	//入队但未派发
	EXPECT_EQ(bus.pending_count(), std::size_t{1});
	EXPECT_EQ(received, 0);
	bus.frame_flush();
	EXPECT_EQ(received, 1);
}

//非域内分类事件被拒收
TEST_F(Ui_Local_Bus_Test, 非域内分类被拒收)
{
	engine::Ui_Local_Bus bus;
	int received = 0;
	bus.subscribe("观察者", [&received](const std::string&, const nlohmann::json&) { ++received; });
	//节点发送 Input 分类事件
	engine::Event_Terminal node;
	auto key = node.key();
	ASSERT_TRUE(key.has_value());
	bus.attach_node(node);
	auto evt = node.build("节点", "", "Input", "Command");
	ASSERT_TRUE(node.send(evt, key.value()));
	//总线不入队、不派发
	EXPECT_EQ(bus.pending_count(), std::size_t{0});
	bus.frame_flush();
	EXPECT_EQ(received, 0);
}

//点击按钮：域内点击事件与本地回调同时生效
TEST_F(Ui_Local_Bus_Test, 点击按钮发布域内事件)
{
	engine::Ui_Context ui;
	//记录域内点击事件
	int events = 0;
	engine::Ui_Handle event_handle = 0;
	ui.bus_get().subscribe("观察者", [&](const std::string& tag, const nlohmann::json& config)
		{
			if (tag != engine::ui_event::click)
				return;
			++events;
			event_handle = config.value(engine::ui_event::field_handle, static_cast<engine::Ui_Handle>(0));
		});
	//记录本地回调
	int callbacks = 0;
	engine::Ui_Handle button = ui.button_create(ui.root_get(), engine::Ui_Rect{ 10.0f, 10.0f, 120.0f, 30.0f },
		"确定", [](engine::Ui_Handle, void* user) { ++(*static_cast<int*>(user)); }, &callbacks);
	ASSERT_NE(button, 0u);

	//按下帧
	engine::Input_Frame press = ui_frame_make(50.0f, 20.0f);
	press.pointer.button_pressed[0] = true;
	press.pointer.button_down[0] = true;
	ui.frame_begin(press);
	//抬起帧（同一位置判定单击）
	engine::Input_Frame release = ui_frame_make(50.0f, 20.0f);
	release.pointer.button_released[0] = true;
	ui.frame_begin(release);

	//事件与回调各一次，句柄一致
	EXPECT_EQ(events, 1);
	EXPECT_EQ(event_handle, button);
	EXPECT_EQ(callbacks, 1);
}

//点击复选框：发布带选中状态的域内切换事件
TEST_F(Ui_Local_Bus_Test, 点击复选框发布切换事件)
{
	engine::Ui_Context ui;
	bool seen = false;
	bool checked = false;
	ui.bus_get().subscribe("观察者", [&](const std::string& tag, const nlohmann::json& config)
		{
			if (tag != engine::ui_event::toggle)
				return;
			seen = true;
			checked = config.value(engine::ui_event::field_checked, false);
		});
	engine::Ui_Handle box = ui.checkbox_create(ui.root_get(), engine::Ui_Rect{ 10.0f, 10.0f, 160.0f, 24.0f }, "启用", false);
	ASSERT_NE(box, 0u);

	//按下并抬起
	engine::Input_Frame press = ui_frame_make(20.0f, 20.0f);
	press.pointer.button_pressed[0] = true;
	press.pointer.button_down[0] = true;
	ui.frame_begin(press);
	engine::Input_Frame release = ui_frame_make(20.0f, 20.0f);
	release.pointer.button_released[0] = true;
	ui.frame_begin(release);

	EXPECT_TRUE(seen);
	EXPECT_TRUE(checked);
}

//拖动滑动条：发布带取值的事件
TEST_F(Ui_Local_Bus_Test, 滑动条发布取值事件)
{
	engine::Ui_Context ui;
	float value = -1.0f;
	ui.bus_get().subscribe("观察者", [&](const std::string& tag, const nlohmann::json& config)
		{
			if (tag == engine::ui_event::value)
				value = config.value(engine::ui_event::field_value, -1.0f);
		});
	engine::Ui_Handle slider = ui.slider_create(ui.root_get(), engine::Ui_Rect{ 10.0f, 10.0f, 100.0f, 20.0f }, 0.0f);
	ASSERT_NE(slider, 0u);

	//按在中点（约 0.5）
	engine::Input_Frame press = ui_frame_make(60.0f, 20.0f);
	press.pointer.button_pressed[0] = true;
	press.pointer.button_down[0] = true;
	ui.frame_begin(press);

	EXPECT_GT(value, 0.3f);
	EXPECT_LT(value, 0.7f);
}

//命令激活控件：事件带网关来源标记（网关不回译）
TEST_F(Ui_Local_Bus_Test, 命令激活带网关来源标记)
{
	engine::Ui_Context ui;
	std::string origin{};
	engine::Ui_Handle event_handle = 0;
	ui.bus_get().subscribe("观察者", [&](const std::string& tag, const nlohmann::json& config)
		{
			if (tag != engine::ui_event::click)
				return;
			origin = config.value(engine::ui_event::field_origin, std::string{});
			event_handle = config.value(engine::ui_event::field_handle, static_cast<engine::Ui_Handle>(0));
		});
	engine::Ui_Handle button = ui.button_create(ui.root_get(), engine::Ui_Rect{ 10.0f, 10.0f, 120.0f, 30.0f }, "打开");
	ASSERT_NE(button, 0u);
	ui.command_bind("Open", button);

	//命令分发（等价于系统命令到达网关后激活控件）
	ASSERT_TRUE(ui.command_dispatch("Open"));
	ui.bus_get().frame_flush();

	EXPECT_EQ(event_handle, button);
	EXPECT_EQ(origin, engine::ui_event::origin_gateway);
	//命令反查
	EXPECT_EQ(ui.command_of(button), std::string{ "Open" });
}
