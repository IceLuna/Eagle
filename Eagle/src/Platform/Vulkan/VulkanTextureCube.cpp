#include "egpch.h"
#include "VulkanTextureCube.h"
#include "VulkanTexture2D.h"
#include "VulkanFramebuffer.h"
#include "VulkanPipelineGraphics.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSampler.h"

#include "Eagle/Renderer/RenderCommandManager.h"

#include <glm/gtx/transform.hpp>

namespace Eagle
{
	static const glm::mat4 g_CaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
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

	VulkanTextureCube::VulkanTextureCube(const Path& filepath, uint32_t layerSize)
		: TextureCube(filepath, layerSize)
	{
		m_Texture2D = MakeRef<VulkanTexture2D>(filepath, Texture2DSpecifications{});
		m_Sampler = Sampler::PointSampler;

		GenerateIBL();
	}

	VulkanTextureCube::VulkanTextureCube(const Ref<Texture2D>& texture, uint32_t layerSize)
		: TextureCube(texture, layerSize)
	{
		m_Sampler = Sampler::PointSampler;

		GenerateIBL();
	}

	void VulkanTextureCube::GenerateIBL()
	{
		ImageSpecifications imageSpecs;
		imageSpecs.Size = m_Size;
		imageSpecs.Format = ImageFormat::R16G16B16A16_Float;
		imageSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
		imageSpecs.Layout = ImageLayoutType::RenderTarget;
		imageSpecs.bIsCube = true;
		m_Image = MakeRef<VulkanImage>(imageSpecs, "CubeImage");

		ImageSpecifications irradianceImageSpecs;
		irradianceImageSpecs.Size = glm::uvec3{ TextureCube::IrradianceSize, TextureCube::IrradianceSize, 1 };
		irradianceImageSpecs.Format = ImageFormat::R16G16B16A16_Float;
		irradianceImageSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		irradianceImageSpecs.Layout = ImageLayoutType::RenderTarget;
		irradianceImageSpecs.bIsCube = true;
		m_IrradianceImage = MakeRef<VulkanImage>(irradianceImageSpecs, "IrradianceCubeImage");

		ImageSpecifications prefilterImageSpecs;
		prefilterImageSpecs.Size = glm::uvec3{ TextureCube::PrefilterSize, TextureCube::PrefilterSize, 1 };
		prefilterImageSpecs.Format = ImageFormat::R16G16B16A16_Float;
		prefilterImageSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
		prefilterImageSpecs.Layout = ImageLayoutType::RenderTarget;
		prefilterImageSpecs.bIsCube = true;
		prefilterImageSpecs.MipsCount = glm::min(CalculateMipCount(prefilterImageSpecs.Size), 6u);
		m_PrefilterImage = MakeRef<VulkanImage>(prefilterImageSpecs, "PrefilterCubeImage");
		m_PrefilterImageSampler = MakeRef<VulkanSampler>(FilterMode::Trilinear, AddressMode::ClampToOpaqueBlack, CompareOperation::Never, 0.f, float(prefilterImageSpecs.MipsCount));

		const void* renderpassHandle = Renderer::GetIBLPipeline()->GetRenderPassHandle();
		const void* irradianceRenderpassHandle = Renderer::GetIrradiancePipeline()->GetRenderPassHandle();
		ImageView imageView{};
		const glm::uvec2 squareSize = { m_Size.x, m_Size.y };
		const glm::uvec2 irradianceSquareSize = { TextureCube::IrradianceSize, TextureCube::IrradianceSize };
		const glm::uvec2 prefilterSquareSize = { TextureCube::PrefilterSize, TextureCube::PrefilterSize };
		for (uint32_t i = 0; i < m_Framebuffers.size(); ++i)
		{
			imageView.Layer = i;
			m_Framebuffers[i] = MakeRef<VulkanFramebuffer>(m_Image, imageView, squareSize, renderpassHandle);
			m_IrradianceFramebuffers[i] = MakeRef<VulkanFramebuffer>(m_IrradianceImage, imageView, irradianceSquareSize, irradianceRenderpassHandle);
		}

		imageView = ImageView{};
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

		Renderer::Submit([this](Ref<CommandBuffer>& cmd)
		{
			struct PushData
			{
				glm::mat4 VP;
			} pushData;

			struct FragmentPushData
			{
				float Roughness;
				uint32_t ResPerFace;
			} fragmentPushData;
			fragmentPushData.ResPerFace = m_Size.x;

			Ref<PipelineGraphics>& iblPipeline = Renderer::GetIBLPipeline();
			Ref<PipelineGraphics>& irradiancePipeline = Renderer::GetIrradiancePipeline();
			Ref<PipelineGraphics>& prefilterPipeline = Renderer::GetPrefilterPipeline();

			iblPipeline->SetImageSampler(m_Texture2D->GetImage(), Sampler::PointSampler, 0, 0);
			irradiancePipeline->SetImageSampler(m_Image, Sampler::PointSampler, 0, 0);
			prefilterPipeline->SetImageSampler(m_Image, Sampler::PointSampler, 0, 0);

			for (uint32_t i = 0; i < m_Framebuffers.size(); ++i)
			{
				pushData.VP = g_CaptureVPs[i];
				cmd->BeginGraphics(iblPipeline, m_Framebuffers[i]);
				cmd->SetGraphicsRootConstants(&pushData, nullptr);
				cmd->Draw(36, 0);
				cmd->EndGraphics();
			}

			for (uint32_t i = 0; i < m_IrradianceFramebuffers.size(); ++i)
			{
				pushData.VP = g_CaptureVPs[i];
				cmd->BeginGraphics(irradiancePipeline, m_IrradianceFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&pushData, nullptr);
				cmd->Draw(36, 0);
				cmd->EndGraphics();
			}

			const uint32_t mipsCount = (uint32_t)m_PrefilterFramebuffers.size();
			for (uint32_t mip = 0; mip < mipsCount; ++mip)
			{
				fragmentPushData.Roughness = (float)mip / (float)(mipsCount - 1);
				auto& currentLayers = m_PrefilterFramebuffers[mip];
				for (uint32_t layer = 0; layer < currentLayers.size(); ++layer)
				{
					pushData.VP = g_CaptureVPs[layer];
					cmd->BeginGraphics(prefilterPipeline, currentLayers[layer]);
					cmd->SetGraphicsRootConstants(&pushData, &fragmentPushData);
					cmd->Draw(36, 0);
					cmd->EndGraphics();
				}
			}

			// Render-pass doesn't transition layout of mips, so we need to do that manually
			for (uint32_t mip = 1; mip < m_PrefilterImage->GetMipsCount(); ++mip)
			{
				ImageView view{ mip };
				cmd->TransitionLayout(m_PrefilterImage, view, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			}
			cmd->GenerateMips(m_PrefilterImage, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
		});
	}
}
