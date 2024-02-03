#include "egpch.h"
#include "VulkanTexture2D.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"

namespace Eagle
{
	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs, const std::string& debugName)
		: Texture2D(format, size, specs), m_DebugName(debugName)
	{
		EG_ASSERT(data);
		size_t dataSize = CalculateImageMemorySize(m_Format, m_Size.x, m_Size.y);
		m_ImageData.emplace_back() = DataBuffer::Copy(data, dataSize);
		CreateImageFromData(true);
	}

	VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const std::vector<DataBuffer>& dataPerMip, const Texture2DSpecifications& specs, const std::string& debugName)
		: Texture2D(format, size, specs), m_DebugName(debugName)
	{
		EG_ASSERT(dataPerMip.size() == specs.MipsCount);
		for (auto& data : dataPerMip)
			m_ImageData.emplace_back(DataBuffer::Copy(data.Data, data.Size));

		CreateImageFromData(false);
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
		CreateImageFromData(true);

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::GenerateMips(const std::vector<DataBuffer>& dataPerMip)
	{
		m_Specs.MipsCount = (uint32_t)dataPerMip.size();
		m_ImageData.clear();

		for (auto& data : dataPerMip)
			m_ImageData.emplace_back(DataBuffer::Copy(data.Data, data.Size));
		CreateImageFromData(false);

		TextureSystem::OnTextureChanged(shared_from_this());
	}

	void VulkanTexture2D::CreateImageFromData(bool bAutogenerateMips)
	{
		if (!m_ImageData[0])
			return;

		m_Specs.MipsCount = glm::min(CalculateMipCount(m_Size), m_Specs.MipsCount);
		const bool bGenerateMips = m_Specs.MipsCount > 1;

		ImageSpecifications imageSpecs;
		imageSpecs.Size = m_Size;
		imageSpecs.Format = m_Format;
		imageSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst; // To sample in shader and to write texture data to it
		imageSpecs.SamplesCount = m_Specs.SamplesCount;
		imageSpecs.MipsCount = m_Specs.MipsCount;
		if (bGenerateMips && bAutogenerateMips)
			imageSpecs.Usage |= ImageUsage::TransferSrc;

		m_Image = MakeRef<VulkanImage>(imageSpecs, m_DebugName);

		const uint32_t mipsCount = m_Image->GetMipsCount();
		m_Sampler = Sampler::Create(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, float(mipsCount - 1), m_Specs.MaxAnisotropy);

		// We need to copy it to insure the safety on RenderThread.
		// If the texture is destroyed before the RT is executed, it'll dereference a dead pointer
		std::vector<ScopedDataBuffer> dataPerMips(m_ImageData.size());
		for (uint32_t i = 0; i < m_ImageData.size(); ++i)
			dataPerMips[i] = DataBuffer::Copy(m_ImageData[i].Data(), m_ImageData[i].Size());

		RenderManager::Submit([image = m_Image, imageData = std::move(dataPerMips), pLoaded = &m_bIsLoaded, bGenerateMips, bAutogenerateMips](Ref<CommandBuffer>& cmd) mutable
		{
			cmd->Write(image, imageData[0].Data(), imageData[0].Size(), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			if (bGenerateMips)
			{
				if (bAutogenerateMips)
					cmd->GenerateMips(image, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
				else
					cmd->GenerateMips(image, imageData, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
			}
			*pLoaded = true;
		});
	}
}
