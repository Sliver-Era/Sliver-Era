#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取骨骼数学工具
#include "src/world/skeleton/Common/骨骼数学.h"

//游戏引擎命名空间
namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//浮点数组有限性检查
		bool finite_check(const float* values, uint32_t count)
		{
			//逐项检查
			for (uint32_t index = 0; index < count; index++)
			{
				if (!std::isfinite(values[index]))
					return false;
			}
			return true;
		}

		//四元数单位化（返回假表示输入非法）
		bool quaternion_clean(const float input[4], float out[4])
		{
			//有限性检查
			if (!finite_check(input, 4))
				return false;
			//长度平方
			float length2 = input[0] * input[0] + input[1] * input[1]
				+ input[2] * input[2] + input[3] * input[3];
			//零四元数非法
			if (length2 <= 0.0f)
				return false;
			//单位化输出
			detail::bone_quaternion_normalize(input, out);
			return true;
		}
	}

	//按资产初始化
	bool Skeleton_Pose::build(const std::shared_ptr<const Skeleton_Asset>& shared_asset)
	{
		//资产必须有效
		if (!shared_asset || !shared_asset->valid())
		{
			logger.warn("Skeleton_Pose::骨骼资产无效，姿态初始化中止");
			return false;
		}

		//记录资产
		asset = shared_asset;
		//骨骼数量
		uint32_t count = asset->bone_count();
		//逐骨骼局部变换
		local_translation.assign(static_cast<uint64_t>(count) * 3, 0.0f);
		local_rotation.assign(static_cast<uint64_t>(count) * 4, 0.0f);
		local_scale.assign(static_cast<uint64_t>(count) * 3, 1.0f);
		//世界矩阵与蒙皮矩阵
		world_matrix.assign(static_cast<uint64_t>(count) * 16, 0.0f);
		skin_matrix.assign(static_cast<uint64_t>(count) * 16, 0.0f);
		//逐骨骼写入绑定姿态
		for (uint32_t index = 0; index < count; index++)
		{
			//当前骨骼
			const Skeleton_Bone* bone = asset->bone_get(index);
			//局部平移
			local_translation[static_cast<uint64_t>(index) * 3 + 0] = bone->bind_translation[0];
			local_translation[static_cast<uint64_t>(index) * 3 + 1] = bone->bind_translation[1];
			local_translation[static_cast<uint64_t>(index) * 3 + 2] = bone->bind_translation[2];
			//局部旋转
			for (uint32_t element = 0; element < 4; element++)
				local_rotation[static_cast<uint64_t>(index) * 4 + element] = bone->bind_rotation[element];
			//局部缩放
			for (uint32_t element = 0; element < 3; element++)
				local_scale[static_cast<uint64_t>(index) * 3 + element] = bone->bind_scale[element];
			//世界矩阵置单位
			detail::bone_matrix_identity(&world_matrix[static_cast<uint64_t>(index) * 16]);
			//蒙皮矩阵置单位
			detail::bone_matrix_identity(&skin_matrix[static_cast<uint64_t>(index) * 16]);
		}

		//标记待求值
		pose_dirty = true;
		skin_dirty = true;
		return true;
	}

	//骨骼数量读取
	uint32_t Skeleton_Pose::bone_count(void) const
	{
		//无资产时为零
		if (!asset)
			return 0;
		return asset->bone_count();
	}

	//姿态复位到绑定姿态
	void Skeleton_Pose::reset(void)
	{
		//无资产时无事可做
		if (!asset)
			return;
		//逐骨骼写回绑定姿态
		uint32_t count = asset->bone_count();
		for (uint32_t index = 0; index < count; index++)
		{
			//当前骨骼
			const Skeleton_Bone* bone = asset->bone_get(index);
			//局部平移
			for (uint32_t element = 0; element < 3; element++)
				local_translation[static_cast<uint64_t>(index) * 3 + element] = bone->bind_translation[element];
			//局部旋转
			for (uint32_t element = 0; element < 4; element++)
				local_rotation[static_cast<uint64_t>(index) * 4 + element] = bone->bind_rotation[element];
			//局部缩放
			for (uint32_t element = 0; element < 3; element++)
				local_scale[static_cast<uint64_t>(index) * 3 + element] = bone->bind_scale[element];
		}
		//标记待求值
		pose_dirty = true;
		skin_dirty = true;
	}

	//骨骼局部变换写入
	bool Skeleton_Pose::local_set(uint32_t bone, const float translation[3],
		const float rotation[4], const float scale[3])
	{
		//下标越界
		if (bone >= bone_count())
			return false;
		//平移与缩放有限性检查
		if (!finite_check(translation, 3) || !finite_check(scale, 3))
			return false;
		//零缩放不可逆
		for (uint32_t index = 0; index < 3; index++)
		{
			if (scale[index] == 0.0f)
				return false;
		}
		//旋转单位化
		float rotation_clean[4] = {};
		if (!quaternion_clean(rotation, rotation_clean))
			return false;

		//写入局部平移
		for (uint32_t element = 0; element < 3; element++)
			local_translation[static_cast<uint64_t>(bone) * 3 + element] = translation[element];
		//写入局部旋转
		for (uint32_t element = 0; element < 4; element++)
			local_rotation[static_cast<uint64_t>(bone) * 4 + element] = rotation_clean[element];
		//写入局部缩放
		for (uint32_t element = 0; element < 3; element++)
			local_scale[static_cast<uint64_t>(bone) * 3 + element] = scale[element];
		//标记待求值
		pose_dirty = true;
		skin_dirty = true;
		return true;
	}

	//骨骼局部平移写入
	bool Skeleton_Pose::translation_set(uint32_t bone, const float translation[3])
	{
		//下标越界
		if (bone >= bone_count())
			return false;
		//有限性检查
		if (!finite_check(translation, 3))
			return false;
		//写入局部平移
		for (uint32_t element = 0; element < 3; element++)
			local_translation[static_cast<uint64_t>(bone) * 3 + element] = translation[element];
		//标记待求值
		pose_dirty = true;
		skin_dirty = true;
		return true;
	}

	//骨骼局部旋转写入
	bool Skeleton_Pose::rotation_set(uint32_t bone, const float rotation[4])
	{
		//下标越界
		if (bone >= bone_count())
			return false;
		//旋转单位化
		float rotation_clean[4] = {};
		if (!quaternion_clean(rotation, rotation_clean))
			return false;
		//写入局部旋转
		for (uint32_t element = 0; element < 4; element++)
			local_rotation[static_cast<uint64_t>(bone) * 4 + element] = rotation_clean[element];
		//标记待求值
		pose_dirty = true;
		skin_dirty = true;
		return true;
	}

	//骨骼局部缩放写入
	bool Skeleton_Pose::scale_set(uint32_t bone, const float scale[3])
	{
		//下标越界
		if (bone >= bone_count())
			return false;
		//有限性检查
		if (!finite_check(scale, 3))
			return false;
		//零缩放不可逆
		for (uint32_t index = 0; index < 3; index++)
		{
			if (scale[index] == 0.0f)
				return false;
		}
		//写入局部缩放
		for (uint32_t element = 0; element < 3; element++)
			local_scale[static_cast<uint64_t>(bone) * 3 + element] = scale[element];
		//标记待求值
		pose_dirty = true;
		skin_dirty = true;
		return true;
	}

	//批量局部变换写入
	uint32_t Skeleton_Pose::local_batch_set(const Skeleton_Local_Transform* entries, uint32_t count)
	{
		//空入参
		if (!entries || count == 0)
			return 0;
		//成功条数
		uint32_t applied = 0;
		//逐条写入（非法条目跳过）
		for (uint32_t index = 0; index < count; index++)
		{
			const Skeleton_Local_Transform& entry = entries[index];
			if (local_set(entry.bone, entry.translation, entry.rotation, entry.scale))
				applied++;
		}
		return applied;
	}

	//骨骼世界矩阵读取
	bool Skeleton_Pose::world_get(uint32_t bone, float out[16]) const
	{
		//下标越界
		if (bone >= bone_count())
			return false;
		//复制世界矩阵
		for (uint32_t element = 0; element < 16; element++)
			out[element] = world_matrix[static_cast<uint64_t>(bone) * 16 + element];
		return true;
	}
}
