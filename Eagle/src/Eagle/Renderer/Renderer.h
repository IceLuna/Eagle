#pragma once

#include "VertexArray.h"
#include "RendererAPI.h"
#include "RenderCommand.h"

#include "Shader.h"

namespace Eagle
{
	class CameraComponent;
	class EditorCamera;
	class PointLightComponent;
	class DirectionalLightComponent;
	class SpotLightComponent;
	class Material;
	class StaticMeshComponent;
	class Cubemap;

	class Renderer
	{
	public:
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

		static void BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void EndScene();

		static void Init();

		static void Draw(const StaticMeshComponent& smComponent, int entityID);
		static void ReflectSkybox(const Ref<Cubemap>& cubemap);

		static void WindowResized(uint32_t width, uint32_t height);

		static void SetClearColor(const glm::vec4& color);

		static void Clear();

	public:
		//Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Vertices = 0;
			uint32_t Indeces = 0;
		};

		static void ResetStats();
		static Statistics GetStats();
	};
}