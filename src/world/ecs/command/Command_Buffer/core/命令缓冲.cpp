#include "../局部命名空间使用.h"

//游戏引擎命名空间
namespace engine
{
	//延迟销毁实体
	void Command_Buffer::entity_destroy(Entity entity)
	{
		commands.push_back(
			[entity](ECS_Store& store)
			{
				store.entity_destroy(entity);
			});
	}

	//提交全部命令
	void Command_Buffer::flush(ECS_Store& store)
	{
		//按记录顺序逐条提交
		for (auto& command : commands)
			command(store);
		//清空命令队列
		commands.clear();
	}

	//清空命令队列
	void Command_Buffer::clear(void)
	{
		commands.clear();
	}

	//命令数量获取
	std::size_t Command_Buffer::size(void) const
	{
		return commands.size();
	}
}
