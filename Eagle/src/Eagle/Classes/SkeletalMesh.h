#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"

#include <vector>
#include <glm/glm.hpp>

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

	struct BoneNode
	{
		glm::mat4 Transformation = glm::mat4(1.f);
		std::string Name;
		std::vector<BoneNode> Children;
	};

	struct BoneInfo
	{
		glm::mat4 Offset = glm::mat4(1.f); // From model space to bone space
		uint32_t BoneID = 0;
	};

	// string - bone name
	using BonesMap = std::unordered_map<std::string, BoneInfo>;

	struct SkeletalMeshInfo
	{
		glm::mat4 InverseTransform = glm::mat4(1.f);
		BoneNode RootBone;
		BonesMap BoneInfoMap;
	};

	class SkeletalMesh
	{
	protected:
		SkeletalMesh() = default;

		SkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<Index>& indices, const SkeletalMeshInfo& skeletal)
			: m_Vertices(vertices)
			, m_Indices(indices)
			, m_Skeletal(skeletal)
		{
		}

		SkeletalMesh(const SkeletalMesh& other)
			: m_Vertices(other.m_Vertices)
			, m_Indices(other.m_Indices)
			, m_Skeletal(other.m_Skeletal)
		{}

	public:
		const Index* GetIndicesData() const { return m_Indices.data(); }
		const std::vector<Index>& GetIndices() const { return m_Indices; }
		size_t GetIndicesCount() const { return m_Indices.size(); }

		const SkeletalVertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<SkeletalVertex>& GetVertices() const { return m_Vertices; }
		size_t GetVerticesCount() const { return m_Vertices.size(); }

		const SkeletalMeshInfo& GetSkeletal() const { return m_Skeletal; }
		SkeletalMeshInfo& GetSkeletal() { return m_Skeletal; }

		// True if vertex & index buffers contain data
		bool IsValid() const { return m_Vertices.size() && m_Indices.size(); }

	public:
		static Ref<SkeletalMesh> Create(const std::vector<SkeletalVertex>& vertices, const std::vector<Index>& indices, const SkeletalMeshInfo& skeletal);
		static Ref<SkeletalMesh> Create(const Ref<SkeletalMesh>& other);

	private:
		std::vector<SkeletalVertex> m_Vertices;
		std::vector<Index> m_Indices;
		SkeletalMeshInfo m_Skeletal;
	};
}
