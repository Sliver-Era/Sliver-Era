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
