#pragma once

#include "Renderer.h"

#include "Eagle/Core/Transform.h"
#include "VidWrappers/Texture.h"
#include "VidWrappers/SubTexture2D.h"
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
	struct GBuffers;

	class Renderer2D
	{
	public:
		
		static void Init(GBuffers& gBufferImages);
		static void Shutdown();

		static void BeginScene(const glm::mat4& viewProj);
		static void EndScene();
		static void Flush(Ref<CommandBuffer>& cmd);

		static void DrawQuad(const Transform& transform, const Ref<Material>& material, int entityID = -1);
		static void DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);
		static void DrawQuad(const SpriteComponent& sprite);

		static void OnResized(glm::uvec2 size);

	private:
		//General function that are being called
		static void DrawQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID = -1);
		static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID = -1);

	public:
		static void ResetStats();
		static Renderer::Statistics2D& GetStats();
	};
}