#include "../局部命名空间使用.h"
//获取骨骼数学工具
#include "src/world/skeleton/Common/骨骼数学.h"

//游戏引擎命名空间
namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//运动增量是否近似为零
		bool motion_identity_check(const float translation[3], const float rotation[4])
		{
			//平移量平方
			float translation2 = translation[0] * translation[0]
				+ translation[1] * translation[1] + translation[2] * translation[2];
			//旋转是否近似单位（q 与 -q 等价，只看标量部分绝对值）
			bool rotation_identity = std::abs(rotation[3]) >= 1.0f - 1e-6f;
			return translation2 <= 1e-12f && rotation_identity;
		}
	}

	//部件运动求值
	void Skeleton_System::link_evaluate(const Skeleton_Pose& pose, Skeleton_Link& link) const
	{
		//清空上一帧输出
		link.motions.clear();
		link.root_motion = Skeleton_Root_Motion();

		//未绑定碰撞体时不产生运动量
		if (link.collider_ID == 0)
			return;
		//姿态与基准矩阵规模检查
		if (pose.bone_count() == 0 || link.links.empty() ||
			link.previous_matrix.size() != link.links.size() * 16)
			return;
		//根骨骼下标越界
		if (link.root_bone >= pose.bone_count())
			return;

		//本帧根骨骼世界矩阵
		float root_current[16] = {};
		for (uint32_t element = 0; element < 16; element++)
			root_current[element] = pose.world_matrix[static_cast<uint64_t>(link.root_bone) * 16 + element];
		//根骨骼逆矩阵（奇异时退化为单位）
		float root_inverse[16] = {};
		if (!detail::bone_matrix_inverse(root_current, root_inverse))
			detail::bone_matrix_identity(root_inverse);

		/*
		根部整体增量
		碰撞侧语义为「先绕自身原点旋转、再按世界轴平移」，
		故平移取原点差、旋转取「上一帧逆 × 本帧」的旋转差。
		*/
		float previous_root_translation[3] = {};
		float previous_root_rotation[4] = {};
		float previous_root_scale[3] = {};
		float current_root_translation[3] = {};
		float current_root_rotation[4] = {};
		float current_root_scale[3] = {};
		if (detail::bone_matrix_decompose(link.previous_root, previous_root_translation,
			previous_root_rotation, previous_root_scale) &&
			detail::bone_matrix_decompose(root_current, current_root_translation,
				current_root_rotation, current_root_scale))
		{
			//平移增量（碰撞体局部/世界轴）
			float translation[3] = {
				current_root_translation[0] - previous_root_translation[0],
				current_root_translation[1] - previous_root_translation[1],
				current_root_translation[2] - previous_root_translation[2] };
			//旋转增量（上一帧逆 × 本帧）
			float previous_inverse[4] = {};
			detail::bone_quaternion_conjugate(previous_root_rotation, previous_inverse);
			float rotation[4] = {};
			detail::bone_quaternion_multiply(previous_inverse, current_root_rotation, rotation);
			detail::bone_quaternion_normalize(rotation, rotation);
			//增量非零才输出
			if (!motion_identity_check(translation, rotation))
			{
				link.root_motion.active = true;
				for (uint32_t element = 0; element < 3; element++)
					link.root_motion.translation[element] = translation[element];
				for (uint32_t element = 0; element < 4; element++)
					link.root_motion.rotation[element] = rotation[element];
			}
		}

		//逐部件求增量（局部矩阵相对根骨骼，增量位于碰撞体局部空间）
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
			//骨骼相对根骨骼的局部矩阵
			float bone_local[16] = {};
			detail::bone_matrix_multiply(root_inverse,
				&pose.world_matrix[static_cast<uint64_t>(part_link.bone) * 16], bone_local);
			//本帧部件局部矩阵 = 骨骼局部矩阵 × 相对变换
			float current[16] = {};
			detail::bone_matrix_multiply(bone_local, offset, current);

			//上一帧与本帧的平移/旋转（碰撞侧语义：平移取原点差，旋转取旋转差）
			float previous_translation[3] = {};
			float previous_rotation[4] = {};
			float previous_scale[3] = {};
			float current_translation[3] = {};
			float current_rotation[4] = {};
			float current_scale[3] = {};
			//分解成功才输出运动量（奇异矩阵时仅刷新基准）
			if (detail::bone_matrix_decompose(&link.previous_matrix[index * 16],
				previous_translation, previous_rotation, previous_scale) &&
				detail::bone_matrix_decompose(current, current_translation,
					current_rotation, current_scale))
			{
				//平移增量
				float translation[3] = {
					current_translation[0] - previous_translation[0],
					current_translation[1] - previous_translation[1],
					current_translation[2] - previous_translation[2] };
				//旋转增量（上一帧逆 × 本帧）
				float previous_inverse[4] = {};
				detail::bone_quaternion_conjugate(previous_rotation, previous_inverse);
				float rotation[4] = {};
				detail::bone_quaternion_multiply(previous_inverse, current_rotation, rotation);
				detail::bone_quaternion_normalize(rotation, rotation);
				//增量非零才输出
				if (!motion_identity_check(translation, rotation))
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

		//刷新根部基准
		for (uint32_t element = 0; element < 16; element++)
			link.previous_root[element] = root_current[element];
	}
}
