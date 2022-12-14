#include "egpch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanTextureCube.h"
#include "Eagle/Utils/PlatformUtils.h"

#include "stb_image.h"

namespace Eagle
{
	Ref<Texture2D> Texture2D::DummyTexture;
	Ref<Texture2D> Texture2D::WhiteTexture;
	Ref<Texture2D> Texture2D::BlackTexture;
	Ref<Texture2D> Texture2D::NoneIconTexture;
	Ref<Texture2D> Texture2D::MeshIconTexture;
	Ref<Texture2D> Texture2D::TextureIconTexture;
	Ref<Texture2D> Texture2D::SceneIconTexture;
	Ref<Texture2D> Texture2D::SoundIconTexture;
	Ref<Texture2D> Texture2D::FolderIconTexture;
	Ref<Texture2D> Texture2D::UnknownIconTexture;
	Ref<Texture2D> Texture2D::PlayButtonTexture;
	Ref<Texture2D> Texture2D::StopButtonTexture;

	std::vector<Ref<Texture>> TextureLibrary::s_Textures;
	std::vector<size_t> TextureLibrary::s_TexturePathHashes;

#ifdef EG_DEBUG
	std::vector<Path> TextureLibrary::s_TexturePaths;
#endif

	Ref<Texture2D> Texture2D::Create(const Path& path, const Texture2DSpecifications& properties, bool bAddToLib)
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

			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
				return nullptr;
		}

		if (bAddToLib && texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	Ref<Texture2D> Texture2D::Create(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& properties, bool bAddToLib)
	{
		Ref<Texture2D> texture;
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan: 
				texture = MakeRef<VulkanTexture2D>(format, size, data, properties);
				break;
				
			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
				return nullptr;
		}

		if (bAddToLib && texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	Ref<TextureCube> TextureCube::Create(const Path& path, uint32_t layerSize)
	{
		Ref<TextureCube> texture;
		switch (Renderer::GetAPI())
		{
		case RendererAPIType::Vulkan:
			texture = MakeRef<VulkanTextureCube>(path, layerSize);
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}

		if (texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	Ref<TextureCube> TextureCube::Create(const Ref<Texture2D>& texture2D, uint32_t layerSize)
	{
		Ref<TextureCube> texture;
		switch (Renderer::GetAPI())
		{
		case RendererAPIType::Vulkan:
			texture = MakeRef<VulkanTextureCube>(texture2D, layerSize);
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}

		if (texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	bool Texture::Load(const Path& path)
	{
		int width, height, channels;

		std::wstring wPathString = m_Path.wstring();

		char cpath[2048];
		WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);

		if (stbi_is_hdr(cpath))
		{
			m_ImageData.Data = stbi_loadf(cpath, &width, &height, &channels, 4);
			m_Format = HDRChannelsToFormat(4);
			m_ImageData.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
		}
		else
		{
			m_ImageData.Data = stbi_load(cpath, &width, &height, &channels, 4);
			m_Format = ChannelsToFormat(4);
			m_ImageData.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
		}

		assert(m_ImageData.Data); // Failed to load
		if (!m_ImageData.Data)
			return false;

		m_Size = { (uint32_t)width, (uint32_t)height, 1u };
		return true;
	}

	//----------------------
	//   Texture Library
	//----------------------

	bool TextureLibrary::Get(const Path& path, Ref<Texture>* outTexture)
	{
		size_t textureHash = std::hash<Path>()(path);
		size_t index = 0;
		for (const auto& hash : s_TexturePathHashes)
		{
			if (textureHash == hash)
			{
				*outTexture = s_Textures[index];
				return true;
			}
			index++;
		}

		return false;
	}

	bool TextureLibrary::Get(const GUID& guid, Ref<Texture>* outTexture)
	{
		for (const auto& texture : s_Textures)
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
		for (const auto& texture : s_Textures)
		{
			if (texture->GetPath() == path)
			{
				return true;
			}
		}

		return false;
	}
	
	bool TextureLibrary::Exist(const GUID& guid)
	{
		for (const auto& texture : s_Textures)
		{
			if (guid == texture->GetGUID())
			{
				return true;
			}
		}

		return false;
	}
}
