#pragma once
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include"../wrapper/checkError.h"
#include<string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include "../glframework//object.h"
#include "../glframework/tool/tool.h"
#include "../glframework/geometry.h"
#include "../glframework/meterial/meterial.h"
#include "../glframework/meterial/phonemeterial.h"
#include "../glframework/texture.h"
#include "../glframework/mesh.h"
#include "../thirdParty/include/assimp/Importer.hpp"
#include "../thirdParty/include/assimp/scene.h"
#include "../thirdParty/include/assimp/postprocess.h"

class AssimpLoader
{
public:
	//加载模型，场景，装入我们自己的Object
	static Object* load(const std::string& path);

private:
	//拆解模型节点
	static void processNode(aiNode* ainode , Object* parent , const aiScene* sence);
	//解析模型几何数据，返回我们自己的mesh
	static Mesh* processMesh(aiMesh* aimesh);
	//矩阵类型转换
	static glm::mat4 getMat4f(aiMatrix4x4 value);
};