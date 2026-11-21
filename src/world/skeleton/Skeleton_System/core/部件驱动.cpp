#include "../局部命名空间使用.h"
//获取骨骼数学工具
#include "src/world/skeleton/Common/骨骼数学.h"

//游戏引擎命名空间
namespace engine
{
	//部件运动求值
	void Skeleton_System::link_evaluate(const Skeleton_Pose& pose, Skeleton_Link& link) const
	{
		//清空上一帧运动量
		link.motions.clear();

		//未绑定碰撞体时不产生运动量
		if (link.collider_ID == 0)
			return;
		//姿态与基准矩阵规模检查
		if (pose.bone_count() == 0 || link.links.empty() ||
			link.previous_matrix.size() != link.links.size() * 16)
			return;

		//逐部件求增量
		for (uint64_t index = 0; index < link.links.size(); index++)
		{
			//当前绑定
			const Skeleton_Part_Link& part_link = link.links[index];
			//骨骼下标越界
			if (part_link.bone >= pose.bone_count())
				continue;

			//相对变换矩阵
			float offset[16] = {};
			detail::bone_matrix_from_transform(part_link.translation, part_link.rotation,
				part_link.scale, offset);
			//本帧部件世界矩阵 = 骨骼世界矩阵 × 相对变换
			float current[16] = {};
			detail::bone_matrix_multiply(
				&pose.world_matrix[static_cast<uint64_t>(part_link.bone) * 16],
				offset, current);

			//上一帧矩阵的逆
			float previous_inverse[16] = {};
			//奇异矩阵时刷新基准后跳过
			if (!detail::bone_matrix_inverse(&link.previous_matrix[index * 16], previous_inverse))
			{
				for (uint32_t element = 0; element < 16; element++)
					link.previous_matrix[index * 16 + element] = current[element];
				continue;
			}

			//增量矩阵 = 上一帧逆 × 本帧
			float delta[16] = {};
			detail::bone_matrix_multiply(previous_inverse, current, delta);

			//增量分解
			float translation[3] = {};
			float rotation[4] = {};
			float scale[3] = {};
			//分解成功才输出运动量
			if (detail::bone_matrix_decompose(delta, translation, rotation, scale))
			{
				//平移量平方
				float translation2 = translation[0] * translation[0]
					+ translation[1] * translation[1] + translation[2] * translation[2];
				//旋转是否近似单位（q 与 -q 等价，只看标量部分绝对值）
				bool rotation_identity = std::abs(rotation[3]) >= 1.0f - 1e-6f;
				//增量非零时输出
				if (translation2 > 1e-12f || !rotation_identity)
				{
					//部件运动量
					Skeleton_Part_Motion motion;
					motion.part = part_link.part_ID;
					for (uint32_t element = 0; element < 3; element++)
						motion.translation[element] = translation[element];
					for (uint32_t element = 0; element < 4; element++)
						motion.rotation[element] = rotation[element];
					link.motions.push_back(motion);
				}
			}

			//刷新上一帧基准
			for (uint32_t element = 0; element < 16; element++)
				link.previous_matrix[index * 16 + element] = current[element];
		}
	}
}
