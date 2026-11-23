/*
渲染 UI 开关测试（第九百二十七轮）
覆盖：切换与回调、绘制记录规模、选中轨道强调色像素。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
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
	struct Switch_Record
	{
		//回调次数
		int count = 0;
		//最近一次状态
		bool last = false;
	};

	//开关切换回调
	void switch_record(Ui_Handle handle, bool checked, void* user)
	{
		Switch_Record* record = static_cast<Switch_Record*>(user);
		if (record == nullptr)
			return;
		++record->count;
		record->last = checked;
	}
}

//渲染 UI 开关测试夹具
class Renderer_Ui_Switch_Test : public ::testing::Test
{
};

//切换：语义命令切换状态并通知，绘制恒为轨道 + 滑块两条
TEST_F(Renderer_Ui_Switch_Test, 切换与绘制规模)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	Switch_Record record{};
	const Ui_Handle toggle = ui->switch_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 40.0f, 20.0f }, false, switch_record, &record);
	ASSERT_NE(toggle, 0u);
	ui->command_bind("toggle", toggle);

	//初始未选中
	EXPECT_FALSE(ui->checked_get(toggle));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().draws, 2u) << "轨道 + 滑块";

	//切换为选中
	ASSERT_TRUE(ui->command_dispatch("toggle"));
	EXPECT_TRUE(ui->checked_get(toggle));
	EXPECT_EQ(record.count, 1);
	EXPECT_TRUE(record.last);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().draws, 2u);

	//再切换回未选中
	ASSERT_TRUE(ui->command_dispatch("toggle"));
	EXPECT_FALSE(ui->checked_get(toggle));
	EXPECT_EQ(record.count, 2);
	EXPECT_FALSE(record.last);
}

//像素：选中后左侧轨道为强调色，未选中同位置为滑块浅色
TEST_F(Renderer_Ui_Switch_Test, 选中轨道像素)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//开关 20..60，纵向 20..40，滑块未选中在左、选中在右
	const Ui_Handle toggle = ui->switch_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 40.0f, 20.0f }, false);
	ASSERT_NE(toggle, 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//未选中：左侧为滑块（近白），右中为灰色轨道
	const std::array<uint8_t, 4> left_off = byjy_render_test::pixel_get(pixels, 28, 30, 320);
	EXPECT_GT(byjy_render_test::luma_of(left_off), 180.0f) << "未选中左侧应为浅色滑块";

	//选中：左侧轨道变强调色（回读 BGRA，蓝占优）
	ASSERT_TRUE(ui->checked_set(toggle, true));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const std::array<uint8_t, 4> left_on = byjy_render_test::pixel_get(pixels, 28, 30, 320);
	EXPECT_GT(static_cast<int>(left_on[0]), static_cast<int>(left_on[2]) + 30)
		<< "选中后左侧轨道应为蓝色强调色";
}
