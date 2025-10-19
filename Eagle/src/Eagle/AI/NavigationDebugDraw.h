#pragma once

#include "Eagle/Renderer/RendererUtils.h"

#include <DebugDraw.h>

namespace Eagle::AINavigation
{
	struct DebugDraw : public duDebugDraw
	{
		DebugDraw(std::vector<RendererLine>& outLines, std::vector<RendererTriangle>& outTriangles) : Lines(outLines), Triangles(outTriangles) {}
		virtual ~DebugDraw() = default;

		void depthMask(bool state) override {}
		void texture(bool state) override {}

		void begin(duDebugDrawPrimitives prim, float size = 1.0f) override;
		void end() override;

		void vertex(const float* pos, unsigned int color) override;
		void vertex(const float x, const float y, const float z, unsigned int color) override;
		void vertex(const float* pos, unsigned int color, const float* uv) override;
		void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override;

		std::vector<RendererLine>& Lines;
		std::vector<RendererTriangle>& Triangles;

	private:
		uint32_t m_VertexIndex = 0u;
		duDebugDrawPrimitives prim_ = DU_DRAW_POINTS;
	};
}
