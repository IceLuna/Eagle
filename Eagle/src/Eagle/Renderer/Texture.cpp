#include "egpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Eagle
{
	Ref<Texture> Texture2D::WhiteTexture;
	Ref<Texture> Texture2D::BlackTexture;

	std::vector<Ref<Texture>> TextureLibrary::m_Textures;

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				return MakeRef<OpenGLTexture2D>(width, height);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, const void* data)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
			return nullptr;

		case RendererAPI::API::OpenGL:
			return MakeRef<OpenGLTexture2D>(width, height, data);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		Ref<Texture2D> texture;
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				texture = MakeRef<OpenGLTexture2D>(path);
		}
		if (texture)
		{
			TextureLibrary::Add(texture);
			return texture;
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	bool TextureLibrary::Get(const std::string& path, Ref<Texture>* texture)
	{
		std::filesystem::path testPath(path);

		for (const auto& t : m_Textures)
		{
			std::filesystem::path currentPath(t->GetPath());

			if (testPath == currentPath)
			{
				*texture = t;
				return true;
			}
		}

		return false;
	}
}
