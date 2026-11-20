/*
渲染离屏编号测试（2026-11-21 自「渲染离屏编号通道测试.cpp」拆出）
覆盖：R8_UINT 编号附件资源层、不影响既有渲染、管线第二颜色附件登记、逐材质色调编号预通道、
      无附件零影响、中间算子、编号通道开销基线。
*/
#include <gtest/gtest.h>
//Vulkan 格式常量（编号附件与目标格式查询）
#include <vulkan/vulkan.h>

#include "渲染离屏目标测试辅助.h"


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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 1));
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
		const uint32_t color_format = renderer.device().scene_color_format();
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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
			renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 1));
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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
