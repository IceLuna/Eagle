#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "Eagle/Renderer/Material.h"

namespace Eagle
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
	};

	class StaticMesh
	{
	public:
		StaticMesh() = default;

		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
		: m_Vertices(vertices)
		, m_Indices(indices) 
		{}

		const uint32_t* GetIndecesData() const { return m_Indices.data(); }
		const std::vector<uint32_t>& GetIndeces() const { return m_Indices; }
		uint32_t GetIndecesCount() const { return (uint32_t)m_Indices.size(); }

		const Vertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		uint32_t GetVerticesCount() const { return (uint32_t)m_Vertices.size(); }

	public:
		Eagle::Material Material;
	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
	};
}
