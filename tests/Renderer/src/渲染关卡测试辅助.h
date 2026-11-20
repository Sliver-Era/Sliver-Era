#pragma once
#include <gtest/gtest.h>

//结构化像素指标（实现收敛到公共辅助）
#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineSystem/src/world/data/scene/Level/关卡文本.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"
#include "Engine/EngineRenderer/src/render/Asset/资产缓存.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"

namespace
{
	//端到端窗口尺寸
	constexpr uint32_t LEVEL_WIDTH = 128;
	constexpr uint32_t LEVEL_HEIGHT = 72;
	//端到端覆盖下限（2×2 四边形在默认相机下占屏比例远大于此）
	constexpr uint32_t LEVEL_MIN_PIXELS = 800;

	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path level_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"关卡测试");
	}

	//关卡文件的 UTF-8 窄路径（供读写入参）
	std::string level_utf8(const char* name)
	{
		return std::string("out/关卡测试/") + name;
	}

	//写一个 UTF-8 文本文件（目录自动创建；宽路径写盘）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}


	//浮点逐位相等（往返口径：%.9g 必须能唯一还原二进制 32 位浮点）
	bool bit_equal(float left, float right)
	{
		return std::memcmp(&left, &right, sizeof(float)) == 0;
	}

	//两帧之间逐通道差 ≥ 6 的像素数（实现收敛到公共辅助）
	inline uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		return byjy_render_test::changed_pixels(left, right, 6);
	}

	//写方盒几何 ＋ 单材质（关卡端到端用）
	void write_box_assets(void)
	{
		write_text(level_dir() / L"box.obj",
			"v -1 -1 0\n"
			"v 1 -1 0\n"
			"v 1 1 0\n"
			"v -1 1 0\n"
			"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
			"vn 0 0 1\n"
			"f 1/1/1 2/2/1 3/3/1\n"
			"f 1/1/1 3/3/1 4/4/1\n");
		write_text(level_dir() / L"box.mtl", "newmtl 甲\nKd 0.8 0.2 0.2\nNs 64\n");
	}

	/*
	写立方体几何 ＋ 单材质（第四百七十二轮；卡通用例需要多变法线）
	六个面各自独立法线，供逐物体卡通色阶产生可见差异；正面 / 右侧 / 顶面按外法线逆时针绕序。
	*/
	void write_cube_assets(void)
	{
		write_text(level_dir() / L"cube.obj",
			"v -1 -1 -1\n"
			"v 1 -1 -1\n"
			"v 1 1 -1\n"
			"v -1 1 -1\n"
			"v -1 -1 1\n"
			"v 1 -1 1\n"
			"v 1 1 1\n"
			"v -1 1 1\n"
			"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
			"vn 0 0 -1\nvn 0 0 1\nvn -1 0 0\nvn 1 0 0\nvn 0 -1 0\nvn 0 1 0\n"
			"f 5/1/2 6/2/2 7/3/2\n"
			"f 5/1/2 7/3/2 8/4/2\n"
			"f 6/1/4 7/2/4 3/3/4\n"
			"f 6/1/4 3/3/4 2/4/4\n"
			"f 8/1/6 7/2/6 3/3/6\n"
			"f 8/1/6 3/3/6 4/4/6\n"
			"f 2/1/1 1/2/1 4/3/1\n"
			"f 2/1/1 4/3/1 3/4/1\n"
			"f 5/1/3 1/2/3 4/3/3\n"
			"f 5/1/3 4/3/3 8/4/3\n"
			"f 1/1/5 2/2/5 6/3/5\n"
			"f 1/1/5 6/3/5 5/4/5\n");
		write_text(level_dir() / L"cube.mtl", "newmtl 甲\nKd 0.7 0.5 0.4\nNs 64\n");
	}

	/*
	八盏聚光关卡（第五百一十五轮抽出）
	八盏聚光各照一块地面与遮挡体；无其它前向灯，确保八盏全部登记（前向光源上限 8 盏）。
	*/
	void write_eight_spot_level(void)
	{
		write_text(level_dir() / L"八盏聚光.level",
			"level 八盏聚光\n"
			"camera position 0 8 10\n"
			"camera target 0 0 0\n"
			"light direction 0 -1 0\n"
			"light color 0.1 0.1 0.1\n"
			"light intensity 0.1\n"
			"light spot -7 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -5 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -1 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 1 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 5 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 7 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"mesh 方块 cube.obj\n"
			"mtl 方块材质 cube.mtl\n"
			"place 方块 方块材质 0 -1.5 0 24 0.2 10\n"
			"place 方块 方块材质 -7 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -5 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -1 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 1 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 5 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 7 0.6 0 1.2 1.2 1.2\n");
	}

	//三通道浮点逐位相等
	bool triple_bit_equal(const float* left, const float* right)
	{
		return bit_equal(left[0], right[0]) && bit_equal(left[1], right[1]) && bit_equal(left[2], right[2]);
	}

	//逐位比较两个关卡的全字段（失败时输出首个差异字段）
	void expect_level_equal(const engine::Level_Data& left, const engine::Level_Data& right)
	{
		EXPECT_EQ(left.name, right.name);
		EXPECT_EQ(left.version, right.version) << "格式版本不一致";

		//相机组
		EXPECT_TRUE(triple_bit_equal(left.camera.position, right.camera.position)) << "相机位置不一致";
		EXPECT_TRUE(triple_bit_equal(left.camera.target, right.camera.target)) << "相机注视点不一致";
		EXPECT_TRUE(triple_bit_equal(left.camera.up, right.camera.up)) << "相机上方向不一致";
		EXPECT_TRUE(bit_equal(left.camera.fov, right.camera.fov)) << "视场角不一致";
		EXPECT_TRUE(bit_equal(left.camera.near_plane, right.camera.near_plane)) << "近裁剪面不一致";
		EXPECT_TRUE(bit_equal(left.camera.far_plane, right.camera.far_plane)) << "远裁剪面不一致";
		EXPECT_EQ(left.camera.ortho, right.camera.ortho) << "正交开关不一致";
		EXPECT_TRUE(bit_equal(left.camera.ortho_height, right.camera.ortho_height)) << "正交高度不一致";
		EXPECT_TRUE(bit_equal(left.camera.aspect, right.camera.aspect)) << "宽高比不一致";
		for (std::size_t index = 0; index < 4; ++index)
			EXPECT_TRUE(bit_equal(left.camera.clear_color[index], right.camera.clear_color[index])) << "清屏色不一致";

		//光照组（标量 / 颜色）
		EXPECT_TRUE(triple_bit_equal(left.light.direction, right.light.direction)) << "光照方向不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.color, right.light.color)) << "光照颜色不一致";
		EXPECT_TRUE(bit_equal(left.light.intensity, right.light.intensity)) << "光照强度不一致";
		EXPECT_TRUE(bit_equal(left.light.ambient, right.light.ambient)) << "环境光强度不一致";
		EXPECT_TRUE(bit_equal(left.light.specular_intensity, right.light.specular_intensity)) << "高光强度不一致";
		EXPECT_TRUE(bit_equal(left.light.specular_shininess, right.light.specular_shininess)) << "高光光泽度不一致";
		EXPECT_EQ(left.light.attenuation_mode, right.light.attenuation_mode) << "衰减模式不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.ambient_color, right.light.ambient_color)) << "环境光颜色不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.ambient_ground, right.light.ambient_ground)) << "环境光地面色不一致";
		EXPECT_TRUE(bit_equal(left.light.ambient_hemisphere, right.light.ambient_hemisphere)) << "半球强度不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.fill_direction, right.light.fill_direction)) << "补光方向不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.fill_color, right.light.fill_color)) << "补光颜色不一致";
		EXPECT_TRUE(bit_equal(left.light.fill_intensity, right.light.fill_intensity)) << "补光强度不一致";
		EXPECT_TRUE(bit_equal(left.light.directional_specular, right.light.directional_specular)) << "方向光高光缩放不一致";
		EXPECT_TRUE(bit_equal(left.light.fill_specular, right.light.fill_specular)) << "补光高光缩放不一致";

		//阴影 / PBR / 卡通 / 描边
		EXPECT_EQ(left.light.shadow_enabled, right.light.shadow_enabled) << "阴影开关不一致";
		EXPECT_TRUE(bit_equal(left.light.shadow_bias, right.light.shadow_bias)) << "阴影偏置不一致";
		EXPECT_TRUE(bit_equal(left.light.shadow_softness, right.light.shadow_softness)) << "阴影软度不一致";
		EXPECT_TRUE(bit_equal(left.light.shadow_strength, right.light.shadow_strength)) << "阴影强度不一致";
		EXPECT_EQ(left.light.shadow_resolution, right.light.shadow_resolution) << "阴影分辨率不一致";
		EXPECT_EQ(left.light.pbr_enabled, right.light.pbr_enabled) << "PBR 开关不一致";
		EXPECT_TRUE(bit_equal(left.light.pbr_roughness, right.light.pbr_roughness)) << "PBR 粗糙度不一致";
		EXPECT_TRUE(bit_equal(left.light.pbr_metallic, right.light.pbr_metallic)) << "PBR 金属度不一致";
		EXPECT_EQ(left.light.toon_steps, right.light.toon_steps) << "卡通色阶不一致";
		EXPECT_TRUE(bit_equal(left.light.toon_rim, right.light.toon_rim)) << "边缘光不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.toon_shadow_tint, right.light.toon_shadow_tint)) << "暗部色调不一致";
		EXPECT_TRUE(bit_equal(left.light.toon_specular, right.light.toon_specular)) << "卡通高光阈值不一致";
		EXPECT_TRUE(bit_equal(left.light.outline_width, right.light.outline_width)) << "描边宽度不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.outline_color, right.light.outline_color)) << "描边色不一致";

		//点光源 / 聚光灯
		EXPECT_EQ(left.light.point_count, right.light.point_count) << "点光源数量不一致";
		EXPECT_EQ(left.light.spot_count, right.light.spot_count) << "聚光灯数量不一致";
		for (uint32_t index = 0; index < left.light.point_count && index < 8u; ++index)
		{
			EXPECT_TRUE(triple_bit_equal(left.light.point_position[index], right.light.point_position[index])) << "点光位置不一致";
			EXPECT_TRUE(triple_bit_equal(left.light.point_color[index], right.light.point_color[index])) << "点光颜色不一致";
			EXPECT_TRUE(bit_equal(left.light.point_radius[index], right.light.point_radius[index])) << "点光半径不一致";
			EXPECT_TRUE(bit_equal(left.light.point_intensity[index], right.light.point_intensity[index])) << "点光强度不一致";
			EXPECT_EQ(left.light.point_attenuation[index], right.light.point_attenuation[index]) << "点光衰减模式不一致";
			EXPECT_TRUE(bit_equal(left.light.point_specular[index], right.light.point_specular[index])) << "点光高光缩放不一致";
			EXPECT_TRUE(bit_equal(left.light.point_shininess[index], right.light.point_shininess[index])) << "点光光泽度不一致";
		}
		for (uint32_t index = 0; index < left.light.spot_count && index < 8u; ++index)
		{
			EXPECT_TRUE(triple_bit_equal(left.light.spot_position[index], right.light.spot_position[index])) << "聚光位置不一致";
			EXPECT_TRUE(triple_bit_equal(left.light.spot_direction[index], right.light.spot_direction[index])) << "聚光方向不一致";
			EXPECT_TRUE(triple_bit_equal(left.light.spot_color[index], right.light.spot_color[index])) << "聚光颜色不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_radius[index], right.light.spot_radius[index])) << "聚光半径不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_intensity[index], right.light.spot_intensity[index])) << "聚光强度不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_inner_deg[index], right.light.spot_inner_deg[index])) << "聚光内锥不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_outer_deg[index], right.light.spot_outer_deg[index])) << "聚光外锥不一致";
			EXPECT_EQ(left.light.spot_attenuation[index], right.light.spot_attenuation[index]) << "聚光衰减模式不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_specular[index], right.light.spot_specular[index])) << "聚光高光缩放不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_shininess[index], right.light.spot_shininess[index])) << "聚光光泽度不一致";
		}

		//雾组
		EXPECT_TRUE(triple_bit_equal(left.fog.color, right.fog.color)) << "雾色不一致";
		EXPECT_TRUE(bit_equal(left.fog.density, right.fog.density)) << "雾密度不一致";
		EXPECT_TRUE(bit_equal(left.fog.start, right.fog.start)) << "雾起点不一致";
		EXPECT_TRUE(bit_equal(left.fog.end, right.fog.end)) << "雾终点不一致";
		EXPECT_EQ(left.fog.mode, right.fog.mode) << "雾模式不一致";
		EXPECT_TRUE(bit_equal(left.fog.height_falloff, right.fog.height_falloff)) << "雾高度衰减不一致";

		//资产段落
		ASSERT_EQ(left.asset_lines.size(), right.asset_lines.size());
		for (std::size_t index = 0; index < left.asset_lines.size(); ++index)
			EXPECT_EQ(left.asset_lines[index], right.asset_lines[index]) << "资产行不一致";

		//材质实例段落（第五百八十三轮）
		ASSERT_EQ(left.materials.size(), right.materials.size());
		for (std::size_t index = 0; index < left.materials.size(); ++index)
		{
			const engine::Level_Material& 左 = left.materials[index];
			const engine::Level_Material& 右 = right.materials[index];
			EXPECT_EQ(左.实例名, 右.实例名);
			EXPECT_EQ(左.模板, 右.模板);
			EXPECT_EQ(左.use_tint, 右.use_tint);
			EXPECT_EQ(左.use_emissive, 右.use_emissive);
			EXPECT_EQ(左.use_toon, 右.use_toon);
			EXPECT_EQ(左.use_pbr, 右.use_pbr);
			EXPECT_EQ(左.use_clearcoat, 右.use_clearcoat);
			EXPECT_TRUE(bit_equal(左.emissive, 右.emissive));
			EXPECT_TRUE(bit_equal(左.clear_coat, 右.clear_coat));
			EXPECT_TRUE(bit_equal(左.clear_coat_roughness, 右.clear_coat_roughness));
			EXPECT_TRUE(bit_equal(左.toon_steps, 右.toon_steps));
			EXPECT_TRUE(bit_equal(左.toon_rim, 右.toon_rim));
			EXPECT_TRUE(bit_equal(左.toon_specular, 右.toon_specular));
			EXPECT_TRUE(bit_equal(左.pbr_roughness, 右.pbr_roughness));
			EXPECT_TRUE(bit_equal(左.pbr_metallic, 右.pbr_metallic));
			for (std::size_t channel = 0; channel < 4; ++channel)
				EXPECT_TRUE(bit_equal(左.tint[channel], 右.tint[channel])) << "实例底色不一致";
		}
	}

	//构造一份「全字段非默认」的关卡
	engine::Level_Data rich_level(void)
	{
		engine::Level_Data level;
		level.name = "测试关卡 一号";

		//相机
		level.camera.position[0] = 1.5f;
		level.camera.position[1] = -2.25f;
		level.camera.position[2] = 7.0f;
		level.camera.target[0] = 0.25f;
		level.camera.target[1] = 0.5f;
		level.camera.target[2] = -1.0f;
		level.camera.fov = 0.9f;
		level.camera.near_plane = 0.05f;
		level.camera.far_plane = 250.0f;
		level.camera.ortho = true;
		level.camera.ortho_height = 3.5f;
		level.camera.aspect = 1.777f;
		level.camera.clear_color[0] = 0.1f;
		level.camera.clear_color[1] = 0.2f;
		level.camera.clear_color[2] = 0.3f;
		level.camera.clear_color[3] = 1.0f;

		//光照
		level.light.direction[0] = 0.3f;
		level.light.direction[1] = -1.0f;
		level.light.direction[2] = 0.2f;
		level.light.color[0] = 1.0f;
		level.light.color[1] = 0.9f;
		level.light.color[2] = 0.8f;
		level.light.intensity = 1.25f;
		level.light.ambient = 0.15f;
		level.light.specular_intensity = 0.6f;
		level.light.specular_shininess = 42.0f;
		level.light.attenuation_mode = 1;
		level.light.ambient_color[0] = 0.9f;
		level.light.ambient_color[1] = 0.95f;
		level.light.ambient_color[2] = 1.0f;
		level.light.ambient_ground[0] = 0.2f;
		level.light.ambient_ground[1] = 0.25f;
		level.light.ambient_ground[2] = 0.3f;
		level.light.ambient_hemisphere = 0.4f;
		level.light.fill_direction[0] = -0.5f;
		level.light.fill_direction[1] = -0.2f;
		level.light.fill_direction[2] = -1.0f;
		level.light.fill_color[0] = 0.5f;
		level.light.fill_color[1] = 0.6f;
		level.light.fill_color[2] = 0.7f;
		level.light.fill_intensity = 0.35f;
		level.light.directional_specular = 0.8f;
		level.light.fill_specular = 0.0f;
		level.light.shadow_enabled = 1;
		level.light.shadow_bias = 0.004f;
		level.light.shadow_softness = 0.5f;
		level.light.shadow_strength = 0.8f;
		level.light.shadow_resolution = 2048;
		level.light.pbr_enabled = 1;
		level.light.pbr_roughness = 0.6f;
		level.light.pbr_metallic = 0.2f;
		level.light.toon_steps = 3;
		level.light.toon_rim = 0.25f;
		level.light.toon_shadow_tint[0] = 0.9f;
		level.light.toon_shadow_tint[1] = 0.85f;
		level.light.toon_shadow_tint[2] = 1.0f;
		level.light.toon_specular = 0.4f;
		level.light.outline_width = 0.02f;
		level.light.outline_color[0] = 0.1f;
		level.light.outline_color[1] = 0.2f;
		level.light.outline_color[2] = 0.3f;

		//两盏点光
		level.light.point_count = 2;
		level.light.point_position[0][0] = 1.0f;
		level.light.point_position[0][1] = 2.0f;
		level.light.point_position[0][2] = 3.0f;
		level.light.point_color[0][0] = 1.0f;
		level.light.point_color[0][1] = 0.5f;
		level.light.point_color[0][2] = 0.25f;
		level.light.point_radius[0] = 4.0f;
		level.light.point_intensity[0] = 2.0f;
		level.light.point_attenuation[0] = 2;
		level.light.point_specular[0] = 0.5f;
		level.light.point_shininess[0] = 10.0f;
		level.light.point_position[1][0] = -1.0f;
		level.light.point_position[1][1] = -2.0f;
		level.light.point_position[1][2] = -3.0f;
		level.light.point_color[1][0] = 0.2f;
		level.light.point_color[1][1] = 0.3f;
		level.light.point_color[1][2] = 0.4f;
		level.light.point_radius[1] = 6.0f;
		level.light.point_intensity[1] = 0.5f;
		level.light.point_attenuation[1] = 0;
		level.light.point_specular[1] = 1.0f;
		level.light.point_shininess[1] = 0.0f;

		//一盏聚光
		level.light.spot_count = 1;
		level.light.spot_position[0][0] = 0.0f;
		level.light.spot_position[0][1] = 5.0f;
		level.light.spot_position[0][2] = 0.0f;
		level.light.spot_direction[0][0] = 0.0f;
		level.light.spot_direction[0][1] = -1.0f;
		level.light.spot_direction[0][2] = 0.0f;
		level.light.spot_color[0][0] = 1.0f;
		level.light.spot_color[0][1] = 1.0f;
		level.light.spot_color[0][2] = 0.9f;
		level.light.spot_radius[0] = 8.0f;
		level.light.spot_intensity[0] = 1.5f;
		level.light.spot_inner_deg[0] = 12.0f;
		level.light.spot_outer_deg[0] = 25.0f;
		level.light.spot_attenuation[0] = 1;
		level.light.spot_specular[0] = 0.7f;
		level.light.spot_shininess[0] = 100.0f;

		//雾
		level.fog.color[0] = 0.4f;
		level.fog.color[1] = 0.5f;
		level.fog.color[2] = 0.6f;
		level.fog.density = 0.35f;
		level.fog.start = 2.0f;
		level.fog.end = 40.0f;
		level.fog.mode = 1;
		level.fog.height_falloff = 0.12f;

		//资产段落
		level.asset_lines.push_back("mesh 甲 box.obj");
		level.asset_lines.push_back("mtl 乙 甲.mtl");
		level.asset_lines.push_back("place 甲 乙 0 0 0 1 1 1");
		return level;
	}
}
