#pragma once

#include "Eagle/Camera/OrthographicCamera.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class Renderer2D
	{
	public:
		
		static void Init();
		static void Shutdown();

		static void BeginScene(const OrthographicCamera& camera);
		static void EndScene();
		static void Flush();

		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);

		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps);

		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);

		//Rotated
		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const glm::vec4& color);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const glm::vec4& color);

		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps);

		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps);

	private:
		static void FlushAndReset();

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