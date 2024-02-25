#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"

#include <vector>
#include <glm/glm.hpp>

namespace Eagle
{
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

		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<Index>& indices)
		: m_Vertices(vertices)
		, m_Indices(indices) 
		{
		}

		StaticMesh(const StaticMesh& other)
		: m_Vertices(other.m_Vertices)
		, m_Indices(other.m_Indices)
		{}

	public:
		const Index* GetIndicesData() const { return m_Indices.data(); }
		const std::vector<Index>& GetIndices() const { return m_Indices; }
		size_t GetIndicesCount() const { return m_Indices.size(); }

		const Vertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		size_t GetVerticesCount() const { return m_Vertices.size(); }

		// True if vertex & index buffers contain data
		bool IsValid() const { return m_Vertices.size() && m_Indices.size(); }

	public:
		static Ref<StaticMesh> Create(const std::vector<Vertex>& vertices, const std::vector<Index>& indices);
		static Ref<StaticMesh> Create(const Ref<StaticMesh>& other);

	private:
		std::vector<Vertex> m_Vertices;
		std::vector<Index> m_Indices;
	};
}
