#pragma once

#include "Eagle/Camera/OrthographicCamera.h"
#include "Texture.h"
#include <glm/glm.hpp>

#include "Shader.h"

namespace Eagle
{
	class Renderer2D
	{
	public:
		
		static void Init();
		static void Shutdown();

		static void BeginScene(const OrthographicCamera& camera);
		static void EndScene();

		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);

		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps);

		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Shader>& shader);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Shader>& shader);

		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const glm::vec4& color);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const glm::vec4& color);

		static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps);
		static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps);
	};
}