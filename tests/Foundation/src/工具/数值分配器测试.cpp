//数值池测试：覆盖升序/降序/随机分配、回收复用顺序、回收归属校验、禁用与解禁、容量与重置、游标回绕与耗尽
#include <gtest/gtest.h>

//获取数值池
#include "Engine/EngineFoundation/src/detail/class/Number_Pool/数值池.h"

//取值并断言有效
static uint64_t value_take(engine::Number_Pool<uint64_t>& pool)
{
	//获取可用数值
	std::optional<uint64_t> value = pool.get();
	//分配失败立即暴露
	EXPECT_TRUE(value.has_value());
	return value.value_or((std::numeric_limits<uint64_t>::max)());
}

//数值池测试夹具
class Number_Pool_Test : public ::testing::Test
{
protected:
	//被测数值池
	engine::Number_Pool<uint64_t> pool;
};

//默认分配：从零开始逐个递增
TEST_F(Number_Pool_Test, 默认从零开始递增)
{
	EXPECT_EQ(value_take(pool), 0u);
	EXPECT_EQ(value_take(pool), 1u);
	EXPECT_EQ(value_take(pool), 2u);
}

//设置起点：从起点开始递增
TEST_F(Number_Pool_Test, 设置起点后从起点递增)
{
	//设置起点
	pool.set(100);
	EXPECT_EQ(value_take(pool), 100u);
	EXPECT_EQ(value_take(pool), 101u);
}

//设置起点：后设值覆盖前设值
TEST_F(Number_Pool_Test, 起点可被覆盖)
{
	pool.set(10);
	pool.set(50);
	EXPECT_EQ(value_take(pool), 50u);
}

//回收复用：已回收数值优先分配
TEST_F(Number_Pool_Test, 回收数值优先分配)
{
	//取出三个数值
	value_take(pool);
	value_take(pool);
	const uint64_t second = value_take(pool);
	//回收中间数值
	EXPECT_TRUE(pool.recycle(second));
	//优先复用被回收数值
	EXPECT_EQ(value_take(pool), second);
	//回收池耗尽后继续按游标分配
	EXPECT_EQ(value_take(pool), 3u);
}

//回收顺序：后进先出
TEST_F(Number_Pool_Test, 回收后进先出)
{
	//先取出再回收，满足归属校验
	const uint64_t first = value_take(pool);
	const uint64_t second = value_take(pool);
	pool.recycle(first);
	pool.recycle(second);
	//后回收的先取回
	EXPECT_EQ(value_take(pool), second);
	EXPECT_EQ(value_take(pool), first);
}

//回收顺序：先进先出
TEST_F(Number_Pool_Test, 回收先进先出)
{
	//切换复用顺序
	pool.set(engine::Num_Reuse_Order::FIFO);
	//先取出再回收
	const uint64_t first = value_take(pool);
	const uint64_t second = value_take(pool);
	pool.recycle(first);
	pool.recycle(second);
	//先回收的先取回
	EXPECT_EQ(value_take(pool), first);
	EXPECT_EQ(value_take(pool), second);
}

//回收校验：重复回收被拒绝
TEST_F(Number_Pool_Test, 重复回收被拒绝)
{
	//取出并回收
	const uint64_t value = value_take(pool);
	EXPECT_TRUE(pool.recycle(value));
	//二次回收同一数值失败
	EXPECT_FALSE(pool.recycle(value));
	//回收池中只保留一份
	EXPECT_EQ(value_take(pool), value);
}

//回收校验：未分配数值回收被拒绝
TEST_F(Number_Pool_Test, 未分配数值回收被拒绝)
{
	//直接回收未分配数值
	EXPECT_FALSE(pool.recycle(999u));
	//分配计数不受影响
	EXPECT_EQ(pool.counts(), 0u);
}

//批量回收：逐个回收并保持后进先出
TEST_F(Number_Pool_Test, 批量回收保持顺序)
{
	//取出三个数值
	const uint64_t first = value_take(pool);
	const uint64_t second = value_take(pool);
	const uint64_t third = value_take(pool);
	//批量回收
	pool.recycle(std::vector<uint64_t>{ first, second, third });
	//后进先出
	EXPECT_EQ(value_take(pool), third);
	EXPECT_EQ(value_take(pool), second);
	EXPECT_EQ(value_take(pool), first);
}

//计数：分配与回收同步变化
TEST_F(Number_Pool_Test, 分配计数随回收变化)
{
	//连续分配三个
	value_take(pool);
	const uint64_t second = value_take(pool);
	value_take(pool);
	EXPECT_EQ(pool.counts(), 3u);
	//回收一个
	pool.recycle(second);
	EXPECT_EQ(pool.counts(), 2u);
}

//禁用：禁用值不会被分配
TEST_F(Number_Pool_Test, 禁用值不被分配)
{
	//禁用数值 5
	EXPECT_TRUE(pool.ban(5));
	//连续分配六十四次
	for (int time = 0; time < 64; time++)
	{
		const uint64_t value = value_take(pool);
		EXPECT_NE(value, 5u);
	}
	//重复禁用被拒绝
	EXPECT_FALSE(pool.ban(5));
}

//解禁：解禁后可正常分配
TEST_F(Number_Pool_Test, 解禁后可分配)
{
	//禁用并解禁
	EXPECT_TRUE(pool.ban(0));
	EXPECT_TRUE(pool.unban(0));
	//解禁后首个数值可用
	EXPECT_EQ(value_take(pool), 0u);
	//重复解禁被拒绝
	EXPECT_FALSE(pool.unban(0));
}

//容量：禁用与重置同步变化
TEST_F(Number_Pool_Test, 容量随禁用与重置变化)
{
	//八位数值池容量为类型最大值
	engine::Number_Pool<uint8_t> byte_pool;
	EXPECT_EQ(byte_pool.capacity(), 255u);
	//禁用后容量减一
	EXPECT_TRUE(byte_pool.ban(static_cast<uint8_t>(5)));
	EXPECT_EQ(byte_pool.capacity(), 254u);
	//解禁后容量恢复
	EXPECT_TRUE(byte_pool.unban(static_cast<uint8_t>(5)));
	EXPECT_EQ(byte_pool.capacity(), 255u);
	//重置后容量保持初始值
	engine::Number_Pool<uint8_t> reset_pool;
	reset_pool.ban(static_cast<uint8_t>(5));
	reset_pool.reset();
	EXPECT_EQ(reset_pool.capacity(), 255u);
}

//重置：回收与禁用记录一并清空
TEST_F(Number_Pool_Test, 重置清空分配记录)
{
	//取出并回收一个数值
	const uint64_t value = value_take(pool);
	pool.recycle(value);
	//禁用另一个数值
	pool.ban(9);
	//重置数值池
	pool.reset();
	//计数归零
	EXPECT_EQ(pool.counts(), 0u);
	//游标回到最小值
	EXPECT_EQ(value_take(pool), 0u);
	//禁用记录已清空
	EXPECT_TRUE(pool.unban(9) == false);
}

//升序回绕：八位数值到最大值后回到最小值
TEST_F(Number_Pool_Test, 升序游标回绕)
{
	//八位数值池
	engine::Number_Pool<uint8_t> byte_pool;
	//游标定位到 254
	byte_pool.set(static_cast<uint8_t>(254));
	//依次取出
	EXPECT_EQ(byte_pool.get().value_or(0), 254);
	EXPECT_EQ(byte_pool.get().value_or(0), 255);
	EXPECT_EQ(byte_pool.get().value_or(0), 0);
}

//降序分配：有符号类型负起点递减
TEST_F(Number_Pool_Test, 降序负起点递减)
{
	//有符号数值池
	engine::Number_Pool<int32_t> signed_pool;
	//设置负起点并切换降序
	signed_pool.set(-5);
	signed_pool.set(engine::Num_Allocate_Order::Greater);
	//按降序递减
	EXPECT_EQ(signed_pool.get().value_or(0), -5);
	EXPECT_EQ(signed_pool.get().value_or(0), -6);
}

//降序回绕：有符号类型到最小值后回到最大值
TEST_F(Number_Pool_Test, 降序游标回绕)
{
	//八位有符号数值池
	engine::Number_Pool<int8_t> signed_pool;
	//游标定位到最小值
	signed_pool.set((std::numeric_limits<int8_t>::min)());
	signed_pool.set(engine::Num_Allocate_Order::Greater);
	//最小值之后回到最大值
	EXPECT_EQ(signed_pool.get().value_or(0), -128);
	EXPECT_EQ(signed_pool.get().value_or(0), 127);
}

//随机分配：禁用值不会被抽出
TEST_F(Number_Pool_Test, 随机分配跳过禁用值)
{
	//切换随机分配
	pool.set(engine::Num_Allocate_Order::Random);
	//禁用数值 5
	pool.ban(5);
	//连续抽取六十四次
	for (int time = 0; time < 64; time++)
	{
		const uint64_t value = value_take(pool);
		EXPECT_NE(value, 5u);
	}
}

//耗尽：八位数值池分配满后返回空值
TEST_F(Number_Pool_Test, 八位数值池耗尽返回空值)
{
	//八位数值池
	engine::Number_Pool<uint8_t> byte_pool;
	//容量为 255，连续分配 255 个
	for (uint32_t time = 0; time < 255; time++)
		EXPECT_TRUE(byte_pool.get().has_value());
	//再分配应返回空值
	EXPECT_FALSE(byte_pool.get().has_value());
}
