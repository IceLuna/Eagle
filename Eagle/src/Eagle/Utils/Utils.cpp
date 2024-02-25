#include "egpch.h"
#include "Utils.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Classes/Animation.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetImporter.h"
#include "Eagle/Asset/AssetManager.h"

#include <glm/gtx/quaternion.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <locale>

namespace Eagle
{
	// templated version of my_equal so it could work with both char and wchar_t
	template<typename charT>
	struct my_equal {
		my_equal(const std::locale& loc) : loc_(loc) {}
		bool operator()(charT ch1, charT ch2) {
			return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
		}
	private:
		const std::locale& loc_;
	};

	// find substring (case insensitive)
	template<typename T>
	static size_t MyFindStrTemplate(const T& str1, const T& str2, const std::locale& loc = std::locale("RU_ru"))
	{
		typename T::const_iterator it = std::search(str1.begin(), str1.end(),
			str2.begin(), str2.end(), my_equal<typename T::value_type>(loc));
		if (it != str1.end()) return it - str1.begin();
		else return std::string::npos; // not found
	}

	static inline glm::mat4 ToGLM(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

	static inline glm::vec3 ToGLM(const aiVector3D& vec)
	{
		return glm::vec3(vec.x, vec.y, vec.z);
	}

	static inline glm::quat ToGLM(const aiQuaternion& pOrientation)
	{
		return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
	}

	std::string Utils::ToUtf8(const std::wstring& str)
	{
		std::string ret;
		int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
		if (len > 0)
		{
			ret.resize(len);
			WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), &ret[0], len, NULL, NULL);
		}
		return ret;
	}
	
	size_t Utils::FindSubstringI(const std::string& str1, const std::string& str2)
	{
		return MyFindStrTemplate(str1, str2);
	}
	
	size_t Utils::FindSubstringI(const std::wstring& str1, const std::wstring& str2)
	{
		return MyFindStrTemplate(str1, str2);
	}
	
	static Ref<StaticMesh> ProcessStaticMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& tr)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(mesh->mNumVertices);
		indices.reserve(mesh->mNumFaces * 3);

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
			vertex.Position = tr * glm::vec4(vertex.Position, 1.f);

			// normals
			if (mesh->HasNormals())
			{
				vector.x = mesh->mNormals[i].x;
				vector.y = mesh->mNormals[i].y;
				vector.z = mesh->mNormals[i].z;
				vertex.Normal = vector;
			}

			//tangent
			if (mesh->HasTangentsAndBitangents())
			{
				vector.x = mesh->mTangents[i].x;
				vector.y = mesh->mTangents[i].y;
				vector.z = mesh->mTangents[i].z;
				vertex.Tangent = vector;
			}

			// texture coordinates
			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
			{
				glm::vec2 vec;
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = -mesh->mTextureCoords[0][i].y;
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

		return StaticMesh::Create(vertices, indices);
	}

	static void ProcessBoneNode(BoneNode& output, const aiNode* node)
	{
		EG_CORE_ASSERT(node);

		output.Name = node->mName.C_Str();
		output.Transformation = ToGLM(node->mTransformation);
		output.Children.resize(node->mNumChildren);
		for (size_t i = 0; i < node->mNumChildren; ++i)
		{
			auto& child = output.Children[i];
			ProcessBoneNode(child, node->mChildren[i]);
		}
	}

	static Ref<SkeletalMesh> ProcessSkeletalMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& tr, BonesMap& bones)
	{
		std::vector<SkeletalVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(mesh->mNumVertices);
		indices.reserve(mesh->mNumFaces * 3);

		// walk through each of the mesh's vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			SkeletalVertex vertex;
			glm::vec3 vector;

			// positions
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.Position = vector;
			vertex.Position = tr * glm::vec4(vertex.Position, 1.f);

			// normals
			if (mesh->HasNormals())
			{
				vector.x = mesh->mNormals[i].x;
				vector.y = mesh->mNormals[i].y;
				vector.z = mesh->mNormals[i].z;
				vertex.Normal = vector;
			}

			//tangent
			if (mesh->HasTangentsAndBitangents())
			{
				vector.x = mesh->mTangents[i].x;
				vector.y = mesh->mTangents[i].y;
				vector.z = mesh->mTangents[i].z;
				vertex.Tangent = vector;
			}

			// texture coordinates
			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
			{
				glm::vec2 vec;
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = -mesh->mTextureCoords[0][i].y;
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

		// Bones
		for (uint32_t i = 0; i < mesh->mNumBones; ++i)
		{
			uint32_t boneID = 0;
			std::string boneName = mesh->mBones[i]->mName.C_Str();
			auto it = bones.find(boneName);
			if (it == bones.end())
			{
				boneID = uint32_t(bones.size());
				BoneInfo info;
				info.BoneID = boneID;
				info.Offset = ToGLM(mesh->mBones[i]->mOffsetMatrix);
				bones.emplace(std::move(boneName), std::move(info));
			}
			else
			{
				boneID = it->second.BoneID;
			}

			const aiVertexWeight* weights = mesh->mBones[i]->mWeights;
			for (uint32_t weightIndex = 0; weightIndex < mesh->mBones[i]->mNumWeights; ++weightIndex)
			{
				const uint32_t vertexID = weights[weightIndex].mVertexId;
				const float weight = weights[weightIndex].mWeight;
				EG_CORE_ASSERT(vertexID < uint32_t(vertices.size()));

				auto& vertex = vertices[vertexID];
				{
					// Check the max bones limit
					static_assert(std::is_same<glm::vec4, decltype(SkeletalVertex::Weights)>::value);
					constexpr int maxBones = glm::vec4::length();

					bool bInserted = false;
					for (int i = 0; i < maxBones; ++i)
					{
						if (vertex.Weights[i] == 0.f)
						{
							vertex.Weights[i] = weight;
							vertex.BoneID[i] = boneID;
							bInserted = true;
							break;
						}
					}
					if (!bInserted) // If failed, try to replace a less influential weight
					{
						int leastInfluentialBoneIndex = -1;
						float minWeight = weight;
						// Find the least influential bone to replace it
						for (int i = 0; i < maxBones; ++i)
						{
							if (vertex.Weights[i] < weight)
							{
								if (vertex.Weights[i] < minWeight)
								{
									leastInfluentialBoneIndex = 1;
									minWeight = vertex.Weights[i];
								}
							}
						}
						if (leastInfluentialBoneIndex != -1)
						{
							vertex.Weights[leastInfluentialBoneIndex] = weight;
							vertex.BoneID[leastInfluentialBoneIndex] = boneID;
						}
					}
				}
			}
		}

		// Gather skeletal info
		SkeletalMeshInfo skeletal;
		ProcessBoneNode(skeletal.RootBone, scene->mRootNode);
		skeletal.InverseTransform = glm::inverse(tr);
		// Note: skeletal.BoneInfoMap will be set at the end

		return SkeletalMesh::Create(vertices, indices, skeletal);
	}

	// processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
	template <typename MeshType>
	static void ProcessNode(aiNode* node, const aiScene* scene, std::vector<Ref<MeshType>>& meshes, BonesMap& bones, const glm::mat4& tr = glm::mat4(1.f))
	{
		glm::mat4 nodeTransform = tr * ToGLM(node->mTransformation);
		// process each mesh located at the current node
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			// the node object only contains indices to index the actual objects in the scene. 
			// the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			if constexpr (std::is_same<MeshType, StaticMesh>::value)
				meshes.push_back(ProcessStaticMesh(mesh, scene, nodeTransform));
			else
				meshes.push_back(ProcessSkeletalMesh(mesh, scene, nodeTransform, bones));
		}
		// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, meshes, bones, nodeTransform);
		}
	}

	static std::vector<SkeletalMeshAnimation> ProcessAnimations(const aiScene* scene, const BonesMap& meshBoneInfoMap)
	{
		const uint32_t animationsCount = scene->mNumAnimations;
		std::vector<SkeletalMeshAnimation> animations(animationsCount);

		for (uint32_t i = 0; i < animationsCount; ++i)
		{
			aiAnimation* assimpAnimation = scene->mAnimations[i];
			auto& animation = animations[i];
			animation.Duration = (float)assimpAnimation->mDuration;
			animation.TicksPerSecond = float(assimpAnimation->mTicksPerSecond > 0.0 ? assimpAnimation->mTicksPerSecond : 25.0);

			//reading channels(bones engaged in an animation and their keyframes)
			for (uint32_t j = 0; j < assimpAnimation->mNumChannels; ++j)
			{
				auto channel = assimpAnimation->mChannels[j];
				const std::string boneName = channel->mNodeName.C_Str();

				const auto it = meshBoneInfoMap.find(boneName);
				if (it == meshBoneInfoMap.end())
					continue;

				BoneAnimation& bone = animation.Bones[boneName];
				bone.BoneID = it->second.BoneID;

				for (uint32_t pI = 0; pI < channel->mNumPositionKeys; ++pI)
				{
					auto& locationKey = bone.Locations.emplace_back();
					locationKey.Location = ToGLM(channel->mPositionKeys[pI].mValue);
					locationKey.TimeStamp = (float)channel->mPositionKeys[pI].mTime;
				}

				for (uint32_t pI = 0; pI < channel->mNumRotationKeys; ++pI)
				{
					auto& rotationKey = bone.Rotations.emplace_back();
					rotationKey.Rotation = ToGLM(channel->mRotationKeys[pI].mValue);
					rotationKey.TimeStamp = (float)channel->mRotationKeys[pI].mTime;
				}

				for (uint32_t pI = 0; pI < channel->mNumScalingKeys; ++pI)
				{
					auto& scaleKey = bone.Scales.emplace_back();
					scaleKey.Scale = ToGLM(channel->mScalingKeys[pI].mValue);
					scaleKey.TimeStamp = (float)channel->mScalingKeys[pI].mTime;
				}
			}
		}

		return animations;
	}

	template <typename MeshType, typename VertexType>
	static Ref<MeshType> MergeMeshes(const std::vector<Ref<MeshType>>& importedMeshes)
	{
		std::vector<VertexType> vertices;
		std::vector<Index> indeces;
		size_t verticesTotalSize = 0;
		size_t indecesTotalSize = 0;

		for (const auto& mesh : importedMeshes)
		{
			verticesTotalSize += mesh->GetVerticesCount();
			indecesTotalSize += mesh->GetIndicesCount();
		}

		vertices.reserve(verticesTotalSize);
		indeces.reserve(indecesTotalSize);

		for (const auto& mesh : importedMeshes)
		{
			const auto& meshVertices = mesh->GetVertices();
			const auto& meshIndeces = mesh->GetIndices();

			const size_t vSizeBeforeCopy = vertices.size();
			vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());

			const size_t iSizeBeforeCopy = indeces.size();
			indeces.insert(indeces.end(), meshIndeces.begin(), meshIndeces.end());
			const size_t iSizeAfterCopy = indeces.size();

			if (vSizeBeforeCopy)
			{
				for (size_t i = iSizeBeforeCopy; i < iSizeAfterCopy; ++i)
					indeces[i] += uint32_t(vSizeBeforeCopy);
			}
		}

		// TODO: How to merge InvAnimTransform of skeletal meshes?
		if constexpr (std::is_same<SkeletalMesh, MeshType>::value)
			return MeshType::Create(vertices, indeces, importedMeshes[0]->GetSkeletal());
		else
			return MeshType::Create(vertices, indeces);
	}

	constexpr static uint32_t s_ImportMeshFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
		| aiProcess_OptimizeGraph | aiProcess_ImproveCacheLocality | aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale;
	constexpr static uint32_t s_ImportAnimFlags = aiProcess_OptimizeGraph | aiProcess_ImproveCacheLocality | aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale;
	constexpr static uint32_t s_ImportMaterialsFlags = aiProcess_OptimizeGraph | aiProcess_RemoveRedundantMaterials;

	Ref<StaticMesh> Utils::ImportStaticMesh(const Path& path)
	{
		Assimp::Importer importer;
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		const aiScene* scene = importer.ReadFile(path.u8string(), s_ImportMeshFlags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			EG_CORE_ERROR("Failed to load Static Mesh. {0} ({1})", importer.GetErrorString(), path.u8string());
			return {};
		}

#if 0
		for (unsigned MetadataIndex = 0; MetadataIndex < scene->mMetaData->mNumProperties; ++MetadataIndex)
			EG_CORE_TRACE("{} - {}", scene->mMetaData->mKeys[MetadataIndex].C_Str(), *(uint32_t*)scene->mMetaData->mValues[MetadataIndex].mData);
#endif

		BonesMap unused1;
		std::vector<Ref<StaticMesh>> importedMeshes;
		ProcessNode(scene->mRootNode, scene, importedMeshes, unused1);
		if (!importedMeshes.empty())
		{
			if (importedMeshes.size() > 1)
			{
				importedMeshes[0] = MergeMeshes<StaticMesh, Vertex>(importedMeshes);
				importedMeshes.resize(1);
			}
			return importedMeshes[0];
		}
		else
			return {};
	}

	Ref<SkeletalMesh> Utils::ImportSkeletalMesh(const Path& path)
	{
		Assimp::Importer importer;
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		const aiScene* scene = importer.ReadFile(path.u8string(), s_ImportMeshFlags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			EG_CORE_ERROR("Failed to load Skeletal Mesh. {0} ({1})", importer.GetErrorString(), path.u8string());
			return {};
		}

		BonesMap bones;
		std::vector<Ref<SkeletalMesh>> importedMeshes;
		ProcessNode(scene->mRootNode, scene, importedMeshes, bones);
		if (importedMeshes.size() > 1)
		{
			importedMeshes[0] = MergeMeshes<SkeletalMesh, SkeletalVertex>(importedMeshes);
			importedMeshes.resize(1);
		}

		if (!importedMeshes.empty())
		{
			importedMeshes[0]->GetSkeletal().BoneInfoMap = std::move(bones);
			return importedMeshes[0];
		}
		else
			return {};
	}
	
	std::vector<SkeletalMeshAnimation> Utils::ImportAnimations(const Path& path, const Ref<SkeletalMesh>& skeletal)
	{
		Assimp::Importer importer;
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		const aiScene* scene = importer.ReadFile(path.u8string(), s_ImportAnimFlags);

		if (!scene)
		{
			EG_CORE_ERROR("Failed to load animations. {0} ({1})", importer.GetErrorString(), path.u8string());
			return {};
		}
		if (scene->mNumAnimations == 0)
		{
			EG_CORE_WARN("No animations in: {}", path.u8string());
			return {};
		}

		return ProcessAnimations(scene, skeletal->GetSkeletal().BoneInfoMap);
	}
	
	static Ref<AssetTexture2D> CreateAssetFromTexture(const Ref<Texture2D>& texture, const Path& saveTo)
	{
		if (!texture)
			return {};

		const Path outputFilename = saveTo / (texture->GetImage()->GetDebugName() + Asset::GetExtension());
		const bool bSuccess = AssetImporter::CreateFromTexture2D(texture, outputFilename);
		if (bSuccess)
		{
			Ref<AssetTexture2D> asset = Cast<AssetTexture2D>(Asset::Create(outputFilename));
			AssetManager::Register(asset);
			return asset;
		}
		return {};
	}

	static Ref<AssetTexture2D> ProcessTextureInMaterial(const Path& path, const aiScene* scene, const aiMaterial* aiMat, const Path& saveTo, aiTextureType textureType, aiTextureType fallbackType = aiTextureType_UNKNOWN)
	{
		aiString aiTexturePath;
		bool hasTexture = aiMat->GetTexture(textureType, 0, &aiTexturePath) == AI_SUCCESS;
		if (!hasTexture && fallbackType != aiTextureType_UNKNOWN)
			hasTexture = aiMat->GetTexture(fallbackType, 0, &aiTexturePath) == AI_SUCCESS;

		if (!hasTexture)
			return {};

		const Path texturePath = (aiTexturePath.C_Str());

		Ref<AssetTexture2D> assetTexture;
		if (hasTexture)
		{
			if (auto aiTexture = scene->GetEmbeddedTexture(aiTexturePath.C_Str()))
			{
				const glm::uvec2 size = { aiTexture->mWidth, aiTexture->mHeight };
				if (size.x > 0 && size.y > 0)
				{
					const std::string textureName = texturePath.stem().u8string();
					Ref<Texture2D> texture = Texture2D::Create(textureName, ImageFormat::R8G8B8A8_UNorm, size, aiTexture->pcData, {});
					assetTexture = CreateAssetFromTexture(texture, saveTo);
				}
			}
			else
			{
				auto path = texturePath.parent_path();
				path /= texturePath;
				if (AssetImporter::Import(path, saveTo, AssetType::Texture2D, {}))
				{
					Path outputFilename = saveTo / (path.stem().u8string() + Asset::GetExtension());
					assetTexture = Cast<AssetTexture2D>(Asset::Create(outputFilename));
					AssetManager::Register(assetTexture);
				}
			}
		}

		return assetTexture;
	}
	
	std::vector<Ref<AssetMaterial>> Utils::ImportMaterials(const Path& path, const Path& saveTo)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path.u8string(), s_ImportMaterialsFlags);

		if (!scene)
		{
			EG_CORE_ERROR("Failed to load materials. {0} ({1})", importer.GetErrorString(), path.u8string());
			return {};
		}

		if (scene->mNumMaterials == 0)
		{
			EG_CORE_WARN("No materials in: {}", path.u8string());
			return {};
		}

		std::vector<Ref<AssetMaterial>> materialAssets;
		materialAssets.reserve(scene->mNumMaterials); // We don't wan't to store invalid assets here

		for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
		{
			// TODO: Load color values when they're supported by eagle materials
			auto aiMaterial = scene->mMaterials[i];
			Ref<Material> material = Material::Create();

			Ref<AssetTexture2D> albedo = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
			Ref<AssetTexture2D> normal = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_NORMALS);
			Ref<AssetTexture2D> roughness = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_DIFFUSE_ROUGHNESS);
			Ref<AssetTexture2D> metalness = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_METALNESS);
			Ref<AssetTexture2D> ao = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_AMBIENT_OCCLUSION, aiTextureType_AMBIENT);
			Ref<AssetTexture2D> emissive = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE);
			Ref<AssetTexture2D> opacity = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_OPACITY);
			material->SetAlbedoAsset(albedo);
			material->SetNormalAsset(normal);
			material->SetRoughnessAsset(roughness);
			material->SetMetallnessAsset(metalness);
			material->SetAOAsset(ao);
			material->SetEmissiveAsset(emissive);
			material->SetOpacityAsset(opacity);
			if (opacity)
				material->SetBlendMode(Material::BlendMode::Translucent);

			const Path materialFilename = AssetImporter::CreateMaterial(saveTo, aiMaterial->GetName().C_Str());
			Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(Asset::Create(materialFilename));
			if (!materialAsset)
				EG_CORE_ERROR("Failed to create a material asset: {}", materialFilename);
			else
			{
				materialAsset->SetMaterial(material);
				Asset::Save(materialAsset);
			}

			materialAssets.emplace_back(std::move(materialAsset));
		}

		return materialAssets;
	}
}
