#include "egpch.h"
#include "NavigationDebugDraw.h"

namespace Eagle::AINavigation
{
	static glm::vec3 rcCol(uint32_t col) noexcept
	{
		glm::vec3 result;

		result.r = float(col % 256);
		col /= 256;
		result.g = float(col % 256);
		col /= 256;
		result.b = float(col % 256);
		col /= 256;
		//result.a = col % 256;
		result /= 255.f;

		return result;
	}

	void DebugDraw::begin(duDebugDrawPrimitives prim, float size)
	{
		prim_ = prim;
		m_VertexIndex = 0u;
	}

	void DebugDraw::end()
	{
	}

	void DebugDraw::vertex(const float* pos, unsigned int color)
	{
		vertex(pos[0], pos[1], pos[2], color);
	}

	void DebugDraw::vertex(const float* pos, unsigned int color, const float* uv)
	{
		vertex(pos[0], pos[1], pos[2], color);
	}

	void DebugDraw::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
	{
		vertex(x, y, z, color);
	}

	void DebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
	{
		if (prim_ == DU_DRAW_POINTS)
		{
			auto& line = Lines.emplace_back();
			line.Start.Location = glm::vec3(x, y, z);
			line.Start.Color = rcCol(color);
			line.End.Location = glm::vec3(x, y, z);
			line.End.Color = rcCol(color);
		}
		else if (prim_ == DU_DRAW_LINES)
		{
			if (m_VertexIndex == 0)
			{
				auto& line = Lines.emplace_back();
				line.Start.Location = glm::vec3(x, y, z);
				line.Start.Color = rcCol(color);
				m_VertexIndex = (m_VertexIndex + 1) % 2;
			}
			else if (m_VertexIndex == 1)
			{
				auto& line = Lines.back();
				line.End.Location = glm::vec3(x, y, z);
				line.End.Color = rcCol(color);
				m_VertexIndex = (m_VertexIndex + 1) % 2;
			}
		}
		else if (prim_ == DU_DRAW_TRIS)
		{
			if (m_VertexIndex == 0)
				Triangles.emplace_back();

			auto& vertex = Triangles.back().Vertices[m_VertexIndex];
			vertex.Location = glm::vec3(x, y, z);
			vertex.Color = rcCol(color);
			m_VertexIndex = (m_VertexIndex + 1) % 3;
		}
		else if (prim_ == DU_DRAW_QUADS)
		{
			if (m_VertexIndex == 0)
				Triangles.emplace_back();

			if (m_VertexIndex == 3)
			{
				auto prevTriangles = Triangles.back(); // Copy
				auto& triangle = Triangles.emplace_back(prevTriangles);
				auto& vertex = triangle.Vertices[1];
				vertex.Location = glm::vec3(x, y, z);
				vertex.Color = rcCol(color);
			}
			else
			{
				auto& vertex = Triangles.back().Vertices[m_VertexIndex];
				vertex.Location = glm::vec3(x, y, z);
				vertex.Color = rcCol(color);
			}
			m_VertexIndex = (m_VertexIndex + 1) % 4;
		}
	}
}
