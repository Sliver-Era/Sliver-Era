/*
渲染材质清漆测试（2026-11-21 自「渲染材质测试.cpp」拆出）
覆盖：清漆层增强高光、点光源下清漆高光、清漆参数非法值归一、逐物体清漆门面开关、实例化绘制携带清漆层。
*/

#include "渲染材质测试辅助.h"

/*
清漆层（第二百三十三轮）
球体在 PBR 模式下叠加清漆层（强度 1、粗糙度 0.05）后，中心高光应更亮；
绑定 0 清除材质状态后回落。判据为「中心像素三通道最小值」，与通道序无关。
*/
TEST_F(Renderer_Material_Test, 清漆层增强高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//球体（法线连续，才有镜面高光）
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 0.6f, 32, 16));
	//方向光沿 −Z 传播（相机在 +Z 3 单位处），球心正对高光；开启 PBR 与固定高光强度
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.2f);
	renderer.scene().specular_set(1.0f, 32.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	/*
	中性灰反照率（0.4 左右）：避免漫反射先把中心顶到饱和，
	否则清漆层叠加的镜面高光没有抬升空间（饱和后两者都读到 255）。
	*/
	uint64_t gray = 0;
	{
		std::vector<uint8_t> gray_texels(4u * 4u * 4u, 100);
		for (std::size_t index = 3; index < gray_texels.size(); index += 4)
			gray_texels[index] = 255;
		ASSERT_TRUE(renderer.resource().texture_load(gray, gray_texels.data(), 4, 4));
	}

	//整幅三通道最小值的累加（亮度判据；避免单像素落在高光峰值外而漏判）
	auto brightness_sum = [](const std::vector<uint8_t>& pixels)
	{
		uint64_t sum = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			uint8_t result = pixels[offset];
			if (pixels[offset + 1] < result)
				result = pixels[offset + 1];
			if (pixels[offset + 2] < result)
				result = pixels[offset + 2];
			sum += result;
		}
		return sum;
	};

	//基准：材质不带清漆
	engine::Material_Data plain_data;
	plain_data.pbr_override = true;
	plain_data.pbr_roughness = 0.5f;
	plain_data.texture = gray;
	engine::Material_Handle plain = 0;
	ASSERT_TRUE(renderer.resource().material_load(plain, plain_data));
	renderer.resource().material_bind_set(plain);
	ASSERT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
	std::vector<uint8_t> base_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(base_pixels.data(), static_cast<uint32_t>(base_pixels.size())));
	uint64_t base_sum = brightness_sum(base_pixels);

	//对照：清漆强度 1、粗糙度 0.05（更锐利的第二层高光）
	engine::Material_Data coat_data;
	coat_data.pbr_override = true;
	coat_data.pbr_roughness = 0.5f;
	coat_data.clear_coat = 1.0f;
	coat_data.clear_coat_roughness = 1.0f;
	coat_data.texture = gray;
	engine::Material_Handle coat = 0;
	ASSERT_TRUE(renderer.resource().material_load(coat, coat_data));
	renderer.resource().material_bind_set(coat);
	ASSERT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
	std::vector<uint8_t> coat_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(coat_pixels.data(), static_cast<uint32_t>(coat_pixels.size())));
	uint64_t coat_sum = brightness_sum(coat_pixels);

	//开启清漆后整幅亮度应抬升（清漆层叠加的镜面高光不会低于 100 级灰度总量）
	EXPECT_GT(coat_sum, base_sum + 100)
		<< "清漆层未抬升画面亮度：" << base_sum << " vs " << coat_sum;

	renderer.resource().material_unload(plain);
	renderer.resource().material_unload(coat);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(gray);
}

/*
清漆层同样作用于点光源（第二百三十四轮）
口径：关闭方向光，仅一盏点光源（位于球体与相机之间）；其余条件与「清漆层增强高光」一致，
      开启清漆后整幅亮度应抬升 —— 证明清漆不局限于方向光分支。
*/
TEST_F(Renderer_Material_Test, 点光源清漆层增强高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 0.6f, 32, 16));
	//关闭方向光，只留一盏点光源
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 0.0f, 0.05f);
	const float point_position[3] = { 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.scene().light_add_point(point_position, color, 3.0f, 1.0f, 0.05f));
	renderer.scene().specular_set(1.0f, 32.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	//中性灰纹理（避免漫反射先饱和）
	uint64_t gray = 0;
	{
		std::vector<uint8_t> gray_texels(4u * 4u * 4u, 100);
		for (std::size_t index = 3; index < gray_texels.size(); index += 4)
			gray_texels[index] = 255;
		ASSERT_TRUE(renderer.resource().texture_load(gray, gray_texels.data(), 4, 4));
	}
	//整幅三通道最小值累加（与方向光用例同一判据）
	auto brightness_sum = [](const std::vector<uint8_t>& pixels)
	{
		uint64_t sum = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			uint8_t result = pixels[offset];
			if (pixels[offset + 1] < result)
				result = pixels[offset + 1];
			if (pixels[offset + 2] < result)
				result = pixels[offset + 2];
			sum += result;
		}
		return sum;
	};

	//基准：不带清漆
	engine::Material_Data plain_data;
	plain_data.pbr_override = true;
	plain_data.pbr_roughness = 0.5f;
	plain_data.texture = gray;
	engine::Material_Handle plain = 0;
	ASSERT_TRUE(renderer.resource().material_load(plain, plain_data));
	renderer.resource().material_bind_set(plain);
	ASSERT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
	std::vector<uint8_t> base_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(base_pixels.data(), static_cast<uint32_t>(base_pixels.size())));
	uint64_t base_sum = brightness_sum(base_pixels);

	//对照：清漆强度 1、粗糙度 1
	engine::Material_Data coat_data;
	coat_data.pbr_override = true;
	coat_data.pbr_roughness = 0.5f;
	coat_data.clear_coat = 1.0f;
	coat_data.clear_coat_roughness = 1.0f;
	coat_data.texture = gray;
	engine::Material_Handle coat = 0;
	ASSERT_TRUE(renderer.resource().material_load(coat, coat_data));
	renderer.resource().material_bind_set(coat);
	ASSERT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
	std::vector<uint8_t> coat_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(coat_pixels.data(), static_cast<uint32_t>(coat_pixels.size())));
	uint64_t coat_sum = brightness_sum(coat_pixels);

	//点光源下开启清漆同样应抬升亮度
	EXPECT_GT(coat_sum, base_sum + 100)
		<< "点光源下的清漆层未抬升画面亮度：" << base_sum << " vs " << coat_sum;

	renderer.resource().material_unload(plain);
	renderer.resource().material_unload(coat);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(gray);
}

/*
清漆参数非法值归一（第二百三十八轮）
口径：门面对非法值一律「回落默认」而非钳制 —— `clear_coat` 非数或不在 (0, 1] 时归一为 0（关闭），
      `clear_coat_roughness` 非数或不在 [0.03, 1] 时归一为 0.15；
      故「非数」与「越界（2.0 / 5.0）」两帧都应等同「不带清漆」的帧（逐字节一致）。
*/
TEST_F(Renderer_Material_Test, 清漆参数非法值归一)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 0.6f, 32, 16));
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.2f);
	renderer.scene().specular_set(1.0f, 32.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	uint64_t gray = 0;
	{
		std::vector<uint8_t> gray_texels(4u * 4u * 4u, 100);
		for (std::size_t index = 3; index < gray_texels.size(); index += 4)
			gray_texels[index] = 255;
		ASSERT_TRUE(renderer.resource().texture_load(gray, gray_texels.data(), 4, 4));
	}
	//渲染一帧并回读到调用方缓冲
	auto render_frame = [&](engine::Material_Handle material, std::vector<uint8_t>& pixels)
	{
		renderer.resource().material_bind_set(material);
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
		pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};

	//基准材质：不带清漆
	engine::Material_Data plain_data;
	plain_data.pbr_override = true;
	plain_data.pbr_roughness = 0.5f;
	plain_data.texture = gray;
	engine::Material_Handle plain = 0;
	ASSERT_TRUE(renderer.resource().material_load(plain, plain_data));

	//非数强度 / 粗糙度（应归一到 0 / 1，等同于关闭）
	engine::Material_Data nan_data = plain_data;
	nan_data.clear_coat = std::numeric_limits<float>::quiet_NaN();
	nan_data.clear_coat_roughness = std::numeric_limits<float>::quiet_NaN();
	engine::Material_Handle nan_coat = 0;
	ASSERT_TRUE(renderer.resource().material_load(nan_coat, nan_data));

	//越界强度 / 粗糙度（应回落默认：强度 0、粗糙度 0.15，等同于关闭）
	engine::Material_Data over_data = plain_data;
	over_data.clear_coat = 2.0f;
	over_data.clear_coat_roughness = 5.0f;
	engine::Material_Handle over_coat = 0;
	ASSERT_TRUE(renderer.resource().material_load(over_coat, over_data));

	std::vector<uint8_t> plain_pixels;
	std::vector<uint8_t> nan_pixels;
	std::vector<uint8_t> over_pixels;
	render_frame(plain, plain_pixels);
	render_frame(nan_coat, nan_pixels);
	render_frame(over_coat, over_pixels);

	//非数参数归一到关闭：与不带清漆逐字节一致
	EXPECT_EQ(nan_pixels, plain_pixels) << "非数清漆参数未归一为关闭";
	//越界参数回落默认：与不带清漆逐字节一致
	EXPECT_EQ(over_pixels, plain_pixels) << "越界清漆参数未回落默认";

	renderer.resource().material_unload(plain);
	renderer.resource().material_unload(nan_coat);
	renderer.resource().material_unload(over_coat);
	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(gray);
}

/*
逐物体清漆门面开关（第二百三十九轮）
口径：`material_clear_coat_set(1, 1)` 后绘制亮度应抬升；`material_clear_coat_clear()` 后画面回落与未设置时逐字节一致；
      非法参数整次拒绝且状态不变；查询接口与吸附状态一致。
*/
TEST_F(Renderer_Material_Test, 逐物体清漆门面开关)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 0.6f, 32, 16));
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.2f);
	renderer.scene().specular_set(1.0f, 32.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	uint64_t gray = 0;
	{
		std::vector<uint8_t> gray_texels(4u * 4u * 4u, 100);
		for (std::size_t index = 3; index < gray_texels.size(); index += 4)
			gray_texels[index] = 255;
		ASSERT_TRUE(renderer.resource().texture_load(gray, gray_texels.data(), 4, 4));
	}
	renderer.resource().texture_bind_set(gray);
	//整幅三通道最小值累加（与前述清漆用例同一判据）
	auto brightness_sum = [](const std::vector<uint8_t>& pixels)
	{
		uint64_t sum = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			uint8_t result = pixels[offset];
			if (pixels[offset + 1] < result)
				result = pixels[offset + 1];
			if (pixels[offset + 2] < result)
				result = pixels[offset + 2];
			sum += result;
		}
		return sum;
	};
	auto render_frame = [&](std::vector<uint8_t>& pixels)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw(sphere, nullptr));
		pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};

	//默认状态
	EXPECT_FALSE(renderer.resource().material_clear_coat_active());
	EXPECT_FLOAT_EQ(renderer.resource().material_clear_coat_strength(), 0.0f);
	EXPECT_FLOAT_EQ(renderer.resource().material_clear_coat_roughness(), 0.15f);

	//基准帧（未设置逐物体清漆）
	std::vector<uint8_t> base_pixels;
	render_frame(base_pixels);

	//开启逐物体清漆
	ASSERT_TRUE(renderer.resource().material_clear_coat_set(1.0f, 1.0f));
	EXPECT_TRUE(renderer.resource().material_clear_coat_active());
	EXPECT_FLOAT_EQ(renderer.resource().material_clear_coat_strength(), 1.0f);
	EXPECT_FLOAT_EQ(renderer.resource().material_clear_coat_roughness(), 1.0f);
	std::vector<uint8_t> coat_pixels;
	render_frame(coat_pixels);
	EXPECT_GT(brightness_sum(coat_pixels), brightness_sum(base_pixels) + 100)
		<< "逐物体清漆未抬升画面亮度";

	//非法参数整次拒绝（状态不变）
	EXPECT_FALSE(renderer.resource().material_clear_coat_set(0.0f, 1.0f));
	EXPECT_FALSE(renderer.resource().material_clear_coat_set(1.0f, 0.0f));
	EXPECT_TRUE(renderer.resource().material_clear_coat_active());

	//清除后回落与基准帧逐字节一致
	renderer.resource().material_clear_coat_clear();
	EXPECT_FALSE(renderer.resource().material_clear_coat_active());
	EXPECT_FLOAT_EQ(renderer.resource().material_clear_coat_strength(), 0.0f);
	std::vector<uint8_t> cleared_pixels;
	render_frame(cleared_pixels);
	EXPECT_EQ(cleared_pixels, base_pixels) << "清除逐物体清漆后画面未回落";

	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(gray);
}

/*
实例化绘制携带清漆层（第二百四十轮）
口径：`mesh_draw_instanced` 与逐项路径共用同一推送块写入。两枚小球在高光区只覆盖约 40 个像素，
      故以「开 / 关清漆的差异像素数」判定（实测实例化与逐项均为 44），不用整幅亮度和（阈值易受 sRGB 舍入影响）。
*/
TEST_F(Renderer_Material_Test, 实例化绘制携带清漆层)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle sphere = 0;
	ASSERT_TRUE(renderer.resource().mesh_sphere_load(sphere, 0.5f, 24, 12));
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	renderer.scene().light_set(direction, color, 1.0f, 0.2f);
	renderer.scene().specular_set(1.0f, 32.0f);
	renderer.scene().pbr_enable_set(true);
	renderer.scene().pbr_roughness_set(0.5f);
	uint64_t gray = 0;
	std::vector<uint8_t> gray_texels(4u * 4u * 4u, 100);
	for (std::size_t index = 3; index < gray_texels.size(); index += 4)
		gray_texels[index] = 255;
	ASSERT_TRUE(renderer.resource().texture_load(gray, gray_texels.data(), 4, 4));
	renderer.resource().texture_bind_set(gray);
	std::array<std::array<float, 16>, 2> matrices{};
	matrices[0] = { 0.5f,0,0,0, 0,0.5f,0,0, 0,0,0.5f,0, -1,0,0,1 };
	matrices[1] = { 0.5f,0,0,0, 0,0.5f,0,0, 0,0,0.5f,0, 1,0,0,1 };
	auto render_frame = [&](std::vector<uint8_t>& pixels)
	{
		EXPECT_TRUE(renderer.resource().mesh_draw_instanced(sphere, matrices[0].data(), 2));
		pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};
	std::vector<uint8_t> base_pixels;
	render_frame(base_pixels);
	ASSERT_TRUE(renderer.resource().material_clear_coat_set(1.0f, 1.0f));
	std::vector<uint8_t> coat_pixels;
	render_frame(coat_pixels);
	std::size_t differing = 0;
	for (std::size_t offset = 0; offset + 3 < base_pixels.size(); offset += 4)
	{
		if (base_pixels[offset] != coat_pixels[offset] || base_pixels[offset + 1] != coat_pixels[offset + 1]
			|| base_pixels[offset + 2] != coat_pixels[offset + 2])
			++differing;
	}
	EXPECT_GT(differing, 20u) << "实例化绘制未携带清漆层（差异像素 " << differing << "）";

	//清除后回落与基准帧逐字节一致
	renderer.resource().material_clear_coat_clear();
	std::vector<uint8_t> cleared_pixels;
	render_frame(cleared_pixels);
	EXPECT_EQ(cleared_pixels, base_pixels) << "实例化路径清除清漆后画面未回落";

	renderer.resource().mesh_unload(sphere);
	renderer.resource().texture_unload(gray);
}
