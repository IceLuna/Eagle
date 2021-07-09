#pragma once

#include "Eagle/Core/Transform.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include "Eagle/Core/Entity.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class CameraComponent;
	class EditorCamera;
	class PointLightComponent;
	class DirectionalLightComponent;
	class SpotLightComponent;
	class Material;
	struct RenderInfo;
	enum class DrawToShadowMap;

	class Renderer2D
	{
	public:
		
		static void Init();
		static void Shutdown();

		//If rendering to Point Light Shadow map, specify pointLightIndex
		static void BeginScene(const glm::vec3& cameraPosition, const RenderInfo& renderInfo);
		static void EndScene();
		static void Flush();

		static bool IsRedrawing();

		static void DrawQuad(const Transform& transform, const Ref<Material>& material, int entityID = -1);
		static void DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);
		static void DrawSkybox(const Ref<Cubemap>& cubemap);

	private:
		//General function that are being called
		static void DrawQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID = -1);
		static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);
		static void DrawCurrentSkybox();

		static void InitSpriteShader();

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

	public:
		static void ResetStats();
		static Statistics GetStats();
	};
}