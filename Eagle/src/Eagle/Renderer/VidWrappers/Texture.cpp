#include "egpch.h"
#include "Texture.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanTextureCube.h"
#include "Eagle/Utils/PlatformUtils.h"

#include "stb_image.h"

namespace Eagle
{
	Ref<Texture2D> Texture2D::DummyTexture;
	Ref<Texture2D> Texture2D::WhiteTexture;
	Ref<Texture2D> Texture2D::BlackTexture;
	Ref<Texture2D> Texture2D::HalfTexture;
	Ref<Texture2D> Texture2D::NoneIconTexture;
	Ref<Texture2D> Texture2D::PointLightIcon;
	Ref<Texture2D> Texture2D::DirectionalLightIcon;
	Ref<Texture2D> Texture2D::SpotLightIcon;

	std::unordered_map<Path, Ref<Texture>> TextureLibrary::s_Textures;

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
		switch (RenderManager::GetAPI())
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

	Ref<Texture2D> Texture2D::Create(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& properties, bool bAddToLib, const std::string& debugName)
	{
		Ref<Texture2D> texture;
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan: 
				texture = MakeRef<VulkanTexture2D>(format, size, data, properties, debugName);
				break;
				
			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
				return nullptr;
		}

		if (bAddToLib && texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	Ref<TextureCube> TextureCube::Create(const Path& path, uint32_t layerSize, bool bAddToLibrary)
	{
		Ref<TextureCube> texture;
		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
			texture = MakeRef<VulkanTextureCube>(path, layerSize);
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}

		if (bAddToLibrary && texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	Ref<TextureCube> TextureCube::Create(const Ref<Texture2D>& texture2D, uint32_t layerSize, bool bAddToLibrary)
	{
		Ref<TextureCube> texture;
		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
			texture = MakeRef<VulkanTextureCube>(texture2D, layerSize);
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}

		if (bAddToLibrary && texture)
			TextureLibrary::Add(texture);
		return texture;
	}

	bool Texture::Load(const Path& path)
	{
		int width, height, channels;

		std::wstring wPathString = m_Path.wstring();

		char cpath[2048];
		WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);

		DataBuffer buffer;
		if (stbi_is_hdr(cpath))
		{
			buffer.Data = stbi_loadf(cpath, &width, &height, &channels, 4);
			m_Format = HDRChannelsToFormat(4);
			buffer.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
		}
		else
		{
			buffer.Data = stbi_load(cpath, &width, &height, &channels, 4);
			m_Format = ChannelsToFormat(4);
			buffer.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
		}
		m_ImageData = std::move(buffer);

		assert(m_ImageData.Data()); // Failed to load
		if (!m_ImageData.Data())
			return false;

		m_Size = { (uint32_t)width, (uint32_t)height, 1u };
		return true;
	}

	//----------------------
	//   Texture Library
	//----------------------

	bool TextureLibrary::Get(const Path& path, Ref<Texture>* outTexture)
	{
		auto it = s_Textures.find(path);
		if (it != s_Textures.end())
		{
			*outTexture = it->second;
			return true;
		}

		return false;
	}

	bool TextureLibrary::Get(const GUID& guid, Ref<Texture>* outTexture)
	{
		if (guid.IsNull())
			return false;

		for (const auto& data : s_Textures)
		{
			const auto& texture = data.second;
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
		return s_Textures.find(path) != s_Textures.end();
	}
	
	bool TextureLibrary::Exist(const GUID& guid)
	{
		for (const auto& data : s_Textures)
		{
			const auto& texture = data.second;
			if (guid == texture->GetGUID())
				return true;
		}

		return false;
	}
}
