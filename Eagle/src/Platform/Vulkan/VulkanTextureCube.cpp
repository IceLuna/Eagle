#include "egpch.h"
#include "VulkanTextureCube.h"
#include "VulkanFramebuffer.h"
#include "VulkanPipelineGraphics.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanTexture2D.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Math/Math.h"

#include <glm/gtx/transform.hpp>

namespace Eagle
{
	static const glm::mat4 g_CaptureProjection = Math::Perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	static const glm::mat4 g_CaptureViews[] =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};
	static const glm::mat4 g_CaptureVPs[] =
	{
		g_CaptureProjection * g_CaptureViews[0],
		g_CaptureProjection * g_CaptureViews[1],
		g_CaptureProjection * g_CaptureViews[2],
		g_CaptureProjection * g_CaptureViews[3],
		g_CaptureProjection * g_CaptureViews[4],
		g_CaptureProjection * g_CaptureViews[5]
	};

	VulkanTextureCube::VulkanTextureCube(const std::string& name, ImageFormat format, const void* data, glm::uvec2 size, uint32_t layerSize, uint32_t prefilterSize, bool bCompress)
		: TextureCube(format, layerSize, prefilterSize, bCompress)
	{
		if (m_Compress)
		{
			m_Texture2D = MakeRef<VulkanTexture2D>(ImageFormat::BC6H_UFloat16, size, name);
			TextureCompressor::CompressHDR(data, size, m_Format, m_Texture2D->GetImage());
		}
		else
		{
			m_Texture2D = Texture2D::Create(name, m_Format, size, data, Texture2DSpecifications{});
		}
		m_Sampler = Sampler::PointSampler;

		// The data is not uploaded to the GPU here.
		// It's called from the outside because it requires `shared_from_this()` to be called for safety.
		// But we can't call it from a constructor
	}

	VulkanTextureCube::VulkanTextureCube(const Ref<Texture2D>& texture, uint32_t layerSize, uint32_t prefilterSize, bool bCompress)
		: TextureCube(texture, layerSize, prefilterSize, bCompress)
	{
		m_Sampler = Sampler::PointSampler;

		// The data is not uploaded to the GPU here.
		// It's called from the outside because it requires `shared_from_this()` to be called for safety.
		// But we can't call it from a constructor
	}

	void VulkanTextureCube::SetLayerSize(uint32_t layerSize)
	{
		if (m_Size.x == layerSize)
			return;

		m_Size = glm::uvec3(layerSize, layerSize, 1u);
		GenerateIBL();
	}

	void VulkanTextureCube::SetPrefilterSize(uint32_t prefilterSize)
	{
		if (m_PrefilterSize == prefilterSize)
			return;

		m_PrefilterSize = prefilterSize;
		GenerateIBL();
	}

	void VulkanTextureCube::SetData(DataBuffer data, ImageFormat format, bool bCompress)
	{
		const bool bCompressStateChanged = bCompress != m_Compress;
		m_Compress = bCompress;
		m_Format = format;
		if (m_Compress)
		{
			const glm::uvec2 size = m_Texture2D->GetSize();
			if (bCompressStateChanged) // Only makes sense to recrete it if the state changed because we need it to be in `BC6H_UFloat16` format
				m_Texture2D = MakeRef<VulkanTexture2D>(ImageFormat::BC6H_UFloat16, size, m_Texture2D->GetImage()->GetDebugName());
			TextureCompressor::CompressHDR(data.Data, size, m_Format, m_Texture2D->GetImage());
		}
		else
		{
			m_Texture2D->SetData(data, format);
		}
		GenerateIBL();
	}

	void VulkanTextureCube::GenerateIBL()
	{
		m_Loaded = false;

		ImageSpecifications imageSpecs;
		imageSpecs.Size = m_Size;
		imageSpecs.Format = m_Format;
		imageSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
		imageSpecs.Layout = ImageLayoutType::RenderTarget;
		imageSpecs.bIsCube = true;
		m_Image = MakeRef<VulkanImage>(imageSpecs, "CubeImage");
		m_CubemapSampler = MakeRef<VulkanSampler>(FilterMode::Trilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, float(m_Image->GetMipsCount() - 1u));

		ImageSpecifications irradianceImageSpecs;
		irradianceImageSpecs.Size = glm::uvec3{ TextureCube::IrradianceSize, TextureCube::IrradianceSize, 1 };
		irradianceImageSpecs.Format = m_Format;
		irradianceImageSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		irradianceImageSpecs.Layout = ImageLayoutType::RenderTarget;
		irradianceImageSpecs.bIsCube = true;
		m_IrradianceImage = MakeRef<VulkanImage>(irradianceImageSpecs, "IrradianceCubeImage");

		ImageSpecifications prefilterImageSpecs;
		prefilterImageSpecs.Size = glm::uvec3{ m_PrefilterSize, m_PrefilterSize, 1 };
		prefilterImageSpecs.Format = m_Format;
		prefilterImageSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
		prefilterImageSpecs.Layout = ImageLayoutType::RenderTarget;
		prefilterImageSpecs.bIsCube = true;
		prefilterImageSpecs.MipsCount = glm::min(CalculateMipCount(prefilterImageSpecs.Size), 6u);
		m_PrefilterImage = MakeRef<VulkanImage>(prefilterImageSpecs, "PrefilterCubeImage");
		m_PrefilterImageSampler = MakeRef<VulkanSampler>(FilterMode::Trilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, float(m_PrefilterImage->GetMipsCount() - 1u));

		m_IBLPipeline = RenderManager::CreateIBLPipeline(m_Image);
		m_IrradiancePipeline = RenderManager::CreateIrradiancePipeline(m_IrradianceImage);
		m_PrefilterPipeline = RenderManager::CreatePrefilterPipeline(m_PrefilterImage);

		const void* renderpassHandle = m_IBLPipeline->GetRenderPassHandle();
		const void* irradianceRenderpassHandle = m_IrradiancePipeline->GetRenderPassHandle();
		ImageView imageView{};
		imageView.LayersCount = 1;
		const glm::uvec2 squareSize = { m_Size.x, m_Size.y };
		const glm::uvec2 irradianceSquareSize = { TextureCube::IrradianceSize, TextureCube::IrradianceSize };
		const glm::uvec2 prefilterSquareSize = { m_PrefilterSize, m_PrefilterSize };

		for (uint32_t i = 0; i < m_Framebuffers.size(); ++i)
		{
			imageView.Layer = i;
			m_Framebuffers[i] = MakeRef<VulkanFramebuffer>(m_Image, imageView, squareSize, renderpassHandle);
			m_IrradianceFramebuffers[i] = MakeRef<VulkanFramebuffer>(m_IrradianceImage, imageView, irradianceSquareSize, irradianceRenderpassHandle);
		}

		imageView = ImageView{};
		imageView.LayersCount = 1;
		const uint32_t mips = prefilterImageSpecs.MipsCount;
		m_PrefilterFramebuffers.resize(mips);
		for (uint32_t mip = 0; mip < mips; ++mip)
		{
			imageView.MipLevel = mip;
			auto& currentLayers = m_PrefilterFramebuffers[mip];

			for (uint32_t layer = 0; layer < currentLayers.size(); ++layer)
			{
				imageView.Layer = layer;
				currentLayers[layer] = MakeRef<VulkanFramebuffer>(m_PrefilterImage, imageView, prefilterSquareSize >> mip, irradianceRenderpassHandle);
			}
		}

		RenderManager::Submit([texture = shared_from_this()](const Ref<CommandBuffer>& cmd)
		{
			const bool bCompress = texture->m_Compress;
			Ref<PipelineGraphics>& iblPipeline = texture->GetIBLPipeline();
			Ref<PipelineGraphics>& irradiancePipeline = texture->GetIrradiancePipeline();
			Ref<PipelineGraphics>& prefilterPipeline = texture->GetPrefilterPipeline();

			iblPipeline->SetImageSampler(texture->m_Texture2D->GetImage(), Sampler::PointSampler, 0, 0);
			irradiancePipeline->SetImageSampler(texture->m_Image, texture->m_CubemapSampler, 0, 0);
			prefilterPipeline->SetImageSampler(texture->m_Image, texture->m_CubemapSampler, 0, 0);

			for (uint32_t i = 0; i < texture->m_Framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(iblPipeline, texture->m_Framebuffers[i]);
				cmd->SetGraphicsRootConstants(&g_CaptureVPs[i], nullptr);
				cmd->Draw(36, 0);
				cmd->EndGraphics();
			}

			if (bCompress)
			{
				texture->m_Image = TextureCompressor::CompressHDR(cmd, texture->m_Image);
			}

			for (uint32_t i = 0; i < texture->m_IrradianceFramebuffers.size(); ++i)
			{
				cmd->BeginGraphics(irradiancePipeline, texture->m_IrradianceFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&g_CaptureVPs[i], nullptr);
				cmd->Draw(36, 0);
				cmd->EndGraphics();
			}

			struct FragmentPushData
			{
				float Roughness;
				uint32_t CubemapRes;
			} fragmentPushData;
			fragmentPushData.CubemapRes = texture->m_Size.x;

			const uint32_t mipsCount = (uint32_t)texture->m_PrefilterFramebuffers.size();
			for (uint32_t mip = 0; mip < mipsCount; ++mip)
			{
				fragmentPushData.Roughness = float(mip) / float(mipsCount - 1);
				auto& currentLayers = texture->m_PrefilterFramebuffers[mip];
				for (uint32_t layer = 0; layer < currentLayers.size(); ++layer)
				{
					cmd->BeginGraphics(prefilterPipeline, currentLayers[layer]);
					cmd->SetGraphicsRootConstants(&g_CaptureVPs[layer], &fragmentPushData);
					cmd->Draw(36, 0);
					cmd->EndGraphics();
				}
			}

			if (bCompress)
			{
				texture->m_PrefilterImage = TextureCompressor::CompressHDR(cmd, texture->m_PrefilterImage);
			}

			texture->m_Loaded = true;

			if (Application::Get().IsGame())
				texture->m_Texture2D.reset(); // Reset in game builds since it's not needed anymore.
		});
	}
}
