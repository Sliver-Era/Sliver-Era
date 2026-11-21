#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
	//辅助工具命名空间
	namespace detail
	{
		/*
		骨骼数学
		矩阵统一为 4x4、列主序（m[列 * 4 + 行]），与 glTF 及渲染层一致；
		本组函数只服务于骨骼与蒙皮求值，不引入外部数值库。
		*/

		//单位矩阵写入
		inline void bone_matrix_identity(float out[16])
		{
			//逐元素清零
			for (uint32_t index = 0; index < 16; index++)
				out[index] = 0.0f;
			//对角置一
			out[0] = 1.0f;
			out[5] = 1.0f;
			out[10] = 1.0f;
			out[15] = 1.0f;
		}

		//矩阵乘法（out = left * right；允许 out 与入参同址）
		inline void bone_matrix_multiply(const float left[16], const float right[16], float out[16])
		{
			//结果缓冲
			float result[16] = {};
			//逐列逐行累加
			for (uint32_t column = 0; column < 4; column++)
			{
				for (uint32_t row = 0; row < 4; row++)
				{
					//累加项
					float sum = 0.0f;
					//四阶点积
					for (uint32_t inner = 0; inner < 4; inner++)
						sum += left[inner * 4 + row] * right[column * 4 + inner];
					//写入结果
					result[column * 4 + row] = sum;
				}
			}
			//回写结果
			for (uint32_t index = 0; index < 16; index++)
				out[index] = result[index];
		}

		//四元数单位化（零四元数退化为单位旋转）
		inline void bone_quaternion_normalize(const float input[4], float out[4])
		{
			//长度平方
			float length2 = input[0] * input[0] + input[1] * input[1]
				+ input[2] * input[2] + input[3] * input[3];
			//零四元数按单位旋转处理
			if (length2 <= 0.0f)
			{
				out[0] = 0.0f;
				out[1] = 0.0f;
				out[2] = 0.0f;
				out[3] = 1.0f;
				return;
			}
			//长度
			float length = std::sqrt(length2);
			//逐分量归一化
			out[0] = input[0] / length;
			out[1] = input[1] / length;
			out[2] = input[2] / length;
			out[3] = input[3] / length;
		}

		//四元数共轭（单位四元数即逆）
		inline void bone_quaternion_conjugate(const float input[4], float out[4])
		{
			out[0] = -input[0];
			out[1] = -input[1];
			out[2] = -input[2];
			out[3] = input[3];
		}

		//四元数乘法（Hamilton 积）
		inline void bone_quaternion_multiply(const float left[4], const float right[4], float out[4])
		{
			//结果分量
			float x = left[3] * right[0] + left[0] * right[3]
				+ left[1] * right[2] - left[2] * right[1];
			float y = left[3] * right[1] - left[0] * right[2]
				+ left[1] * right[3] + left[2] * right[0];
			float z = left[3] * right[2] + left[0] * right[1]
				- left[1] * right[0] + left[2] * right[3];
			float w = left[3] * right[3] - left[0] * right[0]
				- left[1] * right[1] - left[2] * right[2];
			//写入结果
			out[0] = x;
			out[1] = y;
			out[2] = z;
			out[3] = w;
		}

		//变换矩阵构建（列主序，先缩放、再旋转、最后平移）
		inline void bone_matrix_from_transform(const float translation[3], const float rotation[4],
			const float scale[3], float out[16])
		{
			//单位化四元数
			float quaternion[4] = {};
			bone_quaternion_normalize(rotation, quaternion);
			//四元数分量
			float x = quaternion[0];
			float y = quaternion[1];
			float z = quaternion[2];
			float w = quaternion[3];
			//旋转矩阵中间量
			float xx = x * x, yy = y * y, zz = z * z;
			float xy = x * y, xz = x * z, yz = y * z;
			float wx = w * x, wy = w * y, wz = w * z;
			//旋转矩阵（按行列记法）
			float r00 = 1.0f - 2.0f * (yy + zz);
			float r01 = 2.0f * (xy - wz);
			float r02 = 2.0f * (xz + wy);
			float r10 = 2.0f * (xy + wz);
			float r11 = 1.0f - 2.0f * (xx + zz);
			float r12 = 2.0f * (yz - wx);
			float r20 = 2.0f * (xz - wy);
			float r21 = 2.0f * (yz + wx);
			float r22 = 1.0f - 2.0f * (xx + yy);
			//第一列（含 X 缩放）
			out[0] = r00 * scale[0];
			out[1] = r10 * scale[0];
			out[2] = r20 * scale[0];
			out[3] = 0.0f;
			//第二列（含 Y 缩放）
			out[4] = r01 * scale[1];
			out[5] = r11 * scale[1];
			out[6] = r21 * scale[1];
			out[7] = 0.0f;
			//第三列（含 Z 缩放）
			out[8] = r02 * scale[2];
			out[9] = r12 * scale[2];
			out[10] = r22 * scale[2];
			out[11] = 0.0f;
			//第四列（平移）
			out[12] = translation[0];
			out[13] = translation[1];
			out[14] = translation[2];
			out[15] = 1.0f;
		}

		//仿射矩阵求逆（失败时写入单位矩阵并返回假）
		inline bool bone_matrix_inverse(const float input[16], float out[16])
		{
			//取出 3x3 线性部分（a行 列）
			float a00 = input[0], a01 = input[4], a02 = input[8];
			float a10 = input[1], a11 = input[5], a12 = input[9];
			float a20 = input[2], a21 = input[6], a22 = input[10];
			//行列式
			float determinant =
				a00 * (a11 * a22 - a12 * a21)
				- a01 * (a10 * a22 - a12 * a20)
				+ a02 * (a10 * a21 - a11 * a20);
			//奇异矩阵不可逆
			if (std::abs(determinant) <= 1e-12f)
			{
				bone_matrix_identity(out);
				return false;
			}
			//行列式倒数
			float inverse_determinant = 1.0f / determinant;
			//逆矩阵 3x3 分量（inv行 列）
			float i00 = (a11 * a22 - a12 * a21) * inverse_determinant;
			float i10 = (a12 * a20 - a10 * a22) * inverse_determinant;
			float i20 = (a10 * a21 - a11 * a20) * inverse_determinant;
			float i01 = (a02 * a21 - a01 * a22) * inverse_determinant;
			float i11 = (a00 * a22 - a02 * a20) * inverse_determinant;
			float i21 = (a01 * a20 - a00 * a21) * inverse_determinant;
			float i02 = (a01 * a12 - a02 * a11) * inverse_determinant;
			float i12 = (a02 * a10 - a00 * a12) * inverse_determinant;
			float i22 = (a00 * a11 - a01 * a10) * inverse_determinant;
			//原平移
			float tx = input[12], ty = input[13], tz = input[14];
			//第一列
			out[0] = i00;
			out[1] = i10;
			out[2] = i20;
			out[3] = 0.0f;
			//第二列
			out[4] = i01;
			out[5] = i11;
			out[6] = i21;
			out[7] = 0.0f;
			//第三列
			out[8] = i02;
			out[9] = i12;
			out[10] = i22;
			out[11] = 0.0f;
			//第四列（逆平移）
			out[12] = -(i00 * tx + i01 * ty + i02 * tz);
			out[13] = -(i10 * tx + i11 * ty + i12 * tz);
			out[14] = -(i20 * tx + i21 * ty + i22 * tz);
			out[15] = 1.0f;
			return true;
		}

		//仿射矩阵分解（平移 + 旋转 + 缩放；奇异矩阵返回假）
		inline bool bone_matrix_decompose(const float input[16], float translation[3],
			float rotation[4], float scale[3])
		{
			//三列线性部分
			float cx[3] = { input[0], input[1], input[2] };
			float cy[3] = { input[4], input[5], input[6] };
			float cz[3] = { input[8], input[9], input[10] };
			//各列长度即缩放
			float sx = std::sqrt(cx[0] * cx[0] + cx[1] * cx[1] + cx[2] * cx[2]);
			float sy = std::sqrt(cy[0] * cy[0] + cy[1] * cy[1] + cy[2] * cy[2]);
			float sz = std::sqrt(cz[0] * cz[0] + cz[1] * cz[1] + cz[2] * cz[2]);
			//任一轴退化则不可分解
			if (sx <= 1e-12f || sy <= 1e-12f || sz <= 1e-12f)
				return false;
			//叉积判断左右手性
			float cross_x = cy[1] * cz[2] - cy[2] * cz[1];
			float cross_y = cy[2] * cz[0] - cy[0] * cz[2];
			float cross_z = cy[0] * cz[1] - cy[1] * cz[0];
			//行列式
			float determinant = cx[0] * cross_x + cx[1] * cross_y + cx[2] * cross_z;
			//负行列式（镜像）把 X 轴缩放置负
			if (determinant < 0.0f)
			{
				sx = -sx;
				cx[0] = -cx[0];
				cx[1] = -cx[1];
				cx[2] = -cx[2];
			}
			//单位化三列得到旋转矩阵
			float r00 = cx[0] / std::abs(sx);
			float r10 = cx[1] / std::abs(sx);
			float r20 = cx[2] / std::abs(sx);
			float r01 = cy[0] / sy;
			float r11 = cy[1] / sy;
			float r21 = cy[2] / sy;
			float r02 = cz[0] / sz;
			float r12 = cz[1] / sz;
			float r22 = cz[2] / sz;
			//矩阵迹
			float trace = r00 + r11 + r22;
			//四元数
			float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
			//按最大分量分支求解，避免除零
			if (trace > 0.0f)
			{
				float factor = std::sqrt(trace + 1.0f) * 2.0f;
				w = 0.25f * factor;
				x = (r21 - r12) / factor;
				y = (r02 - r20) / factor;
				z = (r10 - r01) / factor;
			}
			else if (r00 > r11 && r00 > r22)
			{
				float factor = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
				w = (r21 - r12) / factor;
				x = 0.25f * factor;
				y = (r01 + r10) / factor;
				z = (r02 + r20) / factor;
			}
			else if (r11 > r22)
			{
				float factor = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
				w = (r02 - r20) / factor;
				x = (r01 + r10) / factor;
				y = 0.25f * factor;
				z = (r12 + r21) / factor;
			}
			else
			{
				float factor = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
				w = (r10 - r01) / factor;
				x = (r02 + r20) / factor;
				y = (r12 + r21) / factor;
				z = 0.25f * factor;
			}
			//写入平移
			translation[0] = input[12];
			translation[1] = input[13];
			translation[2] = input[14];
			//写入缩放
			scale[0] = sx;
			scale[1] = sy;
			scale[2] = sz;
			//归一化并写入旋转
			float quaternion[4] = { x, y, z, w };
			bone_quaternion_normalize(quaternion, rotation);
			return true;
		}

		//矩阵近似单位判定
		inline bool bone_matrix_identity_check(const float input[16], float epsilon = 1e-6f)
		{
			//构造单位矩阵
			float identity[16] = {};
			bone_matrix_identity(identity);
			//逐元素比较
			for (uint32_t index = 0; index < 16; index++)
			{
				if (std::abs(input[index] - identity[index]) > epsilon)
					return false;
			}
			return true;
		}
	}
}
