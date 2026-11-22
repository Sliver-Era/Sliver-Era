/*
渲染 UI 绘制统计测试（第八百九十九轮）
覆盖：纯色面板的顶点 / 索引 / 记录数、文字绘制记录与纹理重绑计数、
      图片绘制的纹理重绑计数。
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
}

//渲染 UI 统计测试夹具
class Renderer_Ui_Stats_Test : public ::testing::Test
{
};

//单个纯色面板：4 顶点 / 6 索引 / 1 记录
TEST_F(Renderer_Ui_Stats_Test, 纯色面板统计)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 10.0f, 10.0f, 100.0f, 50.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.vertices, 4u);
	EXPECT_EQ(stats.indices, 6u);
	EXPECT_EQ(stats.draws, 1u);
	EXPECT_EQ(stats.text_draws, 0u);
	EXPECT_EQ(stats.texture_binds, 0u);
}

//文字：至少一条文字记录且发生一次纹理重绑
TEST_F(Renderer_Ui_Stats_Test, 文字绘制统计)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	const Ui_Handle label = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, "AB");
	ASSERT_NE(label, 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 1u);
	EXPECT_GE(stats.texture_binds, 1u);
	EXPECT_GT(stats.indices, 6u) << "文字应产生额外几何";
}

//图片：一条纹理记录且发生一次纹理重绑，文字计数为零
TEST_F(Renderer_Ui_Stats_Test, 图片绘制统计)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const std::vector<uint8_t> texels = byjy_render_test::solid_green();
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	const Ui_Handle image = ui->image_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 100.0f, 60.0f }, texture);
	ASSERT_NE(image, 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 0u);
	EXPECT_EQ(stats.draws, 1u);
	EXPECT_EQ(stats.texture_binds, 1u);
	EXPECT_EQ(stats.vertices, 4u);
	EXPECT_EQ(stats.indices, 6u);
	renderer.resource().texture_unload(texture);
}

//稳定帧缓存：无变化时第二帧命中，属性变化后重建
TEST_F(Renderer_Ui_Stats_Test, 稳定帧缓存命中)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 10.0f, 10.0f, 100.0f, 50.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	ASSERT_NE(panel, 0u);

	//首帧重建
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& first = renderer.ui().ui_render_stats();
	EXPECT_EQ(first.cache_hits, 0u);
	EXPECT_EQ(first.vertices, 4u);
	//第二帧无变化：命中缓存
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& second = renderer.ui().ui_render_stats();
	EXPECT_EQ(second.cache_hits, 1u);
	EXPECT_EQ(second.vertices, 4u);
	//修改颜色后重建
	ASSERT_TRUE(ui->color_set(panel, Ui_Color{ 0.6f, 0.2f, 0.2f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& third = renderer.ui().ui_render_stats();
	EXPECT_EQ(third.cache_hits, 0u) << "属性变化后应重建几何";
}

//稳定帧上传跳过：同一帧槽重复使用且内容不变时不再上传
TEST_F(Renderer_Ui_Stats_Test, 稳定帧上传跳过)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 10.0f, 10.0f, 100.0f, 50.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	ASSERT_NE(panel, 0u);
	ASSERT_TRUE(renderer.frame_render());

	//多推进若干帧，等待同一帧槽再次被使用
	bool skipped = false;
	for (int frame = 0; frame < 8; ++frame)
	{
		ASSERT_TRUE(renderer.frame_render());
		if (renderer.ui().ui_render_stats().upload_skips == 1)
		{
			skipped = true;
			break;
		}
	}
	EXPECT_TRUE(skipped) << "稳定帧复用同一槽时应跳过缓冲上传";

	//属性变化后恢复上传
	ASSERT_TRUE(ui->color_set(panel, Ui_Color{ 0.7f, 0.3f, 0.1f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& changed = renderer.ui().ui_render_stats();
	EXPECT_EQ(changed.cache_hits, 0u);
	EXPECT_EQ(changed.upload_skips, 0u) << "内容变化后应重新上传";
}

//文本几何缓存：同帧同文本的第二个控件直接命中
TEST_F(Renderer_Ui_Stats_Test, 文本几何缓存同帧命中)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	ASSERT_NE(ui->label_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, "AB"), 0u);
	ASSERT_NE(ui->label_create(ui->root_get(), Ui_Rect{ 20.0f, 60.0f, 200.0f, 24.0f }, "AB"), 0u);
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.text_draws, 2u);
	EXPECT_EQ(stats.text_cache_misses, 1u);
	EXPECT_EQ(stats.text_cache_hits, 1u) << "同文本第二控件应复用几何";
	EXPECT_GT(stats.indices, 12u);
}

//文本几何缓存：整树重建但文本未变时全部命中，改文本后仅该项未命中
TEST_F(Renderer_Ui_Stats_Test, 文本几何缓存重建命中)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	const Ui_Handle panel = ui->panel_create(ui->root_get(),
		Ui_Rect{ 10.0f, 10.0f, 200.0f, 100.0f }, Ui_Color{ 0.2f, 0.2f, 0.2f, 1.0f });
	ASSERT_NE(panel, 0u);
	const Ui_Handle first = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 200.0f, 24.0f }, "AB");
	const Ui_Handle second = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 60.0f, 200.0f, 24.0f }, "CD");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.ui().ui_render_stats().text_cache_misses, 2u);

	//改面板颜色触发整树重建，两段文本均命中缓存
	ASSERT_TRUE(ui->color_set(panel, Ui_Color{ 0.4f, 0.2f, 0.2f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& rebuilt = renderer.ui().ui_render_stats();
	EXPECT_EQ(rebuilt.cache_hits, 0u);
	EXPECT_EQ(rebuilt.text_cache_hits, 2u);
	EXPECT_EQ(rebuilt.text_cache_misses, 0u) << "文本未变应全部复用几何";

	//改其中一个标签文本：仅该项未命中
	ASSERT_TRUE(ui->text_set(first, "EF"));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& edited = renderer.ui().ui_render_stats();
	EXPECT_EQ(edited.text_cache_hits, 1u);
	EXPECT_EQ(edited.text_cache_misses, 1u) << "文本变化项应重新生成几何";
}

//文本块布局缓存：同帧同文本同宽的第二控件直接命中
TEST_F(Renderer_Ui_Stats_Test, 文本块布局缓存同帧命中)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	const Ui_Handle first = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 60.0f }, "ABCDEFGHIJKLMNOP");
	const Ui_Handle second = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 100.0f, 80.0f, 60.0f }, "ABCDEFGHIJKLMNOP");
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_TRUE(ui->text_layout_set(first, true, 0.0f, Ui_Align::Start));
	ASSERT_TRUE(ui->text_layout_set(second, true, 0.0f, Ui_Align::Start));
	ASSERT_TRUE(renderer.frame_render());

	const Ui_Render_Stats& stats = renderer.ui().ui_render_stats();
	EXPECT_EQ(stats.block_cache_misses, 1u);
	EXPECT_EQ(stats.block_cache_hits, 1u) << "同文本同宽第二控件应复用分行结果";
}

//文本块布局缓存：区域宽度与换行开关均参与键
TEST_F(Renderer_Ui_Stats_Test, 文本块布局缓存宽度与换行)
{
	if (!font_available())
		GTEST_SKIP() << "系统字体不存在";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(320, 180)));
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	uint64_t font = 0;
	ASSERT_TRUE(renderer.ui().font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ui->font_set(font);
	const Ui_Handle first = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 80.0f, 60.0f }, "ABCDEFGHIJKLMNOP");
	const Ui_Handle second = ui->label_create(ui->root_get(),
		Ui_Rect{ 20.0f, 100.0f, 80.0f, 60.0f }, "ABCDEFGHIJKLMNOP");
	ASSERT_TRUE(ui->text_layout_set(first, true, 0.0f, Ui_Align::Start));
	ASSERT_TRUE(ui->text_layout_set(second, true, 0.0f, Ui_Align::Start));
	ASSERT_TRUE(renderer.frame_render());

	//改宽度：该控件未命中，另一控件仍命中
	ASSERT_TRUE(ui->rect_set(first, Ui_Rect{ 20.0f, 20.0f, 60.0f, 60.0f }));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& narrowed = renderer.ui().ui_render_stats();
	EXPECT_EQ(narrowed.block_cache_misses, 1u);
	EXPECT_EQ(narrowed.block_cache_hits, 1u) << "宽度变化项应重算分行";

	//关闭换行：第二控件未命中（键含换行开关）
	ASSERT_TRUE(ui->rect_set(first, Ui_Rect{ 20.0f, 20.0f, 80.0f, 60.0f }));
	ASSERT_TRUE(ui->text_layout_set(first, false, 0.0f, Ui_Align::Start));
	ASSERT_TRUE(renderer.frame_render());
	const Ui_Render_Stats& unwrapped = renderer.ui().ui_render_stats();
	EXPECT_EQ(unwrapped.block_cache_misses, 1u);
	EXPECT_EQ(unwrapped.block_cache_hits, 1u) << "换行开关变化项应重算分行";
}
