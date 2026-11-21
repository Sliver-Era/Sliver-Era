//对象池测试：覆盖带键构建、键写入、键判重、单/批量构建、卸载与索引复用、清空与预分配、哈希/排序两种定位模式
#include <gtest/gtest.h>

//获取对象池
#include "Engine/EngineFoundation/src/object/Object_Pool/对象池.h"

//派生测试对象（整数定位键）
struct Test_Object
{
	//定位键
	uint64_t key = 0;
	//附加字段，供排序投影校验
	uint64_t payload = 0;
};

//派生测试对象（字符串定位键）
struct Tagged_Object
{
	//定位键
	std::string tag{};
	//附加字段
	uint64_t payload = 0;
};

//整数键投影器
static std::function<uint64_t&(Test_Object&)> int_key_projector(void)
{
	return [](Test_Object& object) -> uint64_t& { return object.key; };
}

//字符串键投影器
static std::function<std::string&(Tagged_Object&)> string_key_projector(void)
{
	return [](Tagged_Object& object) -> std::string& { return object.tag; };
}

//对象池测试夹具
class Object_Pool_Test : public ::testing::Test
{
protected:
	//整数键对象池
	engine::Object_Pool<Test_Object> pool{ int_key_projector() };
};

//构建：键写入对象且可按键查找
TEST_F(Object_Pool_Test, 构建后键写入且可查找)
{
	//构建目标对象
	const uint64_t index = pool.build(42);
	//槽位索引应从 0 起
	EXPECT_EQ(index, 0u);
	//按键查找
	const auto it = pool.find(42);
	ASSERT_NE(it, pool.end());
	//键应已写入对象
	EXPECT_EQ(it->key, 42u);
}

//构建：键 0 合法可用
TEST_F(Object_Pool_Test, 键零可用)
{
	//编号 0 不再是保留值
	EXPECT_EQ(pool.build(0), 0u);
	EXPECT_NE(pool.find(0), pool.end());
	EXPECT_EQ(pool.size(), 1u);
}

//构建：重复键被拒绝
TEST_F(Object_Pool_Test, 重复键被拒绝)
{
	//首次构建成功
	pool.build(7);
	//重复键返回失败哨兵
	EXPECT_EQ(pool.build(7), (std::numeric_limits<uint64_t>::max)());
	//仅保留一个有效对象
	EXPECT_EQ(pool.size(), 1u);
}

//构建：外部实参中的旧键被对象池覆写
TEST_F(Object_Pool_Test, 外部实参旧键被覆写)
{
	//外部实参携带错误键
	Test_Object object{};
	object.key = 999;
	object.payload = 5;
	//以目标键构建
	pool.build(11, object);
	//按键查找
	const auto it = pool.find(11);
	ASSERT_NE(it, pool.end());
	//键被覆写为目标键
	EXPECT_EQ(it->key, 11u);
	//其余字段保留
	EXPECT_EQ(it->payload, 5u);
	//旧键不可查找
	EXPECT_EQ(pool.find(999), pool.end());
}

//批量构建：仅提供键
TEST_F(Object_Pool_Test, 批量构建仅提供键)
{
	//批量构建
	const std::vector<uint64_t> indices = pool.build(std::vector<uint64_t>{ 1, 2, 3 });
	//返回索引数量应与键数量一致
	EXPECT_EQ(indices.size(), 3u);
	//有效对象数量一致
	EXPECT_EQ(pool.size(), 3u);
	//逐个键均可查找
	EXPECT_NE(pool.find(1), pool.end());
	EXPECT_NE(pool.find(2), pool.end());
	EXPECT_NE(pool.find(3), pool.end());
}

//批量构建：键与实参同步提供
TEST_F(Object_Pool_Test, 批量构建同步提供实参)
{
	//两个实参携带错误键与不同负载
	std::vector<Test_Object> objects(2);
	objects[0].key = 100;
	objects[0].payload = 10;
	objects[1].key = 200;
	objects[1].payload = 20;
	//批量构建
	const std::vector<uint64_t> indices = pool.build(std::vector<uint64_t>{ 5, 6 }, objects);
	//返回索引数量应与键数量一致
	EXPECT_EQ(indices.size(), 2u);
	//键被覆写且负载保留
	ASSERT_NE(pool.find(5), pool.end());
	EXPECT_EQ(pool.find(5)->key, 5u);
	EXPECT_EQ(pool.find(5)->payload, 10u);
	ASSERT_NE(pool.find(6), pool.end());
	EXPECT_EQ(pool.find(6)->key, 6u);
	EXPECT_EQ(pool.find(6)->payload, 20u);
}

//批量构建：重复键被跳过
TEST_F(Object_Pool_Test, 批量构建跳过重复键)
{
	//批量键含重复
	const std::vector<uint64_t> indices = pool.build(std::vector<uint64_t>{ 1, 1, 2 });
	//仅成功构建两个
	EXPECT_EQ(indices.size(), 2u);
	EXPECT_EQ(pool.size(), 2u);
}

//卸载：目标不存在时静默返回
TEST_F(Object_Pool_Test, 尝试卸载不存在键返回假)
{
	//空池尝试卸载
	EXPECT_FALSE(pool.try_unload(1));
	//卸载接口不抛异常
	EXPECT_NO_THROW(pool.unload(1));
	EXPECT_EQ(pool.size(), 0u);
}

//卸载：单对象卸载后不可查找
TEST_F(Object_Pool_Test, 单对象卸载后不可查找)
{
	//构建两个对象
	pool.build(1);
	pool.build(2);
	//卸载第一个
	EXPECT_TRUE(pool.try_unload(1));
	//已卸载对象不可查找
	EXPECT_EQ(pool.find(1), pool.end());
	//其余对象不受影响
	EXPECT_NE(pool.find(2), pool.end());
	//有效数量减一
	EXPECT_EQ(pool.size(), 1u);
}

//卸载：批量卸载
TEST_F(Object_Pool_Test, 批量卸载)
{
	//构建四个对象
	pool.build(std::vector<uint64_t>{ 1, 2, 3, 4 });
	//批量卸载中间两个
	pool.unload(std::vector<uint64_t>{ 2, 3 });
	//已卸载对象不可查找
	EXPECT_EQ(pool.find(2), pool.end());
	EXPECT_EQ(pool.find(3), pool.end());
	//其余对象不受影响
	EXPECT_NE(pool.find(1), pool.end());
	EXPECT_NE(pool.find(4), pool.end());
	//有效数量正确
	EXPECT_EQ(pool.size(), 2u);
}

//索引复用：卸载后新建复用被回收的槽位
TEST_F(Object_Pool_Test, 卸载后新建复用索引)
{
	//构建两个对象
	pool.build(1);
	pool.build(2);
	//卸载首个对象
	pool.unload(1);
	//新建对象应复用被回收的槽位
	const uint64_t reused = pool.build(3);
	//复用索引应落在原有槽位范围内
	EXPECT_LT(reused, 2u);
	//新对象可查找
	EXPECT_NE(pool.find(3), pool.end());
}

//清空：容量与索引分配一并复位
TEST_F(Object_Pool_Test, 清空后索引从头分配)
{
	//构建三个对象
	pool.build(std::vector<uint64_t>{ 1, 2, 3 });
	//清空对象池
	pool.clear();
	//有效数量与容量归零
	EXPECT_EQ(pool.size(), 0u);
	EXPECT_EQ(pool.capacity(), 0u);
	//清空后首个对象仍从索引 0 开始
	EXPECT_EQ(pool.build(9), 0u);
}

//预分配：预留槽位可被后续构建复用
TEST_F(Object_Pool_Test, 预分配槽位可复用)
{
	//预分配八個槽位
	EXPECT_TRUE(pool.reserve(8));
	//容量为八，有效数量为零
	EXPECT_EQ(pool.capacity(), 8u);
	EXPECT_EQ(pool.size(), 0u);
	//构建对象应落在预留槽位内
	const uint64_t index = pool.build(1);
	EXPECT_LT(index, 8u);
	//对象可查找
	EXPECT_NE(pool.find(1), pool.end());
	//有效数量为一
	EXPECT_EQ(pool.size(), 1u);
	//非空时拒绝再次预分配
	EXPECT_FALSE(pool.reserve(8));
}

//排序模式：按键升序迭代
TEST_F(Object_Pool_Test, 排序模式按键升序迭代)
{
	//乱序构建
	pool.build(3);
	pool.build(1);
	pool.build(2);
	//切换排序模式
	pool.mode_set(engine::Object_Order::Sorted, std::ranges::less{});
	//有效区应按键升序排列
	std::vector<uint64_t> keys{};
	for (const Test_Object& object : pool)
		keys.push_back(object.key);
	ASSERT_EQ(keys.size(), 3u);
	EXPECT_EQ(keys[0], 1u);
	EXPECT_EQ(keys[1], 2u);
	EXPECT_EQ(keys[2], 3u);
}

//排序模式：按键降序迭代
TEST_F(Object_Pool_Test, 排序模式按键降序迭代)
{
	//乱序构建
	pool.build(3);
	pool.build(1);
	pool.build(2);
	//切换排序模式并指定降序
	pool.mode_set(engine::Object_Order::Sorted, std::ranges::greater{});
	//有效区应按键降序排列
	std::vector<uint64_t> keys{};
	for (const Test_Object& object : pool)
		keys.push_back(object.key);
	ASSERT_EQ(keys.size(), 3u);
	EXPECT_EQ(keys[0], 3u);
	EXPECT_EQ(keys[1], 2u);
	EXPECT_EQ(keys[2], 1u);
}

//排序模式：查找与新建可用
TEST_F(Object_Pool_Test, 排序模式查找与新建)
{
	//构建两个对象
	pool.build(2);
	pool.build(4);
	//切换排序模式
	pool.mode_set(engine::Object_Order::Sorted, std::ranges::less{});
	//原有键可查找
	EXPECT_NE(pool.find(2), pool.end());
	//排序模式下新建对象
	pool.build(3);
	//新建对象可查找
	EXPECT_NE(pool.find(3), pool.end());
	//有效数量正确
	EXPECT_EQ(pool.size(), 3u);
}

//排序模式：卸载后不可查找
TEST_F(Object_Pool_Test, 排序模式卸载后不可查找)
{
	//构建三个对象
	pool.build(std::vector<uint64_t>{ 1, 2, 3 });
	//切换排序模式
	pool.mode_set(engine::Object_Order::Sorted, std::ranges::less{});
	//卸载中间对象
	pool.unload(2);
	//已卸载对象不可查找
	EXPECT_EQ(pool.find(2), pool.end());
	//其余对象可查找
	EXPECT_NE(pool.find(1), pool.end());
	EXPECT_NE(pool.find(3), pool.end());
}

//定位模式：排序切回哈希后仍可查找
TEST_F(Object_Pool_Test, 排序切回哈希后可查找)
{
	//构建两个对象
	pool.build(1);
	pool.build(2);
	//切换排序模式
	pool.mode_set(engine::Object_Order::Sorted, std::ranges::less{});
	//切回哈希模式
	pool.mode_set(engine::Object_Order::Unorder);
	//两个键均可查找
	EXPECT_NE(pool.find(1), pool.end());
	EXPECT_NE(pool.find(2), pool.end());
}

//字符串键：构建、查找与卸载
TEST_F(Object_Pool_Test, 字符串键可用)
{
	//字符串键对象池
	engine::Object_Pool<Tagged_Object, std::string> tagged_pool{ string_key_projector() };
	//构建两个对象
	tagged_pool.build("alpha");
	tagged_pool.build("beta");
	//按键查找
	ASSERT_NE(tagged_pool.find("alpha"), tagged_pool.end());
	EXPECT_EQ(tagged_pool.find("beta")->tag, "beta");
	//卸载后不可查找
	tagged_pool.unload("alpha");
	EXPECT_EQ(tagged_pool.find("alpha"), tagged_pool.end());
}

//大批量：构建与卸载计数正确
TEST_F(Object_Pool_Test, 大批量构建卸载计数正确)
{
	//批量键
	std::vector<uint64_t> keys{};
	keys.reserve(256);
	for (uint64_t key = 0; key < 256; key++)
		keys.push_back(key);
	//批量构建
	pool.build(keys);
	EXPECT_EQ(pool.size(), 256u);
	//卸载偶数键
	std::vector<uint64_t> removed{};
	for (uint64_t key = 0; key < 256; key += 2)
		removed.push_back(key);
	pool.unload(removed);
	//数量正确
	EXPECT_EQ(pool.size(), 128u);
	//偶数键不可查找，奇数键仍在
	for (uint64_t key = 0; key < 256; key++)
	{
		if (key % 2 == 0)
			EXPECT_EQ(pool.find(key), pool.end());
		else
			EXPECT_NE(pool.find(key), pool.end());
	}
}
