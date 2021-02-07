#pragma once

#include "Eagle/Core/Transform.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class CameraComponent;

	class Renderer2D
	{
	public:
		
		static void Init();
		static void Shutdown();

		static void BeginScene(const CameraComponent& camera);
		static void EndScene();
		static void Flush();

		static void DrawQuad(const Transform& transform, const glm::vec4& color);
		static void DrawQuad(const Transform& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);

		//Rotated
		static void DrawRotatedQuad(const Transform& transform, float radians, const glm::vec4& color);
		static void DrawRotatedQuad(const Transform& transform, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawRotatedQuad(const Transform& transform, float radians, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);

	private:
		//General function that are being called
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);

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