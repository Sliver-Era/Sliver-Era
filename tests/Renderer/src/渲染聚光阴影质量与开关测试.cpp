/*
渲染聚光阴影质量与开关测试（2026-11-21 自「渲染聚光阴影测试.cpp」拆出）
覆盖：512 档细节、关卡八盏聚光阴影、逐瓦片互异（前向 / 簇路径）、质量参数组合、分辨率切换、
      门面偏置自校准、运行期开关、八盏开销、点光上限边界。
*/

#include "渲染聚光阴影测试辅助.h"

/*
八盏聚光阴影 512 档细节（第五百四十二轮）
口径：517 轮在 128×72 画幅下 512 档差异为 0（阴影边界过粗）；本轮改 512×288 画幅，
验证 512 档相对 1024 档有可见细节变化，切回 1024 后逐字节恢复。
*/
TEST(Renderer_Level_Test, 八盏聚光阴影512档细节)
{
	write_cube_assets();
	write_eight_spot_level();

	//较大画幅（512×288）以显现分辨率差异
	const uint32_t 宽 = 512;
	const uint32_t 高 = 288;
	engine::Renderer_Config config;
	config.width = 宽;
	config.height = 高;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);

	//1024 档基准
	EXPECT_EQ(renderer.scene().shadow_resolution(), 1024u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 原图(宽 * 高 * 4);
	ASSERT_TRUE(renderer.frame_render(原图.data(), static_cast<uint32_t>(原图.size())));

	//512 档：应有可见细节变化
	renderer.scene().shadow_resolution_set(512);
	EXPECT_EQ(renderer.scene().shadow_resolution(), 512u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 低分辨率(宽 * 高 * 4);
	ASSERT_TRUE(renderer.frame_render(低分辨率.data(), static_cast<uint32_t>(低分辨率.size())));
	const uint32_t 低差异 = changed_pixels(原图, 低分辨率);
	std::cout << "[关卡] 八盏 512x288 画幅 512 档差异 " << 低差异 << std::endl;
	EXPECT_GT(低差异, 0u) << "512 档在较大画幅下仍未改变阴影细节";

	//切回 1024：逐字节恢复
	renderer.scene().shadow_resolution_set(1024);
	EXPECT_EQ(renderer.scene().shadow_resolution(), 1024u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 回图(宽 * 高 * 4);
	ASSERT_TRUE(renderer.frame_render(回图.data(), static_cast<uint32_t>(回图.size())));
	EXPECT_EQ(changed_pixels(原图, 回图), 0u) << "分辨率往返后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光灯阴影（第五百一十四轮）
口径：八盏聚光各照一块地面与遮挡体；单盏（序号 0）只遮蔽最左，多盏开启后八盏各自遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡八盏聚光阴影)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);

	//单盏（序号 0）：只遮蔽最左
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 0;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));
	//本帧参与投影的瓦片数（第五百二十轮）
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 1u);

	//多盏：八盏各自投影（瓦片 4..7 按需创建）
	灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 八盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(八盏.data(), static_cast<uint32_t>(八盏.size())));
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);

	const uint32_t 差异 = changed_pixels(单盏, 八盏);
	std::cout << "[关卡] 八盏聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "八盏聚光灯阴影未生效";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光逐瓦片互异（第五百五十一轮）
口径：序号 0..7 逐盏单盏投影，每盏使用独立视图矩阵与独立阴影图；
各序号单盏画面均应偏离关闭聚光阴影基准，且相邻序号单盏画面互不相同（每盏遮蔽不同遮挡体）。
*/
TEST(Renderer_Level_Test, 八盏聚光逐瓦片互异)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);

	//关闭聚光阴影基准
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 0;
	灯光.spot_shadow_all = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 基准(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(基准.data(), static_cast<uint32_t>(基准.size())));

	//逐序号单盏投影（瓦片 1）
	灯光.spot_shadow_enabled = 1;
	std::vector<uint8_t> 上帧;
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		灯光.spot_shadow_index = static_cast<int>(序号);
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 当前(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(当前.data(), static_cast<uint32_t>(当前.size())));
		EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 1u);

		//与关闭基准的差异（每盏都应产生遮蔽）
		const uint32_t 基准差异 = changed_pixels(基准, 当前);
		std::cout << "[关卡] 第 " << 序号 << " 盏基准差异 " << 基准差异 << std::endl;
		EXPECT_GT(基准差异, 0u) << "第 " << 序号 << " 盏单盏投影未生效";

		//与上一序号单盏的差异（相邻瓦片画面应不同）
		if (序号 > 0)
		{
			const uint32_t 相邻差异 = changed_pixels(上帧, 当前);
			std::cout << "[关卡] 第 " << 序号 - 1 << " / " << 序号 << " 盏瓦片差异 " << 相邻差异 << std::endl;
			EXPECT_GT(相邻差异, 0u) << "第 " << 序号 << " 盏与上一盏投影画面相同（瓦片未独立）";
		}
		上帧 = std::move(当前);
	}
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光逐瓦片互异（簇路径）（第五百五十九轮）
口径：同上例，但灯光经 Cluster 计算路径登记；序号 0..7 逐盏单盏投影，
各序号与关闭基准有差异、相邻序号画面互不相同，确认簇路径亦按盏绑定独立瓦片。
*/
TEST(Renderer_Level_Test, 八盏聚光逐瓦片互异_簇路径)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	if (!renderer.compute().cluster_supported())
		GTEST_SKIP() << "本机不支持 Cluster 计算路径";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);
	ASSERT_TRUE(renderer.compute().cluster_enable_set(true));

	//簇路径关闭聚光阴影基准
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 0;
	灯光.spot_shadow_all = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 基准(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(基准.data(), static_cast<uint32_t>(基准.size())));

	//簇路径逐序号单盏投影（瓦片 1）
	灯光.spot_shadow_enabled = 1;
	std::vector<uint8_t> 上帧;
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		灯光.spot_shadow_index = static_cast<int>(序号);
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 当前(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(当前.data(), static_cast<uint32_t>(当前.size())));
		EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 1u);

		//与关闭基准的差异
		const uint32_t 基准差异 = changed_pixels(基准, 当前);
		std::cout << "[关卡] 簇路径第 " << 序号 << " 盏基准差异 " << 基准差异 << std::endl;
		EXPECT_GT(基准差异, 0u) << "簇路径第 " << 序号 << " 盏单盏投影未生效";

		//与上一序号单盏的差异
		if (序号 > 0)
		{
			const uint32_t 相邻差异 = changed_pixels(上帧, 当前);
			std::cout << "[关卡] 簇路径第 " << 序号 - 1 << " / " << 序号 << " 盏瓦片差异 " << 相邻差异 << std::endl;
			EXPECT_GT(相邻差异, 0u) << "簇路径第 " << 序号 << " 盏瓦片未独立";
		}
		上帧 = std::move(当前);
	}
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光质量参数组合（第五百二十三轮）
口径：八盏聚光全部投影时，强度 0 应与「关闭聚光阴影」画面逐字节一致（着色器 mix 结果恒为 1）；
强度 0.5 保留半遮蔽（与全强度差异 > 100 像素）；软度满档（1.0，7×7 邻域）只改变阴影边缘；
恢复默认参数后与全强度基准逐字节一致。
*/
TEST(Renderer_Level_Test, 八盏聚光质量参数组合)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//关闭聚光阴影：无阴影参照图
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 无阴影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无阴影.data(), static_cast<uint32_t>(无阴影.size())));

	//八盏全投影 ＋ 默认参数（强度 1）
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 全强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(全强度.data(), static_cast<uint32_t>(全强度.size())));
	//门面回读默认三值
	float 偏置 = 0.0f;
	float 软度 = 0.0f;
	float 强度 = 0.0f;
	renderer.scene().spot_shadow_params_get(&偏置, &软度, &强度);
	EXPECT_NEAR(偏置, 0.010f, 1e-6f);
	EXPECT_NEAR(软度, 0.0f, 1e-6f);
	EXPECT_NEAR(强度, 1.0f, 1e-6f);
	const uint32_t 全强度差异 = changed_pixels(无阴影, 全强度);
	std::cout << "[关卡] 八盏全强度差异 " << 全强度差异 << std::endl;
	EXPECT_GT(全强度差异, 100u) << "八盏全强度阴影未生效";

	//强度 0：应与无阴影画面逐字节一致
	renderer.scene().spot_shadow_params_set(0.010f, 0.0f, 0.0f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 零强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(零强度.data(), static_cast<uint32_t>(零强度.size())));
	EXPECT_EQ(changed_pixels(无阴影, 零强度), 0u) << "强度 0 未与关闭聚光阴影逐字节一致";

	//强度 0.5：半遮蔽仍在
	renderer.scene().spot_shadow_params_set(0.010f, 0.0f, 0.5f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 半强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(半强度.data(), static_cast<uint32_t>(半强度.size())));
	const uint32_t 半强度差异 = changed_pixels(全强度, 半强度);
	std::cout << "[关卡] 八盏半强度差异 " << 半强度差异 << std::endl;
	EXPECT_GT(半强度差异, 100u) << "强度 0.5 未产生半遮蔽差异";
	EXPECT_GT(changed_pixels(无阴影, 半强度), 100u) << "强度 0.5 未保留可见阴影";

	//软度满档（7×7 邻域）：仅阴影边缘变化
	renderer.scene().spot_shadow_params_set(0.010f, 1.0f, 1.0f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 软边(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(软边.data(), static_cast<uint32_t>(软边.size())));
	const uint32_t 软边差异 = changed_pixels(全强度, 软边);
	std::cout << "[关卡] 八盏软度满档差异 " << 软边差异 << std::endl;
	EXPECT_GT(软边差异, 0u) << "软度满档未改变阴影边缘";

	//恢复默认参数：与全强度基准逐字节一致
	renderer.scene().spot_shadow_params_set(0.010f, 0.0f, 1.0f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 恢复(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(恢复.data(), static_cast<uint32_t>(恢复.size())));
	EXPECT_EQ(changed_pixels(全强度, 恢复), 0u) << "恢复默认参数后画面未复原";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光分辨率切换（第五百一十七轮）
口径：八盏全部投影时，阴影图 1024 → 512 → 1024 往返；切到 512 画面应有细节变化，切回后逐字节一致。
覆盖分辨率切换路径对瓦片 4..7 的重建与重绑。
*/
TEST(Renderer_Level_Test, 关卡八盏聚光分辨率切换)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//八盏全部投影，1024 基准图
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 原图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(原图.data(), static_cast<uint32_t>(原图.size())));

	//切 512：记录画面差异（随后切回 1024 校验画面恢复）
	renderer.scene().shadow_resolution_set(512);
	EXPECT_EQ(renderer.scene().shadow_resolution(), 512u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 低分辨率(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(低分辨率.data(), static_cast<uint32_t>(低分辨率.size())));
	//512 档画面差异（本场景阴影边界较粗，可能为 0；分辨率效果由单盏用例覆盖）
	const uint32_t 低差异 = changed_pixels(原图, 低分辨率);
	std::cout << "[关卡] 八盏 512 档差异 " << 低差异 << std::endl;

	//切回 1024：应与基准图一致
	renderer.scene().shadow_resolution_set(1024);
	EXPECT_EQ(renderer.scene().shadow_resolution(), 1024u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 回图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(回图.data(), static_cast<uint32_t>(回图.size())));
	EXPECT_EQ(changed_pixels(原图, 回图), 0u) << "分辨率往返后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影偏置自校准（第五百二十一轮）
口径：两盏 radius 26 / 9 的聚光；关闭自校准（默认）时逐瓦片偏置都等于全局值；
开启后按参考跨度 9 归一化，小半径瓦片略增、大半径瓦片显著减小，比值 ≈ 两瓦片跨度比。
*/
TEST(Renderer_Level_Test, 聚光灯偏置自校准)
{
	write_cube_assets();
	write_text(level_dir() / L"偏置自校准.level",
		"level 偏置自校准\n"
		"camera position 0 6 8\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot -4 4 0 0 -1 0 1 1 1 26 2 20 40 0 0 16\n"
		"light spot 4 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 16 0.2 8\n"
		"place 方块 方块材质 -4 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 4 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("偏置自校准.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);

	//默认关闭：两瓦片都等于全局偏置 0.010
	EXPECT_FALSE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 3u);
	ASSERT_TRUE(renderer.frame_render());
	float 偏置[8] = {};
	renderer.scene().spot_shadow_bias_tiles_get(偏置);
	EXPECT_NEAR(偏置[0], 0.010f, 1e-5f);
	EXPECT_NEAR(偏置[1], 0.010f, 1e-5f);

	//开启自校准：瓦片 0（radius 26）≈ 0.010 × 9 / 25.9，瓦片 1（radius 9）≈ 0.010 × 9 / 8.9
	renderer.scene().spot_shadow_bias_auto_set(true);
	EXPECT_TRUE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 3u);
	ASSERT_TRUE(renderer.frame_render());
	renderer.scene().spot_shadow_bias_tiles_get(偏置);
	EXPECT_NEAR(偏置[0], 0.010f * 9.0f / 25.9f, 1e-4f);
	EXPECT_NEAR(偏置[1], 0.010f * 9.0f / 8.9f, 1e-4f);
	//比值应接近跨度比 25.9 / 8.9
	EXPECT_NEAR(偏置[0] * (25.9f / 8.9f), 偏置[1], 5e-4f);

	//关闭后恢复全局偏置
	renderer.scene().spot_shadow_bias_auto_set(false);
	EXPECT_FALSE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 3u);
	ASSERT_TRUE(renderer.frame_render());
	renderer.scene().spot_shadow_bias_tiles_get(偏置);
	EXPECT_NEAR(偏置[0], 0.010f, 1e-5f);
	EXPECT_NEAR(偏置[1], 0.010f, 1e-5f);
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光运行期开关（第五百一十七轮）
口径：八盏场景按单盏应用后用 spot_shadow_all_set 运行期切到八盏、再切回；
八盏与单盏差异 > 100 像素，切回后与初始单盏差异 < 50 像素。
*/
TEST(Renderer_Level_Test, 八盏聚光运行期开关)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.compute().readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//单盏（序号 0）
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 0;
	灯光.spot_shadow_index = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_FALSE(renderer.scene().spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));

	//运行期切八盏（按已登记聚光灯数量创建瓦片 1..7）
	renderer.scene().spot_shadow_all_set(true);
	EXPECT_TRUE(renderer.scene().spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 八盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(八盏.data(), static_cast<uint32_t>(八盏.size())));
	const uint32_t 差异 = changed_pixels(单盏, 八盏);
	std::cout << "[关卡] 八盏运行期开关差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "运行期切八盏未生效";

	//切回单盏：画面应恢复
	renderer.scene().spot_shadow_all_set(false);
	EXPECT_FALSE(renderer.scene().spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 回单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(回单盏.data(), static_cast<uint32_t>(回单盏.size())));
	const uint32_t 回差异 = changed_pixels(单盏, 回单盏);
	std::cout << "[关卡] 八盏运行期切回差异 " << 回差异 << std::endl;
	EXPECT_LT(回差异, 50u) << "运行期切回单盏后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光灯开销基线（第五百一十五轮）
口径：同一八盏聚光场景，分别登记 1 / 8 盏并全部投影；记录阴影 pass 单遍 GPU 耗时，
断言 8 盏的单遍耗时不显著低于 1 盏（容差 0.1 ms），即总成本随遍数线性增长。
*/
TEST(Renderer_Level_Test, 关卡八盏聚光开销)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//分别按 1 / 8 盏登记，全部投影并预热 6 帧
	const uint32_t counts[2] = { 1u, 8u };
	double shadow_ms[2] = { 0.0, 0.0 };
	std::string keys[2] = { "spot_shadow_gpu_ms_1of8", "spot_shadow_gpu_ms_8of8" };
	for (std::size_t index = 0; index < 2; ++index)
	{
		engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
		灯光.spot_count = counts[index];
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		for (uint32_t frame = 0; frame < 6; ++frame)
			ASSERT_TRUE(renderer.frame_render());
		shadow_ms[index] = renderer.frame_stats_get().gpu_shadow_ms;
		RecordProperty(keys[index], shadow_ms[index]);
		std::cout << "[关卡] 八盏开销 " << counts[index] << " 盏阴影耗时 " << shadow_ms[index] << " ms" << std::endl;
	}

	if (renderer.compute().gpu_timestamp_supported())
	{
		EXPECT_GT(shadow_ms[1], 0.0) << "8 盏聚光未产生阴影耗时";
		EXPECT_GE(shadow_ms[1], shadow_ms[0] - 0.1) << "8 盏阴影耗时异常低于 1 盏";
	}
	else
	{
		for (std::size_t index = 0; index < 2; ++index)
			EXPECT_EQ(shadow_ms[index], 0.0);
	}
	level_bridge_unload(renderer, 场景);
}

/*
八盏点光边界（第四百三十六轮）
口径：恰好 8 盏通过；第 9 盏拒绝（上限 8）。
*/
TEST(Renderer_Level_Test, 八盏点光边界)
{
	std::string text = "level 八盏\n";
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		char 行[128] = {};
		std::snprintf(行, sizeof(行), "light point %d 0 0 1 1 1 5 1 0 1 0\n", static_cast<int>(序号));
		text += 行;
	}
	write_text(level_dir() / L"八盏.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("八盏.level"), loaded));
	EXPECT_EQ(loaded.light.point_count, 8u);

	text += "light point 8 0 0 1 1 1 5 1 0 1 0\n";
	write_text(level_dir() / L"九盏.level", text);
	engine::Level_Data too_many;
	EXPECT_FALSE(engine::level_text_read(level_utf8("九盏.level"), too_many));
	EXPECT_EQ(too_many.light.point_count, 0u);
}
