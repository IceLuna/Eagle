#pragma once

#include "VertexArray.h"
#include "RendererAPI.h"

#include "Eagle/Camera/Camera.h"
#include "Shader.h"

namespace Eagle
{

	class Renderer
	{
	public:
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

		static void BeginScene(const Camera& camera);
		static void EndScene();

		static void Init();

		static void WindowResized(uint32_t width, uint32_t height);

		static void Submit(const Ref<Shader> shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.f));

		static inline void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		static inline void Clear()
		{
			s_RendererAPI->Clear();
		}

	private:
		struct SceneData
		{
			glm::mat4 ViewProjection;
		};

	private:
		static Ref<RendererAPI> s_RendererAPI;
		static Ref<SceneData> m_SceneData;
	};

}