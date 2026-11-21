/*
渲染动画装配测试（方向十 · 收敛轮 861）
覆盖：关卡演员声明（`skeleton` / `anim`）→ 骨架与片段加载 → 调色板区间分配 → 逐帧写入；
      无片段演员按绑定姿态写入；骨架缺失 / 片段非法时整体回退并回收区间；卸载回收区间。
口径：经 `level_bridge_load` 真实走关卡文本与资产文件，不 mock 文件系统。
临时文件写在 `out/anim_assembly_test/`（ASCII 路径，避免宽窄字符转换干扰）。
*/
#include <gtest/gtest.h>

#include "测试辅助/渲染测试辅助.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"

using namespace byjy_render_test;

namespace
{
	//临时文件目录（相对可执行文件目录）
	const char* const TEST_DIR = "out/anim_assembly_test/";

	//写一个文本文件（目录自动创建）
	void write_text(const std::string& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(std::filesystem::path(TEST_DIR), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//两根骨骼的骨架 JSON：根 T(1,2,3) / 逆 T(-1,-2,-3)，子 T(0,1,0) / 逆 T(0,-1,0)
	std::string skeleton_json(void)
	{
		return R"({"bones":[
			{"name":"root","parent":-1,"bind_translation":[1,2,3],"inverse_bind":[
				1,0,0,0, 0,1,0,0, 0,0,1,0, -1,-2,-3,1]},
			{"name":"child","parent":0,"bind_translation":[0,1,0],"inverse_bind":[
				1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-1,0,1]}]})";
	}

	//根平移 0 → 10、时长 1 秒的片段 JSON
	std::string clip_json(void)
	{
		return R"({"name":"walk","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[0,0,0]},{"time":1.0,"value":[10,0,0]}]}]})";
	}

	//写骨架与片段资产，返回关卡文本内容
	std::string assets_write(bool with_clip)
	{
		write_text(std::string(TEST_DIR) + "skeleton.json", skeleton_json());
		write_text(std::string(TEST_DIR) + "walk.anim", clip_json());
		std::string text = "level 装配测试\ncamera position 0 1 3\nskeleton 甲 skeleton.json\n";
		//可选片段声明
		if (with_clip)
			text += "anim 甲 walk.anim\n";
		return text;
	}

	//读取调色板第 index 条矩阵（count 条）
	void palette_read(engine::Renderer& renderer, uint32_t offset, uint32_t index, float out[16])
	{
		ASSERT_TRUE(renderer.resource().animation_palette_readback(out, 1, offset + index));
	}

	//清理临时目录
	void directory_clean(void)
	{
		std::error_code code;
		std::filesystem::remove_all(std::filesystem::path(TEST_DIR), code);
	}
}

//渲染动画装配测试夹具：初始化渲染器并清空临时目录
class Renderer_Animation_Assembly_Test : public ::testing::Test
{
protected:
	void SetUp(void) override { directory_clean(); }
	void TearDown(void) override { directory_clean(); }
};

//关卡演员声明装配：骨架 / 片段加载、区间分配、逐帧按片段写入
TEST_F(Renderer_Animation_Assembly_Test, 关卡演员装配与按片段写入)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	//演员登记
	ASSERT_EQ(scene.actors.size(), 1u);
	const engine::Asset_Actor& actor = scene.actors[0];
	EXPECT_EQ(actor.name, "甲");
	EXPECT_EQ(actor.bone_count, 2u);
	EXPECT_EQ(actor.palette_offset, 0u);
	EXPECT_NE(actor.clip, nullptr);
	EXPECT_EQ(actor.clip->name, "walk");

	//0.5 秒：根世界平移 5，逆绑定 -1 后调色板平移 4 / -2 / -3
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.5f), 1u);
	float matrix[16] = {};
	palette_read(renderer, actor.palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 4.0f);
	EXPECT_FLOAT_EQ(matrix[13], -2.0f);
	EXPECT_FLOAT_EQ(matrix[14], -3.0f);
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.5f);

	//再推 0.75 秒：回绕到 0.25 秒，根世界平移 2.5
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.75f), 1u);
	palette_read(renderer, actor.palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 1.5f);

	//卸载回收区间：重新分配应回到偏移 0
	engine::level_bridge_unload(renderer, scene);
	EXPECT_TRUE(scene.actors.empty());
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(2, offset));
	EXPECT_EQ(offset, 0u);
	renderer.animation().actor_destroy(offset);
}

//无片段演员：按绑定姿态写入调色板
TEST_F(Renderer_Animation_Assembly_Test, 无片段演员按绑定姿态写入)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(false));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	ASSERT_EQ(scene.actors.size(), 1u);
	EXPECT_EQ(scene.actors[0].clip, nullptr);

	//步长非法按 0 处理，仍写绑定姿态
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, -1.0f), 1u);
	float matrix[16] = {};
	//根：绑定 T(1,2,3) × 逆 T(-1,-2,-3) = 单位平移
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 0.0f);
	EXPECT_FLOAT_EQ(matrix[13], 0.0f);
	EXPECT_FLOAT_EQ(matrix[14], 0.0f);
	//子：世界 T(1,3,3) × 逆 T(0,-1,0) → 平移 (1,2,3)
	palette_read(renderer, scene.actors[0].palette_offset, 1, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 1.0f);
	EXPECT_FLOAT_EQ(matrix[13], 2.0f);
	EXPECT_FLOAT_EQ(matrix[14], 3.0f);
	engine::level_bridge_unload(renderer, scene);
}

//骨架缺失：装载整体失败，关卡数据与场景清空，区间未泄漏
TEST_F(Renderer_Animation_Assembly_Test, 骨架缺失整体回退)
{
	write_text(std::string(TEST_DIR) + "level.txt",
		"level 装配失败\ncamera position 0 1 3\nskeleton 乙 missing.json\n");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	engine::Level_Data level;
	engine::Asset_Scene scene;
	EXPECT_FALSE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	//失败契约：输出清空
	EXPECT_TRUE(level.name.empty());
	EXPECT_TRUE(level.actors.empty());
	EXPECT_TRUE(scene.actors.empty());
	EXPECT_TRUE(scene.objects.empty());
	//区间未泄漏
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(2, offset));
	EXPECT_EQ(offset, 0u);
	renderer.animation().actor_destroy(offset);
}

//片段非法：骨架已加载也必须整体回退，区间不泄漏
TEST_F(Renderer_Animation_Assembly_Test, 片段非法整体回退)
{
	write_text(std::string(TEST_DIR) + "skeleton.json", skeleton_json());
	write_text(std::string(TEST_DIR) + "broken.anim", "{ not json");
	write_text(std::string(TEST_DIR) + "level.txt",
		"level 装配失败\ncamera position 0 1 3\nskeleton 丙 skeleton.json\nanim 丙 broken.anim\n");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	engine::Level_Data level;
	engine::Asset_Scene scene;
	EXPECT_FALSE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	EXPECT_TRUE(scene.actors.empty());
	uint32_t offset = UINT32_MAX;
	ASSERT_TRUE(renderer.animation().actor_create(2, offset));
	EXPECT_EQ(offset, 0u);
	renderer.animation().actor_destroy(offset);
}

//多演员：各自独立区间，更新只写有骨架的登记项
TEST_F(Renderer_Animation_Assembly_Test, 多演员独立区间)
{
	write_text(std::string(TEST_DIR) + "skeleton.json", skeleton_json());
	write_text(std::string(TEST_DIR) + "walk.anim", clip_json());
	write_text(std::string(TEST_DIR) + "level.txt",
		"level 多演员\ncamera position 0 1 3\n"
		"skeleton 甲 skeleton.json\nanim 甲 walk.anim\n"
		"skeleton 乙 skeleton.json\n");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	ASSERT_EQ(scene.actors.size(), 2u);
	//区间不重叠
	EXPECT_EQ(scene.actors[0].palette_offset, 0u);
	EXPECT_EQ(scene.actors[1].palette_offset, 2u);
	//两个演员都更新成功
	EXPECT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.5f), 2u);
	//第一个按片段（平移 4），第二个按绑定（平移 0）
	float matrix[16] = {};
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 4.0f);
	palette_read(renderer, scene.actors[1].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 0.0f);
	engine::level_bridge_unload(renderer, scene);
}

//演员片段淡入：中间权重混合，到时长后只留目标片段
TEST_F(Renderer_Animation_Assembly_Test, 演员片段淡入与结束)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	//第二片段：根平移 100 → 110
	write_text(std::string(TEST_DIR) + "run.anim",
		R"({"name":"run","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[100,0,0]},{"time":1.0,"value":[110,0,0]}]}]})");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	ASSERT_EQ(scene.actors.size(), 1u);

	//加载第二片段并启动 1 秒淡入
	std::shared_ptr<const engine::Animation_Clip> run;
	ASSERT_TRUE(renderer.animation().clip_load(std::string(TEST_DIR) + "run.anim", run));
	ASSERT_TRUE(engine::level_bridge_animation_transition(scene, "甲", run, 1.0f));
	EXPECT_EQ(scene.actors[0].blend_weight, 0.0f);

	//推进 0.5 秒：walk(5) 与 run(105) 取中 → 根世界 55，调色板平移 54
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.5f), 1u);
	EXPECT_FLOAT_EQ(scene.actors[0].blend_weight, 0.5f);
	float matrix[16] = {};
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 54.0f);

	//再推进 0.6 秒：权重到 1，源片段清空；run 时间回绕到 0.1 → 根世界 101，调色板平移 100
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.6f), 1u);
	EXPECT_EQ(scene.actors[0].blend_from, nullptr);
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 100.0f);
	engine::level_bridge_unload(renderer, scene);
}

//零时长切换：立即替换片段且不保留过渡源
TEST_F(Renderer_Animation_Assembly_Test, 零时长立即切换)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	write_text(std::string(TEST_DIR) + "run.anim",
		R"({"name":"run","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[100,0,0]},{"time":1.0,"value":[110,0,0]}]}]})");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	std::shared_ptr<const engine::Animation_Clip> run;
	ASSERT_TRUE(renderer.animation().clip_load(std::string(TEST_DIR) + "run.anim", run));
	ASSERT_TRUE(engine::level_bridge_animation_transition(scene, "甲", run, 0.0f));
	EXPECT_EQ(scene.actors[0].blend_from, nullptr);
	//推进 0.25 秒：run 时间 0.25 → 根世界 102.5，调色板平移 101.5
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.25f), 1u);
	float matrix[16] = {};
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 101.5f);
	engine::level_bridge_unload(renderer, scene);
}

//非法切换：未知演员、空片段、非有限时长均拒绝且不改动状态
TEST_F(Renderer_Animation_Assembly_Test, 非法切换拒绝)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	std::shared_ptr<const engine::Animation_Clip> 原片段 = scene.actors[0].clip;
	//未知演员
	EXPECT_FALSE(engine::level_bridge_animation_transition(scene, "乙", 原片段, 1.0f));
	//空片段
	EXPECT_FALSE(engine::level_bridge_animation_transition(scene, "甲", nullptr, 1.0f));
	//非有限时长
	EXPECT_FALSE(engine::level_bridge_animation_transition(scene, "甲", 原片段,
		std::numeric_limits<float>::infinity()));
	//状态未变
	EXPECT_EQ(scene.actors[0].clip, 原片段);
	EXPECT_EQ(scene.actors[0].blend_from, nullptr);
	engine::level_bridge_unload(renderer, scene);
}

//演员事件查询：循环跨末尾按尾段 → 头段，未知演员与无事件片段返回假
TEST_F(Renderer_Animation_Assembly_Test, 演员事件区间查询)
{
	//带事件的片段（根平移 0 → 10，事件在 0.05 与 0.95）
	write_text(std::string(TEST_DIR) + "skeleton.json", skeleton_json());
	write_text(std::string(TEST_DIR) + "event.anim",
		R"({"name":"evt","duration":1.0,"tracks":[{"bone":"root","channel":"translation",
			"keys":[{"time":0.0,"value":[0,0,0]},{"time":1.0,"value":[10,0,0]}]}],
			"events":[{"time":0.05,"name":"开场"},{"time":0.95,"name":"收束"}]})");
	write_text(std::string(TEST_DIR) + "level.txt",
		"level 事件\ncamera position 0 1 3\nskeleton 甲 skeleton.json\nanim 甲 event.anim\n");
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));
	ASSERT_EQ(scene.actors.size(), 1u);
	ASSERT_NE(scene.actors[0].clip, nullptr);
	ASSERT_EQ(scene.actors[0].clip->events.size(), 2u);

	//0.9 → 1.1 循环：先收束（0.95），再开场（0.05）
	std::vector<std::string> names;
	ASSERT_TRUE(engine::level_bridge_animation_events(scene, "甲", 0.9f, 1.1f, true, names));
	ASSERT_EQ(names.size(), 2u);
	EXPECT_EQ(names[0], "收束");
	EXPECT_EQ(names[1], "开场");
	//未知演员
	std::vector<std::string> unknown;
	EXPECT_FALSE(engine::level_bridge_animation_events(scene, "乙", 0.0f, 1.0f, false, unknown));
	EXPECT_TRUE(unknown.empty());
	engine::level_bridge_unload(renderer, scene);
}

//暂停：时间与调色板保持不动
TEST_F(Renderer_Animation_Assembly_Test, 暂停保持当前姿态)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));

	//先推进 0.5 秒：根世界 5，调色板平移 4
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.5f), 1u);
	float matrix[16] = {};
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 4.0f);
	//暂停后再推进：时间与画面不变
	ASSERT_TRUE(engine::level_bridge_animation_pause(scene, "甲", true));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.5f), 1u);
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.5f);
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 4.0f);
	//恢复后可继续推进
	ASSERT_TRUE(engine::level_bridge_animation_pause(scene, "甲", false));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.25f), 1u);
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.75f);
	engine::level_bridge_unload(renderer, scene);
}

//变速：倍率缩放推进；倍率 0 等价暂停
TEST_F(Renderer_Animation_Assembly_Test, 变速推进)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));

	//2 倍速：0.25 秒步长推进 0.5 秒
	ASSERT_TRUE(engine::level_bridge_animation_speed_set(scene, "甲", 2.0f));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.25f), 1u);
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.5f);
	//倍率 0：时间停止
	ASSERT_TRUE(engine::level_bridge_animation_speed_set(scene, "甲", 0.0f));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.5f), 1u);
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.5f);
	engine::level_bridge_unload(renderer, scene);
}

//时间跳转：循环回绕，调色板随新时间更新
TEST_F(Renderer_Animation_Assembly_Test, 时间跳转与回绕)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));

	//跳到 0.75 秒：根世界 7.5，调色板平移 6.5
	ASSERT_TRUE(engine::level_bridge_animation_time_set(scene, "甲", 0.75f));
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.0f), 1u);
	float matrix[16] = {};
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 6.5f);
	//跳 1.25 秒（循环）：回绕到 0.25 → 根世界 2.5，调色板平移 1.5
	ASSERT_TRUE(engine::level_bridge_animation_time_set(scene, "甲", 1.25f));
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.25f);
	ASSERT_EQ(engine::level_bridge_animation_update(renderer, scene, 0.0f), 1u);
	palette_read(renderer, scene.actors[0].palette_offset, 0, matrix);
	EXPECT_FLOAT_EQ(matrix[12], 1.5f);
	engine::level_bridge_unload(renderer, scene);
}

//非法播放控制：未知演员、非有限或越界参数均拒绝且不改状态
TEST_F(Renderer_Animation_Assembly_Test, 非法播放控制拒绝)
{
	write_text(std::string(TEST_DIR) + "level.txt", assets_write(true));
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Level_Data level;
	engine::Asset_Scene scene;
	ASSERT_TRUE(engine::level_bridge_load(renderer, level, scene, std::string(TEST_DIR) + "level.txt"));

	//未知演员
	EXPECT_FALSE(engine::level_bridge_animation_pause(scene, "乙", true));
	EXPECT_FALSE(engine::level_bridge_animation_speed_set(scene, "乙", 1.0f));
	EXPECT_FALSE(engine::level_bridge_animation_time_set(scene, "乙", 0.5f));
	//非有限 / 越界倍率
	EXPECT_FALSE(engine::level_bridge_animation_speed_set(scene, "甲",
		std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(engine::level_bridge_animation_speed_set(scene, "甲", -1.0f));
	EXPECT_FALSE(engine::level_bridge_animation_speed_set(scene, "甲", 9.0f));
	//非有限时间
	EXPECT_FALSE(engine::level_bridge_animation_time_set(scene, "甲",
		std::numeric_limits<float>::infinity()));
	//状态未变
	EXPECT_FLOAT_EQ(scene.actors[0].speed, 1.0f);
	EXPECT_FALSE(scene.actors[0].paused);
	EXPECT_FLOAT_EQ(scene.actors[0].time_seconds, 0.0f);
	engine::level_bridge_unload(renderer, scene);
}
