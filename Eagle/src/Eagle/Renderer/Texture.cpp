#include "egpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Eagle
{
	Ref<Texture2D> Texture2D::WhiteTexture;
	Ref<Texture2D> Texture2D::BlackTexture;
	Ref<Texture2D> Texture2D::NoneTexture;
	Ref<Texture2D> Texture2D::MeshIconTexture;
	Ref<Texture2D> Texture2D::TextureIconTexture;
	Ref<Texture2D> Texture2D::SceneIconTexture;
	Ref<Texture2D> Texture2D::FolderIconTexture;
	Ref<Texture2D> Texture2D::UnknownIconTexture;
	Ref<Texture2D> Texture2D::PlayButtonTexture;
	Ref<Texture2D> Texture2D::StopButtonTexture;

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

	Ref<Texture2D> Texture2D::Create(const std::filesystem::path& path, bool bLoadAsSRGB, bool bAddToLibrary)
	{
		if (std::filesystem::exists(path) == false)
		{
			EG_CORE_ERROR("Could not load the texture : {0}", path);
			return nullptr;
		}

		Ref<Texture2D> texture;
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				texture = MakeRef<OpenGLTexture2D>(path, bLoadAsSRGB);
		}
		if (texture)
		{
			if (bAddToLibrary)
				TextureLibrary::Add(texture);
			return texture;
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	bool TextureLibrary::Get(const std::filesystem::path& path, Ref<Texture>* outTexture)
	{
		for (const auto& texture : m_Textures)
		{
			std::filesystem::path currentPath(texture->GetPath());

			if (std::filesystem::equivalent(path, currentPath))
			{
				*outTexture = texture;
				return true;
			}
		}

		return false;
	}

	bool TextureLibrary::Get(const GUID& guid, Ref<Texture>* outTexture)
	{
		for (const auto& texture : m_Textures)
		{
			if (guid == texture->GetGUID())
			{
				*outTexture = texture;
				return true;
			}
		}

		return false;
	}

	bool TextureLibrary::Exist(const std::filesystem::path& path)
	{
		for (const auto& texture : m_Textures)
		{
			std::filesystem::path currentPath(texture->GetPath());

			if (std::filesystem::equivalent(path, currentPath))
			{
				return true;
			}
		}

		return false;
	}
	
	bool TextureLibrary::Exist(const GUID& guid)
	{
		for (const auto& texture : m_Textures)
		{
			if (guid == texture->GetGUID())
			{
				return true;
			}
		}

		return false;
	}
}
