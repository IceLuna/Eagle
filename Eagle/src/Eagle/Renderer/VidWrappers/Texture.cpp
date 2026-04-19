#include "egpch.h"
#include "Texture.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanTextureCube.h"

namespace Eagle
{
	Ref<Texture2D> Texture2D::DummyTexture;
	Ref<Texture2D> Texture2D::WhiteTexture;
	Ref<Texture2D> Texture2D::BlackTexture;
	Ref<Texture2D> Texture2D::GrayTexture;
	Ref<Texture2D> Texture2D::RedTexture;
	Ref<Texture2D> Texture2D::GreenTexture;
	Ref<Texture2D> Texture2D::BlueTexture;
	Ref<Texture2D> Texture2D::NoneIconTexture;
	Ref<Texture2D> Texture2D::PointLightIcon;
	Ref<Texture2D> Texture2D::DirectionalLightIcon;
	Ref<Texture2D> Texture2D::SpotLightIcon;

	Texture::~Texture()
	{
		TextureSystem::RemoveTexture(m_GUID);
	}

	Ref<Texture2D> Texture2D::Create(const Path& path, const Texture2DSpecifications& specs)
	{
		int width = 0, height = 0, channels = 0;
		ScopedDataBuffer imageData = Utils::LoadTextureFromFile(path, &width, &height, &channels, 4);
		if (!imageData)
		{
			EG_CORE_ERROR("Failed to load a texture: {}", path);
			return {};
		}

		const ImageFormat imageFormat = ImageFormat::B8G8R8A8_UNorm;
		Ref<Texture2D> result;

		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
		{
			auto texture2D = MakeRef<VulkanTexture2D>(imageFormat, glm::uvec2(width, height), imageData.Data(), specs, Utils::AsString(path.stem()));
			texture2D->CreateImageFromData(true); // It's here because can't call `shared_from_this` inside of a constructor
			result = texture2D;
			break;
		}

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		}

		return result;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& name, ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& properties)
	{
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan:
			{
				auto texture2D = MakeRef<VulkanTexture2D>(format, size, data, properties, name);
				texture2D->CreateImageFromData(true); // It's here because can't call `shared_from_this` inside of a constructor
				return texture2D;
			}
				
			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
				return nullptr;
		}
	}

	Ref<Texture2D> Texture2D::Create(const std::string& name, ImageFormat format, glm::uvec2 size, const std::vector<ScopedDataBuffer>& dataPerMip, const Texture2DSpecifications& properties)
	{
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan:
			{
				auto texture2D = MakeRef<VulkanTexture2D>(format, size, dataPerMip, properties, name);
				texture2D->CreateImageFromData(false); // It's here because can't call `shared_from_this` inside of a constructor
				return texture2D;
			}

			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
				return nullptr;
		}
	}

	Ref<TextureCube> TextureCube::Create(const std::string& name, ImageFormat format, const void* data, glm::uvec2 size, uint32_t layerSize, uint32_t prefilterSize)
	{
		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
		{
			auto texture = MakeRef<VulkanTextureCube>(name, format, data, size, layerSize, prefilterSize);
			texture->GenerateIBL();
			return texture;
		}

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}
	}

	Ref<TextureCube> TextureCube::Create(const Ref<Texture2D>& texture2D, uint32_t layerSize, uint32_t prefilterSize)
	{
		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
		{
			auto texture = MakeRef<VulkanTextureCube>(texture2D, layerSize, prefilterSize);
			texture->GenerateIBL();
			return texture;
		}

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}
	}
}
