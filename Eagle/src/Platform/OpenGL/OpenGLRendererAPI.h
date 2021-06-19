#pragma once

#include "Eagle/Renderer/RendererAPI.h"

namespace Eagle
{
	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void Init() override;

		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void SetDepthMask(bool depthMask) override;
		virtual void SetDepthFunc(DepthFunc func) override;

		virtual void DrawIndexed(uint32_t count) override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
	};
}