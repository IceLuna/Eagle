#include "egpch.h"
#include "VulkanTexture2D.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/RenderCommandManager.h"

#include "stb_image.h"

namespace Eagle
{
	namespace Utils
	{
		static ImageFormat ChannelsToFormat(int channels, bool bIsSRGB)
		{
			switch (channels)
			{
				case 1: return bIsSRGB ? ImageFormat::R8_UNorm_SRGB : ImageFormat::R8_UNorm;
				case 2: return bIsSRGB ? ImageFormat::R8G8_UNorm_SRGB : ImageFormat::R8G8_UNorm;
				case 3: return bIsSRGB ? ImageFormat::R8G8B8_UNorm_SRGB : ImageFormat::R8G8B8_UNorm;
				case 4: return bIsSRGB ? ImageFormat::R8G8B8A8_UNorm_SRGB : ImageFormat::R8G8B8A8_UNorm;
			}
			EG_CORE_ASSERT(!"Invalid channels count");
			return ImageFormat::Unknown;
		}

		static ImageFormat HDRChannelsToFormat(int channels)
		{
			switch (channels)
			{
				case 1: return ImageFormat::R32_Float;
				case 2: return ImageFormat::R32G32_Float;
				case 3: return ImageFormat::R32G32B32_Float;
				case 4: return ImageFormat::R32G32B32A32_Float;
			}
			EG_CORE_ASSERT(!"Invalid channels count");
			return ImageFormat::Unknown;
		}
	}

	VulkanTexture2D::VulkanTexture2D(const Path& filepath, const Texture2DSpecifications& specs)
		: Texture2D(filepath, specs)
	{
		if (Load(m_Path))
		{
			m_bIsLoaded = false; // False since we're about to write to it
			CreateImageFromData();
		}
		else
		{
			EG_CORE_ASSERT(!"Failed to load texture");
			ImageSpecifications imageSpecs;
			imageSpecs.Size = glm::uvec3{ 1, 1, 1 };
			imageSpecs.Format = ImageFormat::Unknown;
			imageSpecs.Usage = ImageUsage::Sampled;
			imageSpecs.Layout = ImageLayoutType::Unknown;
			m_Image = MakeRef<VulkanImage>(VK_NULL_HANDLE, imageSpecs, true);
			m_Sampler = MakeRef<VulkanSampler>(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, 0.f, m_Specs.MaxAnisotropy);
		}
	}
	
	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs)
		: Texture2D(format, size, data, specs)
	{
		if (data)
		{
			size_t dataSize = CalculateImageMemorySize(m_Format, m_Size.x, m_Size.y);
			m_ImageData = DataBuffer::Copy(data, dataSize);
			m_bIsLoaded = false; // False since we're about to write to it
			CreateImageFromData();
		}
	}

	void VulkanTexture2D::SetSRGB(bool bSRGB)
	{
		// TODO: Make it work
		m_Specs.bSRGB = bSRGB;
		CreateImageFromData();
	}

	void VulkanTexture2D::CreateImageFromData()
	{
		if (!m_ImageData)
			return;

		const uint32_t mipsCount = m_Specs.bGenerateMips ? CalculateMipCount(m_Size.x, m_Size.y) : 1;

		ImageSpecifications imageSpecs;
		imageSpecs.Size = m_Size;
		imageSpecs.Format = m_Format;
		imageSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst; // To sample in shader and to write texture data to it
		imageSpecs.Layout = ImageLayoutType::CopyDest; // Since we're about to write texture data to it
		imageSpecs.SamplesCount = m_Specs.SamplesCount;
		imageSpecs.MipsCount = mipsCount;
		std::string debugName = m_Path.filename().u8string();
		m_Image = MakeRef<VulkanImage>(imageSpecs, debugName);

		Renderer::Submit([image = m_Image, imageData = m_ImageData, pLoaded = &m_bIsLoaded](Ref<CommandBuffer>& cmd) mutable
		{
			cmd->Write(image, imageData.Data, imageData.Size, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			*pLoaded = true;
		});

		m_Sampler = MakeRef<VulkanSampler>(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, mipsCount > 1 ? float(mipsCount) : 0.f, m_Specs.MaxAnisotropy);
	}

	bool VulkanTexture2D::Load(const Path& path)
	{
		int width, height, channels;

		std::wstring wPathString = m_Path.wstring();

		char cpath[2048];
		WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);

		if (stbi_is_hdr(cpath))
		{
			m_ImageData.Data = stbi_loadf(cpath, &width, &height, &channels, 4);
			m_Format = Utils::HDRChannelsToFormat(4);
			m_ImageData.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
		}
		else
		{
			m_ImageData.Data = stbi_load(cpath, &width, &height, &channels, 4);
			m_Format = Utils::ChannelsToFormat(4, m_Specs.bSRGB);
			m_ImageData.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
		}

		assert(m_ImageData.Data); // Failed to load
		if (!m_ImageData.Data)
			return false;

		m_Size = { (uint32_t)width, (uint32_t)height, 1u };
		return true;
	}
}
