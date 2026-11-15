#include "assimpLoader.h"

 Object* AssimpLoader::load(const std::string& path)
{
	//Object根节点
	Object* rootNode = new Object();

	//创建一个导入器
	Assimp::Importer importer;
	//读取文件的              将四边型变三角形        为模型生成法线
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);

	//验证读取是否正确顺利
	// 自身为空         文件读取不完整                         有空节点
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		return nullptr;
	}
	//解析根节点
	processNode(scene->mRootNode, rootNode, scene);

	return rootNode;
}

 void AssimpLoader::processNode(aiNode* ainode , Object* parent , const aiScene* scene)
 {
	 //Object子节点
	 Object* node = new Object();
	 parent->addChild(node);

	 //读取外部矩阵中的transform数据
	 glm::mat4 localMatrix = getMat4f(ainode->mTransformation);
	 //位置旋转缩放
	 glm::vec3 position, eulerAngle, scale;
	 Tool::decompose(localMatrix, position, eulerAngle, scale);
	 node->setPosition(position);
	 node->setAngleX(eulerAngle.x);
	 node->setAngleY(eulerAngle.y);
	 node->setAngleZ(eulerAngle.z);
	 node->setScale(scale);

	 //检查有没有mesh，并且解析
	 for (int i = 0; i < ainode->mNumMeshes; i++)
	 {
		 int meshID = ainode->mMeshes[i];
		 aiMesh* aimesh = scene->mMeshes[meshID];
		 Mesh* mesh = processMesh(aimesh);
		 node->addChild(mesh);
	 }

	 //递归子节点
	 for (int i = 0; i < ainode->mNumChildren; i++)
	 {
		 processNode(ainode->mChildren[i] , node , scene);
	 }
 }

 //解析模型几何数据，返回我们自己的mesh
 Mesh* AssimpLoader::processMesh(aiMesh* aimesh)
 {
	 std::vector<float> positions;
	 std::vector<float> normals;
	 std::vector<float> uvs;
	 std::vector<unsigned int> indices;

	 //遍历所有顶点，拿到几何数据
	 for (int i = 0; i < aimesh->mNumVertices; i++)
	 {
		 //拿到外部mesh的顶点数据
		 positions.push_back(aimesh->mVertices[i].x);
		 positions.push_back(aimesh->mVertices[i].y);
		 positions.push_back(aimesh->mVertices[i].z);

		 //外部mesh的法线数据
		 normals.push_back(aimesh->mNormals[i].x);
		 normals.push_back(aimesh->mNormals[i].y);
		 normals.push_back(aimesh->mNormals[i].z);

		 //uv数据      一般情况下第零套uv都是贴图数据
		 if (aimesh->mTextureCoords[0])
		 {
			 uvs.push_back(aimesh->mTextureCoords[0][i].x);
			 uvs.push_back(aimesh->mTextureCoords[0][i].y);
		 }
		 else
		 {
			 uvs.push_back(0.0f);
			 uvs.push_back(0.0f);
		 }
	 }

	 //遍历所有aiIndices
	 //先遍历每一个面
	 for (int i = 0; i < aimesh->mNumFaces;i++)
	 {
		 aiFace face = aimesh->mFaces[i];
		 //然后是每一个面的索引数据
		 for (int j = 0; j < face.mNumIndices; j++)
		 {
			 indices.push_back(face.mIndices[j]);
		 }
	 }
	 //构造一个geometry
	 Geometry* geometry = new Geometry(positions, normals, uvs, indices);
	 //构造一个材质
	 PhoneMeterial* meterial = new PhoneMeterial();
	 meterial->mDiffuse = new Texture("assets/texture/xiaojuzhihua.png", 0);
	 
	 //最后将这些数据装进Mesh返回
	 return new Mesh(geometry, meterial);
 }
 //将aiSence中的矩阵转换成glm的
glm::mat4 AssimpLoader::getMat4f(aiMatrix4x4 value)
{
	glm::mat4 to(
		value.a1, value.a2, value.a3, value.a4,
		value.b1, value.b2, value.b3, value.b4,
		value.c1, value.c2, value.c3, value.c4,
		value.d1, value.d2, value.d3, value.d4
	);
		return to;
}

