/*
渲染离屏目标测试 · 编号通道（2026-11-20 自「渲染离屏目标测试.cpp」拆出）
覆盖：R8_UINT 编号附件资源层、逐材质色调编号预通道（单采样 / 多重采样 / 静态合批 / 纹素裁剪）、
      场景管线颜色格式与目标原生格式等格式契约查询。
公共前提（窗口 / 像素 / 纹理 / 几何 / 矩阵 / 夹具）见「渲染离屏目标测试辅助.h」。
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
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

/*
编号通道 · 多重采样目标（第 822 轮）
口径：多重采样目标的编号先写 msaa 编号图，按样本 0 解析到单采样编号图；
      读回单采样编号图应得到与单采样路径一致的逐材质编号。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道多重采样目标)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//切到 4× MSAA（重建交换链与全部管线；不支持则跳过）
	renderer.device().msaa_samples_set(4);
	if (renderer.device().msaa_samples() != 4)
		GTEST_SKIP() << "本机不支持 4× MSAA";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	//目标跟随默认采样数（4）＋ 编号附件
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 0));
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
	renderer.device().tone_id_pass_set(true);
	renderer.device().render_target_set(target);
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, full.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//编号读回（解析后的单采样图）：中心 5
	std::vector<uint8_t> ids(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
		static_cast<uint32_t>(ids.size()), &width, &height));
	EXPECT_EQ(ids[static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH + WINDOW_WIDTH / 2], 5u)
		<< "多重采样目标编号解析失败";
	//清理（先销毁目标再恢复采样数，避免目标随采样数重建）
	renderer.device().render_target_set(0);
	renderer.resource().material_unload(material);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
	renderer.device().msaa_samples_set(1);
}

/*
编号通道 · 多重采样解析边界（第 823 轮）
口径：4× 目标的编号按「样本 0」解析，几何边缘落在像素内部时读回值仍是某一侧的完整编号，
      不得出现两种编号的插值；遮挡关系仍由深度决定（近处算子胜出），且远景全屏覆盖下无空洞。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道多重采样解析边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//切到 4× MSAA（不支持则跳过）
	renderer.device().msaa_samples_set(4);
	if (renderer.device().msaa_samples() != 4)
		GTEST_SKIP() << "本机不支持 4× MSAA";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	//目标跟随默认采样数（4）＋ 编号附件
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 0));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//远景 AgX（算子 4 → 编号 5）、近景 ACES（算子 0 → 编号 1）
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Data far_data = base;
	far_data.tone_operator = 4;
	engine::Material_Handle far_material = 0;
	ASSERT_TRUE(renderer.resource().material_load(far_material, far_data));
	engine::Material_Data near_data = base;
	near_data.tone_operator = 0;
	engine::Material_Handle near_material = 0;
	ASSERT_TRUE(renderer.resource().material_load(near_material, near_data));
	renderer.device().tone_id_pass_set(true);
	renderer.device().render_target_set(target);
	//构造缩放 ＋ 平移矩阵（列主序；默认相机在 +Z 3 单位处，z 越大越近）
	auto make_matrix = [](float scale, float x, float z)
	{
		return std::array<float, 16>{
			scale, 0.0f, 0.0f, 0.0f,
			0.0f, scale, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, 0.0f, z, 1.0f,
		};
	};
	//远景铺满全屏（尺度 6 → 越过视口边界，避免边缘像素的样本覆盖歧义；z = −1）先画
	renderer.resource().material_bind_set(far_material);
	std::array<float, 16> far_matrix = make_matrix(6.0f, 0.0f, -1.0f);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, far_matrix.data()));
	//近景半幅 ＋ 非整像素偏移（尺度 0.5、x = 0.137、z = 1）→ 左右边缘均落在像素内部
	renderer.resource().material_bind_set(near_material);
	std::array<float, 16> near_matrix = make_matrix(0.5f, 0.137f, 1.0f);
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, near_matrix.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//读回解析后的编号图
	std::vector<uint8_t> ids(static_cast<std::size_t>(WINDOW_WIDTH) * WINDOW_HEIGHT);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.resource().render_target_readback_tone_id(target, ids.data(),
		static_cast<uint32_t>(ids.size()), &width, &height));
	ASSERT_EQ(width, WINDOW_WIDTH);
	ASSERT_EQ(height, WINDOW_HEIGHT);
	//近景内部（x = 40）为 ACES 编号、远景角落为 AgX 编号
	EXPECT_EQ(ids[static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH + 40], 1u)
		<< "近景未被编号覆盖";
	EXPECT_EQ(ids[2 * WINDOW_WIDTH + 2], 5u) << "远景编号缺失";
	//全屏远景覆盖下不得出现空洞，也不得出现两种编号的插值（只允许 1 / 5）
	std::size_t hole_count = 0;
	std::size_t mixed_count = 0;
	std::size_t near_count = 0;
	std::size_t far_count = 0;
	std::string hole_text;
	for (std::size_t index = 0; index < ids.size(); ++index)
	{
		if (ids[index] == 0)
		{
			++hole_count;
			if (hole_count <= 8)
				hole_text += "(" + std::to_string(index % WINDOW_WIDTH) + ","
					+ std::to_string(index / WINDOW_WIDTH) + ") ";
		}
		else if (ids[index] == 1)
			++near_count;
		else if (ids[index] == 5)
			++far_count;
		else
			++mixed_count;
	}
	EXPECT_EQ(hole_count, 0u) << "全屏远景覆盖下编号出现空洞：" << hole_text;
	EXPECT_EQ(mixed_count, 0u) << "MSAA 解析产生了两种编号的插值";
	EXPECT_GT(near_count, 0u);
	EXPECT_GT(far_count, 0u);
	//边缘为硬过渡：沿中行只允许一次 1 → 5 的阶跃（近景矩形左右各一次，故恰好两段）
	uint32_t transition_count = 0;
	const std::size_t row_offset = static_cast<std::size_t>(WINDOW_HEIGHT / 2) * WINDOW_WIDTH;
	for (uint32_t x = 1; x < WINDOW_WIDTH; ++x)
	{
		if (ids[row_offset + x] != ids[row_offset + x - 1])
			++transition_count;
	}
	EXPECT_EQ(transition_count, 2u) << "MSAA 编号边缘出现半透明过渡";
	//清理（先销毁目标再恢复采样数，避免目标随采样数重建）
	renderer.device().render_target_set(0);
	renderer.resource().material_unload(near_material);
	renderer.resource().material_unload(far_material);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
	renderer.device().msaa_samples_set(1);
}

/*
编号通道 · 单采样与多重采样一致（第 824 轮）
口径：同一场景分别渲染到单采样目标与 4× 目标后，两张编号图在内部区域（避开目标边界 2 像素）
      必须逐字节一致；固定采样数目标不受设备采样数切换影响。
*/
TEST_F(Renderer_Offscreen_Test, 编号通道单采样与多重采样一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	//单采样目标（显式采样数 1，不跟随设备切换）
	uint64_t single_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(single_target, WINDOW_WIDTH, WINDOW_HEIGHT,
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	//远景 AgX（编号 5）与近景 ACES（编号 1）
	engine::Material_Data base;
	base.texture = texture;
	base.pbr_override = true;
	base.pbr_roughness = 0.4f;
	base.pbr_metallic = 0.0f;
	engine::Material_Data far_data = base;
	far_data.tone_operator = 4;
	engine::Material_Handle far_material = 0;
	ASSERT_TRUE(renderer.resource().material_load(far_material, far_data));
	engine::Material_Data near_data = base;
	near_data.tone_operator = 0;
	engine::Material_Handle near_material = 0;
	ASSERT_TRUE(renderer.resource().material_load(near_material, near_data));
	renderer.device().tone_id_pass_set(true);
	//构造缩放 ＋ 平移矩阵（列主序）
	auto make_matrix = [](float scale, float x, float z)
	{
		return std::array<float, 16>{
			scale, 0.0f, 0.0f, 0.0f,
			0.0f, scale, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, 0.0f, z, 1.0f,
		};
	};
	//按目标渲染一帧并读回编号图
	auto render_ids = [&](uint64_t target)
	{
		renderer.resource().material_bind_set(far_material);
		std::array<float, 16> far_matrix = make_matrix(6.0f, 0.0f, -1.0f);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, far_matrix.data()));
		renderer.resource().material_bind_set(near_material);
		std::array<float, 16> near_matrix = make_matrix(0.5f, 0.137f, 1.0f);
		EXPECT_TRUE(renderer.resource().mesh_draw(quad, near_matrix.data()));
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
	//单采样路径
	std::vector<uint8_t> single_ids = render_ids(single_target);
	//切到 4× MSAA（不支持则跳过；固定采样数目标不受影响）
	renderer.device().msaa_samples_set(4);
	if (renderer.device().msaa_samples() != 4)
		GTEST_SKIP() << "本机不支持 4× MSAA";
	uint64_t msaa_target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(msaa_target, WINDOW_WIDTH, WINDOW_HEIGHT,
		renderer.device().scene_color_format(), static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 4));
	//多重采样路径
	std::vector<uint8_t> msaa_ids = render_ids(msaa_target);
	//内部区域逐字节一致（避开目标边界两像素）
	std::size_t mismatch_count = 0;
	std::string mismatch_text;
	for (uint32_t y = 2; y + 2 < WINDOW_HEIGHT; ++y)
	{
		for (uint32_t x = 2; x + 2 < WINDOW_WIDTH; ++x)
		{
			const std::size_t index = static_cast<std::size_t>(y) * WINDOW_WIDTH + x;
			if (single_ids[index] == msaa_ids[index])
				continue;
			++mismatch_count;
			if (mismatch_count <= 8)
				mismatch_text += "(" + std::to_string(x) + "," + std::to_string(y) + ")"
					+ std::to_string(single_ids[index]) + "/" + std::to_string(msaa_ids[index]) + " ";
		}
	}
	EXPECT_EQ(mismatch_count, 0u) << "单采样与多重采样编号不一致：" << mismatch_text;
	//清理（先销毁目标再恢复采样数）
	renderer.device().render_target_set(0);
	renderer.resource().render_target_destroy(msaa_target);
	renderer.resource().render_target_destroy(single_target);
	renderer.resource().material_unload(near_material);
	renderer.resource().material_unload(far_material);
	renderer.resource().mesh_unload(quad);
	renderer.resource().texture_unload(texture);
	renderer.device().msaa_samples_set(1);
}

/*
场景管线颜色格式查询（第 826 轮）
口径：`device().scene_color_format()` 返回场景管线烘焙用的颜色格式——默认与交换链一致，
      HDR 默认路径为 16F；按该格式建离屏目标后渲染可正常回读。
*/
TEST_F(Renderer_Offscreen_Test, 场景管线颜色格式查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认路径：场景格式 = 交换链格式
	const uint32_t scene_format = renderer.device().scene_color_format();
	EXPECT_NE(scene_format, 0u);
	EXPECT_EQ(scene_format, renderer.device().swapchain_format());
	//按查询到的格式建目标（管线声明格式一致，不触发动态渲染格式不一致）
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		scene_format, static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT), 0, 1));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.resource().texture_load(texture, texels.data(), 2, 2));
	renderer.resource().texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.resource().mesh_load(quad, quad_data()));
	renderer.device().render_target_set(target);
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.resource().mesh_draw(quad, full.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "按场景格式建目标后渲染异常：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	//清理
	renderer.device().render_target_set(0);
	renderer.resource().mesh_unload(quad);
	renderer.resource().render_target_destroy(target);
	renderer.resource().texture_unload(texture);
	//HDR 默认路径：场景格式为 16F，默认目标创建沿用同一格式
	engine::Renderer_Config hdr_config = hidden_config();
	hdr_config.hdr_targets = true;
	engine::Renderer hdr_renderer;
	ASSERT_TRUE(hdr_renderer.init(hdr_config));
	EXPECT_EQ(hdr_renderer.device().scene_color_format(),
		static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT));
	EXPECT_NE(hdr_renderer.device().scene_color_format(), hdr_renderer.device().swapchain_format());
	uint64_t hdr_target = 0;
	ASSERT_TRUE(hdr_renderer.resource().render_target_create(hdr_target, WINDOW_WIDTH, WINDOW_HEIGHT));
	hdr_renderer.resource().render_target_destroy(hdr_target);
}

/*
渲染目标原生信息 · 格式查询（第 828 轮）
口径：`render_target_native_get` 返回颜色 / 深度格式；调用方可据此核对目标格式与 `scene_color_format()` 一致。
*/
TEST_F(Renderer_Offscreen_Test, 目标原生信息含格式)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const uint32_t scene_format = renderer.device().scene_color_format();
	uint64_t target = 0;
	ASSERT_TRUE(renderer.resource().render_target_create_format_ex(target, WINDOW_WIDTH, WINDOW_HEIGHT,
		scene_format, static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT),
		static_cast<uint32_t>(VK_FORMAT_R8_UINT), 1));
	engine::Render_Target_Native_Info info;
	ASSERT_TRUE(renderer.resource().render_target_native_get(target, info));
	EXPECT_EQ(info.color_format, scene_format);
	EXPECT_EQ(info.depth_format, static_cast<uint32_t>(VK_FORMAT_D32_SFLOAT));
	EXPECT_EQ(info.width, WINDOW_WIDTH);
	EXPECT_EQ(info.height, WINDOW_HEIGHT);
	EXPECT_EQ(info.sample_count, 1u);
	//销毁后查询失败且出参复位
	renderer.resource().render_target_destroy(target);
	EXPECT_FALSE(renderer.resource().render_target_native_get(target, info));
	EXPECT_EQ(info.color_format, 0u);
}
