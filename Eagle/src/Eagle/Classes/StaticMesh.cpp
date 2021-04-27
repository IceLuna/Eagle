#include "egpch.h"
#include "StaticMesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Eagle
{
	std::vector<Ref<StaticMesh>> StaticMeshLibrary::m_Meshes;

	std::vector<Ref<Texture2D>> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& filename)
	{
		std::vector<Ref<Texture2D>> textures;
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
		{
			aiString str;
			mat->GetTexture(type, i, &str);
			std::string relativePath = str.C_Str();
			std::filesystem::path absolutePath(filename);
			absolutePath = absolutePath.parent_path() / relativePath;
			EG_CORE_TRACE("SM Texture Path: {0}", absolutePath.string());
			if (std::filesystem::exists(absolutePath))
			{
				if (!TextureLibrary::Exist(absolutePath.string()))
					textures.push_back(Texture2D::Create(absolutePath.string()));
			}

		}
		return textures;
	}

	StaticMesh processMesh(aiMesh* mesh, const aiScene* scene, const std::string& filename, bool bLazy)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// walk through each of the mesh's vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			glm::vec3 vector;

			// positions
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.Position = vector;

			// normals
			if (mesh->HasNormals())
			{
				vector.x = mesh->mNormals[i].x;
				vector.y = mesh->mNormals[i].y;
				vector.z = mesh->mNormals[i].z;
				vertex.Normal = vector;
			}
			// texture coordinates
			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
			{
				glm::vec2 vec;
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.TexCoords = vec;
			}
			else
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);

			vertices.push_back(vertex);
		}
		// now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			// retrieve all indices of the face and store them in the indices vector
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		if (!bLazy)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Ref<Texture2D>> diffuseTextures = loadMaterialTextures(material, aiTextureType_DIFFUSE, filename);
			std::vector<Ref<Texture2D>> specularTextures = loadMaterialTextures(material, aiTextureType_SPECULAR, filename);

			StaticMesh sm(vertices, indices);
			if (diffuseTextures.size())
				sm.Material.DiffuseTexture = diffuseTextures[0];
			if (specularTextures.size())
				sm.Material.SpecularTexture = specularTextures[0];

			// return a mesh object created from the extracted mesh data
			return sm;
		}
		else 
			return StaticMesh(vertices, indices);
	}

	// processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
	static void processNode(aiNode* node, const aiScene* scene, std::vector<StaticMesh>& meshes, const std::string& filename, bool bLazy)
	{
		// process each mesh located at the current node
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			// the node object only contains indices to index the actual objects in the scene. 
			// the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(processMesh(mesh, scene, filename, bLazy));
		}
		// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			processNode(node->mChildren[i], scene, meshes, filename, bLazy);
		}

	}

	//if bLazy is set to true, textures won't be loaded.
	Ref<StaticMesh> StaticMesh::Create(const std::string& filename, bool bLazy /* = false */)
	{
		if (!std::filesystem::exists(filename))
		{
			EG_CORE_ERROR("Failed to load Static Mesh. File doesn't exist! ({0})", filename);
			return MakeRef<StaticMesh>();
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals 
															);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			EG_CORE_ERROR("Failed to load Static Mesh. {0} ({1})", importer.GetErrorString(), filename);
			return MakeRef<StaticMesh>();
		}

		std::vector<StaticMesh> meshes;
		processNode(scene->mRootNode, scene, meshes, filename, bLazy);
		int meshesCount = meshes.size();

		if (meshesCount == 0)
			return MakeRef<StaticMesh>();

		std::filesystem::path path(filename);
		std::string fileStem = path.stem().string();
		if (meshesCount > 1)
		{
			fileStem += "_";
			Ref<StaticMesh> firstSM = MakeRef<StaticMesh>(meshes[0].GetVertices(), meshes[0].GetIndeces());
			firstSM->m_Path = filename;
			firstSM->m_AssetName = fileStem + std::to_string(0);
			StaticMeshLibrary::Add(firstSM);

			for (int i = 1; i < meshesCount; ++i)
			{
				Ref<StaticMesh> sm = MakeRef<StaticMesh>(meshes[i].GetVertices(), meshes[i].GetIndeces());
				sm->m_Path = filename;
				sm->m_AssetName = fileStem + std::to_string(i);
				sm->m_Index = (uint32_t)i;
				StaticMeshLibrary::Add(sm);
			}
			return firstSM;
		}

		Ref<StaticMesh> sm = MakeRef<StaticMesh>(meshes[0]);
		sm->m_Path = filename;
		sm->m_AssetName = fileStem;
		StaticMeshLibrary::Add(sm);

		return sm;
	}

	bool StaticMeshLibrary::Get(const std::string& path, Ref<StaticMesh>* staticMesh, uint32_t index /* = 0u */)
	{
		std::filesystem::path testPath(path);

		for (const auto& mesh : m_Meshes)
		{
			std::filesystem::path currentPath(mesh->GetPath());

			if (testPath == currentPath)
			{
				if (mesh->GetIndex() == index)
				{
					*staticMesh = mesh;
					return true;
				}
			}
		}
		return false;
	}
}
