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
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
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

//九宫格切分：四边切分后产生 9 条绘制记录，像素仍铺满图片区域
TEST_F(Renderer_Ui_Image_Test, 九宫格切分绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	const Ui_Handle image = ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, texture);
	ASSERT_NE(image, 0u);
	ASSERT_TRUE(ui->image_slice_set(image, 0.25f, 0.25f, 0.25f, 0.25f));

	//读取回一致
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	ui->image_slice_get(image, left, top, right, bottom);
	EXPECT_FLOAT_EQ(left, 0.25f);
	EXPECT_FLOAT_EQ(top, 0.25f);
	EXPECT_FLOAT_EQ(right, 0.25f);
	EXPECT_FLOAT_EQ(bottom, 0.25f);

	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.draws, 1u) << "九宫格九块共用一条绘制记录";
	EXPECT_EQ(stats.vertices, 36u);
	EXPECT_EQ(stats.indices, 54u);
	EXPECT_EQ(stats.texture_binds, 1u) << "同一纹理只绑定一次描述符";

	//仅左侧切分：退化为 2 块，仍为单条绘制记录
	ASSERT_TRUE(ui->image_slice_set(image, 0.5f, 0.0f, 0.0f, 0.0f));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& partial = renderer.ui().ui_render_stats();
	EXPECT_EQ(partial.draws, 1u);
	EXPECT_EQ(partial.vertices, 8u) << "零宽 / 零高子块应被跳过";
	EXPECT_EQ(partial.indices, 12u);

	//回读：四角与中心均为纹理色
	if (renderer.compute().readback_supported())
	{
		std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		EXPECT_TRUE(byjy_render_test::pixel_is_green(byjy_render_test::pixel_get(pixels, 70, 50, 320)))
			<< "中心块应显示纹理";
		EXPECT_TRUE(byjy_render_test::pixel_is_green(byjy_render_test::pixel_get(pixels, 25, 25, 320)))
			<< "左上角块应显示纹理";
	}
	renderer.resource().texture_unload(texture);
}

//九宫格切分：全 0 退化为整图，越界参数被钳制，未知句柄返回假
TEST_F(Renderer_Ui_Image_Test, 九宫格切分退化与钳制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	const Ui_Handle image = ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, texture);
	ASSERT_NE(image, 0u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().draws, 1u) << "切分全 0 应整图拉伸";

	//越界钳制：左右之和不超过 1
	ASSERT_TRUE(ui->image_slice_set(image, 0.8f, 0.0f, 0.8f, 0.0f));
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	ui->image_slice_get(image, left, top, right, bottom);
	EXPECT_FLOAT_EQ(left, 0.8f);
	EXPECT_NEAR(left + right, 1.0f, 1e-6f);
	EXPECT_FLOAT_EQ(top, 0.0f);
	EXPECT_FLOAT_EQ(bottom, 0.0f);

	//未知句柄
	EXPECT_FALSE(ui->image_slice_set(image + 100, 0.1f, 0.1f, 0.1f, 0.1f));
	ui->image_slice_get(image + 100, left, top, right, bottom);
	EXPECT_FLOAT_EQ(left, 0.0f);
	EXPECT_FLOAT_EQ(top, 0.0f);
	renderer.resource().texture_unload(texture);
}

//九宫格像素边距：按纹理尺寸换算为归一化切分，非法尺寸返回假
TEST_F(Renderer_Ui_Image_Test, 九宫格像素边距换算)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle image = ui->image_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, 0);
	ASSERT_NE(image, 0u);

	//64×32 纹理、四边 16 / 8 像素 → 0.25 四边
	ASSERT_TRUE(ui->image_slice_pixels_set(image, 64, 32, 16.0f, 8.0f, 16.0f, 8.0f));
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	ui->image_slice_get(image, left, top, right, bottom);
	EXPECT_FLOAT_EQ(left, 0.25f);
	EXPECT_FLOAT_EQ(top, 0.25f);
	EXPECT_FLOAT_EQ(right, 0.25f);
	EXPECT_FLOAT_EQ(bottom, 0.25f);

	//非法纹理尺寸：返回假且不改变现状
	EXPECT_FALSE(ui->image_slice_pixels_set(image, 0, 32, 8.0f, 8.0f, 8.0f, 8.0f));
	ui->image_slice_get(image, left, top, right, bottom);
	EXPECT_FLOAT_EQ(left, 0.25f);
	EXPECT_FLOAT_EQ(top, 0.25f);

	//未知句柄
	EXPECT_FALSE(ui->image_slice_pixels_set(image + 100, 64, 32, 8.0f, 8.0f, 8.0f, 8.0f));
}
