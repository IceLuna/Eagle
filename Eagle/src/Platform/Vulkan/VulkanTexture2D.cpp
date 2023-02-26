#include "egpch.h"
#include "VulkanTexture2D.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/RenderCommandManager.h"

namespace Eagle
{
	VulkanTexture2D::VulkanTexture2D(const Path& filepath, const Texture2DSpecifications& specs)
		: Texture2D(filepath, specs)
	{
		if (Load(m_Path))
		{
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
			m_bIsLoaded = true; // Loaded meaning we can use it.
		}
	}
	
	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs)
		: Texture2D(format, size, specs)
	{
		EG_ASSERT(data);
		size_t dataSize = CalculateImageMemorySize(m_Format, m_Size.x, m_Size.y);
		m_ImageData = DataBuffer::Copy(data, dataSize);
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

		Renderer::Submit([image = m_Image, imageData = m_ImageData.GetDataBuffer(), pLoaded = &m_bIsLoaded](Ref<CommandBuffer>& cmd) mutable
		{
			cmd->Write(image, imageData.Data, imageData.Size, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			*pLoaded = true;
		});

		m_Sampler = MakeRef<VulkanSampler>(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, mipsCount > 1 ? float(mipsCount) : 0.f, m_Specs.MaxAnisotropy);
	}
}
