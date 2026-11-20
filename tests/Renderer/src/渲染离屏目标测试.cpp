/*
渲染离屏目标测试（L2 隐藏窗口 + 像素回读）
覆盖：渲染目标的创建 / 销毁；把场景渲染到离屏目标并回读目标内容；
      以目标作纹理采样做「离屏 → 采样合成」闭环；无效目标句柄回落交换链路径。
断言一律「格式无关」：绿判定只依赖绿通道（RGBA 与 BGRA 下恒为第 2 字节，下标 1），
      R / B 通道序随表面格式互换，故用「外通道取较大者」判定。
纯绿（0 与 1 两种端点）在 UNORM 与 sRGB 附件下编码一致，故判定不随附件传输函数变化。
*/
#include <gtest/gtest.h>
//Vulkan 格式常量（第 801 轮：色调编号附件用例；测试层经渲染层外部库目录取得头文件）
#include <vulkan/vulkan.h>


//获取渲染器门面（含混合档枚举）与网格数据
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"
#include "测试辅助/渲染测试辅助.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		return byjy_render_test::hidden_config(WINDOW_WIDTH, WINDOW_HEIGHT);
	}

	//本帧像素字节数（RGBA8 紧排）
	inline uint32_t pixel_bytes(void)
	{
		return byjy_render_test::pixel_bytes(WINDOW_WIDTH, WINDOW_HEIGHT);
	}

	//取指定像素的四个字节
	inline std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		return byjy_render_test::pixel_get(pixels, x, y, WINDOW_WIDTH);
	}

	//失败诊断文本
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//外通道较大者（R 与 B 通道；避开 std::max —— 本层包含链有 min/max 宏）
	uint8_t outer_max(const std::array<uint8_t, 4>& value)
	{
		return value[0] > value[2] ? value[0] : value[2];
	}

	//判断像素是否近似纯绿（绿通道高、外通道低）
	bool pixel_is_green(const std::array<uint8_t, 4>& value)
	{
		//绿通道（下标 1）须为高
		if (value[1] < 200)
			return false;
		//外通道须为低
		if (outer_max(value) > 60)
			return false;
		return true;
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	inline engine::Mesh_Data quad_data(void)
	{
		return byjy_render_test::quad_data();
	}

	//构造纯绿纹理（RGBA8：rgb = 绿，alpha = 255）
	std::vector<uint8_t> solid_green(uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 0;
			pixels[index + 1] = 255;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	/*
	构造「铺满全屏」模型矩阵（列主序）
	x / y 各放大 4 倍，使四边形在 x / y 上越过 NDC 边界被裁剪，恰好铺满整个渲染目标；
	放大倍数足够大，故铺满效果不依赖相机视场角的具体取值。
	*/
	std::array<float, 16> full_screen_matrix(void)
	{
		return {
			4.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 4.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	}
}

//渲染离屏目标测试夹具
class Renderer_Offscreen_Test : public ::testing::Test
{
};

//未初始化时创建渲染目标失败
TEST_F(Renderer_Offscreen_Test, 未初始化创建目标失败)
{
	engine::Renderer renderer;
	//未 init 直接创建
	uint64_t handle = 7;
	EXPECT_FALSE(renderer.resource().render_target_create(handle, 32, 32));
	//句柄须被复位为 0（无效）
	EXPECT_EQ(handle, 0u);
}

//渲染目标的创建与销毁幂等
TEST_F(Renderer_Offscreen_Test, 创建销毁目标幂等)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//首次创建
	uint64_t first = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(first, 32, 32));
	EXPECT_GE(first, 1u);
	//销毁后再销毁（幂等）不崩溃
	renderer.resource().render_target_destroy(first);
	renderer.resource().render_target_destroy(first);
	//槽可复用：再次创建成功
	uint64_t second = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(second, 32, 32));
	EXPECT_GE(second, 1u);
	renderer.resource().render_target_destroy(second);
}

//把场景渲染到离屏目标并回读目标内容
TEST_F(Renderer_Offscreen_Test, 渲染到目标并回读)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建与窗口同尺寸的渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));

	//本帧渲染到目标
	renderer.device().render_target_set(target);
	renderer.resource().texture_bind_set(texture);
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	//回读来源为目标颜色附件
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//目标内容应为纯绿（铺满全屏）——证明离屏渲染确实发生
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "离屏目标中心非纯绿：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2)))
		<< "离屏目标左上角非纯绿：" << pixel_text(pixel_get(pixels, 2, 2));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH - 3, WINDOW_HEIGHT - 3)))
		<< "离屏目标右下角非纯绿：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH - 3, WINDOW_HEIGHT - 3));

	//渲染到目标不改变交换链路径的可用性：切回交换链再渲染一帧应正常
	renderer.device().render_target_set(0);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	std::vector<uint8_t> swapchain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(swapchain_pixels.data(), static_cast<uint32_t>(swapchain_pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(swapchain_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "切回交换链路径后中心非纯绿";

	renderer.resource().mesh_unload(handle);
	renderer.resource().texture_unload(texture);
	renderer.resource().render_target_destroy(target);
}

//以目标作纹理采样：完成「离屏渲染 → 采样合成」闭环
TEST_F(Renderer_Offscreen_Test, 目标可作纹理采样闭环)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();

	//第一帧：把纯绿场景渲染进离屏目标（不呈现、不回读）
	renderer.device().render_target_set(target);
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	/*
	第二帧：切回交换链并把目标颜色绑定为当前纹理
	先显式绑定内置默认白纹理，使「采样失败」会呈现为白（而非恰好同为绿），
	故本用例能真正区分「采样到目标」与「回落默认纹理」。
	*/
	renderer.device().render_target_set(0);
	renderer.resource().texture_bind_set(0);
	renderer.resource().texture_bind_target(target);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//交换链内容应为目标的绿（若采样失败则回落为默认白，绿通道判定会失败）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_green(center)) << "采样合成后中心非目标绿（疑似回落默认纹理）：" << pixel_text(center);
	//角落同样为目标的绿（全屏铺满）
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2))) << "采样合成后角落非目标绿";

	renderer.resource().mesh_unload(handle);
	renderer.resource().texture_unload(texture);
	renderer.resource().render_target_destroy(target);
}

//无效渲染目标句柄回落交换链路径
TEST_F(Renderer_Offscreen_Test, 无效目标句柄回落交换链)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//设定一个从未创建的渲染目标句柄
	renderer.device().render_target_set(9999u);
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	//应回落交换链路径：回读到交换链的绿
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "无效目标句柄未回落交换链路径";

	renderer.resource().mesh_unload(handle);
	renderer.resource().texture_unload(texture);
}

/*
离屏目标读回（第二十六轮）：render_target_readback 直接从目标颜色图像取像素
口径：先把绿场景渲染进目标（frame_render 不回读），再读回目标内容；
      应得到与目标尺寸一致的绿；连续两次读回结果一致（读回不破坏内容与布局）。
*/
TEST_F(Renderer_Offscreen_Test, 读回离屏目标内容)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//创建与窗口同尺寸的目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//绿纹理与全屏四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	renderer.device().render_target_set(target);
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	//读回目标内容
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback(target, pixels.data(),
		static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_green(center)) << "目标读回中心非纯绿：" << pixel_text(center);
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2))) << "目标读回角落非纯绿";

	//第二次读回：内容不因读回而改变
	std::vector<uint8_t> again(pixel_bytes());
	ASSERT_TRUE(renderer.resource().render_target_readback(target, again.data(), static_cast<uint32_t>(again.size()), nullptr, nullptr));
	EXPECT_EQ(pixels, again);

	renderer.resource().mesh_unload(handle);
	renderer.resource().texture_unload(texture);
	renderer.resource().render_target_destroy(target);
}

/*
离屏目标子区域读回（第二十七轮）：分块读回拼接应与整幅读回逐字节一致
口径：绿场景 → 渲染进 64×36 目标 → 整幅读回一次；再按「左上 / 右上 / 左下 / 右下」四块读回并拼接；
      逐字节比较两者；越界区域应整次拒绝（返回假、尺寸出参保持 0）。
*/
TEST_F(Renderer_Offscreen_Test, 子区域读回与整幅一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	renderer.device().render_target_set(target);
	renderer.resource().texture_bind_set(texture);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	//整幅读回（基准）
	std::vector<uint8_t> whole(pixel_bytes());
	ASSERT_TRUE(renderer.resource().render_target_readback(target, whole.data(), static_cast<uint32_t>(whole.size()), nullptr, nullptr));

	//四块读回并拼接
	const uint32_t half_width = WINDOW_WIDTH / 2;
	const uint32_t half_height = WINDOW_HEIGHT / 2;
	std::vector<uint8_t> stitched(pixel_bytes(), 0);
	const uint32_t origins[4][2] = { { 0, 0 }, { half_width, 0 }, { 0, half_height }, { half_width, half_height } };
	for (const auto& origin : origins)
	{
		std::vector<uint8_t> block(static_cast<std::size_t>(half_width) * half_height * 4);
		uint32_t block_width = 0;
		uint32_t block_height = 0;
		ASSERT_TRUE(renderer.resource().render_target_readback_region(target, origin[0], origin[1],
			half_width, half_height, block.data(), static_cast<uint32_t>(block.size()),
			&block_width, &block_height));
		EXPECT_EQ(block_width, half_width);
		EXPECT_EQ(block_height, half_height);
		//逐行拷入拼接缓冲
		for (uint32_t row = 0; row < half_height; ++row)
		{
			std::size_t dst = (static_cast<std::size_t>(origin[1] + row) * WINDOW_WIDTH + origin[0]) * 4;
			std::size_t src = static_cast<std::size_t>(row) * half_width * 4;
			for (uint32_t byte = 0; byte < half_width * 4; ++byte)
				stitched[dst + byte] = block[src + byte];
		}
	}
	EXPECT_EQ(whole, stitched) << "分块读回拼接与整幅读回不一致";

	//越界区域整次拒绝
	std::vector<uint8_t> probe(static_cast<std::size_t>(half_width) * half_height * 4);
	uint32_t width = 77;
	uint32_t height = 88;
	EXPECT_FALSE(renderer.resource().render_target_readback_region(target, half_width + 1, 0,
		half_width, half_height, probe.data(), static_cast<uint32_t>(probe.size()), &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);

	renderer.resource().mesh_unload(handle);
	renderer.resource().texture_unload(texture);
	renderer.resource().render_target_destroy(target);
}

/*
离屏目标读回的参数校验（第二十六轮）：未初始化 / 句柄无效 / 容量不足均返回假
*/
TEST_F(Renderer_Offscreen_Test, 读回参数校验)
{
	engine::Renderer renderer;
	std::vector<uint8_t> pixels(64 * 36 * 4);
	//未初始化
	EXPECT_FALSE(renderer.resource().render_target_readback(1, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	ASSERT_TRUE(renderer.init(hidden_config()));
	//句柄无效（从未创建）
	EXPECT_FALSE(renderer.resource().render_target_readback(9999u, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	//容量不足
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	uint32_t width = 123;
	uint32_t height = 456;
	EXPECT_FALSE(renderer.resource().render_target_readback(target, pixels.data(), 16, &width, &height));
	//失败时尺寸出参保持 0
	EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);
        renderer.resource().render_target_destroy(target);
}

/*
深度读回（第二十八轮）：全屏四边形覆盖处深度 < 1，清屏背景深度 ≈ 1
口径：绿场景渲染进目标后读深度原始值（D32_SFLOAT，按 float 解释）；
      中心被四边形覆盖，应落在 (0, 1) 内且明显小于 1；角落无几何，应为清屏深度 1.0。
*/
TEST_F(Renderer_Offscreen_Test, 深度读回区分几何与清屏)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        uint64_t target = 0;
        ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
        uint64_t texture = 0;
        std::vector<uint8_t> texels = solid_green(2, 2);
        ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
        engine::Mesh_Handle handle = 0;
        ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
        //用单位矩阵（四边形不放大）：中心被覆盖、四角仍为清屏背景
        renderer.device().render_target_set(target);
        renderer.resource().texture_bind_set(texture);
        ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
        ASSERT_TRUE(renderer.frame_render());

        //读回深度（宽×高 个 32 位值）
        std::vector<uint32_t> depth(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
        uint32_t width = 0;
        uint32_t height = 0;
        ASSERT_TRUE(renderer.resource().render_target_readback_depth(target, depth.data(),
                static_cast<uint32_t>(depth.size()), &width, &height));
        EXPECT_EQ(width, WINDOW_WIDTH);
        EXPECT_EQ(height, WINDOW_HEIGHT);
        //中心与角落的深度（按 float 解释）
        float center_depth = 0.0f;
        float corner_depth = 0.0f;
        std::size_t center_index = static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH + WINDOW_WIDTH / 2;
        std::memcpy(&center_depth, &depth[center_index], 4);
        std::memcpy(&corner_depth, &depth[2 * WINDOW_WIDTH + 2], 4);
        RecordProperty("depth_center", center_depth);
        RecordProperty("depth_corner", corner_depth);
        EXPECT_GT(center_depth, 0.0f);
        EXPECT_LT(center_depth, 0.999f) << "四边形覆盖处深度未小于 1";
        EXPECT_GT(corner_depth, 0.999f) << "清屏区域深度应接近 1";

        //容量不足被拒（元素个数不足）
        std::vector<uint32_t> tiny(16);
        uint32_t small_width = 9;
        uint32_t small_height = 9;
        EXPECT_FALSE(renderer.resource().render_target_readback_depth(target, tiny.data(),
                static_cast<uint32_t>(tiny.size()), &small_width, &small_height));
        EXPECT_EQ(small_width, 0u);
        EXPECT_EQ(small_height, 0u);

        renderer.resource().mesh_unload(handle);
        renderer.resource().texture_unload(texture);
        renderer.resource().render_target_destroy(target);
}

/*
渲染目标尺寸查询（第七十七轮）
口径：创建任意尺寸的目标后可直接查回实际尺寸；未初始化 / 无效句柄 / 已销毁句柄返回假且出参归零。
*/
TEST_F(Renderer_Offscreen_Test, 目标尺寸可查询)
{
        engine::Renderer renderer;
        //未初始化：返回假且出参归零
        uint32_t width = 7;
        uint32_t height = 7;
        EXPECT_FALSE(renderer.resource().render_target_size(1, &width, &height));
        EXPECT_EQ(width, 0u);
        EXPECT_EQ(height, 0u);

        ASSERT_TRUE(renderer.init(hidden_config()));
        //任意尺寸（与交换链不同）也应原样查回
        uint64_t target = 0;
        uint32_t target_width = WINDOW_WIDTH / 2;
        uint32_t target_height = WINDOW_HEIGHT / 2;
        ASSERT_TRUE(renderer.resource().render_target_create(target, target_width, target_height));
        uint32_t queried_width = 0;
        uint32_t queried_height = 0;
        EXPECT_TRUE(renderer.resource().render_target_size(target, &queried_width, &queried_height));
        EXPECT_EQ(queried_width, target_width);
        EXPECT_EQ(queried_height, target_height);

        //只取宽（高指针为空）亦有效
        uint32_t only_width = 0;
        EXPECT_TRUE(renderer.resource().render_target_size(target, &only_width, nullptr));
        EXPECT_EQ(only_width, target_width);

        //无效句柄被拒且出参归零
        uint32_t invalid_width = 5;
        uint32_t invalid_height = 5;
        EXPECT_FALSE(renderer.resource().render_target_size(0, &invalid_width, &invalid_height));
        EXPECT_EQ(invalid_width, 0u);
        EXPECT_EQ(invalid_height, 0u);
        EXPECT_FALSE(renderer.resource().render_target_size(target + 99, &invalid_width, &invalid_height));

        //销毁后同样失效
        renderer.resource().render_target_destroy(target);
        EXPECT_FALSE(renderer.resource().render_target_size(target, &invalid_width, &invalid_height));
        EXPECT_EQ(invalid_width, 0u);
}

/*
深度子区域读回（第一百六十三轮）
口径：全屏四边形渲染到离屏目标后，取中心 8×4 区域读回：
      ① 区域尺寸正确；② 与整幅读回对应位置逐元素一致；③ 区域外像素不参与；
      ④ 越界区域 / 容量不足 / 无效句柄均被拒绝且尺寸归零。
*/
TEST_F(Renderer_Offscreen_Test, 深度子区域读回)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
	//渲染一帧（四边形覆盖中心）
	renderer.device().render_target_set(target);
	renderer.resource().texture_bind_set(0);
	ASSERT_TRUE(renderer.resource().mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render());

	//整幅读回作为基准
	std::vector<uint32_t> whole(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT, 0);
	uint32_t whole_width = 0;
	uint32_t whole_height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_depth(target, whole.data(),
		static_cast<uint32_t>(whole.size()), &whole_width, &whole_height));

	//中心 8×4 区域
	const uint32_t region_x = WINDOW_WIDTH / 2 - 4;
	const uint32_t region_y = WINDOW_HEIGHT / 2 - 2;
	const uint32_t region_w = 8;
	const uint32_t region_h = 4;
	std::vector<uint32_t> region(region_w * region_h, 0);
	uint32_t region_width = 0;
	uint32_t region_height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_depth_region(target, region_x, region_y,
		region_w, region_h, region.data(), static_cast<uint32_t>(region.size()), &region_width, &region_height));
	//① 区域尺寸
	EXPECT_EQ(region_width, region_w);
	EXPECT_EQ(region_height, region_h);
	//② 与整幅对应位置逐元素一致
	uint32_t mismatches = 0;
	for (uint32_t row = 0; row < region_h; ++row)
		for (uint32_t column = 0; column < region_w; ++column)
		{
			std::size_t whole_index = static_cast<std::size_t>(region_y + row) * WINDOW_WIDTH
				+ region_x + column;
			if (whole[whole_index] != region[static_cast<std::size_t>(row) * region_w + column])
				++mismatches;
		}
	EXPECT_EQ(mismatches, 0u) << "子区域与整幅读回不一致";

	//④ 越界 / 容量不足 / 无效句柄
	uint32_t width = 9;
	uint32_t height = 9;
	EXPECT_FALSE(renderer.resource().render_target_readback_depth_region(target, WINDOW_WIDTH - 2, 0,
		region_w, region_h, region.data(), static_cast<uint32_t>(region.size()), &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
	//容量不足（需要 32 个元素，仅给 16）
	std::vector<uint32_t> tiny(16, 0);
	EXPECT_FALSE(renderer.resource().render_target_readback_depth_region(target, 0, 0,
		region_w, region_h, tiny.data(), static_cast<uint32_t>(tiny.size()), nullptr, nullptr));
	EXPECT_FALSE(renderer.resource().render_target_readback_depth_region(9999u, 0, 0,
		region_w, region_h, region.data(), static_cast<uint32_t>(region.size()), nullptr, nullptr));

	renderer.resource().mesh_unload(handle);
	renderer.resource().render_target_destroy(target);
}

/*
色调编号附件 · 资源层（第 801 轮 · 8 位路径 MRT 段 1）
口径：单采样目标可带一个 R8_UINT 编号附件，句柄可查询、销毁后不再可用；
      多重采样目标不创建该附件（告警后按未启用处理），目标本身仍创建成功。
      本段只验证资源，不接入渲染路径，故既有画面逐字节不变。
*/
TEST_F(Renderer_Offscreen_Test, 色调编号附件资源层)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//单采样 ＋ R8_UINT 编号附件
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	ASSERT_NE(target, 0u);
	uint64_t id_image = 0;
	uint64_t id_view = 0;
	uint32_t id_format = 0;
	ASSERT_TRUE(renderer.resource().render_target_tone_id_get(target, id_image, id_view, id_format));
	EXPECT_NE(id_image, 0u);
	EXPECT_NE(id_view, 0u);
	EXPECT_EQ(id_format, static_cast<uint32_t>(VK_FORMAT_R8_UINT));
	//销毁后查询应失败
	renderer.resource().render_target_destroy(target);
	EXPECT_FALSE(renderer.resource().render_target_tone_id_get(target, id_image, id_view, id_format));
	//多重采样目标：单采样编号图（解析目标）＋ 多重采样编号图（第 821 轮）
	uint64_t msaa_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(msaa_target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 4));
	ASSERT_NE(msaa_target, 0u);
	ASSERT_TRUE(renderer.resource().render_target_tone_id_get(msaa_target, id_image, id_view, id_format));
	EXPECT_EQ(id_format, static_cast<uint32_t>(VK_FORMAT_R8_UINT));
	uint64_t msaa_id_image = 0;
	uint64_t msaa_id_view = 0;
	ASSERT_TRUE(renderer.resource().render_target_tone_id_msaa_get(msaa_target, msaa_id_image, msaa_id_view));
	EXPECT_NE(msaa_id_image, 0u);
	EXPECT_NE(msaa_id_view, 0u);
	//销毁后两者均不可用
	renderer.resource().render_target_destroy(msaa_target);
	EXPECT_FALSE(renderer.resource().render_target_tone_id_get(msaa_target, id_image, id_view, id_format));
	EXPECT_FALSE(renderer.resource().render_target_tone_id_msaa_get(msaa_target, msaa_id_image, msaa_id_view));
	//未指定格式时与既有接口一致（无编号附件）
	uint64_t plain_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format(plain_target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 1));
	ASSERT_NE(plain_target, 0u);
	EXPECT_FALSE(renderer.resource().render_target_tone_id_get(plain_target, id_image, id_view, id_format));
	EXPECT_FALSE(renderer.resource().render_target_tone_id_msaa_get(plain_target, msaa_id_image, msaa_id_view));
	renderer.resource().render_target_destroy(plain_target);
}

/*
色调编号附件 · 不改变既有渲染 / 回读（第 802 轮）
口径：段 1 只提供资源，未接入管线与帧链路，故「带编号附件」与「不带」的目标在同一场景下回读应逐字节一致。
*/
TEST_F(Renderer_Offscreen_Test, 色调编号附件不影响既有渲染)
{
	//按同一场景渲染一帧并回读目标内容
	auto render_frame = [](bool with_tone_id)
	{
		std::vector<uint8_t> pixels;
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		if (!renderer.compute().readback_supported())
			return pixels;
		uint64_t target = 0;
		const uint32_t color_format = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);
		const uint32_t depth_format = static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT);
		if (with_tone_id)
			EXPECT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
				color_format, depth_format, static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
		else
			EXPECT_TRUE(renderer.resource().render_target_create_format(target, WINDOW_WIDTH, WINDOW_HEIGHT,
				color_format, depth_format, 1));
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_green(2, 2);
		EXPECT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		renderer.device().render_target_set(target);
		renderer.resource().texture_bind_set(texture);
		std::array<float, 16> full = full_screen_matrix();
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
		pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.resource().mesh_unload(handle);
		renderer.resource().texture_unload(texture);
		renderer.resource().render_target_destroy(target);
		return pixels;
	};
	std::vector<uint8_t> reference = render_frame(false);
	if (reference.empty())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> chained = render_frame(true);
	ASSERT_EQ(reference.size(), chained.size());
	EXPECT_EQ(reference, chained) << "编号附件改变了既有渲染 / 回读结果";
	//顺带确认对照帧确实是纯绿（渲染确实发生）
	EXPECT_TRUE(pixel_is_green(pixel_get(chained, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "带编号附件目标中心非纯绿：" << pixel_text(pixel_get(chained, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
}

/*
管线第二颜色附件登记（第 804 轮 · 8 位 MRT 段 2）
口径：`pipeline_load_ex` 可声明第二颜色附件格式（R8_UNORM），登记成功且可卸载；
      未声明时行为与既有管线一致（默认 0，既有用例覆盖）。
*/
TEST_F(Renderer_Offscreen_Test, 管线第二颜色附件登记)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t handle = 0;
	ASSERT_TRUE(renderer.resource().pipeline_load_ex(handle, "后处理.vert.spv", "反色.frag.spv",
		engine::Blend_Mode::Opaque, false, false, true, false, true, false, 0, 0, 0,
		static_cast<uint32_t>(VK_FORMAT_R8_UNORM)));
	ASSERT_NE(handle, 0u);
	EXPECT_TRUE(renderer.resource().pipeline_valid(handle));
	renderer.resource().pipeline_unload(handle);
	EXPECT_FALSE(renderer.resource().pipeline_valid(handle));
}

/*
逐材质色调编号预通道（第 808 轮 · 8 位路径方案 B）
口径：开启 tone_id_pass 后，帧链路在场景 pass 之后以同一深度图重绘不透明项，
      只有最终可见片元写入编号（算子 + 1，0 = 无覆盖）；
      被遮挡物体不得改写近处编号，可见远处区域仍保留自身编号。
*/
TEST_F(Renderer_Offscreen_Test, 逐材质色调编号预通道)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	//单采样目标 ＋ R8_UINT 编号附件
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//两种材质：AgX（4）与 ACES（0）
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Data agx_data = base;
	agx_data.tone_operator = 4;
	engine::Material_Handle agx_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(agx_handle, agx_data));
	engine::Material_Data aces_data = base;
	aces_data.tone_operator = 0;
	engine::Material_Handle aces_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(aces_handle, aces_data));
	//开启编号通道
	renderer.device().tone_id_pass_set(true);
	ASSERT_TRUE(renderer.device().tone_id_pass());
	//构造缩放 ＋ z 平移的模型矩阵（列主序；默认相机在 +Z 3 单位处）
	auto make_matrix = [](float scale, float z)
	{
		return std::array<float, 16>{
			scale, 0.0f, 0.0f, 0.0f,
			0.0f, scale, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, z, 1.0f,
		};
	};
	//按给定绘制顺序渲染一帧并读回编号附件
	auto tone_ids_by = [&](engine::Material_Handle first_material, float first_scale, float first_z,
		engine::Material_Handle second_material, float second_scale, float second_z)
	{
		renderer.resource().material_bind_set(first_material);
		std::array<float, 16> first_matrix = make_matrix(first_scale, first_z);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, first_matrix.data()));
		renderer.resource().material_bind_set(second_material);
		std::array<float, 16> second_matrix = make_matrix(second_scale, second_z);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, second_matrix.data()));
		renderer.device().render_target_set(target);
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		std::vector<uint8_t> ids(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
		uint32_t width = 0;
		uint32_t height = 0;
		EXPECT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
			static_cast<uint32_t>(ids.size()), &width, &height));
		EXPECT_EQ(width, WINDOW_WIDTH);
		EXPECT_EQ(height, WINDOW_HEIGHT);
		return ids;
	};
	//编号采样（逐像素单字节）
	auto sample = [](const std::vector<uint8_t>& ids, uint32_t x, uint32_t y)
	{
		return ids[static_cast<std::size_t>(y) * WINDOW_WIDTH + x];
	};
	//远平面（AgX，铺满屏幕）先画、近平面（ACES，覆盖中央）后画：中心应为近处编号
	std::vector<uint8_t> near_last = tone_ids_by(agx_handle, 4.0f, -1.0f, aces_handle, 0.6f, 1.0f);
	EXPECT_EQ(sample(near_last, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), 1u) << "近处物体未写入 ACES 编号";
	EXPECT_EQ(sample(near_last, 2, 2), 5u) << "远处可见区域未写入 AgX 编号";
	//绘制顺序反转：远处 AgX 不得覆盖近处 ACES（深度 LESS_OR_EQUAL 只标记可见片元）
	std::vector<uint8_t> far_last = tone_ids_by(aces_handle, 0.6f, 1.0f, agx_handle, 4.0f, -1.0f);
	EXPECT_EQ(sample(far_last, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), 1u) << "被遮挡物体改写了近处编号";
	EXPECT_EQ(sample(far_last, 2, 2), 5u) << "远处可见区域编号缺失";
	//清理
	renderer.device().render_target_set(0);
	renderer.resource().material_unload(aces_handle);
	renderer.resource().material_unload(agx_handle);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
}

/*
编号通道 · 目标无编号附件时零影响（第 809 轮）
口径：`tone_id_pass` 开启但目标未建立编号附件（或编号管线未登记）时，帧链路不得产生额外命令，
      画面与通道关闭时逐字节一致 —— 通道的启用与否不能隐式改变既有渲染结果。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道无附件时零影响)
{
	//按指定开关渲染一帧并回读目标内容（目标不带编号附件）
	auto render_frame = [](bool tone_pass)
	{
		std::vector<uint8_t> pixels;
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		if (!renderer.compute().readback_supported())
			return pixels;
		//普通目标（无编号附件）
		uint64_t target = 0;
		EXPECT_TRUE(renderer.resource().render_target_create_format(target, WINDOW_WIDTH, WINDOW_HEIGHT,
			static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 1));
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_green(2, 2);
		EXPECT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.resource().mesh_load(handle, quad_data()));
		//按开关切换编号通道（目标无编号附件 → 应在录制期跳过）
		renderer.device().tone_id_pass_set(tone_pass);
		renderer.device().render_target_set(target);
		renderer.resource().texture_bind_set(texture);
		std::array<float, 16> full = full_screen_matrix();
		EXPECT_TRUE(renderer.resource().mesh_draw(handle, full.data()));
		pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.resource().mesh_unload(handle);
		renderer.resource().texture_unload(texture);
		renderer.resource().render_target_destroy(target);
		return pixels;
	};
	std::vector<uint8_t> reference = render_frame(false);
	if (reference.empty())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> enabled = render_frame(true);
	ASSERT_EQ(reference.size(), enabled.size());
	EXPECT_EQ(reference, enabled) << "目标无编号附件时开关改变了既有渲染结果";
}

/*
逐材质色调编号 · 中间算子编码（第 811 轮）
口径：编号编码覆盖 1..5（算子 0..4）；本用例取靠近区间中段的「胶片（1）/ 指数（2）」两种材质，
      验证近处 / 远处分别写入 2 / 3，排除只对 0 与 4 成立的巧合。
*/
TEST_F(Renderer_Offscreen_Test, 逐材质色调编号中间算子)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//两种中间算子材质：胶片（1，近处）与指数（2，远处）
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Data film_data = base;
	film_data.tone_operator = 1;
	engine::Material_Handle film_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(film_handle, film_data));
	engine::Material_Data exponential_data = base;
	exponential_data.tone_operator = 2;
	engine::Material_Handle exponential_handle = 0;
	ASSERT_TRUE(renderer.resource().material_load(exponential_handle, exponential_data));
	renderer.device().tone_id_pass_set(true);
	//远处铺满屏幕、近处覆盖中央（与第 808 轮同一几何口径，仅替换算子与绘制顺序）
	const auto scale_matrix = [](float scale, float z)
	{
		return std::array<float, 16>{
			scale, 0.0f, 0.0f, 0.0f,
			0.0f, scale, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, z, 1.0f,
		};
	};
	renderer.resource().material_bind_set(exponential_handle);
	std::array<float, 16> far_matrix = scale_matrix(4.0f, -1.0f);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, far_matrix.data()));
	renderer.resource().material_bind_set(film_handle);
	std::array<float, 16> near_matrix = scale_matrix(0.6f, 1.0f);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, near_matrix.data()));
	renderer.device().render_target_set(target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	std::vector<uint8_t> ids(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
		static_cast<uint32_t>(ids.size()), &width, &height));
	//中心 = 近处胶片（1 + 1），角落 = 远处指数（2 + 1）
	EXPECT_EQ(ids[static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH + WINDOW_WIDTH / 2], 2u);
	EXPECT_EQ(ids[static_cast<std::size_t>(2) * WINDOW_WIDTH + 2], 3u);
	//清理
	renderer.device().render_target_set(0);
	renderer.resource().material_unload(exponential_handle);
	renderer.resource().material_unload(film_handle);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
}

/*
编号通道 · 开销基线（第 813 轮）
口径：同一场景在中性目标上开 / 关编号通道各跑 20 帧，记录平均帧耗时；
      断言两态画面逐字节一致（通道只写编号附件，不改颜色），耗时差异仅作基线记录。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道开销基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	base.tone_operator = 4;
	engine::Material_Handle material = 0;
	ASSERT_TRUE(renderer.resource().material_load(material, base));
	renderer.resource().material_bind_set(material);
	renderer.device().render_target_set(target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//按指定状态跑 20 帧并返回平均帧耗时（毫秒）与末帧像素
	auto run_frames = [&](bool enabled, std::vector<uint8_t>& last_pixels)
	{
		renderer.device().tone_id_pass_set(enabled);
		double total_ms = 0.0;
		const uint32_t iterations = 20;
		for (uint32_t frame = 0; frame < iterations; ++frame)
		{
			EXPECT_TRUE(renderer.resource().mesh_draw(quad, nullptr));
			EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
			total_ms += renderer.frame_stats_get().frame_ms;
		}
		last_pixels = pixels;
		return total_ms / iterations;
	};
	std::vector<uint8_t> off_pixels;
	std::vector<uint8_t> on_pixels;
	//先跑关闭态（含预热），再跑开启态
	double off_ms = run_frames(false, off_pixels);
	double on_ms = run_frames(true, on_pixels);
	RecordProperty("id_pass_off_ms", off_ms);
	RecordProperty("id_pass_on_ms", on_ms);
	EXPECT_EQ(off_pixels, on_pixels) << "编号通道改变了中性目标的颜色输出";
	EXPECT_GT(off_ms, 0.0);
	EXPECT_GT(on_ms, 0.0);
	//非常宽松的上界：仅防「数量级异常」，不用于性能门禁
	EXPECT_LT(on_ms, off_ms * 4.0 + 10.0) << "编号通道开销异常（" << off_ms << " → " << on_ms << " ms）";
	//清理
	renderer.device().render_target_set(0);
	renderer.resource().material_unload(material);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
}

/*
编号通道 · 按纹素应用 alpha 裁剪（第 819 轮）
口径：编号片元与主 pass 同式采样反照率并按阈值 discard；
      不透明纹素照常写编号，透明纹素（alpha 低于阈值）不写编号。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道按纹素应用alpha裁剪)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//两张反照率贴图：不透明（alpha 255）与全透明（alpha 0）
	uint64_t opaque_texture = 0;
	std::vector<uint8_t> opaque_texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(opaque_texture, opaque_texels.data(), 2, 2));
	std::vector<uint8_t> clear_texels(2 * 2 * 4, 0);
	for (std::size_t index = 0; index < clear_texels.size(); index += 4)
	{
		clear_texels[index + 1] = 255;
		clear_texels[index + 3] = 0;
	}
	uint64_t clear_texture = 0;
	ASSERT_TRUE(renderer.resource().texture_load(clear_texture, clear_texels.data(), 2, 2));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	renderer.device().tone_id_pass_set(true);
	renderer.device().render_target_set(target);
	std::vector<uint8_t> ids(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
	uint32_t width = 0;
	uint32_t height = 0;
	//按给定材质渲染一帧并读回编号中心值
	auto center_id_by = [&](engine::Material_Data data)
	{
		engine::Material_Handle material = 0;
		EXPECT_TRUE(renderer.resource().material_load(material, data));
		renderer.resource().material_bind_set(material);
		renderer.resource().texture_bind_set(data.texture);
		std::array<float, 16> full = full_screen_matrix();
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, full.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		std::fill(ids.begin(), ids.end(), 255);
		EXPECT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
			static_cast<uint32_t>(ids.size()), &width, &height));
		renderer.resource().material_unload(material);
		return ids[static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH + WINDOW_WIDTH / 2];
	};
	//基础材质（AgX 算子 4）
	engine::Material_Data base;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	base.tone_operator = 4;
	//不透明贴图 ＋ 裁剪：不触发 discard，编号 5
	engine::Material_Data opaque_clipped = base;
	opaque_clipped.texture = opaque_texture;
	opaque_clipped.alpha_cutoff = 0.5f;
	EXPECT_EQ(center_id_by(opaque_clipped), 5u) << "不透明纹素不应被裁剪";
	//全透明贴图 ＋ 裁剪：全像素 discard，编号 0
	engine::Material_Data clear_clipped = base;
	clear_clipped.texture = clear_texture;
	clear_clipped.alpha_cutoff = 0.5f;
	EXPECT_EQ(center_id_by(clear_clipped), 0u) << "透明纹素应被编号通道裁剪";
	//全透明贴图 ＋ 不裁剪：照常写编号（关闭阈值时零影响）
	engine::Material_Data clear_plain = base;
	clear_plain.texture = clear_texture;
	EXPECT_EQ(center_id_by(clear_plain), 5u) << "未声明裁剪时编号不应丢失";
	//清理
	renderer.device().render_target_set(0);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(clear_texture);
	renderer.resource().texture_unload(opaque_texture);
}

/*
编号通道 · 静态合批下的逐实例编号（第 820 轮）
口径：同材质多物体被静态合批为一个绘制项（多实例）后，编号预通道仍按实例属性逐物体写编号。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道与静态合批)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	engine::Material_Data data;
	data.texture = texture;
	data.pbr_override = true;
	data.pbr_roughness = 0.4f;
	data.pbr_metallic = 0.0f;
	data.tone_operator = 4;
	engine::Material_Handle material = 0;
	ASSERT_TRUE(renderer.resource().material_load(material, data));
	renderer.resource().material_bind_set(material);
	//开启静态合批与编号通道
	renderer.device().static_batching_set(true);
	renderer.device().tone_id_pass_set(true);
	renderer.device().render_target_set(target);
	//左右两枚小四边形（同材质 → 应合批为一个多实例绘制项）
	auto place_matrix = [](float x)
	{
		return std::array<float, 16>{
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, 0.0f, 0.0f, 1.0f,
		};
	};
	std::array<float, 16> left = place_matrix(-1.0f);
	std::array<float, 16> right = place_matrix(1.0f);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, left.data()));
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, right.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//合批：1 个绘制项 / 2 个实例
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.last_draw_items, 1u) << "同材质两物体未合批";
	EXPECT_EQ(stats.last_instances, 2u);
	//编号读回：左右测点均为 5
	std::vector<uint8_t> ids(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
		static_cast<uint32_t>(ids.size()), &width, &height));
	const std::size_t row = static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH;
	EXPECT_EQ(ids[row + WINDOW_WIDTH * 3 / 10], 5u) << "左侧实例编号缺失";
	EXPECT_EQ(ids[row + WINDOW_WIDTH * 7 / 10], 5u) << "右侧实例编号缺失";
	//清理
	renderer.device().render_target_set(0);
	renderer.device().static_batching_set(false);
	renderer.resource().material_unload(material);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
}
