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

		//Draws COUNT elements from Bound VA
		static inline void DrawIndexed(uint32_t count)
		{
			s_RendererAPI->DrawIndexed(count);
		}

		static inline void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		static inline void Clear()
		{
			s_RendererAPI->Clear();
		}

		static inline void ClearDepthBuffer()
		{
			s_RendererAPI->ClearDepthBuffer();
		}

		static inline void SetDepthMask(bool depthMask)
		{
			s_RendererAPI->SetDepthMask(depthMask);
		}

		static inline void SetDepthFunc(DepthFunc func)
		{
			s_RendererAPI->SetDepthFunc(func);
		}

		static inline void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

	private:
		static Ref<RendererAPI> s_RendererAPI;
	};
}