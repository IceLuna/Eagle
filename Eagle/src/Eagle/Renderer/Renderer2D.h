#pragma once

#include "Eagle/Core/Transform.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include <glm/glm.hpp>
#include "Eagle/Core/Entity.h"

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

	class Renderer2D
	{
	public:
		
		static void Init();
		static void Shutdown();

		static void BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void BeginScene(const CameraComponent& camera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void EndScene();
		static void Flush();

		static void DrawQuad(const Transform& transform, const Material& material, int entityID = -1);
		static void DrawQuad(const Transform& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps, int entityID = -1);
		static void DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);

	private:
		//General function that are being called
		static void DrawQuad(const glm::mat4& transform, const Material& material, int entityID = -1);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps, int entityID = -1);
		static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);

	private:
		static void NextBatch();
		static void StartBatch();

	public:
		//Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			inline uint32_t GetVertexCount() const { return QuadCount * 4; }
			inline uint32_t GetIndexCount() const { return QuadCount * 6; }
		};

		static void ResetStats();
		static Statistics GetStats();

	};
}