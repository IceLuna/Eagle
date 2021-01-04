#pragma once

#include "Renderer.h"
#include "Renderer2D.h"

namespace Eagle
{
	class RenderCommand
	{
	public:
		
		static inline void Init()
		{
			s_RendererAPI->Init();
		}

		static inline void DrawIndexed(const Ref<VertexArray>& vertexArray)
		{
			s_RendererAPI->DrawIndexed(vertexArray);
		}

		static inline void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		static inline void Clear()
		{
			s_RendererAPI->Clear();
		}

		static inline void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

	private:
		static Ref<RendererAPI> s_RendererAPI;
	};
}