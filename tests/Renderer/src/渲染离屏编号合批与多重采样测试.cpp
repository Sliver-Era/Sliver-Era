/*
渲染离屏编号合批与多重采样测试（2026-11-21 自「渲染离屏编号通道测试.cpp」拆出）
覆盖：按纹素 alpha 裁剪、静态合批、多重采样目标、多重采样解析边界、单采样与多重采样一致、
      场景管线颜色格式查询、目标原生信息含格式。
*/
#include <gtest/gtest.h>
//Vulkan 格式常量（编号附件与目标格式查询）
#include <vulkan/vulkan.h>

#include "渲染离屏目标测试辅助.h"


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
