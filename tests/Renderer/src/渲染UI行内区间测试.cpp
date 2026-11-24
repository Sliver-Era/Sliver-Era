/*
渲染 UI 行内着色区间测试（第九百四十一轮）
覆盖：区间增删与边界对齐、区间切分产生多条文字记录、重叠时后者优先。
像素用例按回读能力门控。
*/
#include <gtest/gtest.h>

#include <filesystem>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI渲染器.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//测试字体
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

	//测试字体是否存在
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path(FONT_PATH), code);
	}

	//构造隐藏窗口配置
	Renderer_Config hidden_config(uint32_t width, uint32_t height)
	{
		Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}

	//统计矩形区域内亮度达到阈值的像素数
	uint32_t bright_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0,
		uint32_t x1, uint32_t y1, float threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1; ++y)
		{
			for (uint32_t x = x0; x < x1; ++x)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y, 320)) >= threshold)
					++count;
			}
		}
		return count;
	}

	//扫描区域内最下方亮像素纵坐标（无亮像素返回 -1）
	int bottommost_lit_row(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t x1,
		uint32_t y0, uint32_t y1, float threshold)
	{
		for (uint32_t y = y1; y > y0; --y)
		{
			for (uint32_t x = x0; x < x1; ++x)
			{
				if (byjy_render_test::luma_of(byjy_render_test::pixel_get(pixels, x, y - 1, 320)) >= threshold)
					return static_cast<int>(y - 1);
			}
		}
		return -1;
	}
}

//渲染 UI 行内区间测试夹具
class Renderer_Ui_Text_Span_Test : public ::testing::Test
{
};

//区间增删：数量变化、空区间被拒、未知句柄
TEST_F(Renderer_Ui_Text_Span_Test, 区间增删与边界)
{
	Ui_Context ui;
	const Ui_Handle label = ui.label_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, "AABB", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	EXPECT_EQ(ui.text_span_count(label), 0u);

	//正常区间
	ASSERT_TRUE(ui.text_span_add(label, 0, 2, Ui_Color{ 0.0f, 0.0f, 0.0f, 1.0f }));
	EXPECT_EQ(ui.text_span_count(label), 1u);
	//空 / 反向区间被拒
	EXPECT_FALSE(ui.text_span_add(label, 2, 2, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }));
	EXPECT_FALSE(ui.text_span_add(label, 3, 2, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }));
	EXPECT_EQ(ui.text_span_count(label), 1u);
	//越界夹到串尾后仍有效
	ASSERT_TRUE(ui.text_span_add(label, 2, 100, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }));
	EXPECT_EQ(ui.text_span_count(label), 2u);

	//未知句柄
	EXPECT_FALSE(ui.text_span_add(label + 100, 0, 1, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }));
	EXPECT_EQ(ui.text_span_count(label + 100), 0u);

	//清空
	ASSERT_TRUE(ui.text_span_clear(label));
	EXPECT_EQ(ui.text_span_count(label), 0u);
	EXPECT_FALSE(ui.text_span_clear(label + 100));
}

//区间绘制：切分后记录 +1，后添加区间覆盖先添加者
TEST_F(Renderer_Ui_Text_Span_Test, 区间切分与后者优先)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//加载并绑定测试字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 40.0f }, "AABB", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 20.0f, Ui_Align::Start));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const uint32_t base_draws = renderer.ui().ui_render_stats().text_draws;
	EXPECT_EQ(base_draws, 1u) << "未切分时整行一条文字记录";
	EXPECT_GT(bright_count(pixels, 20, 20, 39, 42, 120.0f), 0u) << "左半应有白色正文";

	//首个区间取黑色：左半变暗、记录 +1
	ASSERT_TRUE(ui->text_span_add(label, 0, 2, Ui_Color{ 0.0f, 0.0f, 0.0f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws + 1u) << "区间切分应新增一条文字记录";
	EXPECT_EQ(bright_count(pixels, 20, 20, 39, 42, 120.0f), 0u) << "左半应被黑色区间覆盖";
	EXPECT_GT(bright_count(pixels, 40, 20, 62, 42, 120.0f), 0u) << "右半应保留正文色";

	//后添加白色区间覆盖同一范围：后者优先
	ASSERT_TRUE(ui->text_span_add(label, 0, 2, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws + 1u);
	EXPECT_GT(bright_count(pixels, 20, 20, 39, 42, 120.0f), 0u) << "后添加区间应覆盖前者";

	//清空区间：恢复整行单条记录
	ASSERT_TRUE(ui->text_span_clear(label));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.ui().ui_render_stats().text_draws, base_draws);
	EXPECT_GT(bright_count(pixels, 20, 20, 39, 42, 120.0f), 0u);
}

//区间字号：大字号区间像素更高、行高随之增大（第九百四十二轮）
TEST_F(Renderer_Ui_Text_Span_Test, 区间字号与行高)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	//加载小 / 大两种字号
	uint64_t small_font = 0;
	uint64_t large_font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(small_font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ASSERT_TRUE(renderer.ui().font_load_file(large_font, FONT_PATH, 32));
	ui->font_set(small_font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 140.0f }, "AA\nBB", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 0.0f, Ui_Align::Start));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const int base_bottom = bottommost_lit_row(pixels, 20, 60, 20, 120, 120.0f);
	ASSERT_GT(base_bottom, 20) << "基线两行应有可见墨迹";

	//首行区间改用大字号：字更高、行高增大，整块底部下移
	ASSERT_TRUE(ui->text_span_add(label, 0, 2, Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f }, large_font));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const int large_bottom = bottommost_lit_row(pixels, 20, 60, 20, 140, 120.0f);
	EXPECT_GT(large_bottom, base_bottom + 8) << "大字号区间应撑高行高并下推后续行";

	//清空区间：底部恢复
	ASSERT_TRUE(ui->text_span_clear(label));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(bottommost_lit_row(pixels, 20, 60, 20, 120, 120.0f), base_bottom);
}

//富文本布局缓存：区间不变命中、改区间失效（第九百四十三轮）
TEST_F(Renderer_Ui_Text_Span_Test, 富文本布局缓存命中与失效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);

	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 40.0f }, "AA", Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(ui->text_layout_set(label, false, 20.0f, Ui_Align::Start));

	//首帧：布局未缓存，必有未命中
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.ui().ui_render_stats().block_cache_misses, 1u);

	//只改背景色（不影响布局与区间）：应命中布局缓存
	ASSERT_TRUE(ui->color_set(label, Ui_Color{ 0.2f, 0.3f, 0.4f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.ui().ui_render_stats().block_cache_hits, 1u) << "区间与布局未变应命中";
	EXPECT_EQ(renderer.ui().ui_render_stats().block_cache_misses, 0u);

	//新增区间：键变化，应失效一次
	ASSERT_TRUE(ui->text_span_add(label, 0, 1, Ui_Color{ 1.0f, 0.0f, 0.0f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.ui().ui_render_stats().block_cache_misses, 1u) << "区间变化应使布局缓存失效";
}
