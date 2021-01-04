#pragma once

#include "VertexArray.h"
#include "RendererAPI.h"
#include "RenderCommand.h"

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

		static void SetClearColor(const glm::vec4& color);

		static void Clear();

	private:
		struct SceneData
		{
			glm::mat4 ViewProjection;
		};

	private:
		static Ref<SceneData> m_SceneData;
	};

}