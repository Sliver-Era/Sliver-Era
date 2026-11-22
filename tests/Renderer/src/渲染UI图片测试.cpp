/*
渲染 UI 图片测试（第八百九十三轮）
覆盖：图片纹理绘制为像素、纹理句柄设置 / 读取与未知句柄、
      无纹理不绘制、图片不参与交互命中。
像素用例按回读能力门控。
*/
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>

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
}

//渲染 UI 图片测试夹具
class Renderer_Ui_Image_Test : public ::testing::Test
{
};

//图片按纹理铺满矩形绘制
TEST_F(Renderer_Ui_Image_Test, 图片纹理绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//加载纯绿纹理
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	ASSERT_NE(texture, 0u);
	const Ui_Handle image = ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, texture);
	ASSERT_NE(image, 0u);
	EXPECT_EQ(ui->image_texture_get(image), texture);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//图片区域为绿色
	EXPECT_TRUE(byjy_render_test::pixel_is_green(byjy_render_test::pixel_get(pixels, 60, 45, 320)))
		<< "图片区域应显示纹理颜色";
	//图片外仍为背景
	EXPECT_FALSE(byjy_render_test::pixel_is_green(byjy_render_test::pixel_get(pixels, 250, 150, 320)));
	renderer.resource().texture_unload(texture);
}

//纹理句柄存取与未知句柄；无纹理时不绘制，补设纹理后绘制
TEST_F(Renderer_Ui_Image_Test, 纹理存取与空纹理)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Handle image = ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, 0);
	ASSERT_NE(image, 0u);
	EXPECT_EQ(ui->image_texture_get(image), 0u);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_FALSE(byjy_render_test::pixel_is_green(byjy_render_test::pixel_get(pixels, 60, 45, 320)))
		<< "纹理为 0 时不应绘制";

	//补设纹理后当帧绘制
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	ASSERT_TRUE(ui->image_texture_set(image, texture));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_TRUE(byjy_render_test::pixel_is_green(byjy_render_test::pixel_get(pixels, 60, 45, 320)))
		<< "设置纹理后应绘制图片";

	//未知句柄
	EXPECT_FALSE(ui->image_texture_set(image + 100, texture));
	EXPECT_EQ(ui->image_texture_get(image + 100), 0u);
	renderer.resource().texture_unload(texture);
}

//图片不参与交互命中（悬停不落在图片上）
TEST_F(Renderer_Ui_Image_Test, 图片不参与命中)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	Input_Aggregator* input = renderer.ui().input_aggregator();
	ASSERT_NE(ui, nullptr);
	ASSERT_NE(input, nullptr);

	const Ui_Handle image = ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, 0);
	ASSERT_NE(image, 0u);
	ASSERT_TRUE(renderer.frame_render());

	SDL_Event motion = mouse_motion(60.0f, 40.0f);
	input->event_feed(&motion);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_FALSE(ui->hovered_get(image)) << "图片不应参与交互命中";
}

//内容阶段性能基线：64 个图片节点交替两种纹理，60 帧（不设硬阈值）
TEST_F(Renderer_Ui_Image_Test, 内容阶段性能基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//两张交替使用的纹理
	const std::vector<uint8_t> green = byjy_render_test::solid_green();
	const std::vector<uint8_t> white = byjy_render_test::solid_white();
	uint64_t green_texture = 0;
	uint64_t white_texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(green_texture, green.data(), 2, 2));
	ASSERT_TRUE(renderer.resource().texture_load(white_texture, white.data(), 2, 2));

	//64 个图片节点铺成网格
	for (int index = 0; index < 64; ++index)
	{
		const int column = index % 8;
		const int row = index / 8;
		const Ui_Rect rect{ static_cast<float>(column) * 40.0f, static_cast<float>(row) * 22.0f, 36.0f, 20.0f };
		const uint64_t texture = (index % 2 == 0) ? green_texture : white_texture;
		const Ui_Handle image = ui->image_create(ui->root_get(), rect, texture);
		ASSERT_NE(image, 0u);
	}

	//60 帧绘制
	const auto start = std::chrono::steady_clock::now();
	for (int frame = 0; frame < 60; ++frame)
		ASSERT_TRUE(renderer.frame_render());
	const auto end = std::chrono::steady_clock::now();
	const double milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
	std::cout << "[UI 内容基线] 64 图片节点 60 帧：" << (milliseconds / 60.0) << " ms/帧（含绘制）" << std::endl;
	EXPECT_GE(milliseconds, 0.0);

	renderer.resource().texture_unload(green_texture);
	renderer.resource().texture_unload(white_texture);
}
