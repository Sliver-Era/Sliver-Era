/*
渲染纹理测试（2026-11-21 自「渲染网格纹理测试.cpp」拆出）
覆盖：未初始化守卫、字节流纹理加载、棋盘格绑定跑帧、无效句柄回落、卸载后重绑定、
      多纹理逐条卸载、纹理读回与输入一致、区域更新与区域读回一致、纹理尺寸查询。
*/

#include "渲染网格纹理测试辅助.h"

TEST_F(Renderer_Texture_Test, 未初始化加载纹理失败)
{
	engine::Renderer renderer;
	//4×4 白色像素（RGBA8 紧排）
	const uint8_t pixels[64] = {};
	uint64_t handle = 9999u;
	EXPECT_FALSE(renderer.resource().texture_load(handle, pixels, 4, 4));
	EXPECT_EQ(handle, 0u);
}

//以字节流加载 4×4 纹理由成功，绑定后推进一帧
TEST_F(Renderer_Texture_Test, 加载字节流纹理成功)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//4×4 不透明红（RGBA8 紧排）
	uint8_t pixels[64] = {};
	for (uint32_t index = 0; index < 64; index += 4)
	{
		pixels[index] = 255;
		pixels[index + 3] = 255;
	}
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.resource().texture_load(handle, pixels, 4, 4));
	EXPECT_NE(handle, 0u);
	//绑定该纹理
	renderer.resource().texture_bind_set(handle);
	EXPECT_TRUE(renderer.frame_render());
	renderer.resource().texture_unload(handle);
}

//程序生成棋盘格纹理，绑定后推进一帧
TEST_F(Renderer_Texture_Test, 棋盘格纹理绑定跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.resource().texture_load_checkerboard(handle, 64, 64, 8));
	EXPECT_NE(handle, 0u);
	renderer.resource().texture_bind_set(handle);
	EXPECT_TRUE(renderer.frame_render());
	renderer.resource().texture_unload(handle);
}

//绑定无效句柄回落内置默认白纹理，推进一帧不报错
TEST_F(Renderer_Texture_Test, 绑定无效句柄回落默认)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//句柄为零即回落默认白纹理
	renderer.resource().texture_bind_set(0);
	EXPECT_TRUE(renderer.frame_render());
}

//纹理卸载后再绑定该句柄，回落默认白纹理且推进一帧不崩
TEST_F(Renderer_Texture_Test, 卸载后重绑定跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.resource().texture_load_checkerboard(handle, 32, 32, 4));
	renderer.resource().texture_bind_set(handle);
	ASSERT_TRUE(renderer.frame_render());
	//卸载该纹理
	renderer.resource().texture_unload(handle);
	//再次绑定已卸载句柄（内部回落默认白纹理）
	renderer.resource().texture_bind_set(handle);
	EXPECT_TRUE(renderer.frame_render());
	//重复卸载不报错
	renderer.resource().texture_unload(handle);
}

//多纹理创建后逐条卸载，随后推进一帧不崩
TEST_F(Renderer_Texture_Test, 多纹理创建后逐条卸载)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        //四条独立纹理
        uint64_t handles[4] = {};
        for (uint32_t index = 0; index < 4; ++index)
                ASSERT_TRUE(renderer.resource().texture_load_checkerboard(handles[index], 16, 16, 4));
        //逐条卸载（销毁顺序无关）
        for (uint32_t index = 0; index < 4; ++index)
                renderer.resource().texture_unload(handles[index]);
        EXPECT_TRUE(renderer.frame_render());
}

/*
纹理读回（第二十七轮）：texture_readback 读回第 0 级像素与输入逐字节一致
纹理为 RGBA8，读回走原样拷贝，故四个像素的字节应与输入完全相同；
同时覆盖无效句柄与容量不足两条拒绝路径。
*/
TEST_F(Renderer_Texture_Test, 纹理读回与输入一致)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        //2×2 四色像素（RGBA8 紧排）
        const unsigned char texels[16] = {
                255, 0, 0, 255,
                0, 255, 0, 255,
                0, 0, 255, 255,
                255, 255, 0, 255,
        };
        uint64_t texture = 0;
        ASSERT_TRUE(renderer.resource().texture_load(texture, texels, 2, 2));
        //读回第 0 级
        std::vector<uint8_t> pixels(16);
        uint32_t width = 0;
        uint32_t height = 0;
        ASSERT_TRUE(renderer.resource().texture_readback(texture, pixels.data(), 16, &width, &height));
        EXPECT_EQ(width, 2u);
        EXPECT_EQ(height, 2u);
        for (std::size_t index = 0; index < 16; ++index)
                EXPECT_EQ(pixels[index], texels[index]) << "第 " << index << " 字节不一致";
        //容量不足 / 无效句柄 / 未初始化语义：均返回假且尺寸为 0
        uint32_t small_width = 9;
        uint32_t small_height = 9;
        EXPECT_FALSE(renderer.resource().texture_readback(texture, pixels.data(), 15, &small_width, &small_height));
        EXPECT_EQ(small_width, 0u);
        EXPECT_EQ(small_height, 0u);
        EXPECT_FALSE(renderer.resource().texture_readback(9999u, pixels.data(), 16, nullptr, nullptr));
        renderer.resource().texture_unload(texture);
        //卸载后句柄失效
        EXPECT_FALSE(renderer.resource().texture_readback(texture, pixels.data(), 16, nullptr, nullptr));
}

/*
纹理区域更新（第一百五十一轮）
口径：4×4 白色纹理，把 (1,1) 起 2×2 区域更新为纯红：
      读回整幅后该区域四像素应为红、其余仍为白；越界区域与空指针均被拒绝且不改内容。
*/
TEST_F(Renderer_Texture_Test, 纹理区域更新)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//4×4 全白（RGBA8 紧排）
	std::vector<uint8_t> white(4 * 4 * 4, 255);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(texture, white.data(), 4, 4));
	//2×2 纯红
	const unsigned char red[16] = {
		255, 0, 0, 255,
		255, 0, 0, 255,
		255, 0, 0, 255,
		255, 0, 0, 255,
	};
	ASSERT_TRUE(renderer.resource().texture_update_region(texture, 1, 1, 2, 2, red));
	//整幅读回
	std::vector<uint8_t> pixels(4 * 4 * 4, 0);
	ASSERT_TRUE(renderer.resource().texture_readback(texture, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	//区域内四像素为红
	const uint32_t inside[4][2] = { { 1, 1 }, { 2, 1 }, { 1, 2 }, { 2, 2 } };
	for (uint32_t index = 0; index < 4; ++index)
	{
		std::size_t offset = (static_cast<std::size_t>(inside[index][1]) * 4 + inside[index][0]) * 4;
		EXPECT_EQ(pixels[offset], 255) << "区域红通道未更新";
		EXPECT_EQ(pixels[offset + 1], 0) << "区域绿通道未更新";
		EXPECT_EQ(pixels[offset + 2], 0) << "区域蓝通道未更新";
	}
	//区域外仍为白（取左上角与原右下角）
	std::size_t corner = 0;
	EXPECT_EQ(pixels[corner], 255);
	EXPECT_EQ(pixels[corner + 1], 255);
	EXPECT_EQ(pixels[corner + 2], 255);
	std::size_t last = (static_cast<std::size_t>(3) * 4 + 3) * 4;
	EXPECT_EQ(pixels[last], 255);
	EXPECT_EQ(pixels[last + 1], 255);
	//越界 / 空指针被拒绝
	EXPECT_FALSE(renderer.resource().texture_update_region(texture, 3, 3, 2, 2, red));
	EXPECT_FALSE(renderer.resource().texture_update_region(texture, 0, 0, 1, 1, nullptr));
	renderer.resource().texture_unload(texture);
}

/*
纹理区域读回（第二十八轮）：分块读回拼接应与整幅读回逐字节一致；越界整次拒绝
*/
TEST_F(Renderer_Texture_Test, 纹理区域读回与整幅一致)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        const unsigned char texels[16] = {
                255, 0, 0, 255,
                0, 255, 0, 255,
                0, 0, 255, 255,
                255, 255, 0, 255,
        };
        uint64_t texture = 0;
        ASSERT_TRUE(renderer.resource().texture_load(texture, texels, 2, 2));
        std::vector<uint8_t> whole(16);
        ASSERT_TRUE(renderer.resource().texture_readback(texture, whole.data(), 16, nullptr, nullptr));
        //四个 1×1 子块
        std::vector<uint8_t> stitched(16, 0);
        const uint32_t origins[4][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
        for (uint32_t index = 0; index < 4; ++index)
        {
                uint8_t block[4] = {};
                uint32_t width = 0;
                uint32_t height = 0;
                ASSERT_TRUE(renderer.resource().texture_readback_region(texture, origins[index][0], origins[index][1],
                        1, 1, block, 4, &width, &height));
                EXPECT_EQ(width, 1u);
                EXPECT_EQ(height, 1u);
                for (uint32_t byte = 0; byte < 4; ++byte)
                        stitched[index * 4 + byte] = block[byte];
        }
        EXPECT_EQ(whole, stitched) << "纹理分块读回与整幅不一致";
        //越界（x=1 时宽 2）整次拒绝
        uint8_t probe[16] = {};
        uint32_t width = 5;
        uint32_t height = 6;
        EXPECT_FALSE(renderer.resource().texture_readback_region(texture, 1, 0, 2, 1, probe, 8, &width, &height));
        EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);
        renderer.resource().texture_unload(texture);
}
