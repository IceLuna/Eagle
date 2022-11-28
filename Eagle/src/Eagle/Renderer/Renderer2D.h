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
	class SpriteComponent;
	class Material;
	class Image;
	class CommandBuffer;

	class Renderer2D
	{
	public:
		
		static void Init(const Ref<Image>& albedoImage, const Ref<Image>& normalImage, const Ref<Image>& depthImage);
		static void Shutdown();

		//If rendering to Point Light Shadow map, specify pointLightIndex
		static void BeginScene(const glm::mat4& viewProj);
		static void EndScene();
		static void Flush(Ref<CommandBuffer>& cmd);

		static void DrawQuad(const Transform& transform, const Ref<Material>& material, int entityID = -1);
		static void DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);
		static void DrawQuad(const SpriteComponent& sprite);
		static void DrawSkybox(const Ref<Cubemap>& cubemap);
		static void DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);

		static void OnResized(glm::uvec2 size);

	private:
		//General function that are being called
		static void DrawQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID = -1);
		static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);
		static void DrawCurrentSkybox();
		static void DrawLines();
		static void ResetLinesData();

	private:

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
		static Statistics& GetStats();
	};
}