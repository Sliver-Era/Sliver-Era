/*
渲染 UI 样式主题测试（第八百八十三轮）
覆盖：默认主题的角色常态色、按钮状态色、禁用色、自定义角色回退、
      主题设置 / 读取与控件样式角色存取。
纯数据用例直接构造 Ui_Context，不依赖 GPU。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//颜色分量近似相等
	void expect_color(const Ui_Color& actual, const Ui_Color& expected)
	{
		EXPECT_FLOAT_EQ(actual.r, expected.r);
		EXPECT_FLOAT_EQ(actual.g, expected.g);
		EXPECT_FLOAT_EQ(actual.b, expected.b);
		EXPECT_FLOAT_EQ(actual.a, expected.a);
	}
}

//渲染 UI 样式测试夹具
class Renderer_Ui_Style_Test : public ::testing::Test
{
};

//按钮：常态 / 悬停 / 按下 / 禁用分别取主题对应色
TEST_F(Renderer_Ui_Style_Test, 按钮状态色)
{
	const Ui_Theme theme;
	expect_color(ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Normal), theme.button);
	expect_color(ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Hovered), theme.button_hover);
	expect_color(ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Pressed), theme.button_pressed);
	expect_color(ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Disabled), theme.disabled);
	//焦点状态沿用常态背景（焦点环由绘制侧单独处理）
	expect_color(ui_theme_color(theme, Ui_Role::Button, Ui_Visual_State::Focused), theme.button);
}

//各角色常态色
TEST_F(Renderer_Ui_Style_Test, 角色常态色)
{
	const Ui_Theme theme;
	expect_color(ui_theme_color(theme, Ui_Role::Panel, Ui_Visual_State::Normal), theme.panel);
	expect_color(ui_theme_color(theme, Ui_Role::Text_Box, Ui_Visual_State::Normal), theme.text_box);
	expect_color(ui_theme_color(theme, Ui_Role::Label, Ui_Visual_State::Normal), theme.text);
	//复选框 / 滑动条取控件底色（第九百二十八轮），选中 / 滑块前景另用强调色
	expect_color(ui_theme_color(theme, Ui_Role::Checkbox, Ui_Visual_State::Normal), theme.text_box);
	expect_color(ui_theme_color(theme, Ui_Role::Slider, Ui_Visual_State::Normal), theme.text_box);
}

//禁用：标签用次级文本色，其余用禁用背景
TEST_F(Renderer_Ui_Style_Test, 禁用色)
{
	const Ui_Theme theme;
	expect_color(ui_theme_color(theme, Ui_Role::Label, Ui_Visual_State::Disabled), theme.text_dim);
	expect_color(ui_theme_color(theme, Ui_Role::Panel, Ui_Visual_State::Disabled), theme.disabled);
	expect_color(ui_theme_color(theme, Ui_Role::Checkbox, Ui_Visual_State::Disabled), theme.disabled);
}

//自定义角色不取主题，回退为白色
TEST_F(Renderer_Ui_Style_Test, 自定义角色回退)
{
	const Ui_Theme theme;
	expect_color(ui_theme_color(theme, Ui_Role::Custom, Ui_Visual_State::Normal), Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	expect_color(ui_theme_color(theme, Ui_Role::Custom, Ui_Visual_State::Disabled), Ui_Color{ 1.0f, 1.0f, 1.0f, 1.0f });
}

//主题设置 / 读取与控件样式角色存取
TEST_F(Renderer_Ui_Style_Test, 主题与角色存取)
{
	Ui_Context ui;
	//默认主题可读取
	const Ui_Theme& default_theme = ui.theme_get();
	EXPECT_FLOAT_EQ(default_theme.button.r, 0.20f);
	//自定义主题写入后原样读回
	Ui_Theme custom = default_theme;
	custom.button = Ui_Color{ 0.5f, 0.25f, 0.75f, 1.0f };
	ui.theme_set(custom);
	EXPECT_FLOAT_EQ(ui.theme_get().button.r, 0.5f);
	EXPECT_FLOAT_EQ(ui.theme_get().button.g, 0.25f);

	//角色默认 Custom，可设置并读回
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 10.0f, 10.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	EXPECT_EQ(ui.role_get(panel), Ui_Role::Custom);
	ASSERT_TRUE(ui.role_set(panel, Ui_Role::Panel));
	EXPECT_EQ(ui.role_get(panel), Ui_Role::Panel);
	//未知句柄拒绝设置，读取回退 Custom
	EXPECT_FALSE(ui.role_set(panel + 100, Ui_Role::Button));
	EXPECT_EQ(ui.role_get(panel + 100), Ui_Role::Custom);
}

//样式角色与显式同色控件绘制结果一致（像素等价）
TEST_F(Renderer_Ui_Style_Test, 角色色与显式色像素等价)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	//本机不支持像素回读时跳过
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//左侧：样式角色按钮；右侧：显式写入主题按钮色的自定义按钮
	const Ui_Handle themed = ui->button_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, "");
	const Ui_Handle explicit_color = ui->button_create(ui->root_get(),
		Ui_Rect{ 140.0f, 20.0f, 80.0f, 30.0f }, "", nullptr, nullptr,
		ui->theme_get().button);
	ASSERT_NE(themed, 0u);
	ASSERT_NE(explicit_color, 0u);
	ASSERT_TRUE(ui->role_set(themed, Ui_Role::Button));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const std::array<uint8_t, 4> left = byjy_render_test::pixel_get(pixels, 60, 35, 320);
	const std::array<uint8_t, 4> right = byjy_render_test::pixel_get(pixels, 180, 35, 320);
	EXPECT_EQ(left, right) << "样式角色按钮应与显式同色按钮像素一致";
}

//控件默认角色：复选框 / 滑动条跟随主题底色，Custom 仍可覆盖（第九百二十八轮）
TEST_F(Renderer_Ui_Style_Test, 控件默认角色与主题跟随)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	//复选框与滑动条默认引用各自角色
	const Ui_Handle check = ui->checkbox_create(ui->root_get(),
		Ui_Rect{ 20.0f, 20.0f, 24.0f, 24.0f }, "");
	const Ui_Handle bar = ui->slider_create(ui->root_get(),
		Ui_Rect{ 20.0f, 80.0f, 160.0f, 20.0f }, 0.0f);
	ASSERT_NE(check, 0u);
	ASSERT_NE(bar, 0u);
	EXPECT_EQ(ui->role_get(check), Ui_Role::Checkbox);
	EXPECT_EQ(ui->role_get(bar), Ui_Role::Slider);

	//主题文本框底色改为红色：两者底面随之变红
	Ui_Theme theme = ui->theme_get();
	theme.text_box = Ui_Color{ 0.8f, 0.1f, 0.1f, 1.0f };
	ui->theme_set(theme);

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//欠选中的方框中心（回读 BGRA，红为下标 2）
	const std::array<uint8_t, 4> box_pixel = byjy_render_test::pixel_get(pixels, 32, 32, 320);
	//滑动条轨道中点（纵坐标对齐轨道）
	const std::array<uint8_t, 4> track_pixel = byjy_render_test::pixel_get(pixels, 120, 90, 320);
	EXPECT_GT(static_cast<int>(box_pixel[2]), static_cast<int>(box_pixel[0]) + 40)
		<< "复选框底面应跟随主题红";
	EXPECT_GT(static_cast<int>(track_pixel[2]), static_cast<int>(track_pixel[0]) + 40)
		<< "滑动条轨道应跟随主题红";

	//显式回退 Custom：改为暗灰后不再跟随主题
	ASSERT_TRUE(ui->role_set(check, Ui_Role::Custom));
	ASSERT_TRUE(ui->color_set(check, Ui_Color{ 0.1f, 0.1f, 0.1f, 1.0f }));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const std::array<uint8_t, 4> custom_pixel = byjy_render_test::pixel_get(pixels, 32, 32, 320);
	EXPECT_LT(static_cast<int>(custom_pixel[2]), 120) << "Custom 角色应使用节点颜色";
}

//不透明度：越界夹紧、存取与未知句柄
TEST_F(Renderer_Ui_Style_Test, 不透明度夹紧与存取)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 10.0f, 10.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	//默认不透明
	EXPECT_FLOAT_EQ(ui.opacity_get(panel), 1.0f);
	//上界夹紧
	ASSERT_TRUE(ui.opacity_set(panel, 2.0f));
	EXPECT_FLOAT_EQ(ui.opacity_get(panel), 1.0f);
	//下界夹紧
	ASSERT_TRUE(ui.opacity_set(panel, -1.0f));
	EXPECT_FLOAT_EQ(ui.opacity_get(panel), 0.0f);
	//正常值
	ASSERT_TRUE(ui.opacity_set(panel, 0.5f));
	EXPECT_FLOAT_EQ(ui.opacity_get(panel), 0.5f);
	//未知句柄
	EXPECT_FALSE(ui.opacity_set(panel + 100, 0.5f));
	EXPECT_FLOAT_EQ(ui.opacity_get(panel + 100), 1.0f);
}

//不透明度影响最终像素：半透明面板明显暗于不透明同色面板
TEST_F(Renderer_Ui_Style_Test, 不透明度影响像素)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);
	const Ui_Color gray{ 0.6f, 0.6f, 0.6f, 1.0f };
	const Ui_Handle opaque = ui->panel_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 30.0f }, gray);
	const Ui_Handle semi = ui->panel_create(ui->root_get(), Ui_Rect{ 140.0f, 20.0f, 80.0f, 30.0f }, gray);
	ASSERT_NE(opaque, 0u);
	ASSERT_NE(semi, 0u);
	ASSERT_TRUE(ui->opacity_set(semi, 0.5f));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	const std::array<uint8_t, 4> full = byjy_render_test::pixel_get(pixels, 60, 35, 320);
	const std::array<uint8_t, 4> half = byjy_render_test::pixel_get(pixels, 180, 35, 320);
	//取 RGB 前三通道均值的近似亮度（读回通道顺序不敏感）
	auto luma = [](const std::array<uint8_t, 4>& value)
		{
			return static_cast<int>(value[0]) + value[1] + value[2];
		};
	EXPECT_GT(luma(full), luma(half) + 60) << "半透明面板应明显暗于不透明面板";
}

//圆角半径与描边：默认值、越界夹紧与存取（第八百八十七轮）
TEST_F(Renderer_Ui_Style_Test, 圆角半径与描边存取)
{
	Ui_Context ui;
	const Ui_Handle panel = ui.panel_create(ui.root_get(), Ui_Rect{ 0.0f, 0.0f, 40.0f, 40.0f }, Ui_Color{});
	ASSERT_NE(panel, 0u);
	//默认直角且无描边
	EXPECT_FLOAT_EQ(ui.corner_radius_get(panel), 0.0f);
	EXPECT_FLOAT_EQ(ui.border_width_get(panel), 0.0f);
	//正常写入与读回
	ASSERT_TRUE(ui.corner_radius_set(panel, 8.0f));
	EXPECT_FLOAT_EQ(ui.corner_radius_get(panel), 8.0f);
	//负半径按直角
	ASSERT_TRUE(ui.corner_radius_set(panel, -5.0f));
	EXPECT_FLOAT_EQ(ui.corner_radius_get(panel), 0.0f);
	//描边写入与读回（负宽度按无描边）
	const Ui_Color border{ 0.9f, 0.1f, 0.1f, 1.0f };
	ASSERT_TRUE(ui.border_set(panel, 3.0f, border));
	EXPECT_FLOAT_EQ(ui.border_width_get(panel), 3.0f);
	expect_color(ui.border_color_get(panel), border);
	ASSERT_TRUE(ui.border_set(panel, -2.0f, border));
	EXPECT_FLOAT_EQ(ui.border_width_get(panel), 0.0f);
	//未知句柄
	EXPECT_FALSE(ui.corner_radius_set(panel + 100, 4.0f));
	EXPECT_FLOAT_EQ(ui.corner_radius_get(panel + 100), 0.0f);
	EXPECT_FALSE(ui.border_set(panel + 100, 2.0f, border));
	EXPECT_FLOAT_EQ(ui.border_width_get(panel + 100), 0.0f);
}

//半径为零的圆角绘制与普通直角完全一致（像素等价，保证既有路径不变）
TEST_F(Renderer_Ui_Style_Test, 半径为零绘制等价直角)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color gray{ 0.7f, 0.7f, 0.7f, 1.0f };
	//左：默认直角；右：显式写入半径 0
	const Ui_Handle plain = ui->panel_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 60.0f, 60.0f }, gray);
	const Ui_Handle zeroed = ui->panel_create(ui->root_get(), Ui_Rect{ 140.0f, 20.0f, 60.0f, 60.0f }, gray);
	ASSERT_NE(plain, 0u);
	ASSERT_NE(zeroed, 0u);
	ASSERT_TRUE(ui->corner_radius_set(zeroed, 0.0f));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//四角与中心按相对偏移逐一比对
	const std::array<std::array<uint32_t, 2>, 5> samples{ {
		{ 21, 21 }, { 78, 21 }, { 21, 78 }, { 78, 78 }, { 50, 50 } } };
	for (const std::array<uint32_t, 2>& sample : samples)
	{
		const std::array<uint8_t, 4> left = byjy_render_test::pixel_get(pixels, sample[0], sample[1], 320);
		const std::array<uint8_t, 4> right = byjy_render_test::pixel_get(pixels, sample[0] + 120, sample[1], 320);
		EXPECT_EQ(left, right) << "半径 0 应与直角绘制一致，偏移 (" << sample[0] << "," << sample[1] << ")";
	}
}

//圆角与直角在四角像素不同、中心与边中点保持一致
TEST_F(Renderer_Ui_Style_Test, 圆角四角差异)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(byjy_render_test::hidden_config(320, 180)));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机不支持像素回读";
	Ui_Context* ui = renderer.ui().ui_context();
	ASSERT_NE(ui, nullptr);

	const Ui_Color bright{ 0.8f, 0.8f, 0.8f, 1.0f };
	//左：圆角；右：直角
	const Ui_Handle rounded = ui->panel_create(ui->root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 80.0f }, bright);
	const Ui_Handle square = ui->panel_create(ui->root_get(), Ui_Rect{ 140.0f, 20.0f, 80.0f, 80.0f }, bright);
	ASSERT_NE(rounded, 0u);
	ASSERT_NE(square, 0u);
	ASSERT_TRUE(ui->corner_radius_set(rounded, 16.0f));

	std::vector<uint8_t> pixels(byjy_render_test::pixel_bytes(320, 180), 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//左上角：圆角区域被挖空，直角区域为填充色
	const std::array<uint8_t, 4> rounded_corner = byjy_render_test::pixel_get(pixels, 21, 21, 320);
	const std::array<uint8_t, 4> square_corner = byjy_render_test::pixel_get(pixels, 141, 21, 320);
	EXPECT_NE(rounded_corner, square_corner) << "圆角四角应不同于直角四角";
	//中心像素一致
	const std::array<uint8_t, 4> rounded_center = byjy_render_test::pixel_get(pixels, 60, 60, 320);
	const std::array<uint8_t, 4> square_center = byjy_render_test::pixel_get(pixels, 180, 60, 320);
	EXPECT_EQ(rounded_center, square_center) << "圆角与直角中心应同为填充色";
	//上边中点一致（圆角不改变直边）
	const std::array<uint8_t, 4> rounded_top = byjy_render_test::pixel_get(pixels, 60, 21, 320);
	const std::array<uint8_t, 4> square_top = byjy_render_test::pixel_get(pixels, 180, 21, 320);
	EXPECT_EQ(rounded_top, square_top) << "圆角不应影响直边中部";
}
