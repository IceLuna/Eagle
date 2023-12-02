#include "egpch.h"
#include "VulkanTexture2D.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"

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
			m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, 0.f, m_Specs.MaxAnisotropy);
			m_bIsLoaded = true; // Loaded meaning we can use it.
		}
	}
	
	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs, const std::string& debugName)
		: Texture2D(format, size, specs)
	{
		EG_ASSERT(data);
		size_t dataSize = CalculateImageMemorySize(m_Format, m_Size.x, m_Size.y);
		m_ImageData = DataBuffer::Copy(data, dataSize);
		m_Path = debugName;
		CreateImageFromData();
	}

	void VulkanTexture2D::SetAnisotropy(float anisotropy)
	{
		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), anisotropy);
		m_Specs.MaxAnisotropy = m_Sampler->GetMaxAnisotropy();

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::SetFilterMode(FilterMode filterMode)
	{
		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Specs.FilterMode = filterMode;
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), m_Specs.MaxAnisotropy);

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::SetAddressMode(AddressMode addressMode)
	{
		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Specs.AddressMode = addressMode;
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), m_Specs.MaxAnisotropy);

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::GenerateMips(uint32_t mipsCount)
	{
		m_Specs.MipsCount = mipsCount;
		CreateImageFromData();

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::CreateImageFromData()
	{
		if (!m_ImageData)
			return;

		m_Specs.MipsCount = glm::min(CalculateMipCount(m_Size), m_Specs.MipsCount);
		const bool bGenerateMips = m_Specs.MipsCount > 1;

		ImageSpecifications imageSpecs;
		imageSpecs.Size = m_Size;
		imageSpecs.Format = m_Format;
		imageSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst; // To sample in shader and to write texture data to it
		imageSpecs.SamplesCount = m_Specs.SamplesCount;
		imageSpecs.MipsCount = m_Specs.MipsCount;
		if (bGenerateMips)
			imageSpecs.Usage |= ImageUsage::TransferSrc;

		std::string debugName = m_Path.filename().u8string();
		m_Image = MakeRef<VulkanImage>(imageSpecs, debugName);

		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), m_Specs.MaxAnisotropy);

		RenderManager::Submit([image = m_Image, imageData = m_ImageData.GetDataBuffer(), pLoaded = &m_bIsLoaded, bGenerateMips](Ref<CommandBuffer>& cmd) mutable
		{
			cmd->Write(image, imageData.Data, imageData.Size, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			if (bGenerateMips)
				cmd->GenerateMips(image, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
			*pLoaded = true;
		});
	}
}
