#include "egpch.h"
#include "VulkanTexture2D.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"

namespace Eagle
{
	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const std::string& debugName)
		: Texture2D(format, size, Texture2DSpecifications{}), m_DebugName(debugName)
	{
		m_Image = CreateImage();

		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), m_Specs.MaxAnisotropy);

		m_bIsLoaded = true;
	}

	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs, const std::string& debugName)
		: Texture2D(format, size, specs), m_DebugName(debugName)
	{
		EG_ASSERT(data);
		size_t dataSize = CalculateImageMemorySize(m_Format, m_Size.x, m_Size.y);
		m_ImageData.emplace_back() = DataBuffer::Copy(data, dataSize);
		
		// The data is not uploaded to the GPU here.
		// It's called from the outside because it requires `shared_from_this()` to be called for safety.
		// But we can't call it from a constructor
	}

	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const std::vector<ScopedDataBuffer>& dataPerMip, const Texture2DSpecifications& specs, const std::string& debugName)
		: Texture2D(format, size, specs), m_DebugName(debugName)
	{
		EG_ASSERT(dataPerMip.size() == specs.MipsCount);
		for (auto& data : dataPerMip)
			m_ImageData.emplace_back(ScopedDataBuffer::Copy(data));

		// The data is not uploaded to the GPU here.
		// It's called from the outside because it requires `shared_from_this()` to be called for safety.
		// But we can't call it from a constructor
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
		m_Specs.MipsCount = glm::min(CalculateMipCount(m_Size), mipsCount);

		Ref<Image> oldImage = m_Image;
		m_Image = CreateImage();

		RenderManager::Submit([oldImage = std::move(oldImage), image = m_Image](const Ref<CommandBuffer>& cmd) mutable
		{
			cmd->CopyImage(oldImage, image, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->GenerateMips(image, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
		});

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::SetData(const std::vector<ScopedDataBuffer>& dataPerMip, ImageFormat format)
	{
		if (format != ImageFormat::Unknown)
			m_Format = format;
		m_Specs.MipsCount = (uint32_t)dataPerMip.size();
		m_ImageData.clear();

		for (auto& data : dataPerMip)
			m_ImageData.emplace_back(ScopedDataBuffer::Copy(data));
		CreateImageFromData(false);

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::SetData(DataBuffer data, ImageFormat format)
	{
		m_Format = format;
		m_ImageData.clear();

		const size_t dataSize = CalculateImageMemorySize(m_Format, m_Size.x, m_Size.y);
		EG_CORE_ASSERT(data.Size == dataSize);
		m_ImageData.emplace_back() = DataBuffer::Copy(data);
		CreateImageFromData(true);

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	Ref<Image> VulkanTexture2D::CreateImage()
	{
		ImageSpecifications imageSpecs;
		imageSpecs.Size = m_Size;
		imageSpecs.Format = m_Format;
		imageSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst; // To sample in shader and to write texture data to it
		imageSpecs.SamplesCount = m_Specs.SamplesCount;
		imageSpecs.MipsCount = m_Specs.MipsCount;
		if (imageSpecs.MipsCount > 1)
			imageSpecs.Usage |= ImageUsage::TransferSrc;

		return MakeRef<VulkanImage>(imageSpecs, m_DebugName);
	}

	void VulkanTexture2D::CreateImageFromData(bool bAutogenerateMips)
	{
		if (!m_ImageData[0])
			return;

		m_bIsLoaded = false;
		m_Specs.MipsCount = glm::min(CalculateMipCount(m_Size), m_Specs.MipsCount);
		m_Image = CreateImage();

		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), m_Specs.MaxAnisotropy);
		const bool bGenerateMips = m_Specs.MipsCount > 1;

		RenderManager::Submit([textureRef = shared_from_this(), image = m_Image, imageData = std::move(m_ImageData), bGenerateMips, bAutogenerateMips](const Ref<CommandBuffer>& cmd) mutable
		{
			cmd->Write(image, imageData[0].Data(), imageData[0].Size(), image->GetLayout(), ImageReadAccess::PixelShaderRead);
			if (bGenerateMips)
			{
				if (bAutogenerateMips)
					cmd->GenerateMips(image, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
				else
					cmd->GenerateMips(image, imageData, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
			}
			textureRef->m_bIsLoaded = true;
		});
	}
}
