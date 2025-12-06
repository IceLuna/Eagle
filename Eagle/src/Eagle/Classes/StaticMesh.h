#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"
#include "Eagle/Math/AABB.h"

#include <vector>
#include <glm/glm.hpp>

namespace Eagle
{
	// TODO: Should compress Normal, Tangent, and use fp16 for UVs?
	// The size will go down from 44bytes to 24bytes, but will it noticably affect the quality?
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec2 TexCoords;

		bool operator==(const Vertex& other) const
		{
			return Position == other.Position &&
				Normal == other.Normal &&
				Tangent == other.Tangent &&
				TexCoords == other.TexCoords;
		}

		bool operator!=(const Vertex& other) const
		{
			return !((*this) == other);
		}
	};

	class StaticMesh
	{
	protected:
		StaticMesh() = default;

		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const AABB& aabb)
			: m_Vertices(vertices)
			, m_IndicesPerMaterial(indicesPerMaterial)
			, m_AABB(aabb)
			, m_MaterialSlots((uint32_t)m_IndicesPerMaterial.size())
			, m_Materials(m_MaterialSlots)
		{
		}

		StaticMesh(const StaticMesh& other)
			: m_Vertices(other.m_Vertices)
			, m_IndicesPerMaterial(other.m_IndicesPerMaterial)
			, m_AABB(other.m_AABB)
			, m_MaterialSlots(other.m_MaterialSlots)
			, m_Materials(other.m_Materials)
		{}

	public:
		const Index* GetIndicesData(uint32_t materialIndex) const { return m_IndicesPerMaterial[materialIndex].data(); }
		const std::vector<Index>& GetIndices(uint32_t materialIndex) const { return m_IndicesPerMaterial[materialIndex]; }
		size_t GetIndicesCount(uint32_t materialIndex) const { return m_IndicesPerMaterial[materialIndex].size(); }
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

		const Vertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		size_t GetVerticesCount() const { return m_Vertices.size(); }

		const AABB& GetAABB() const { return m_AABB; }

		// True if vertex & index buffers contain data
		bool IsValid() const { return m_Vertices.size() && m_IndicesPerMaterial.size(); }

		uint32_t GetMaterialSlotsCount() const { return m_MaterialSlots; }

		void SetMaterialAsset(uint32_t index, const Ref<AssetMaterial>& asset)
		{
			m_Materials[index] = asset;
		}

		const Ref<AssetMaterial>& GetMaterialAsset(uint32_t index) { return m_Materials[index]; }

	public:
		static Ref<StaticMesh> Create(const std::vector<Vertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const AABB& aabb);
		static Ref<StaticMesh> Create(const Ref<StaticMesh>& other);

	private:
		std::vector<Vertex> m_Vertices;
		std::vector<std::vector<Index>> m_IndicesPerMaterial; // Indices per material. So, indices that correspond to `material slot = 0` is `m_IndicesPerMaterial[0]`
		AABB m_AABB;
		uint32_t m_MaterialSlots;
		std::vector<Ref<AssetMaterial>> m_Materials;
	};
}
