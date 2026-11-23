/*
渲染 UI 单选按钮测试（第九百二十六轮）
覆盖：组内互斥、跨组独立、已选中不再变化、选中圆点像素。
同组互斥用例使用语义命令激活，像素用例使用隐藏窗口渲染器回读。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
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

	//切换回调记录
	struct Radio_Record
	{
		//回调次数
		int count = 0;
		//最近一次状态
		bool last = false;
	};

	//单选按钮切换回调
	void radio_record(Ui_Handle handle, bool checked, void* user)
	{
		Radio_Record* record = static_cast<Radio_Record*>(user);
		if (record == nullptr)
			return;
		++record->count;
		record->last = checked;
	}
}

//渲染 UI 单选按钮测试夹具
class Renderer_Ui_Radio_Test : public ::testing::Test
{
};

//组内互斥：同组选中一项其余取消，跨组互不影响
TEST_F(Renderer_Ui_Radio_Test, 组内互斥与跨组独立)
{
	Ui_Context ui;
	//另一个容器作为跨组标识
	const Ui_Handle other_group = ui.panel_create(ui.root_get(),
		Ui_Rect{ 0.0f, 200.0f, 100.0f, 40.0f }, Ui_Color{});

	Radio_Record record_a{};
	Radio_Record record_b{};
	Radio_Record record_c{};
	//A / B 同父成组，C 归入另一组
	const Ui_Handle a = ui.radio_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, "A",
		false, radio_record, &record_a);
	const Ui_Handle b = ui.radio_create(ui.root_get(), Ui_Rect{ 0.0f, 24.0f, 100.0f, 20.0f }, "B",
		true, radio_record, &record_b);
	const Ui_Handle c = ui.radio_create(ui.root_get(), Ui_Rect{ 0.0f, 48.0f, 100.0f, 20.0f }, "C",
		true, radio_record, &record_c);
	ASSERT_NE(a, 0u);
	ASSERT_NE(b, 0u);
	ASSERT_NE(c, 0u);
	ASSERT_TRUE(ui.radio_group_set(c, other_group));
	EXPECT_EQ(ui.radio_group_get(c), other_group);
	//未知句柄
	EXPECT_FALSE(ui.radio_group_set(999999u, other_group));
	EXPECT_EQ(ui.radio_group_get(999999u), 0u);

	ui.command_bind("a", a);
	ui.command_bind("b", b);
	ui.command_bind("c", c);

	//选中 A：B 取消，C 保持
	ASSERT_TRUE(ui.command_dispatch("a"));
	EXPECT_TRUE(ui.checked_get(a));
	EXPECT_FALSE(ui.checked_get(b));
	EXPECT_TRUE(ui.checked_get(c));
	EXPECT_EQ(record_a.count, 1);
	EXPECT_EQ(record_b.count, 1);
	EXPECT_FALSE(record_b.last) << "B 应收到取消通知";
	EXPECT_EQ(record_c.count, 0);

	//再次激活已选中的 A：不变化也不通知
	EXPECT_FALSE(ui.command_dispatch("a"));
	EXPECT_EQ(record_a.count, 1);

	//选中 B：A 取消，C 仍保持
	ASSERT_TRUE(ui.command_dispatch("b"));
	EXPECT_FALSE(ui.checked_get(a));
	EXPECT_TRUE(ui.checked_get(b));
	EXPECT_TRUE(ui.checked_get(c));
	EXPECT_FALSE(record_a.last);
	EXPECT_EQ(record_c.count, 0);
}

//像素：选中项中心为强调色，未选中项中心为深色
TEST_F(Renderer_Ui_Radio_Test, 选中圆点像素)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//两个 20 像素见方的单选按钮，圆心分别在 (30,30) 与 (60,30)
	const Ui_Handle checked_radio = ui->radio_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 20.0f, 20.0f }, "", true);
	const Ui_Handle empty_radio = ui->radio_create(ui->root_get(),
		Ui_Rect{ 50.0f, 20.0f, 20.0f, 20.0f }, "", false);
	ASSERT_NE(checked_radio, 0u);
	ASSERT_NE(empty_radio, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//选中项中心：蓝色强调色占优（回读 BGRA）
	const std::array<uint8_t, 4> center_checked = byjy_render_test::pixel_get(pixels, 30, 30, 320);
	const std::array<uint8_t, 4> center_empty = byjy_render_test::pixel_get(pixels, 60, 30, 320);
	EXPECT_GT(static_cast<int>(center_checked[0]), static_cast<int>(center_checked[2]))
		<< "选中圆点应为蓝色强调色";
	EXPECT_GT(byjy_render_test::luma_of(center_checked), byjy_render_test::luma_of(center_empty) + 20.0f)
		<< "选中项中心应明显亮于未选中项";
}
