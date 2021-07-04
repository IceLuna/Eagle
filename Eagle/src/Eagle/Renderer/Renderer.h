#pragma once

#include "VertexArray.h"
#include "RendererAPI.h"
#include "RenderCommand.h"

#include "Shader.h"

#define MAXPOINTLIGHTS 4
#define MAXSPOTLIGHTS 4

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
	class Framebuffer;
	class SpriteComponent;

	class Renderer
	{
	public:
		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

		static void BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void EndScene();

		static void Init();

		static void DrawMesh(const StaticMeshComponent& smComponent, int entityID);
		static void DrawSprite(const SpriteComponent& sprite, int entityID = -1);
		static void DrawSkybox(const Ref<Cubemap>& cubemap);

		static void WindowResized(uint32_t width, uint32_t height);

		static void SetClearColor(const glm::vec4& color);
		static void SetRenderNormals(bool bRender);
		static bool IsRenderingNormals();

		static void Clear();

		static float& Gamma();
		static Ref<Framebuffer>& GetMainFramebuffer();

	private:
		static void SetupLightUniforms(const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void SetupMatricesUniforms(const glm::mat4& view, const glm::mat4& projection);

		static void StartBatch();
		static void DrawPassedMeshes(bool bDrawToShadowMap, bool bRedraw);
		static void DrawPassedSprites(const glm::vec3& cameraPosition, bool bDrawToShadowMap);
		static void FlushMeshes(const Ref<Shader>& shader, bool bDrawToShadowMap, bool bRedrawing);
		static void FlushMeshesToShadowMap();

		static void PrepareRendering();
		static void FinishRendering();

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