/*
渲染聚光阴影基础测试（2026-11-21 自「渲染聚光阴影测试.cpp」拆出）
覆盖：八盏聚光阴影（前向 / Cluster）、MSAA 组合、软度开销、偏置自校准开关与生效、
      按需重绘、偏置画面差异、与方向光阴影共存。
*/

#include "渲染聚光阴影测试辅助.h"

/*
Cluster 路径八盏聚光阴影（第五百二十五轮）
口径：八盏关卡在 Cluster 开启下，关闭 / 开启八盏阴影差异 > 100 像素且瓦片数为 8；
随后关闭 Cluster 再出图，应与此前 Cluster 开启的有影图逐字节一致（两条路径同结果）。
*/
TEST(Renderer_Level_Test, 关卡Cluster八盏聚光阴影)
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

	//Cluster 开启：关闭聚光阴影出参照图
	ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//Cluster 开启：八盏全投影
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 簇有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(簇有影.data(), static_cast<uint32_t>(簇有影.size())));
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);
	const uint32_t 簇差异 = changed_pixels(无影, 簇有影);
	std::cout << "[关卡] Cluster 八盏聚光阴影差异 " << 簇差异 << std::endl;
	EXPECT_GT(簇差异, 100u) << "Cluster 路径八盏聚光阴影未生效";

	//关闭 Cluster：前向路径应得到同一结果
	ASSERT_TRUE(renderer.compute().cluster_enable_set(false));
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 前向有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(前向有影.data(), static_cast<uint32_t>(前向有影.size())));
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);
	const uint32_t 路径差异 = changed_pixels(簇有影, 前向有影);
	std::cout << "[关卡] 八盏阴影前后向路径差异 " << 路径差异 << std::endl;
	EXPECT_EQ(路径差异, 0u) << "Cluster 与前向路径的八盏阴影结果不一致";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光阴影与 MSAA 组合（第五百二十六轮）
口径：1× 与 4× 采样下八盏阴影均生效（关闭 / 开启差异 > 100 且瓦片 8）；
4× 档追加 Cluster 开 / 关两态，画面逐字节一致（与 278 轮方向光组合矩阵同口径）。
*/
TEST(Renderer_Level_Test, 八盏聚光阴影与MSAA组合)
{
	write_cube_assets();
	write_eight_spot_level();

	const uint32_t msaa_list[2] = { 1u, 4u };
	for (uint32_t msaa_index = 0; msaa_index < 2; ++msaa_index)
	{
		engine::Renderer_Config config;
		config.width = LEVEL_WIDTH;
		config.height = LEVEL_HEIGHT;
		config.hidden = true;
		config.msaa_samples = msaa_list[msaa_index];
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.compute().readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";

		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

		//关闭聚光阴影：参照图
		engine::Light_Data 灯光 = 关卡.light;
			灯光.spot_shadow_enabled = 0;
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

		//八盏全投影
			灯光.spot_shadow_enabled = 1;
			灯光.spot_shadow_all = 1;
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(有影.data(), static_cast<uint32_t>(有影.size())));
		EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);
		const uint32_t 差异 = changed_pixels(无影, 有影);
		std::cout << "[关卡] 八盏阴影 " << msaa_list[msaa_index] << "x 差异 " << 差异 << std::endl;
		EXPECT_GT(差异, 100u) << "MSAA " << msaa_list[msaa_index] << "x 下八盏聚光阴影未生效";

		//4× 档：Cluster 开 / 关两态逐字节一致
		if (msaa_list[msaa_index] == 4u && renderer.compute().cluster_supported())
		{
			ASSERT_TRUE(renderer.compute().cluster_enable_set(true));
			EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
			std::vector<uint8_t> 簇有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
			ASSERT_TRUE(renderer.frame_render(簇有影.data(), static_cast<uint32_t>(簇有影.size())));
			ASSERT_TRUE(renderer.compute().cluster_enable_set(false));
			EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
			std::vector<uint8_t> 前向有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
			ASSERT_TRUE(renderer.frame_render(前向有影.data(), static_cast<uint32_t>(前向有影.size())));
			const uint32_t 开关差异 = changed_pixels(簇有影, 前向有影);
			std::cout << "[关卡] 4x 下 Cluster 两态差异 " << 开关差异 << std::endl;
			EXPECT_EQ(开关差异, 0u) << "4x 下 Cluster 开关改变了八盏阴影画面";
		}
		level_bridge_unload(renderer, 场景);
	}
}

/*
八盏聚光阴影软度开销（第五百二十七轮）
口径：1920×1080 ＋ 八盏聚光全投影；三态（关闭阴影 / 软度 0 单点 / 软度 1 的 7×7 邻域）
每态预热 2 帧后取样 5 帧，记录场景 GPU 帧耗时均值（设备不支持时间戳时三态均为 0）。
只记录数据与弱断言（软度 1 不低于软度 0 减 0.1 ms），不做跨机器阈值断言。
*/
TEST(Renderer_Level_Test, 八盏聚光软度开销)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = 1920;
	config.height = 1080;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//三态软度（负值表示关闭阴影）
	const float 软度值[3] = { -1.0f, 0.0f, 1.0f };
	const char* 记录键[3] = { "spot_softness_gpu_ms_off", "spot_softness_gpu_ms_hard",
		"spot_softness_gpu_ms_soft" };
	double 帧耗时[3] = { 0.0, 0.0, 0.0 };
	for (std::size_t index = 0; index < 3; ++index)
	{
		//关闭态：直接应用无阴影
		engine::Light_Data 灯光 = 关卡.light;
		if (软度值[index] < 0.0f)
			灯光.spot_shadow_enabled = 0;
		else
		{
			灯光.spot_shadow_enabled = 1;
			灯光.spot_shadow_all = 1;
		}
		renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
		//scene_apply 会按关卡字段重置质量参数，故在其后显式设置软度
		if (软度值[index] >= 0.0f)
			renderer.scene().spot_shadow_params_set(0.010f, 软度值[index], 1.0f);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		//预热 2 帧
		for (uint32_t frame = 0; frame < 2; ++frame)
			ASSERT_TRUE(renderer.frame_render());
		//取样 5 帧取均值
		double 合计 = 0.0;
		for (uint32_t frame = 0; frame < 5; ++frame)
		{
			ASSERT_TRUE(renderer.frame_render());
			合计 += renderer.frame_stats_get().gpu_frame_ms;
		}
		帧耗时[index] = 合计 / 5.0;
		RecordProperty(记录键[index], 帧耗时[index]);
		std::cout << "[关卡] 软度开销 " << 记录键[index] << " " << 帧耗时[index] << " ms" << std::endl;
	}

	if (renderer.compute().gpu_timestamp_supported())
	{
		//三态均应产生非零帧耗时
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_GT(帧耗时[index], 0.0) << "软度开销态 " << index << " 未产生帧耗时";
		//软度 1 不应显著低于软度 0（容差 0.1 ms）
		EXPECT_GE(帧耗时[2], 帧耗时[1] - 0.1) << "软度 1 帧耗时异常低于软度 0";
	}
	else
	{
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_EQ(帧耗时[index], 0.0);
	}
	level_bridge_unload(renderer, 场景);
}

/*
关卡偏置自校准开关往返（第五百二十九轮）
口径：`light spot_shadow_bias_auto 1` 读回为 1、写出再读回一致；缺省为 0；
非法值（2 / 非数字 / 缺值）整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡偏置自校准开关往返)
{
	write_text(level_dir() / L"偏置自校准开关.level",
		"level 偏置自校准开关\n"
		"light spot_shadow_bias_auto 1\n");

	engine::Level_Data 读取;
	ASSERT_TRUE(engine::level_text_read(level_utf8("偏置自校准开关.level"), 读取));
	EXPECT_EQ(读取.light.spot_shadow_bias_auto, 1);

	//写出再读回一致
	ASSERT_TRUE(engine::level_text_write(读取, level_utf8("偏置自校准开关_回写.level")));
	engine::Level_Data 回读;
	ASSERT_TRUE(engine::level_text_read(level_utf8("偏置自校准开关_回写.level"), 回读));
	EXPECT_EQ(回读.light.spot_shadow_bias_auto, 1);

	//缺省为 0
	write_text(level_dir() / L"偏置自校准缺省.level", "level 偏置自校准缺省\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("偏置自校准缺省.level"), 缺省));
	EXPECT_EQ(缺省.light.spot_shadow_bias_auto, 0);

	//非法值整份拒绝
	const char* 非法值[3] = { "2", "x", "" };
	for (const char* 值 : 非法值)
	{
		std::string 文本 = std::string("level 偏置自校准非法\nlight spot_shadow_bias_auto ") + 值 + "\n";
		write_text(level_dir() / L"偏置自校准非法.level", 文本);
		engine::Level_Data 拒绝;
		EXPECT_FALSE(engine::level_text_read(level_utf8("偏置自校准非法.level"), 拒绝))
			<< "非法值未拒绝：" << 值;
	}
}

/*
关卡偏置自校准生效（第五百二十九轮）
口径：八盏关卡追加 `light spot_shadow_bias_auto 1` 后装载并应用，门面为真、
逐瓦片偏置按 9 / 8.9 ≈ 0.010112（八盏半径均为 9）；关闭后恢复全局 0.010。
*/
TEST(Renderer_Level_Test, 关卡偏置自校准生效)
{
	write_cube_assets();
	write_eight_spot_level();
	//读取原关卡文本并追加自校准行（独立文件，避免影响其它八盏用例）
	std::string 原文;
	{
		std::ifstream 输入(level_dir() / L"八盏聚光.level", std::ios::binary);
		std::string 行;
		while (std::getline(输入, 行))
			原文 += 行 + "\n";
	}
	write_text(level_dir() / L"八盏聚光校准.level", 原文 + "light spot_shadow_bias_auto 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光校准.level"), false));
	EXPECT_EQ(关卡.light.spot_shadow_bias_auto, 1);

	//应用场景后门面应为开启
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_TRUE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	float 偏置[8] = {};
	renderer.scene().spot_shadow_bias_tiles_get(偏置);
	for (std::size_t index = 0; index < 8; ++index)
		EXPECT_NEAR(偏置[index], 0.010f * 9.0f / 8.9f, 1e-4f) << "瓦片 " << index;

	//关闭自校准：恢复全局 0.010
	renderer.scene().spot_shadow_bias_auto_set(false);
	EXPECT_FALSE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	renderer.scene().spot_shadow_bias_tiles_get(偏置);
	for (std::size_t index = 0; index < 8; ++index)
		EXPECT_NEAR(偏置[index], 0.010f, 1e-5f) << "瓦片 " << index;
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光阴影按需重绘（第五百三十三轮）
口径：八盏全投影时静态连续 4 帧阴影 pass 计数不增长；
把第 7 盏聚光灯位置移动后签名变化 → 重绘一次（与 486 轮单盏口径同规则，扩到 8 瓦片）。
*/
TEST(Renderer_Level_Test, 八盏聚光阴影按需重绘)
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

	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);

	//静态连续渲染：阴影 pass 不应再增长
	const uint64_t 静态后 = renderer.scene().shadow_pass_count();
	for (int frame = 0; frame < 4; ++frame)
	{
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		ASSERT_TRUE(renderer.frame_render());
	}
	EXPECT_EQ(renderer.scene().shadow_pass_count(), 静态后) << "八盏静态场景仍重绘阴影";

	//移动第 7 盏聚光灯（瓦片 6）：签名变化 → 重绘一次
	灯光.spot_position[6][0] += 1.0f;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.scene().shadow_pass_count(), 静态后) << "八盏中单盏移动后未重绘阴影";
	level_bridge_unload(renderer, 场景);
}

/*
八盏偏置自校准画面差异（第五百三十四轮）
口径：八盏全投影时，开启偏置自校准后逐瓦片偏置均为 0.010 × 9 / 8.9 ≈ 0.010112（统计口径）；
八盏同半径时该差异仅 1.1%，小画面不可见（差异只记录且上限 20 像素）；
关闭后与默认图逐字节一致（默认关闭 → 画面与引入前一致）。
*/
TEST(Renderer_Level_Test, 八盏偏置自校准画面差异)
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

	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 默认图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(默认图.data(), static_cast<uint32_t>(默认图.size())));

	//开启自校准：逐瓦片偏置 ≈ 0.010112，画面应有边界偏移差异
	renderer.scene().spot_shadow_bias_auto_set(true);
	EXPECT_TRUE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 校准图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(校准图.data(), static_cast<uint32_t>(校准图.size())));
	float 偏置[8] = {};
	renderer.scene().spot_shadow_bias_tiles_get(偏置);
	for (std::size_t index = 0; index < 8; ++index)
		EXPECT_NEAR(偏置[index], 0.010f * 9.0f / 8.9f, 1e-4f) << "瓦片 " << index;
	const uint32_t 校准差异 = changed_pixels(默认图, 校准图);
	std::cout << "[关卡] 八盏偏置自校准画面差异 " << 校准差异 << std::endl;
	/*
	八盏半径均为 9 → 归一化偏置只变化 1.1%（0.010 → 0.010112），128×72 画面下不可见，
	故此差异只记录、不作下限断言；统计口径由上面的逐瓦片偏置断言覆盖。
	*/
	EXPECT_LE(校准差异, 20u) << "开启偏置自校准改变了过多像素（应仅为边界抖动）";

	//关闭自校准：与默认图逐字节一致
	renderer.scene().spot_shadow_bias_auto_set(false);
	EXPECT_FALSE(renderer.scene().spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 复原图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(复原图.data(), static_cast<uint32_t>(复原图.size())));
	EXPECT_EQ(changed_pixels(默认图, 复原图), 0u) << "关闭偏置自校准后画面未复原";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光与方向光阴影共存（第五百三十九轮）
口径：八盏全投影 ＋ 方向光阴影同时开启；三态对照（共存 / 仅方向光 / 仅八盏聚光），
共存图与「仅聚光」差异 > 100（聚光生效）、与「仅方向光」差异 > 0（方向光生效），两套阴影互不干扰。
*/
TEST(Renderer_Level_Test, 八盏聚光与方向光阴影共存)
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

	//方向光调高并斜射（原关卡 0.1 且正下，方向光阴影与聚光阴影完全重叠而不可见）
	engine::Light_Data 灯光 = 关卡.light;
		灯光.intensity = 0.6f;
		灯光.direction[0] = 0.6f;
		灯光.direction[1] = -0.62f;
		灯光.direction[2] = -0.24f;
		for (std::size_t 通道 = 0; 通道 < 3; ++通道)
			灯光.color[通道] = 0.4f;
		灯光.shadow_enabled = 1;
		灯光.shadow_bias = 0.004f;
		灯光.shadow_softness = 0.35f;
		灯光.shadow_strength = 0.8f;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;

	//共存：方向光 ＋ 八盏聚光
	renderer.scene().scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 共存(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(共存.data(), static_cast<uint32_t>(共存.size())));

	//仅方向光（关闭聚光阴影）
	engine::Light_Data 仅方向 = 灯光;
		仅方向.spot_shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 仅方向, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 仅方向图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(仅方向图.data(), static_cast<uint32_t>(仅方向图.size())));

	//仅八盏聚光（关闭方向光阴影）
	engine::Light_Data 仅聚光 = 灯光;
		仅聚光.shadow_enabled = 0;
	renderer.scene().scene_apply(关卡.camera, 仅聚光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 仅聚光图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(仅聚光图.data(), static_cast<uint32_t>(仅聚光图.size())));

	//聚光生效
	const uint32_t 聚光差异 = changed_pixels(共存, 仅方向图);
	std::cout << "[关卡] 八盏共存 聚光贡献 " << 聚光差异 << std::endl;
	EXPECT_GT(聚光差异, 100u) << "共存态中八盏聚光阴影未生效";
	//方向光生效（八盏场景方向光阴影覆盖较弱，只要求非零）
	const uint32_t 方向差异 = changed_pixels(共存, 仅聚光图);
	std::cout << "[关卡] 八盏共存 方向光贡献 " << 方向差异 << std::endl;
	EXPECT_GT(方向差异, 0u) << "共存态中方向光阴影未生效";
	level_bridge_unload(renderer, 场景);
}
