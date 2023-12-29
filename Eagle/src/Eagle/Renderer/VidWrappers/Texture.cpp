#include "egpch.h"
#include "Texture.h"

#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanTextureCube.h"

#include <stb_image.h>

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

	Ref<Texture2D> Texture2D::Create(const Path& path, const Texture2DSpecifications& specs)
	{
		void* imageData = nullptr;
		int width = 0, height = 0;

		// Load data
		{
			int channels;
			char cpath[2048];
			std::wstring wPathString = path.wstring();
			WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);
			imageData = stbi_load(cpath, &width, &height, &channels, 4);
			if (!imageData)
			{
				EG_CORE_ERROR("Failed to load a texture: {}", path);
				return {};
			}
		}

		const ImageFormat imageFormat = ImageFormat::B8G8R8A8_UNorm;
		Ref<Texture2D> result;

		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
			result = MakeRef<VulkanTexture2D>(imageFormat, glm::uvec2(width, height), imageData, specs, path.stem().u8string());
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		}

		stbi_image_free(imageData);
		return result;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& name, ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& properties)
	{
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan: 
				return MakeRef<VulkanTexture2D>(format, size, data, properties, name);
				break;
				
			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
				return nullptr;
		}
	}

	Ref<TextureCube> TextureCube::Create(const std::string& name, ImageFormat format, const void* data, glm::uvec2 size, uint32_t layerSize)
	{
		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
			return MakeRef<VulkanTextureCube>(name, format, data, size, layerSize);
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}
	}

	Ref<TextureCube> TextureCube::Create(const Ref<Texture2D>& texture2D, uint32_t layerSize)
	{
		switch (RenderManager::GetAPI())
		{
		case RendererAPIType::Vulkan:
			return MakeRef<VulkanTextureCube>(texture2D, layerSize);
			break;

		default:
			EG_CORE_ASSERT(false, "Unknown RendererAPI!");
			return nullptr;
		}
	}
}
