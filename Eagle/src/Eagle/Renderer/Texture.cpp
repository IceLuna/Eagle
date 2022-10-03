#include "egpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/Vulkan/VulkanTexture2D.h"

namespace Eagle
{
	Ref<Texture2D> Texture2D::WhiteTexture;
	Ref<Texture2D> Texture2D::BlackTexture;
	Ref<Texture2D> Texture2D::NoneTexture;
	Ref<Texture2D> Texture2D::MeshIconTexture;
	Ref<Texture2D> Texture2D::TextureIconTexture;
	Ref<Texture2D> Texture2D::SceneIconTexture;
	Ref<Texture2D> Texture2D::SoundIconTexture;
	Ref<Texture2D> Texture2D::FolderIconTexture;
	Ref<Texture2D> Texture2D::UnknownIconTexture;
	Ref<Texture2D> Texture2D::PlayButtonTexture;
	Ref<Texture2D> Texture2D::StopButtonTexture;

	std::vector<Ref<Texture>> TextureLibrary::m_Textures;

	Ref<Texture2D> Texture2D::Create(const Path& path, const Texture2DSpecifications& properties)
	{
		if (std::filesystem::exists(path) == false)
		{
			EG_CORE_ERROR("Could not load the texture : {0}", path);
			return nullptr;
		}

		Ref<Texture2D> texture;
		switch (Renderer::GetAPI())
		{
		case RendererAPIType::Vulkan:
			texture = MakeRef<VulkanTexture2D>(path, properties);
			break;
		}

		if (texture)
		{
			TextureLibrary::Add(texture);
			return texture;
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& properties)
	{
		Ref<Texture2D> texture;
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan: 
				texture = MakeRef<VulkanTexture2D>(format, size, data, properties);
				break;
		}

		if (texture)
		{
			TextureLibrary::Add(texture);
			return texture;
		}

		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	bool TextureLibrary::Get(const Path& path, Ref<Texture>* outTexture)
	{
		for (const auto& texture : m_Textures)
		{
			Path currentPath(texture->GetPath());
			if (!std::filesystem::exists(currentPath))
				continue;

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

	bool TextureLibrary::Exist(const Path& path)
	{
		for (const auto& texture : m_Textures)
		{
			Path currentPath(texture->GetPath());

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
