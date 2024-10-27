#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"
#include "Eagle/Math/AABB.h"
#include "Eagle/Math/Transform.h"

#include <vector>
#include <glm/glm.hpp>

#define EG_MAX_BONES_PER_VERTEX 4

namespace Eagle
{
	struct SkeletalVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec2 TexCoords;
		glm::vec4 Weights = glm::vec4{ 0.f }; // EG_MAX_BONES_PER_VERTEX
		glm::uvec4 BoneID = glm::uvec4{ 0u };

		bool operator==(const SkeletalVertex& other) const
		{
			return Position == other.Position &&
				Normal == other.Normal &&
				Tangent == other.Tangent &&
				TexCoords == other.TexCoords &&
				Weights[0] == other.Weights[0] &&
				Weights[1] == other.Weights[1] &&
				Weights[2] == other.Weights[2] &&
				Weights[3] == other.Weights[3] &&
				BoneID[0] == other.BoneID[0] &&
				BoneID[1] == other.BoneID[1] &&
				BoneID[2] == other.BoneID[2] &&
				BoneID[3] == other.BoneID[3];
		}

		bool operator!=(const SkeletalVertex& other) const
		{
			return !((*this) == other);
		}
	};
	static_assert(EG_MAX_BONES_PER_VERTEX == decltype(SkeletalVertex::Weights)::length());

	struct BoneNode
	{
		glm::mat4 Transformation = glm::mat4(1.f);
		std::string Name;
		std::vector<BoneNode> Children;

		bool bVirtualBone = false;
	};

	struct BoneInfo
	{
		glm::mat4 Offset = glm::mat4(1.f); // From model space to bone space
		uint32_t BoneID = 0;
	};

	// TODO: Optimize these structs by using `std::vector` and storing indices into it, instead of making a look-up into a hash map
	// string - bone name
	using BonesMap = std::unordered_map<std::string, BoneInfo>;

	struct SkeletalMeshInfo
	{
		glm::mat4 InverseTransform = glm::mat4(1.f);
		BoneNode RootBone;
		BonesMap BoneInfoMap;
	};

	struct SkeletalRagdollBones
	{
		glm::mat4 LocalTransform = glm::mat4(1.f);
		Transform UserOffset;
		std::string Name;
		AABB AABB;
		std::vector<SkeletalRagdollBones> Children;
	};

	class SkeletalMesh
	{
	protected:
		SkeletalMesh() = default;
		SkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
			const std::unordered_map<std::string, Transform>& ragdollOffsets = {}, float minRagdollBoneSize = 0.1f, float maxRagdollTwist = 22.5f, float maxRagdollSwing = 45.f);
		SkeletalMesh(const SkeletalMesh& other);

	public:
		const Index* GetIndicesData(uint32_t materialIndex) const { return m_IndicesPerMaterial[materialIndex].data(); }
		const std::vector<Index>& GetIndices(uint32_t materialIndex) const { return m_IndicesPerMaterial[materialIndex]; }
		size_t GetIndicesCount(uint32_t materialIndex) const { return m_IndicesPerMaterial[materialIndex].size(); }

		const SkeletalVertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<SkeletalVertex>& GetVertices() const { return m_Vertices; }
		size_t GetVerticesCount() const { return m_Vertices.size(); }
		size_t GetIndicesOffset(uint32_t materialIndex) const
		{
			size_t offset = 0;
			for (int i = int(materialIndex) - 1; i >= 0; --i)
				offset += GetIndicesCount(i);
			return offset;
		}
		size_t GetTotalIndicesCount() const
		{
			size_t result = 0;
			for (const auto& indices : m_IndicesPerMaterial)
				result += indices.size();

			return result;
		}

		const AABB& GetAABB() const { return m_AABB; }

		const SkeletalMeshInfo& GetSkeletalMeshInfo() const { return m_Skeletal; }
		SkeletalMeshInfo& GetSkeletalMeshInfo() { return m_Skeletal; }

		// True if vertex & index buffers contain data
		bool IsValid() const { return m_Vertices.size() && m_IndicesPerMaterial.size(); }
		
		uint32_t GetMaterialSlotsCount() const { return m_MaterialSlots; }

		void SetMaterialAsset(uint32_t index, const Ref<AssetMaterial>& asset)
		{
			m_Materials[index] = asset;
		}

		const Ref<AssetMaterial>& GetMaterialAsset(uint32_t index) { return m_Materials[index]; }

		void RegenerateRagdollData(float minBoneSize);
		void SetRagdollMaxTwist(float twist) { m_MaxRagdollTwist = twist; }
		void SetRagdollMaxSwing(float swing) { m_MaxRagdollSwing = swing; }
		const SkeletalRagdollBones& GetRagdollRoot() const { return m_RagdollRoot; }
		SkeletalRagdollBones& GetRagdollRoot() { return m_RagdollRoot; }
		float GetMinRagdollBoneSize() const { return m_MinRagdollBoneSize; }
		float GetRagdollMaxTwist() const { return m_MaxRagdollTwist; }
		float GetRagdollMaxSwing() const { return m_MaxRagdollSwing; }

	public:
		// @ragdollOffsets. Can be used to override `UserOffset` inside `SkeletalRagdollBones`. std::string is a bone name which `UserOffset` needs to be overwritten
		static Ref<SkeletalMesh> Create(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& m_IndicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
			const std::unordered_map<std::string, Transform>& ragdollOffsets = {}, float minRagdollBoneSize = 0.1f, float maxRagdollTwist = 22.5f, float maxRagdollSwing = 45.f);
		static Ref<SkeletalMesh> Create(const Ref<SkeletalMesh>& other);

	private:
		std::vector<SkeletalVertex> m_Vertices;
		std::vector<std::vector<Index>> m_IndicesPerMaterial; // Indices of different materials are split. So, indices that correspond to `material slot = 0` is `m_IndicesPerMaterial[0]
		SkeletalMeshInfo m_Skeletal;
		AABB m_AABB;
		uint32_t m_MaterialSlots;
		std::vector<Ref<AssetMaterial>> m_Materials;
		SkeletalRagdollBones m_RagdollRoot;
		float m_MinRagdollBoneSize = 0.1f;
		float m_MaxRagdollTwist = 22.5f;
		float m_MaxRagdollSwing = 45.0f;
	};
}
