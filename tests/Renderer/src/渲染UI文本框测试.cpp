/*
渲染 UI 文本框编辑测试（第九百三十轮）
覆盖：文本设置与读取、光标钳制、UTF-8 续字节回退、未知句柄行为。
纯数据用例直接构造 Ui_Context，不依赖 GPU。
*/
#include <gtest/gtest.h>

#include "Engine/EngineRenderer/src/render/UI/用户界面.h"

//游戏引擎命名空间
using namespace engine;

//渲染 UI 文本框测试夹具
class Renderer_Ui_Text_Box_Test : public ::testing::Test
{
};

//文本设置：内容读回、光标随长度钳制
TEST_F(Renderer_Ui_Text_Box_Test, 文本设置与光标钳制)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "abc");
	ASSERT_NE(box, 0u);
	//创建时光标在末尾
	EXPECT_EQ(ui.caret_get(box), 3u);
	EXPECT_EQ(ui.text_box_text_get(box), "abc");

	//光标移到中段后换成单字（3 字节）：光标钳到串尾
	ASSERT_TRUE(ui.caret_set(box, 2));
	ASSERT_TRUE(ui.text_box_text_set(box, "中"));
	EXPECT_EQ(ui.text_box_text_get(box), "中");
	EXPECT_EQ(ui.caret_get(box), 3u) << "光标应钳到新文本长度";

	//换成空串：光标归零
	ASSERT_TRUE(ui.text_box_text_set(box, ""));
	EXPECT_EQ(ui.text_box_text_get(box), "");
	EXPECT_EQ(ui.caret_get(box), 0u);

	//未知句柄
	EXPECT_FALSE(ui.text_box_text_set(box + 100, "x"));
	EXPECT_EQ(ui.text_box_text_get(box + 100), "");
	EXPECT_FALSE(ui.caret_set(box + 100, 1));
	EXPECT_EQ(ui.caret_get(box + 100), 0u);
}

//退格：按字符删除光标前一字符并前移光标（第九百三十一轮）
TEST_F(Renderer_Ui_Text_Box_Test, 退格按字符删除)
{
	Ui_Context ui;
	//"中a" = 4 字节，光标在末尾
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "中a");
	ASSERT_NE(box, 0u);
	ASSERT_EQ(ui.caret_get(box), 4u);

	//退格删除 ASCII 末字符
	ASSERT_TRUE(ui.text_box_backspace(box));
	EXPECT_EQ(ui.text_box_text_get(box), "中");
	EXPECT_EQ(ui.caret_get(box), 3u);
	//退格删除整个中文字符
	ASSERT_TRUE(ui.text_box_backspace(box));
	EXPECT_EQ(ui.text_box_text_get(box), "");
	EXPECT_EQ(ui.caret_get(box), 0u);
	//串首无可删内容
	EXPECT_FALSE(ui.text_box_backspace(box));
	//未知句柄
	EXPECT_FALSE(ui.text_box_backspace(box + 100));
}

//删除：删除光标后一字符且光标不动
TEST_F(Renderer_Ui_Text_Box_Test, 删除后一字符)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "中a");
	ASSERT_NE(box, 0u);

	//光标移到串首后删除：中文整体移除，光标保持 0
	ASSERT_TRUE(ui.caret_set(box, 0));
	ASSERT_TRUE(ui.text_box_delete(box));
	EXPECT_EQ(ui.text_box_text_get(box), "a");
	EXPECT_EQ(ui.caret_get(box), 0u);
	//串尾无可删内容
	ASSERT_TRUE(ui.caret_set(box, 1));
	EXPECT_FALSE(ui.text_box_delete(box));
	//未知句柄
	EXPECT_FALSE(ui.text_box_delete(box + 100));
}

//光标移动：按码点前后移动，越界夹到串首 / 串尾（第九百三十二轮）
TEST_F(Renderer_Ui_Text_Box_Test, 光标按码点移动)
{
	Ui_Context ui;
	//"中ab" = 3 + 1 + 1 = 5 字节
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "中ab");
	ASSERT_NE(box, 0u);
	ASSERT_EQ(ui.caret_get(box), 5u);

	//左移 1：跨到 "a" 前（4）
	ASSERT_TRUE(ui.caret_move(box, -1));
	EXPECT_EQ(ui.caret_get(box), 4u);
	//左移 1 再 1：先到 3，再整体跨过中文到 0
	ASSERT_TRUE(ui.caret_move(box, -1));
	EXPECT_EQ(ui.caret_get(box), 3u);
	ASSERT_TRUE(ui.caret_move(box, -1));
	EXPECT_EQ(ui.caret_get(box), 0u);
	//串首继续左移不变化
	EXPECT_FALSE(ui.caret_move(box, -1));

	//右移 2：中文（3）后到 3，再到 4
	ASSERT_TRUE(ui.caret_move(box, 2));
	EXPECT_EQ(ui.caret_get(box), 4u);
	//右移越界夹到串尾
	ASSERT_TRUE(ui.caret_move(box, 10));
	EXPECT_EQ(ui.caret_get(box), 5u);
	//串尾继续右移不变化
	EXPECT_FALSE(ui.caret_move(box, 1));
	//零步不移动
	EXPECT_FALSE(ui.caret_move(box, 0));
	//未知句柄
	EXPECT_FALSE(ui.caret_move(box + 100, 1));
}

//Home / End：移到串首 / 串尾并返回是否移动
TEST_F(Renderer_Ui_Text_Box_Test, 光标Home与End)
{
	Ui_Context ui;
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "中a");
	ASSERT_NE(box, 0u);

	//从串尾移到串首
	EXPECT_TRUE(ui.caret_home(box));
	EXPECT_EQ(ui.caret_get(box), 0u);
	EXPECT_FALSE(ui.caret_home(box)) << "已在串首不移动";
	//移到串尾
	EXPECT_TRUE(ui.caret_end(box));
	EXPECT_EQ(ui.caret_get(box), 4u);
	EXPECT_FALSE(ui.caret_end(box)) << "已在串尾不移动";
	//未知句柄
	EXPECT_FALSE(ui.caret_home(box + 100));
	EXPECT_FALSE(ui.caret_end(box + 100));
}

//选中区间：反向选区归一化、边界回退与清空（第九百三十三轮）
TEST_F(Renderer_Ui_Text_Box_Test, 选中区间模型)
{
	Ui_Context ui;
	//"中ab" = 5 字节
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "中ab");
	ASSERT_NE(box, 0u);

	//反向选区（4 → 1）：归一化为 0..4（1 回退到中文字符头）
	std::size_t begin = 99;
	std::size_t end = 99;
	ASSERT_TRUE(ui.select_set(box, 4, 1));
	ui.select_get(box, begin, end);
	EXPECT_EQ(begin, 0u);
	EXPECT_EQ(end, 4u);

	//正向选区（3 → 5）
	ASSERT_TRUE(ui.select_set(box, 3, 5));
	ui.select_get(box, begin, end);
	EXPECT_EQ(begin, 3u);
	EXPECT_EQ(end, 5u);

	//越界夹紧
	ASSERT_TRUE(ui.select_set(box, 100, 0));
	ui.select_get(box, begin, end);
	EXPECT_EQ(begin, 0u);
	EXPECT_EQ(end, 5u);

	//清空
	ASSERT_TRUE(ui.select_clear(box));
	ui.select_get(box, begin, end);
	EXPECT_EQ(begin, 0u);
	EXPECT_EQ(end, 0u);

	//未知句柄
	EXPECT_FALSE(ui.select_set(box + 100, 0, 1));
	EXPECT_FALSE(ui.select_clear(box + 100));
	ui.select_get(box + 100, begin, end);
	EXPECT_EQ(begin, 0u);
	EXPECT_EQ(end, 0u);
}

//光标落在 UTF-8 续字节时回退到字符头
TEST_F(Renderer_Ui_Text_Box_Test, 光标字符边界回退)
{
	Ui_Context ui;
	//"中a" = E4 B8 AD 61（4 字节）
	const Ui_Handle box = ui.text_box_create(ui.root_get(),
		Ui_Rect{ 0.0f, 0.0f, 200.0f, 24.0f }, "中a");
	ASSERT_NE(box, 0u);

	//落在首字中间的续字节：回退到 0
	ASSERT_TRUE(ui.caret_set(box, 1));
	EXPECT_EQ(ui.caret_get(box), 0u);
	ASSERT_TRUE(ui.caret_set(box, 2));
	EXPECT_EQ(ui.caret_get(box), 0u);
	//字符头与串尾保持原值
	ASSERT_TRUE(ui.caret_set(box, 3));
	EXPECT_EQ(ui.caret_get(box), 3u);
	ASSERT_TRUE(ui.caret_set(box, 4));
	EXPECT_EQ(ui.caret_get(box), 4u);
	//越界夹到串尾
	ASSERT_TRUE(ui.caret_set(box, 100));
	EXPECT_EQ(ui.caret_get(box), 4u);
}
