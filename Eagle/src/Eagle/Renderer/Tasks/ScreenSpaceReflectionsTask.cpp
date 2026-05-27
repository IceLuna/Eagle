#include "egpch.h"
#include "ScreenSpaceReflectionsTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/Asset.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanImage.h"
#include <FidelityFX/host/backends/vk/ffx_vk.h>

namespace Eagle
{
	static FfxSurfaceFormat ToFfxFormat(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::R16G16B16A16_Float: return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
		}

		EG_CORE_ASSERT(!"Unknown format");
		return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
	}

	FfxResource ToFfxResource(const Ref<Image>& image, const wchar_t* name)
	{
		VkImage vkImage = image ? (VkImage)image->GetHandle() : VK_NULL_HANDLE;
		FfxResourceDescription desc = ffxGetImageResourceDescriptionVK(vkImage, image ? Cast<VulkanImage>(image)->GetCreateInfo() : VkImageCreateInfo{}, FFX_RESOURCE_USAGE_READ_ONLY);

		return ffxGetResourceVK(vkImage, desc, name, FFX_RESOURCE_STATE_COMPUTE_READ);
	}

	ScreenSpaceReflectionsTask::ScreenSpaceReflectionsTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		m_Size = m_Renderer.GetViewportSize();

		InitPipeline();
		InitResources();

		CreateSSSRContext();
	}

	ScreenSpaceReflectionsTask::~ScreenSpaceReflectionsTask()
	{
		DestroySSSRContext();
	}

	void ScreenSpaceReflectionsTask::CreateSSSRContext()
	{
		// Currently, only supported for VK.
		// To support DX12, we need to abstract SSSR function calls
		EG_CORE_ASSERT(RendererContext::Current() == RendererAPIType::Vulkan);

		const VulkanDevice* device = VulkanContext::GetDevice();
		const VulkanPhysicalDevice* physicalDevice = device->GetPhysicalDevice();

		VkPhysicalDevice physicalDeviceVK = physicalDevice->GetVulkanPhysicalDevice();
		VkDevice deviceVK = device->GetVulkanDevice();

		VkDeviceContext vkDeviceContext = { deviceVK, physicalDeviceVK, vkGetDeviceProcAddr };
		FfxDevice ffxDevice = ffxGetDeviceVK(&vkDeviceContext);

		// Initialize the FFX backend
		const size_t scratchBufferSize = ffxGetScratchMemorySizeVK(physicalDeviceVK, FFX_SSSR_CONTEXT_COUNT);
		m_ScratchBuffer.Allocate(scratchBufferSize);
		m_ScratchBuffer.SetToZero();
		FfxErrorCode errorCode = ffxGetInterfaceVK(&m_InitializationParameters.backendInterface, ffxDevice, m_ScratchBuffer.Data(), scratchBufferSize, FFX_SSSR_CONTEXT_COUNT);
		EG_CORE_ASSERT(errorCode == FFX_OK);

		m_InitializationParameters.flags = FFX_SSSR_ENABLE_DEPTH_INVERTED;
		m_InitializationParameters.renderSize.width = m_Size.x;
		m_InitializationParameters.renderSize.height = m_Size.y;
		m_InitializationParameters.normalsHistoryBufferFormat = ToFfxFormat(m_Normals->GetFormat());

		errorCode = ffxSssrContextCreate(&m_Context, &m_InitializationParameters);
		EG_CORE_ASSERT(errorCode == FFX_OK);
	}

	void ScreenSpaceReflectionsTask::DestroySSSRContext()
	{
		RenderManager::SubmitResourceFree([ffxContext = m_Context, scratchBuffer = std::move(m_ScratchBuffer)]() mutable
		{
			ffxSssrContextDestroy(&ffxContext);
			scratchBuffer.Release();
		});

		m_InitializationParameters.backendInterface.scratchBuffer = nullptr;
	}
	
	void ScreenSpaceReflectionsTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Screen-Space Reflections");
		EG_CPU_TIMING_SCOPED("Screen-Space Reflections");

		const auto& settings = m_Renderer.GetOptions_RT().ScreenSpaceReflections;

		auto& gbuffer = m_Renderer.GetGBuffer();
		auto& color = m_Renderer.GetHDROutput();

		const ImageLayout colorLayout = color->GetLayout();
		const ImageLayout depthLayout = gbuffer.Depth->GetLayout();
		const ImageLayout normalsLayout = gbuffer.Normals->GetLayout();
		const ImageLayout albedoLayout = gbuffer.Albedo->GetLayout();
		const ImageLayout materialLayout = gbuffer.MaterialData->GetLayout();
		const ImageLayout motionLayout = gbuffer.Motion->GetLayout();

		const ImageLayout targetLayout = ImageReadAccess::PixelShaderRead | ImageReadAccess::NonPixelShaderRead;
		cmd->TransitionLayout(color, colorLayout, targetLayout);
		cmd->TransitionLayout(gbuffer.Depth, depthLayout, targetLayout);
		cmd->TransitionLayout(gbuffer.Normals, normalsLayout, targetLayout);
		cmd->TransitionLayout(gbuffer.Albedo, albedoLayout, targetLayout);
		cmd->TransitionLayout(gbuffer.MaterialData, materialLayout, targetLayout);
		cmd->TransitionLayout(gbuffer.Motion, motionLayout, targetLayout);

		cmd->ClearColorImage(m_Result, glm::vec4(0), m_Result->GetLayout(), targetLayout);

		// Decode normals for SSSR dispatch
		{
			EG_GPU_TIMING_SCOPED(cmd, "Screen-Space Reflections. Prepare normals");

			cmd->TransitionLayout(m_Normals, m_Normals->GetLayout(), ImageLayoutType::StorageImage);

			m_DecodeNormalsPipeline->SetImageSampler(gbuffer.Normals, Sampler::PointSampler, 0, 0);
			m_DecodeNormalsPipeline->SetImage(m_Normals, 0, 1);

			const glm::uvec3 groupSize = m_DecodeNormalsPipeline->GetWorkGroupSize();
			const glm::uvec2 numGroups = CalcNumGroups(m_Size, groupSize);
			cmd->Dispatch(m_DecodeNormalsPipeline, numGroups.x, numGroups.y, 1, &m_Size);

			cmd->TransitionLayout(m_Normals, ImageLayoutType::StorageImage, targetLayout);
		}

		// FFX SSSR Dispatch
		{
			EG_GPU_TIMING_SCOPED(cmd, "Screen-Space Reflections. Main pass");

			// Note: We're not passing IBL because lighting for it are already calculated.
			// Otherwise, we'd get double contribution

			FfxSssrDispatchDescription dispatchParameters = {};
			dispatchParameters.commandList = ffxGetCommandListVK((VkCommandBuffer)cmd->GetHandle());
			dispatchParameters.color = ToFfxResource(color, L"SSSR_InputColor");
			dispatchParameters.depth = ToFfxResource(gbuffer.Depth, L"SSSR_InputDepth");
			dispatchParameters.motionVectors = ToFfxResource(gbuffer.Motion, L"SSSR_InputMotionVectors");
			dispatchParameters.normal = ToFfxResource(m_Normals, L"SSSR_InputNormal");
			dispatchParameters.materialParameters = ToFfxResource(gbuffer.MaterialData, L"SSSR_InputMetallicAoRoughness");
			dispatchParameters.environmentMap = ToFfxResource(RenderManager::GetDummyIBL()->GetImage(), L"SSSR_InputEnvironmentMapTexture");
			dispatchParameters.brdfTexture = ToFfxResource(RenderManager::GetBRDFLUTImage(), L"SSSR_InputBRDFTexture");
			dispatchParameters.output = ToFfxResource(m_Result, L"SSSR_Output");
			dispatchParameters.normalUnPackMul = 1.0f;
			dispatchParameters.normalUnPackAdd = 0.0f;
			dispatchParameters.motionVectorScale.x = 1.0f;
			dispatchParameters.motionVectorScale.y = 1.0f;
			dispatchParameters.roughnessChannel = 2; // Roughness is in B channel
			dispatchParameters.isRoughnessPerceptual = true;
			dispatchParameters.renderSize.width = m_Size.x;
			dispatchParameters.renderSize.height = m_Size.y;
			dispatchParameters.iblFactor = 0.0f;
			dispatchParameters.temporalStabilityFactor = settings.TemporalStabilityFactor;
			dispatchParameters.depthBufferThickness = settings.DepthBufferThickness;
			dispatchParameters.roughnessThreshold = settings.RoughnessThreshold;
			dispatchParameters.varianceThreshold = settings.VarianceThreshold;
			dispatchParameters.maxTraversalIntersections = settings.MaxTraversalIterations;
			dispatchParameters.minTraversalOccupancy = settings.MinTraversalOccupancy;
			dispatchParameters.mostDetailedMip = 0;
			dispatchParameters.samplesPerQuad = settings.SamplesPerQuad;
			dispatchParameters.temporalVarianceGuidedTracingEnabled = settings.bTemporalVarianceGuidedTracing;

			CameraData matrices = m_Renderer.GetCameraMatrices();
			if (m_Renderer.IsProjectionFlipped())
			{
				// Flip it back for SSSR
				matrices.Proj[1][1] *= -1.f;
				matrices.PrevProj[1][1] *= -1;

				matrices.InvProj = glm::inverse(matrices.Proj);
				matrices.InvViewProj = glm::inverse(matrices.Proj * matrices.View);
				matrices.PrevViewProj = matrices.PrevProj * matrices.PrevView;
			}

			const glm::mat4 invView = glm::inverse(matrices.View);

			memcpy(&dispatchParameters.invViewProjection, &matrices.InvViewProj[0][0], sizeof(glm::mat4));
			memcpy(&dispatchParameters.projection, &matrices.Proj[0][0], sizeof(glm::mat4));
			memcpy(&dispatchParameters.invProjection, &matrices.InvProj[0][0], sizeof(glm::mat4));
			memcpy(&dispatchParameters.view, &matrices.View[0][0], sizeof(glm::mat4));
			memcpy(&dispatchParameters.invView, &invView[0][0], sizeof(glm::mat4));
			memcpy(&dispatchParameters.prevViewProjection, &matrices.PrevViewProj[0][0], sizeof(glm::mat4));

			FfxErrorCode errorCode = ffxSssrContextDispatch(&m_Context, &dispatchParameters);
			EG_CORE_ASSERT(errorCode == FFX_OK);
		}

		// Apply SSSR result
		{
			EG_GPU_TIMING_SCOPED(cmd, "Screen-Space Reflections. Apply results");

			cmd->TransitionLayout(color, targetLayout, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_Result, targetLayout, ImageLayoutType::StorageImage);

			struct PushData
			{
				glm::uvec2 Size;
				glm::vec2 TexelSize;
				glm::vec3 CameraDir;
				uint32_t bVisualizeReflections;
			} pushData;

			pushData.Size = m_Size;
			pushData.TexelSize = 1.f / glm::vec2(m_Size);
			pushData.CameraDir = m_Renderer.GetViewDirection();
			pushData.bVisualizeReflections = settings.bVisualizeReflections ? 1u : 0u;

			m_CompositePipeline->SetImage(m_Result, 0, 0);
			m_CompositePipeline->SetImageSampler(gbuffer.Normals, Sampler::PointSamplerClamp, 0, 1);
			m_CompositePipeline->SetImageSampler(gbuffer.Albedo, Sampler::PointSamplerClamp, 0, 2);
			m_CompositePipeline->SetImageSampler(gbuffer.MaterialData, Sampler::PointSamplerClamp, 0, 3);
			m_CompositePipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSamplerClamp, 0, 4);
			m_CompositePipeline->SetImage(color, 0, 5);

			const glm::uvec3 groupSize = m_CompositePipeline->GetWorkGroupSize();
			const glm::uvec2 numGroups = CalcNumGroups(m_Size, groupSize);
			cmd->Dispatch(m_CompositePipeline, numGroups.x, numGroups.y, 1, &pushData);
		}

		cmd->TransitionLayout(color, color->GetLayout(), colorLayout);
		cmd->TransitionLayout(gbuffer.Depth, targetLayout, depthLayout);
		cmd->TransitionLayout(gbuffer.Normals, targetLayout, normalsLayout);
		cmd->TransitionLayout(gbuffer.Albedo, targetLayout, albedoLayout);
		cmd->TransitionLayout(gbuffer.MaterialData, targetLayout, materialLayout);
		cmd->TransitionLayout(gbuffer.Motion, targetLayout, motionLayout);
	}

	void ScreenSpaceReflectionsTask::OnResize(const glm::uvec2 size)
	{
		m_Size = size;
		InitResources();

		DestroySSSRContext();
		CreateSSSRContext();
	}

	void ScreenSpaceReflectionsTask::InitPipeline()
	{
		{
			PipelineComputeState state{};
			state.ComputeShader = Shader::Create("screen_space_reflections/decode_normals.comp", ShaderType::Compute);
			m_DecodeNormalsPipeline = PipelineCompute::Create(state);
		}

		{
			PipelineComputeState state{};
			state.ComputeShader = Shader::Create("screen_space_reflections/composite.comp", ShaderType::Compute);
			m_CompositePipeline = PipelineCompute::Create(state);
		}
	}

	void ScreenSpaceReflectionsTask::InitResources()
	{
		// Normals
		{
			ImageSpecifications specs{};
			specs.Format = ImageFormat::R16G16B16A16_Float;
			specs.Size = glm::uvec3(m_Size, 1u);
			specs.Usage = ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::TransferSrc;
			m_Normals = Image::Create(specs, "SSSR_Normals");
		}

		// Result
		{
			ImageSpecifications specs{};
			specs.Format = ImageFormat::R16G16B16A16_Float;
			specs.Size = glm::uvec3(m_Size, 1u);
			specs.Usage = ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::TransferDst;
			m_Result = Image::Create(specs, "SSSR_Result");
		}
	}
}
