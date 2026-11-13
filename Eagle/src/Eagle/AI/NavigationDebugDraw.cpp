#include "egpch.h"
#include "NavigationDebugDraw.h"

namespace Eagle::AINavigation
{
	static glm::vec4 rcCol(uint32_t col) noexcept
	{
		glm::vec4 result;

		result.r = float(col % 256);
		col /= 256;
		result.g = float(col % 256);
		col /= 256;
		result.b = float(col % 256);
		col /= 256;
		result.a = float(col % 256);
		result /= 255.f;

		return result;
	}

	void DebugDraw::SetColor(RendererDebugVertex& vertex, uint32_t color) const
	{
		const glm::vec4 rgba = rcCol(color);
		vertex.Color = glm::vec3(rgba);
		vertex.Opacity = glm::clamp(rgba.a * Opacity, 0.f, 1.f);
	}

	unsigned int DebugDraw::areaToCol(unsigned int area)
	{
		if (!AreaColor)
			return duDebugDraw::areaToCol(area);

		const glm::vec3 color = glm::clamp(*AreaColor, glm::vec3(0.f), glm::vec3(1.f)) * 255.f;
		return duRGBA(int(color.r + 0.5f), int(color.g + 0.5f), int(color.b + 0.5f), 255);
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

	void DebugDraw::vertex(const float x, const float y_, const float z, unsigned int color)
	{
		const float y = y_ + OffsetY;
		if (prim_ == DU_DRAW_POINTS)
		{
			auto& line = Lines.emplace_back();
			line.Start.Location = glm::vec3(x, y, z);
			SetColor(line.Start, color);
			line.End.Location = glm::vec3(x, y, z);
			SetColor(line.End, color);
		}
		else if (prim_ == DU_DRAW_LINES)
		{
			if (m_VertexIndex == 0)
			{
				auto& line = Lines.emplace_back();
				line.Start.Location = glm::vec3(x, y, z);
				SetColor(line.Start, color);
				m_VertexIndex = (m_VertexIndex + 1) % 2;
			}
			else if (m_VertexIndex == 1)
			{
				auto& line = Lines.back();
				line.End.Location = glm::vec3(x, y, z);
				SetColor(line.End, color);
				m_VertexIndex = (m_VertexIndex + 1) % 2;
			}
		}
		else if (prim_ == DU_DRAW_TRIS)
		{
			if (m_VertexIndex == 0)
				Triangles.emplace_back();

			auto& vertex = Triangles.back().Vertices[m_VertexIndex];
			vertex.Location = glm::vec3(x, y, z);
			SetColor(vertex, color);
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
				SetColor(vertex, color);
			}
			else
			{
				auto& vertex = Triangles.back().Vertices[m_VertexIndex];
				vertex.Location = glm::vec3(x, y, z);
				SetColor(vertex, color);
			}
			m_VertexIndex = (m_VertexIndex + 1) % 4;
		}
	}
}
