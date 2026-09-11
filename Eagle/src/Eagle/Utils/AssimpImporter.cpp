#include "egpch.h"
#include "AssimpImporter.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetImporter.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <glm/gtx/quaternion.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>

namespace Eagle
{
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

	static inline glm::vec2 ToGLM(const aiVector2D& vec)
	{
		return glm::vec2(vec.x, vec.y);
	}

	static inline glm::vec3 ToGLM(const aiVector3D& vec)
	{
		return glm::vec3(vec.x, vec.y, vec.z);
	}

	static inline glm::vec3 ToGLM(const aiColor3D& vec)
	{
		return glm::vec3(vec.r, vec.g, vec.b);
	}

	static inline glm::quat ToGLM(const aiQuaternion& pOrientation)
	{
		return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
	}

	static bool StartsWith(std::string_view src, std::string_view pattern)
	{
		if (src.size() < pattern.size())
			return false;

		const size_t size = pattern.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (src[i] != pattern[i])
				return false;
		}

		return true;
	}

	void BoneWeightToUnorm16(SkeletalVertex& vertex)
	{
		float fp32Weights[EG_MAX_BONES_PER_VERTEX];
		float totalWeight = 0.f;
		for (uint32_t i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
		{
			fp32Weights[i] = Utils::ToFloat32(vertex.Weights[i]);
			totalWeight += fp32Weights[i];
		}

		const float scale = UINT16_MAX / totalWeight;
		uint32_t unormWeights[EG_MAX_BONES_PER_VERTEX];
		uint32_t uSum = 0;
		uint32_t maxIdx = 0;
		for (uint32_t i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
		{
			unormWeights[i] = uint32_t(fp32Weights[i] * scale + 0.5f);
			uSum += unormWeights[i];
			if (fp32Weights[i] > fp32Weights[maxIdx])
				maxIdx = i;
		}
		unormWeights[maxIdx] += UINT16_MAX - uSum; // Forces exact sum = UINT16_MAX

		for (uint32_t i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
			vertex.Weights[i] = uint16_t(unormWeights[i]);
	}

	// This function can be used to rotate the mesh into engines coord system.
	// Otherwise, some meshes may be laying on the floor because of diff in coord system
	static glm::mat4 GetCorrectionMatrix(const aiScene* scene)
	{
		if (scene == nullptr)
			return glm::mat4(1);

		if (scene->mMetaData == nullptr)
			return glm::mat4(1);

		int32_t UpAxis = 1, UpAxisSign = 1, FrontAxis = 2, FrontAxisSign = 1, CoordAxis = 0, CoordAxisSign = 1;
		double UnitScaleFactor = 1.0;
		for (unsigned MetadataIndex = 0; MetadataIndex < scene->mMetaData->mNumProperties; ++MetadataIndex)
		{
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "UpAxis") == 0)
			{
				scene->mMetaData->Get<int32_t>(MetadataIndex, UpAxis);
			}
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "UpAxisSign") == 0)
			{
				scene->mMetaData->Get<int32_t>(MetadataIndex, UpAxisSign);
			}
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "FrontAxis") == 0)
			{
				scene->mMetaData->Get<int32_t>(MetadataIndex, FrontAxis);
			}
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "FrontAxisSign") == 0)
			{
				scene->mMetaData->Get<int32_t>(MetadataIndex, FrontAxisSign);
			}
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "CoordAxis") == 0)
			{
				scene->mMetaData->Get<int32_t>(MetadataIndex, CoordAxis);
			}
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "CoordAxisSign") == 0)
			{
				scene->mMetaData->Get<int32_t>(MetadataIndex, CoordAxisSign);
			}
			if (strcmp(scene->mMetaData->mKeys[MetadataIndex].C_Str(), "UnitScaleFactor") == 0)
			{
				scene->mMetaData->Get<double>(MetadataIndex, UnitScaleFactor);
			}
		}

		aiVector3D upVec, forwardVec, rightVec;
		upVec[UpAxis] = UpAxisSign * static_cast<float>(UnitScaleFactor);
		forwardVec[FrontAxis] = FrontAxisSign * static_cast<float>(UnitScaleFactor);
		rightVec[CoordAxis] = CoordAxisSign * (float)UnitScaleFactor;

		aiMatrix4x4 mat(rightVec.x, rightVec.y, rightVec.z, 0.0f,
			upVec.x, upVec.y, upVec.z, 0.0f,
			forwardVec.x, forwardVec.y, forwardVec.z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		return ToGLM(mat);
	}

	static void PreprocessBoneName(std::string* boneName)
	{
		const size_t nameFilterPos = (*boneName).find_first_of(':');
		if (nameFilterPos != std::string::npos)
			(*boneName) = (*boneName).substr(nameFilterPos + 1);
	}

	static Utils::StaticMeshImportData ProcessStaticMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& coordCorrection, const glm::mat4& tr, bool bResetLocation)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(mesh->mNumVertices);
		indices.reserve(mesh->mNumFaces * 3);

		glm::mat4 trAdjusted = tr;
		if (bResetLocation)
		{
			// `mesh->mAABB` reflects the mesh's actual, final local-space extents - i.e. whatever ended up
			// baked into its raw vertex data, whether that's the node's own transform or, for example, a
			// residual offset left behind by aiProcess_OptimizeGraph merging sibling nodes together.
			// Compute its center in "tr space" (before coordCorrection) and fold a compensating shift into `tr`
			// itself, so it's a single consistent transform from here on (and, for skeletal meshes, so the
			// same adjusted transform can be reused for `InverseTransform` to keep skinning correct).
			AABB localAabb;
			localAabb.Min = glm::vec4(ToGLM(mesh->mAABB.mMin), 1.f);
			localAabb.Max = glm::vec4(ToGLM(mesh->mAABB.mMax), 1.f);
			localAabb.Transform(tr);
			const glm::vec3 center = glm::vec3(localAabb.Min + localAabb.Max) * 0.5f;
			trAdjusted[3] -= glm::vec4(center, 0.f);
		}

		const glm::mat4 correctedTr = coordCorrection * trAdjusted;
		const glm::mat3 normalTr = glm::transpose(glm::inverse(glm::mat3(correctedTr)));

		// walk through each of the mesh's vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			// positions
			vertex.Position = ToGLM(mesh->mVertices[i]);
			vertex.Position = correctedTr * glm::vec4(vertex.Position, 1.f);

			// normals
			if (mesh->HasNormals())
				vertex.Normal = glm::normalize(normalTr * ToGLM(mesh->mNormals[i]));

			//tangent
			if (mesh->HasTangentsAndBitangents())
				vertex.Tangent = glm::normalize(normalTr * ToGLM(mesh->mTangents[i]));

			// texture coordinates
			if (mesh->HasTextureCoords(0)) // does the mesh contain texture coordinates?
			{
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vertex.TexCoords = ToGLM(mesh->mTextureCoords[0][i]);
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

		AABB aabb;
		aabb.Min = glm::vec4(ToGLM(mesh->mAABB.mMin), 1.f);
		aabb.Max = glm::vec4(ToGLM(mesh->mAABB.mMax), 1.f);
		aabb.Transform(correctedTr);

		Utils::StaticMeshImportData result{ StaticMesh::Create(vertices, { indices }, aabb), { mesh->mMaterialIndex } };
		result.Name = mesh->mName.C_Str();
		return result;
	}

	static bool ProcessBoneNode(BoneNode& output, const aiNode* node, const BonesMap& bones)
	{
		EG_CORE_ASSERT(node);

		std::string boneName = node->mName.C_Str();
		PreprocessBoneName(&boneName);

		const bool bShouldAdd = bones.find(boneName) != bones.end();

		output.SetName(std::move(boneName));
		output.Transformation = ToGLM(node->mTransformation);
		output.Children.reserve(node->mNumChildren);
		for (size_t i = 0; i < node->mNumChildren; ++i)
		{
			BoneNode resultNode;
			if (ProcessBoneNode(resultNode, node->mChildren[i], bones))
				output.Children.emplace_back(std::move(resultNode));
			else
				for (auto& result : resultNode.Children)
				{
					auto& inserted = output.Children.emplace_back(std::move(result));
					inserted.Transformation = resultNode.Transformation * inserted.Transformation;
				}
		}

		return bShouldAdd;
	}

	static Utils::SkeletalMeshImportData ProcessSkeletalMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& coordCorrection, const glm::mat4& tr, BonesMap& bones, bool bResetLocation)
	{
		std::vector<SkeletalVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(mesh->mNumVertices);
		indices.reserve(mesh->mNumFaces * 3);

		glm::mat4 trAdjusted = tr;
		if (bResetLocation)
		{
			// `mesh->mAABB` reflects the mesh's actual, final local-space extents - i.e. whatever ended up
			// baked into its raw vertex data, whether that's the node's own transform or, for example, a
			// residual offset left behind by aiProcess_OptimizeGraph merging sibling nodes together.
			// Computed in the same (uncorrected) `tr` space that vertex data below is kept in, and folded
			// into `tr` itself so `InverseTransform` (used for skinning) stays consistent with it.
			AABB localAabb;
			localAabb.Min = glm::vec4(ToGLM(mesh->mAABB.mMin), 1.f);
			localAabb.Max = glm::vec4(ToGLM(mesh->mAABB.mMax), 1.f);
			localAabb.Transform(tr);
			const glm::vec3 center = glm::vec3(localAabb.Min + localAabb.Max) * 0.5f;
			trAdjusted[3] -= glm::vec4(center, 0.f);
		}

		// Note: we don't apply "coordCorrection" to skeletal vertex data since it'll be applied to vertices through animation matrices inside of a shader
		// So, we only correct skeletal root bone, and animation's root bone
		const glm::mat3 normalTr = glm::transpose(glm::inverse(glm::mat3(trAdjusted)));

		// walk through each of the mesh's vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			SkeletalVertex vertex;

			// positions
			vertex.Position = ToGLM(mesh->mVertices[i]);
			vertex.Position = trAdjusted * glm::vec4(vertex.Position, 1.f);

			// normals
			if (mesh->HasNormals())
				vertex.Normal = normalTr * ToGLM(mesh->mNormals[i]);

			//tangent
			if (mesh->HasTangentsAndBitangents())
				vertex.Tangent = normalTr * ToGLM(mesh->mTangents[i]);

			// texture coordinates
			if (mesh->HasTextureCoords(0)) // does the mesh contain texture coordinates?
			{
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vertex.TexCoords = ToGLM(mesh->mTextureCoords[0][i]);
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
			PreprocessBoneName(&boneName);

			auto it = bones.find(boneName);
			if (it == bones.end())
			{
				boneID = uint32_t(bones.size());
				BoneInfo info;
				info.BoneID = boneID;
				info.Offset = ToGLM(mesh->mBones[i]->mOffsetMatrix);
				it = bones.emplace(std::move(boneName), std::move(info)).first;
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
					bool bInserted = false;
					for (int i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
					{
						if (Utils::ToFloat32(vertex.Weights[i]) == 0.f)
						{
							vertex.Weights[i] = Utils::ToFloat16(weight);
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
						for (int i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
						{
							float vertexWeight32 = Utils::ToFloat32(vertex.Weights[i]);
							if (vertexWeight32 < weight)
							{
								if (vertexWeight32 < minWeight)
								{
									leastInfluentialBoneIndex = 1;
									minWeight = vertexWeight32;
								}
							}
						}
						if (leastInfluentialBoneIndex != -1)
						{
							vertex.Weights[leastInfluentialBoneIndex] = Utils::ToFloat16(weight);
							vertex.BoneID[leastInfluentialBoneIndex] = boneID;
							bInserted = true;
						}
					}
				}
			}
		}

		for (auto& vertex : vertices)
		{
			BoneWeightToUnorm16(vertex);
		}

		// Gather skeletal info
		SkeletalMeshInfo skeletal;
		skeletal.InverseTransform = glm::inverse(trAdjusted);
		skeletal.CoordCorrection = coordCorrection;
		// Note: skeletal.BoneInfoMap and RootBone will be set at the end

		const glm::mat4 correctedTr = coordCorrection * trAdjusted;
		AABB aabb;
		aabb.Min = correctedTr * glm::vec4(ToGLM(mesh->mAABB.mMin), 1.f);
		aabb.Max = correctedTr * glm::vec4(ToGLM(mesh->mAABB.mMax), 1.f);
		Utils::SkeletalMeshImportData result{ SkeletalMesh::Create(vertices, { indices }, skeletal, aabb), { mesh->mMaterialIndex } };
		result.Name = mesh->mName.C_Str();
		return result;
	}

	// processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
	// @bResetLocation. If true, each mesh gets recentered so it ends up at (0, 0, 0) instead of keeping its
	//		original place in the scene. Rotation & scale are kept. Only makes sense when meshes are being imported
	//		as separate assets.
	// Note: this can't simply zero out the node's translation - some postprocess steps (e.g. aiProcess_OptimizeGraph)
	// can merge/restructure nodes and bake a mesh's offset directly into its raw vertex data instead of leaving it
	// in a node transform. Process*Mesh() below derives the actual offset to remove from `mesh->mAABB` itself,
	// which always reflects the mesh's real, final local-space extents regardless of where that offset came from.
	template <typename MeshImportData>
	static void ProcessNode(aiNode* node, const aiScene* scene, std::vector<MeshImportData>& meshes, BonesMap& bones, const glm::mat4& coordCorrection, bool bResetLocation, const glm::mat4& tr = glm::mat4(1.f))
	{
		// These meshes represent some extra data that shouldn't be imported/rendered. For example, collision meshes.
		// TODO: Add support for collision meshes during import
		static const std::vector<const char*> prefixes = {
			"UCX_", "UBX_", "USP_", "UCP_",
			"SOCKET_",
			"COL_", "Collision_",
			"PHYS_", "Physics_",
			"NAV_", "NavMesh_",
			"TRIGGER_", "HELPER_"
		};

		glm::mat4 nodeTransform = tr * ToGLM(node->mTransformation);
		// process each mesh located at the current node
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			// the node object only contains indices to index the actual objects in the scene. 
			// the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

			std::string_view name = mesh->mName.C_Str();
			bool bHelperMesh = false;
			for (const auto& p : prefixes)
			{
				if (StartsWith(name, p))
				{
					bHelperMesh = true;
					break;
				}
			}

			if (bHelperMesh)
				continue;

			if constexpr (std::is_same<MeshImportData, Utils::StaticMeshImportData>::value)
				meshes.push_back(ProcessStaticMesh(mesh, scene, coordCorrection, nodeTransform, bResetLocation));
			else
				meshes.push_back(ProcessSkeletalMesh(mesh, scene, coordCorrection, nodeTransform, bones, bResetLocation));
		}
		// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, meshes, bones, coordCorrection, bResetLocation, nodeTransform);
		}
	}

	static std::vector<SkeletalMeshAnimation> ProcessAnimations(const aiScene* scene, const SkeletalMeshInfo& skeletalInfo, const RootMotionMode& rootMotionMode)
	{
		const auto& meshBoneInfoMap = skeletalInfo.GetBoneInfoMap();
		const uint32_t animationsCount = scene->mNumAnimations;
		std::vector<SkeletalMeshAnimation> animations(animationsCount);
		const glm::mat4 correction = GetCorrectionMatrix(scene);

		for (uint32_t i = 0; i < animationsCount; ++i)
		{
			aiAnimation* assimpAnimation = scene->mAnimations[i];
			auto& animation = animations[i];
			animation.Duration = (float)assimpAnimation->mDuration;
			animation.TicksPerSecond = float(assimpAnimation->mTicksPerSecond > 0.0 ? assimpAnimation->mTicksPerSecond : 25.0);

			//reading channels(bones engaged in an animation and their keyframes)
			BonesAnimMap animBones;
			for (uint32_t j = 0; j < assimpAnimation->mNumChannels; ++j)
			{
				auto channel = assimpAnimation->mChannels[j];
				std::string boneName = channel->mNodeName.C_Str();
				PreprocessBoneName(&boneName);

				const auto it = meshBoneInfoMap.find(boneName);
				if (it == meshBoneInfoMap.end())
					continue;

				BoneAnimation& bone = animBones[boneName];
				bone.BoneID = it->second.BoneID;
				bone.Locations.reserve(channel->mNumPositionKeys);
				bone.Rotations.reserve(channel->mNumRotationKeys);
				bone.Scales.reserve(channel->mNumScalingKeys);

				// Process positions
				{
					uint32_t pI = 0;
					if (channel->mNumPositionKeys > 0 && channel->mPositionKeys[pI].mTime > 0.0)
					{
						{
							auto& locationKey = bone.Locations.emplace_back();
							locationKey.Location = ToGLM(channel->mPositionKeys[pI].mValue);
							locationKey.TimeStamp = 0.f;
							if (boneName == skeletalInfo.RootBone.GetName())
								locationKey.Location = correction * glm::vec4(locationKey.Location, 1.f);
						}

						auto& locationKey = bone.Locations.emplace_back();
						locationKey.Location = ToGLM(channel->mPositionKeys[pI].mValue);
						locationKey.TimeStamp = (float)channel->mPositionKeys[pI].mTime;
						if (boneName == skeletalInfo.RootBone.GetName())
							locationKey.Location = correction * glm::vec4(locationKey.Location, 1.f);
						pI++;
					}

					for (; pI < channel->mNumPositionKeys; ++pI)
					{
						auto& locationKey = bone.Locations.emplace_back();
						locationKey.Location = ToGLM(channel->mPositionKeys[pI].mValue);
						locationKey.TimeStamp = (float)channel->mPositionKeys[pI].mTime;
						if (boneName == skeletalInfo.RootBone.GetName())
							locationKey.Location = correction * glm::vec4(locationKey.Location, 1.f);
					}
				}

				// Process rotations
				{
					uint32_t pI = 0;
					if (channel->mNumRotationKeys > 0 && channel->mRotationKeys[pI].mTime > 0.0)
					{
						{
							auto& rotationKey = bone.Rotations.emplace_back();
							rotationKey.Rotation = ToGLM(channel->mRotationKeys[pI].mValue);
							rotationKey.TimeStamp = 0.f;
							if (boneName == skeletalInfo.RootBone.GetName())
								rotationKey.Rotation = glm::toQuat(glm::mat3(correction) * glm::toMat3(rotationKey.Rotation));
						}

						auto& rotationKey = bone.Rotations.emplace_back();
						rotationKey.Rotation = ToGLM(channel->mRotationKeys[pI].mValue);
						rotationKey.TimeStamp = (float)channel->mRotationKeys[pI].mTime;
						if (boneName == skeletalInfo.RootBone.GetName())
							rotationKey.Rotation = glm::toQuat(glm::mat3(correction) * glm::toMat3(rotationKey.Rotation));
						pI++;
					}

					for (; pI < channel->mNumRotationKeys; ++pI)
					{
						auto& rotationKey = bone.Rotations.emplace_back();
						rotationKey.Rotation = ToGLM(channel->mRotationKeys[pI].mValue);
						rotationKey.TimeStamp = (float)channel->mRotationKeys[pI].mTime;
						if (boneName == skeletalInfo.RootBone.GetName())
							rotationKey.Rotation = glm::toQuat(glm::mat3(correction) * glm::toMat3(rotationKey.Rotation));
					}
				}

				// Process scales
				{
					uint32_t pI = 0;
					if (channel->mNumScalingKeys > 0 && channel->mScalingKeys[pI].mTime > 0.0)
					{
						{
							auto& scaleKey = bone.Scales.emplace_back();
							scaleKey.Scale = ToGLM(channel->mScalingKeys[pI].mValue);
							scaleKey.TimeStamp = 0.f;
						}

						auto& scaleKey = bone.Scales.emplace_back();
						scaleKey.Scale = ToGLM(channel->mScalingKeys[pI].mValue);
						scaleKey.TimeStamp = (float)channel->mScalingKeys[pI].mTime;
						pI++;
					}

					for (; pI < channel->mNumScalingKeys; ++pI)
					{
						auto& scaleKey = bone.Scales.emplace_back();
						scaleKey.Scale = ToGLM(channel->mScalingKeys[pI].mValue);
						scaleKey.TimeStamp = (float)channel->mScalingKeys[pI].mTime;
					}
				}
			}

			animation.SetAnimationBones(std::move(animBones));
			if (rootMotionMode != RootMotionMode::Disabled)
				animation.ExtractRootMotion(skeletalInfo, rootMotionMode);
		}

		return animations;
	}

	template <typename MeshImportData, typename VertexType>
	static MeshImportData MergeMeshes(const std::vector<MeshImportData>& importedMeshes)
	{
		std::vector<VertexType> vertices;
		size_t verticesTotalSize = 0;

		// Key - Material Index; Value - index into `importedMeshes`
		std::map<uint32_t, std::vector<size_t>> meshesPerMaterial;

		for (size_t i = 0; i < importedMeshes.size(); ++i)
		{
			const auto& data = importedMeshes[i];
			const auto& mesh = data.Mesh;
			verticesTotalSize += mesh->GetVerticesCount();

			meshesPerMaterial[data.MaterialIndices[0]].push_back(i);
		}
		vertices.reserve(verticesTotalSize);

		const size_t meshesPerMaterialCount = meshesPerMaterial.size();
		std::vector<std::vector<Index>> indicesPerMaterial(meshesPerMaterialCount);
		// Reserve space for indices
		{
			auto it = meshesPerMaterial.begin();
			size_t i = 0;
			for (; i < meshesPerMaterialCount; ++i, ++it)
			{
				size_t indicesTotalSize = 0;
				for (const auto& meshIndex : it->second)
				{
					const auto& mesh = importedMeshes[meshIndex].Mesh;
					indicesTotalSize += mesh->GetIndicesCount(0);
				}
				indicesPerMaterial[i].reserve(indicesTotalSize);
			}
		}

		// Merge vertices
		std::vector<size_t> indicesOffsets(importedMeshes.size());
		for (size_t i = 0; i < importedMeshes.size(); ++i)
		{
			const auto& mesh = importedMeshes[i].Mesh;
			const auto& meshVertices = mesh->GetVertices();

			const size_t vSizeBeforeCopy = vertices.size();
			indicesOffsets[i] = vSizeBeforeCopy;
			vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
		}

		// Merge indices
		{
			auto it = meshesPerMaterial.begin();
			size_t i = 0;
			for (; i < meshesPerMaterialCount; ++i, ++it)
			{
				for (const auto& meshIndex : it->second)
				{
					const auto& mesh = importedMeshes[meshIndex].Mesh;
					const auto& meshIndeces = mesh->GetIndices(0);
					auto& indices = indicesPerMaterial[i];

					const size_t iSizeBeforeCopy = indices.size();
					indices.insert(indices.end(), meshIndeces.begin(), meshIndeces.end());
					const size_t iSizeAfterCopy = indices.size();

					const size_t vSizeBeforeCopy = indicesOffsets[meshIndex];
					if (vSizeBeforeCopy)
					{
						for (size_t j = iSizeBeforeCopy; j < iSizeAfterCopy; ++j)
							indices[j] += uint32_t(vSizeBeforeCopy);
					}
				}
			}
		}

		AABB aabb{};
		if (const size_t count = importedMeshes.size())
		{
			for (size_t i = 0; i < count; ++i)
				aabb.Grow(importedMeshes[i].Mesh->GetAABB());
		}

		std::vector<uint32_t> materialIndices;
		materialIndices.reserve(meshesPerMaterialCount);
		for (const auto& [materialIndex, meshIndices] : meshesPerMaterial)
			materialIndices.push_back(materialIndex);

		// TODO: How to merge InvAnimTransform of skeletal meshes?
		if constexpr (std::is_same<Utils::SkeletalMeshImportData, MeshImportData>::value)
			return Utils::SkeletalMeshImportData{ SkeletalMesh::Create(vertices, indicesPerMaterial, importedMeshes[0].Mesh->GetSkeletalMeshInfo(), aabb), materialIndices };
		else
			return Utils::StaticMeshImportData{ StaticMesh::Create(vertices, indicesPerMaterial, aabb), materialIndices };
	}

	constexpr static uint32_t s_ImportMeshFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
		| aiProcess_OptimizeGraph | aiProcess_ImproveCacheLocality | aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale | aiProcess_GenBoundingBoxes | aiProcess_FlipUVs;
	constexpr static uint32_t s_ImportAnimFlags = aiProcess_OptimizeGraph | aiProcess_ImproveCacheLocality | aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale;
	constexpr static uint32_t s_ImportMaterialsFlags = aiProcess_OptimizeGraph | aiProcess_RemoveRedundantMaterials;

	std::vector<Utils::StaticMeshImportData> Utils::ImportStaticMesh(const Path& path, bool bCombineMeshes, bool bResetLocation)
	{
		Assimp::Importer importer;
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		ScopedDataBuffer fileBinary = FileSystem::Read(path);
		const aiScene* scene = importer.ReadFileFromMemory(fileBinary.Data(), fileBinary.Size(), s_ImportMeshFlags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			EG_CORE_ERROR("Failed to load Static Mesh. {0} ({1})", importer.GetErrorString(), path);
			return {};
		}

#if 0
		for (unsigned MetadataIndex = 0; MetadataIndex < scene->mMetaData->mNumProperties; ++MetadataIndex)
			EG_CORE_TRACE("{} - {}", scene->mMetaData->mKeys[MetadataIndex].C_Str(), *(uint32_t*)scene->mMetaData->mValues[MetadataIndex].mData);
#endif

		const glm::mat4 coordCorrection = GetCorrectionMatrix(scene);
		BonesMap unused1;
		std::vector<Utils::StaticMeshImportData> importedMeshes;
		// Resetting each mesh's location only makes sense when they're kept as separate, independent assets;
		// doing it while combining would misalign the parts of the resulting merged mesh
		ProcessNode(scene->mRootNode, scene, importedMeshes, unused1, coordCorrection, bResetLocation && !bCombineMeshes);
		if (importedMeshes.empty())
			return {};

		if (bCombineMeshes && importedMeshes.size() > 1)
		{
			Utils::StaticMeshImportData merged = MergeMeshes<Utils::StaticMeshImportData, Vertex>(importedMeshes);
			merged.Name = Utils::AsString(path.stem());
			importedMeshes.clear();
			importedMeshes.push_back(std::move(merged));
		}

		return importedMeshes;
	}

	std::vector<Utils::SkeletalMeshImportData> Utils::ImportSkeletalMesh(const Path& path, bool bCombineMeshes, bool bResetLocation)
	{
		Assimp::Importer importer;
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		ScopedDataBuffer fileBinary = FileSystem::Read(path);
		const aiScene* scene = importer.ReadFileFromMemory(fileBinary.Data(), fileBinary.Size(), s_ImportMeshFlags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			EG_CORE_ERROR("Failed to import Skeletal Mesh. {0} ({1})", importer.GetErrorString(), path);
			return {};
		}

		const glm::mat4 coordCorrection = GetCorrectionMatrix(scene);
		BonesMap bones;
		std::vector<Utils::SkeletalMeshImportData> importedMeshes;
		// Resetting each mesh's location only makes sense when they're kept as separate, independent assets;
		// doing it while combining would misalign the parts of the resulting merged mesh
		ProcessNode(scene->mRootNode, scene, importedMeshes, bones, coordCorrection, bResetLocation && !bCombineMeshes);
		if (bones.empty())
		{
			EG_CORE_ERROR("Failed to import Skeletal Mesh. It has no bones ({})", path);
			return {};
		}
		if (importedMeshes.empty())
			return {};

		if (bCombineMeshes && importedMeshes.size() > 1)
		{
			Utils::SkeletalMeshImportData merged = MergeMeshes<Utils::SkeletalMeshImportData, SkeletalVertex>(importedMeshes);
			merged.Name = Utils::AsString(path.stem());
			importedMeshes.clear();
			importedMeshes.push_back(std::move(merged));
		}

		// All meshes in the file (combined or not) come from the same armature, so they share one skeleton.
		// Finalize bone/root-bone data using the first mesh, then propagate the exact same result to the rest.
		auto& skeletalInfo = importedMeshes[0].Mesh->GetSkeletalMeshInfo();
		skeletalInfo.SetBonesInfoMap(std::move(bones)); // `bones` is consumed here; use `skeletalInfo.GetBoneInfoMap()` as the source from now on
		ProcessBoneNode(skeletalInfo.RootBone, scene->mRootNode, skeletalInfo.GetBoneInfoMap());
		skeletalInfo.RootBone.Transformation = coordCorrection * skeletalInfo.RootBone.Transformation;

		// It's possible that we have two root bones: one from the mesh, and one from assimp. So keep only one of them
		{
			const bool bValidBone = skeletalInfo.IsValid(skeletalInfo.FindBoneInfo(skeletalInfo.RootBone.GetName()));
			if (!bValidBone)
			{
				// It's assimp root, delete it if we have another root
				if (skeletalInfo.RootBone.Children.size() == 1)
				{
					auto children = std::move(skeletalInfo.RootBone.Children);
					children[0].Transformation = skeletalInfo.RootBone.Transformation * children[0].Transformation;
					skeletalInfo.RootBone = children[0];
				}
			}
		}

		importedMeshes[0].Mesh->RegenerateRagdollData(importedMeshes[0].Mesh->GetMinRagdollBoneSize());

		// Propagate the same finalized skeleton to every other mesh (relevant only when bCombineMeshes == false).
		for (size_t i = 1; i < importedMeshes.size(); ++i)
		{
			auto& otherInfo = importedMeshes[i].Mesh->GetSkeletalMeshInfo();
			otherInfo.SetBonesInfoMap(BonesMap(skeletalInfo.GetBoneInfoMap()));
			otherInfo.RootBone = skeletalInfo.RootBone;
			importedMeshes[i].Mesh->RegenerateRagdollData(importedMeshes[i].Mesh->GetMinRagdollBoneSize());
		}

		return importedMeshes;
	}

	std::vector<SkeletalMeshAnimation> Utils::ImportAnimations(const Path& path, const Ref<SkeletalMesh>& skeletal, const RootMotionMode& rootMotionMode)
	{
		Assimp::Importer importer;
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		ScopedDataBuffer fileBinary = FileSystem::Read(path);
		const aiScene* scene = importer.ReadFileFromMemory(fileBinary.Data(), fileBinary.Size(), s_ImportAnimFlags);

		if (!scene)
		{
			EG_CORE_ERROR("Failed to load animations. {0} ({1})", importer.GetErrorString(), path);
			return {};
		}
		if (scene->mNumAnimations == 0)
		{
			EG_CORE_WARN("No animations in: {}", path);
			return {};
		}

		return ProcessAnimations(scene, skeletal->GetSkeletalMeshInfo(), rootMotionMode);
	}

	static Ref<AssetTexture2D> CreateAssetFromEncoded(DataBuffer buffer, const Path& saveTo, const std::string& filename, bool bNormalMap)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);

		int width, height, channels;
		stbi_info_from_memory((uint8_t*)buffer.Data, (int)buffer.Size, &width, &height, &channels);

		AssetImportSettings settings{};
		auto& textureSettings = settings.Texture2DSettings;
		textureSettings.bNormalMap = bNormalMap;
		textureSettings.ImportFormat = ChannelsToAssetTexture2DFormat(channels);
		textureSettings.MipsCount = CalculateMipCount(width, height);

		return AssetImporter::ImportTexture2DFromMemory(buffer, saveTo, filename, textureSettings);
	}

	static Ref<AssetTexture2D> ProcessTextureInMaterial(const Path& path, const aiScene* scene, const aiMaterial* aiMat, const Path& saveTo, aiTextureType textureType, aiTextureType fallbackType = aiTextureType_UNKNOWN)
	{
		aiString aiTexturePath;
		bool hasTexture = aiMat->GetTexture(textureType, 0, &aiTexturePath) == AI_SUCCESS;
		if (!hasTexture && fallbackType != aiTextureType_UNKNOWN)
			hasTexture = aiMat->GetTexture(fallbackType, 0, &aiTexturePath) == AI_SUCCESS;

		if (!hasTexture)
			return {};

		const bool bNormalMap = textureType == aiTextureType_NORMALS;
		const Path filename = Path(aiTexturePath.C_Str()).filename();
		Path texturePath = path.parent_path() / filename;

		Ref<AssetTexture2D> assetTexture;
		if (hasTexture)
		{
			if (auto aiTexture = scene->GetEmbeddedTexture(aiTexturePath.C_Str()))
			{
				glm::uvec2 size = { aiTexture->mWidth, aiTexture->mHeight };
				// aiTexture->mHeight can be zero, in this case `aiTexture->pcData` is not RGB values but compressed JPEG/PNG data
				if (size.y == 0u)
				{
					const std::string textureName = Utils::AsString(texturePath.stem());
					assetTexture = CreateAssetFromEncoded(DataBuffer(aiTexture->pcData, aiTexture->mWidth), saveTo, textureName, bNormalMap);
				}
				else if (size.x > 0 && size.y > 0 && (strcmp(aiTexture->achFormatHint, "rgba8888") == 0))
				{
					constexpr uint32_t numChannels = 4;
					constexpr size_t texelSize = sizeof(uint8_t) * numChannels; // RGBA8
					const size_t memSize = size.x * size.y * texelSize;
					DataBuffer decoded = { aiTexture->pcData, memSize };

					// We need an encoded (png/jpg etc) image data for asset creation
					const ScopedDataBuffer png = Utils::ToPNG(decoded, size, numChannels);
					const std::string textureName = Utils::AsString(texturePath.stem());
					assetTexture = CreateAssetFromEncoded(png.GetDataBuffer(), saveTo, textureName, bNormalMap);
				}
				else
				{
					EG_CORE_ERROR("Failed to read an embedded texture: {}", filename);
				}
			}
			else
			{
				if (!std::filesystem::exists(texturePath))
				{
					texturePath = texturePath.parent_path() / "textures" / texturePath.filename();
				}

				AssetImportSettings importSettings{};
				{
					auto& settings = importSettings.Texture2DSettings;
					settings.bNormalMap = Utils::IsNormalMap(texturePath);

					int comp = 1;
					int unused = 0;
					stbi_info(Utils::AsString(texturePath).c_str(), &unused, &unused, &comp);
					settings.ImportFormat = ChannelsToAssetTexture2DFormat(comp);
				}
				if (AssetImporter::Import(texturePath, saveTo, AssetType::Texture2D, importSettings))
				{
					Path outputFilename = saveTo / Utils::AsPath(Utils::AsString(texturePath.stem()) + Asset::GetExtension());
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
		ScopedDataBuffer fileBinary = FileSystem::Read(path);
		const aiScene* scene = importer.ReadFileFromMemory(fileBinary.Data(), fileBinary.Size(), s_ImportMaterialsFlags);

		if (!scene)
		{
			EG_CORE_ERROR("Failed to load materials. {0} ({1})", importer.GetErrorString(), path);
			return {};
		}

		if (scene->mNumMaterials == 0)
		{
			EG_CORE_WARN("No materials in: {}", path);
			return {};
		}

		std::vector<Ref<AssetMaterial>> materialAssets;
		materialAssets.reserve(scene->mNumMaterials); // We don't wan't to store invalid assets here

		for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
		{
			auto aiMaterial = scene->mMaterials[i];
			Ref<Material> material = Material::Create();

			Ref<AssetTexture2D> albedo = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
			Ref<AssetTexture2D> normal = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_NORMALS);
			Ref<AssetTexture2D> roughness = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_DIFFUSE_ROUGHNESS);
			Ref<AssetTexture2D> metalness = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_METALNESS);
			Ref<AssetTexture2D> ao = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_AMBIENT_OCCLUSION, aiTextureType_AMBIENT);
			Ref<AssetTexture2D> emissive = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE);
			Ref<AssetTexture2D> opacity = ProcessTextureInMaterial(path, scene, aiMaterial, saveTo, aiTextureType_OPACITY);
			material->SetNormalAsset(normal);
			if (ao)
			{
				material->SetAOAsset(ao);
				material->SetRawAOUsed(false);
			}
			if (opacity)
			{
				material->SetRawOpacityUsed(false);
				material->SetBlendMode(MaterialBlendMode::Translucent);
			}

			{
				aiColor3D aiValue;

				if (albedo)
				{
					material->SetAlbedoAsset(albedo);
					material->SetRawAlbedoUsed(false);
				}
				else
				{
					if (aiMaterial->Get(AI_MATKEY_BASE_COLOR, aiValue) == AI_SUCCESS || aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiValue) == AI_SUCCESS)
					{
						material->SetAlbedo(ToGLM(aiValue));
						material->SetRawAlbedoUsed(true);
					}
				}

				if (emissive)
				{
					material->SetEmissiveAsset(emissive);
					material->SetRawEmissiveUsed(false);
				}
				else
				{
					if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiValue) == AI_SUCCESS)
					{
						material->SetEmissive(ToGLM(aiValue));
						material->SetRawEmissiveUsed(true);
					}
				}

				if (aiMaterial->Get(AI_MATKEY_EMISSIVE_INTENSITY, aiValue) == AI_SUCCESS)
				{
					material->SetEmissiveIntensity(ToGLM(aiValue));
				}

				if (roughness)
				{
					material->SetRoughnessAsset(roughness);
					material->SetRawRoughnessUsed(false);
				}
				else
				{
					if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, aiValue) == AI_SUCCESS)
					{
						material->SetRoughness(aiValue.r);
						material->SetRawRoughnessUsed(true);
					}
				}

				if (metalness)
				{
					material->SetMetalnessAsset(metalness);
					material->SetRawMetalnessUsed(false);
				}
				else
				{
					if (aiMaterial->Get(AI_MATKEY_REFLECTIVITY, aiValue) == AI_SUCCESS || aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, aiValue) == AI_SUCCESS)
					{
						material->SetMetalness(aiValue.r);
						material->SetRawMetalnessUsed(true);
					}
				}

				if (aiMaterial->Get(AI_MATKEY_OPACITY, aiValue) == AI_SUCCESS)
				{
					material->SetOpacity(aiValue.r);
					if (!opacity && aiValue.r < 1.f)
					{
						material->SetBlendMode(MaterialBlendMode::Translucent);
						material->SetRawOpacityUsed(true);
					}
				}
				else if (aiMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, aiValue) == AI_SUCCESS)
				{
					material->SetOpacity(1.f - aiValue.r);
					if (!opacity && (1.f - aiValue.r) < 1.f)
					{
						material->SetBlendMode(MaterialBlendMode::Translucent);
						material->SetRawOpacityUsed(true);
					}
				}
			}

			// We create a material, but don't save it to disk yet.
			// It should be saved by the called if necessary
			const Path materialFilename = Utils::GetUniqueAssetFilepath(saveTo, aiMaterial->GetName().C_Str());
			Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(Asset::Create(Serializer::SerializeAssetMaterial(nullptr), materialFilename));
			materialAsset->SetMaterial(material);
			materialAssets.emplace_back(std::move(materialAsset));
		}

		return materialAssets;
	}
}
