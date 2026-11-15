#include "tool.h"


//传入一个矩阵，解析得到位置，欧拉角 ， 
void Tool::decompose(glm::mat4 matrix, glm::vec3& position, glm::vec3& eulerAngle, glm::vec3& scale)
{
	//四元数
	glm::quat quaternion;
	glm::vec3 skew;
	glm::vec4 perspective;
	//拆解函数
	glm::decompose(matrix, scale, quaternion, position , skew , perspective);
	glm::mat4 rotation = glm::toMat4(quaternion);
	glm::extractEulerAngleXYZ(rotation, eulerAngle.x, eulerAngle.y, eulerAngle.z);

	//将四元数装换为欧拉角
	eulerAngle.x = glm::degrees(eulerAngle.x);
	eulerAngle.y = glm::degrees(eulerAngle.y);
	eulerAngle.z = glm::degrees(eulerAngle.z);
}