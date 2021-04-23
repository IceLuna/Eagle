#pragma once

#include "VertexArray.h"
#include "RendererAPI.h"
#include "RenderCommand.h"

#include "Shader.h"

namespace Eagle
{
	class CameraComponent;

	class Renderer
	{
	public:
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

		static void BeginScene(const CameraComponent& camera);
		static void EndScene();

		static void Init();

		static void WindowResized(uint32_t width, uint32_t height);

		static void SetClearColor(const glm::vec4& color);

		static void Clear();

	private:
		struct SceneData
		{
			glm::mat4 ViewProjection;
		};

	private:
		static Ref<SceneData> s_SceneData;
	};

}