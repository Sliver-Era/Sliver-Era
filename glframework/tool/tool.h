#pragma once
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <../../thirdParty/include/glm/gtx/euler_angles.hpp>
#include <../../thirdParty/include/glm/gtx/quaternion.hpp>
#include <../../thirdParty/include/glm/gtx/matrix_decompose.hpp>

class Tool
{
public:

	static void decompose(glm::mat4 matrix, glm::vec3& position, glm::vec3& eulerAngle, glm::vec3& scale);
};