/*
渲染 UI 数值插值测试（第九百四十五轮）
覆盖：插值速率存取与未知句柄、按帧时间趋近、目标值读取不变。
像素用例按回读能力门控。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>

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

	//构造零输入帧（仅携带时间步与窗口尺寸）
	Input_Frame step_frame(double dt)
	{
		Input_Frame input{};
		input.dt = dt;
		input.window_width = 320;
		input.window_height = 180;
		return input;
	}

	//统计区域内高饱和像素数（强调色填充；通道序无关）
	uint32_t saturated_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0,
		uint32_t x1, uint32_t y1, uint32_t threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1; ++y)
		{
			for (uint32_t x = x0; x < x1; ++x)
			{
				const std::array<uint8_t, 4> value = byjy_render_test::pixel_get(pixels, x, y, 320);
				//取三通道极差作为饱和度近似
				const uint8_t high = std::max({ value[0], value[1], value[2] });
				const uint8_t low = std::min({ value[0], value[1], value[2] });
				if (static_cast<uint32_t>(high - low) >= threshold)
					++count;
			}
		}
		return count;
	}
}

//渲染 UI 数值插值测试夹具
class Renderer_Ui_Anim_Test : public ::testing::Test
{
};

//插值速率：默认 0、读写、未知句柄
TEST_F(Renderer_Ui_Anim_Test, 插值速率存取)
{
	Ui_Context ui;
	const Ui_Handle progress = ui.progress_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, 0.5f);
	ASSERT_NE(progress, 0u);
	EXPECT_FLOAT_EQ(ui.anim_speed_get(progress), 0.0f);

	ASSERT_TRUE(ui.anim_speed_set(progress, 2.0f));
	EXPECT_FLOAT_EQ(ui.anim_speed_get(progress), 2.0f);
	//关闭
	ASSERT_TRUE(ui.anim_speed_set(progress, 0.0f));
	EXPECT_FLOAT_EQ(ui.anim_speed_get(progress), 0.0f);

	//未知句柄
	EXPECT_FALSE(ui.anim_speed_set(progress + 100, 1.0f));
	EXPECT_FLOAT_EQ(ui.anim_speed_get(progress + 100), 0.0f);
}

//按帧时间趋近：显示值驱动绘制，目标值读取不变
TEST_F(Renderer_Ui_Anim_Test, 按帧时间趋近)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//满值进度条：整条强调色填充
	const Ui_Handle progress = ui->progress_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 20.0f }, 1.0f);
	ASSERT_NE(progress, 0u);
	ASSERT_TRUE(ui->anim_speed_set(progress, 2.0f));

	//目标值改为 0：显示值仍为 1
	ASSERT_TRUE(ui->value_set(progress, 0.0f));
	float target = -1.0f;
	ASSERT_TRUE(ui->value_get(progress, target));
	EXPECT_FLOAT_EQ(target, 0.0f);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_GT(saturated_count(pixels, 75, 22, 118, 38, 40), 0u) << "未推进时应保持满填充";

	//推进 0.25 秒、速率 2/s：显示值降到 0.5
	ui->frame_begin(step_frame(0.25));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(saturated_count(pixels, 75, 22, 118, 38, 40), 0u) << "右半应已回落";
	EXPECT_GT(saturated_count(pixels, 25, 22, 65, 38, 40), 0u) << "左半应仍填充";

	//再推进 1 秒：显示值到位 0
	ui->frame_begin(step_frame(1.0));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(saturated_count(pixels, 25, 22, 118, 38, 40), 0u) << "应全部回落至背景";
}
