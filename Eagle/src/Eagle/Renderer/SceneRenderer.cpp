#include "egpch.h"
#include "SceneRenderer.h"
#include "RenderManager.h"
#include "Eagle/Renderer/VidWrappers/StagingManager.h"

#include "VidWrappers/RenderCommandManager.h"

#include "Tasks/BloomPassTask.h" 
#include "Tasks/SkyboxPassTask.h" 
#include "Tasks/PostprocessingPassTask.h" 
#include "Tasks/GridTask.h" 
#include "Tasks/TransparencyTask.h"
#include "Tasks/RenderSkeletalMeshesTask.h"
#include "Tasks/RenderMeshesTask.h"
#include "Tasks/RenderSpritesTask.h"
#include "Tasks/TAATask.h"
#include "Tasks/VolumetricLightTask.h"
#include "Tasks/DOFTask.h"
#include "Tasks/MotionBlurTask.h"
#include "Tasks/ScreenSpaceReflectionsTask.h"
#include "Tasks/ParticleSystemTask.h"

#include "Eagle/Debug/CPUTimings.h" 
#include "Eagle/Debug/GPUTimings.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Camera/Camera.h"

namespace Eagle
{
	struct CameraData
	{
		glm::mat4 View;
		glm::mat4 InvViewProj;
	};

	template <typename TaskClass, typename Task, typename... Args>
	static void InitOptionalTask(Ref<Task>& task, const SceneRendererSettings& settings, bool bEnabled, Args&&... args)
	{
		if (task)
		{
			if (!bEnabled)
				task.reset(); // Deallocating task
			else
				task->InitWithOptions(settings);
		}
		else if (bEnabled)
			task = MakeRef<TaskClass>(std::forward<Args>(args)...);
	}

	SceneRenderer::SceneRenderer(const glm::uvec2 size, const SceneRendererSettings& options)
		: m_Size(size)
	{
		m_bIsGame = Application::Get().IsGame();
		SetOptions(options);
		m_Options_RT = m_Options;

		{
			BufferSpecifications cameraViewDataBufferSpecs;
			cameraViewDataBufferSpecs.Size = sizeof(CameraData);
			cameraViewDataBufferSpecs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
			cameraViewDataBufferSpecs.Layout = BufferReadAccess::Uniform;
			m_CameraDataBuffer = Buffer::Create(cameraViewDataBufferSpecs, "CameraData");
		}

		m_FinalImage = Image::Create(GetOutputImageSpecs(), "Renderer_LDR");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R11G11B10_Float;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { m_Size.x, m_Size.y, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferSrc;
		colorSpecs.MipsCount = UINT_MAX;
		m_HDRRTImage = Image::Create(colorSpecs, "Renderer_HDR");

		m_GBuffer.Init({ m_Size, 1 });
		m_GBuffer.InitOptional(m_Options.InternalState, glm::uvec3(m_Size, 1u));
		// Create tasks
		m_RenderMeshesTask = MakeRef<RenderMeshesTask>(*this);
		m_RenderSkeletalMeshesTask = MakeRef<RenderSkeletalMeshesTask>(*this);
		m_RenderSpritesTask = MakeRef<RenderSpritesTask>(*this);
		m_RenderDecalsTask = MakeRef<RenderDecalsTask>(*this);
		m_LightsManagerTask = MakeRef<LightsManagerTask>(*this);
		m_GeometryManagerTask = MakeRef<GeometryManagerTask>(*this);
		m_RenderLinesTask = MakeRef<RenderLinesTask>(*this);
		m_RenderTrianglesTask = MakeRef<RenderTrianglesTask>(*this);
		m_RenderBillboardsTask = MakeRef<RenderBillboardsTask>(*this, m_HDRRTImage);
		m_RenderLitTextTask = MakeRef<RenderTextLitTask>(*this);
		m_RenderUnlitTextTask = MakeRef<RenderTextUnlitTask>(*this, m_HDRRTImage);
		m_PBRPassTask = MakeRef<PBRPassTask>(*this, m_HDRRTImage);
		m_ShadowPassTask = MakeRef<ShadowPassTask>(*this);
		m_SkyboxPassTask = MakeRef<SkyboxPassTask>(*this, m_HDRRTImage);
		m_PostProcessingPassTask = MakeRef<PostprocessingPassTask>(*this, m_HDRRTImage);
		m_GridTask = MakeRef<GridTask>(*this);
		m_TransparencyTask = MakeRef<TransparencyTask>(*this);
		m_Text2DTask = MakeRef<RenderText2DTask>(*this);
		m_Images2DTask = MakeRef<RenderImages2DTask>(*this);
		m_DOFTask = MakeRef<DOFTask>(*this);
		m_ParticleTask = MakeRef<ParticleSystemTask>(*this);
		
		InitOptionalTask<BloomPassTask>(m_BloomTask, options, options.BloomSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<SSAOTask>(m_SSAOTask, options, options.AO == AmbientOcclusion::SSAO, *this);
		InitOptionalTask<GTAOTask>(m_GTAOTask, options, options.AO == AmbientOcclusion::GTAO, *this);
		InitOptionalTask<TAATask>(m_TAATask, options, options.AA == AAMethod::TAA, *this);
		InitOptionalTask<VolumetricLightTask>(m_VolumetricTask, options, options.VolumetricSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<FogPassTask>(m_FogTask, options, options.FogSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<MotionBlurTask>(m_MotionBlurTask, options, options.MotionBlur.bEnable, *this);
		InitOptionalTask<ScreenSpaceReflectionsTask>(m_ScreenSpaceReflectionsTask, options, options.ScreenSpaceReflections.bEnable, *this);

		InitWithOptions();
	}

	void SceneRenderer::Render(const Camera* camera, const glm::mat4& viewMat, glm::vec3 viewPosition, glm::vec3 viewDirection)
	{
		EG_ASSERT(camera);

		std::vector<glm::mat4> cameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> cameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);

		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			cameraCascadeProjections[i] = camera->GetCascadeProjection(i);
			cameraCascadeFarPlanes[i] = camera->GetCascadeFarPlane(i);
		}

		RenderManager::Submit([renderer = shared_from_this(), viewMat, proj = camera->GetProjection(), viewPosition, viewDirection, bRenderGrid = m_bGridEnabled, options = m_Options,
			cascadeProjections = std::move(cameraCascadeProjections), cascadeFarPlanes = std::move(cameraCascadeFarPlanes), shadowDistance = camera->GetShadowFarClip(),
			cascadesSmoothTransitionAlpha = camera->GetCascadesSmoothTransitionAlpha(), zNear = camera->GetPerspectiveNearClip(), zFar = camera->GetPerspectiveFarClip(),
			cameraFov = camera->GetPerspectiveVerticalFOV()](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_ZNear = zNear;
			renderer->m_ZFar = zFar;
			renderer->m_CameraFOV = cameraFov;
			if (renderer->m_Options_RT != options)
			{
				renderer->m_Options_RT = options;
				renderer->InitWithOptions();
			}
			renderer->m_Options_RT.InternalState.CascadesSmoothTransitionAlpha = cascadesSmoothTransitionAlpha;

			renderer->m_Stats[renderer->m_FrameIndex] = Statistics();
			renderer->m_Stats2D[renderer->m_FrameIndex] = Statistics2D();

			renderer->m_PrevView = renderer->m_View;
			renderer->m_PrevProjection = renderer->m_Projection;
			renderer->m_PrevViewProjection = renderer->m_ViewProjection;

			renderer->m_View = viewMat;
			renderer->m_Projection = proj;
			renderer->m_ViewProjection = renderer->m_Projection * renderer->m_View;
			renderer->m_InvViewProjection = glm::inverse(renderer->m_ViewProjection);
			renderer->m_ViewPos = viewPosition;
			renderer->m_ViewDir = viewDirection;
			renderer->m_CameraCascadeProjections = std::move(cascadeProjections);
			renderer->m_CameraCascadeFarPlanes = std::move(cascadeFarPlanes);
			renderer->m_MaxShadowDistance = shadowDistance;

			if (options.InternalState.bJitter)
			{
				// The range of numbers from Halton sequence is between 0 to 1.
				// In order to use these numbers as offset for jittering,
				// we need to adjust the range so that the positions are jittered both in positiveand negative directionsand are not jittered more than the size
				glm::vec2 jitter = RenderManager::GetHalton();
				jitter = ((jitter - 0.5f) / glm::vec2(renderer->m_Size)) * 2.f;
				cmd->Write(renderer->m_Jitter, &jitter, sizeof(glm::vec2), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
				cmd->Barrier(renderer->m_Jitter);
			}

			// Update camera data
			{
				CameraData cameraData;
				cameraData.View = renderer->m_View;
				cameraData.InvViewProj = renderer->m_InvViewProjection;
				cmd->Write(renderer->m_CameraDataBuffer, &cameraData, sizeof(CameraData), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
			}

			renderer->m_LightsManagerTask->RecordCommandBuffer(cmd);
			renderer->m_GeometryManagerTask->RecordCommandBuffer(cmd);
			renderer->m_RenderMeshesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderSpritesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderSkeletalMeshesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderLitTextTask->RecordCommandBuffer(cmd);
			renderer->m_RenderDecalsTask->RecordCommandBuffer(cmd);
			renderer->m_ShadowPassTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.AO == AmbientOcclusion::SSAO)
				renderer->m_SSAOTask->RecordCommandBuffer(cmd);
			else if (renderer->m_Options_RT.AO == AmbientOcclusion::GTAO)
				renderer->m_GTAOTask->RecordCommandBuffer(cmd);

			renderer->m_PBRPassTask->RecordCommandBuffer(cmd);

			if (renderer->IsSkyboxEnabled() && renderer->IsRenderSkyboxEnabled())
				renderer->m_SkyboxPassTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.FogSettings.bEnable)
				renderer->m_FogTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.VolumetricSettings.bEnable)
				renderer->m_VolumetricTask->RecordCommandBuffer(cmd);

			renderer->m_RenderBillboardsTask->RecordCommandBuffer(cmd);
			renderer->m_RenderUnlitTextTask->RecordCommandBuffer(cmd);
			renderer->m_RenderLinesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderTrianglesTask->RecordCommandBuffer(cmd);
			
			if (renderer->m_MotionBlurTask)
				renderer->m_MotionBlurTask->RecordCommandBuffer(cmd);

			// TODO v0.7: Particle reflections in SSR pass?
			// TODO: Should this be after `TransparencyTask`?
			renderer->m_ParticleTask->RecordCommandBuffer(cmd);
			
			if (renderer->m_ScreenSpaceReflectionsTask)
				renderer->m_ScreenSpaceReflectionsTask->RecordCommandBuffer(cmd);

			renderer->m_TransparencyTask->RecordCommandBuffer(cmd);
			renderer->m_DOFTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.AA == AAMethod::TAA)
				renderer->m_TAATask->RecordCommandBuffer(cmd);

			if (renderer->m_GBuffer.DepthHistory)
				cmd->CopyImage(renderer->m_GBuffer.Depth, renderer->m_GBuffer.DepthHistory, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			if (renderer->m_GBuffer.NormalsHistory)
				cmd->CopyImage(renderer->m_GBuffer.Geometry_Shading_Normals, renderer->m_GBuffer.NormalsHistory, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);

			renderer->m_Images2DTask->RecordCommandBuffer(cmd);
			renderer->m_Text2DTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.BloomSettings.bEnable)
				renderer->m_BloomTask->RecordCommandBuffer(cmd);
			renderer->m_PostProcessingPassTask->RecordCommandBuffer(cmd);

			if (bRenderGrid)
				renderer->m_GridTask->RecordCommandBuffer(cmd);

			// Handle object picking. Always enabled in editor mode
			if (!renderer->IsRuntime() || options.bEnableObjectPicking)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Copying ObjectID image");
				EG_CPU_TIMING_SCOPED("Copying ObjectID image");

				if (!renderer->m_GBuffer.ObjectIDCopy)
				{
					ImageSpecifications objectIDCopySpecs;
					objectIDCopySpecs.Format = ImageFormat::R32_SInt;
					objectIDCopySpecs.Size = renderer->m_GBuffer.ObjectID->GetSize();
					objectIDCopySpecs.Usage = ImageUsage::TransferSrc | ImageUsage::TransferDst | ImageUsage::Sampled;
					objectIDCopySpecs.MemoryType = MemoryType::GpuToCpu;
					renderer->m_GBuffer.ObjectIDCopy = Image::Create(objectIDCopySpecs, "GBuffer_ObjectIDCopy");
				}

				cmd->CopyImage(renderer->m_GBuffer.ObjectID, renderer->m_GBuffer.ObjectIDCopy,
					ImageLayoutType::Unknown, ImageReadAccess::CopySource);
			}
			else
			{
				// Free resource
				renderer->m_GBuffer.ObjectIDCopy.reset();
			}

			if (renderer->m_bIsGame)
				RenderManager::SetPresentImage(renderer->m_FinalImage);

			renderer->m_FrameIndex = (renderer->m_FrameIndex + 1) % RendererConfig::FramesInFlight;
		});
	}

	void SceneRenderer::SetOutputImage(const Ref<Image>& image)
	{
		RenderManager::Submit([renderer = shared_from_this(), image](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_FinalImage = image;
		});
	}

	ImageSpecifications SceneRenderer::GetOutputImageSpecs() const
	{
		ImageSpecifications specs;
		specs.Format = ImageFormat::R8G8B8A8_UNorm;
		specs.Layout = ImageLayoutType::RenderTarget;
		specs.Size = { m_Size.x, m_Size.y, 1 };
		specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferSrc | ImageUsage::TransferDst;

		return specs;
	}

	void SceneRenderer::AddParticleSystem(const ParticleSystemComponent& system)
	{
		m_ParticleTask->AddParticleSystem(system);
	}

	void SceneRenderer::UpdateParticleSystem(const ParticleSystemComponent& system)
	{
		m_ParticleTask->UpdateParticleSystem(system);
	}

	void SceneRenderer::RemoveParticleSystem(const ParticleSystemComponent& system)
	{
		m_ParticleTask->RemoveParticleSystem(system);
	}

	void SceneRenderer::UpdateParticleTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		m_ParticleTask->UpdateTransforms(systems);
	}

	void SceneRenderer::RemoveAllParticleSystems()
	{
		m_ParticleTask->RemoveAllParticleSystems();
	}

	void SceneRenderer::SetSkybox(const Ref<AssetTextureCube>& cubemap)
	{
		RenderManager::Submit([renderer = shared_from_this(), cubemap](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_Cubemap = cubemap;
		});
	}

	void SceneRenderer::SetSkybox(const SkySettings& sky)
	{
		RenderManager::Submit([renderer = shared_from_this(), sky](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_Sky = sky;
		});
	}

	void SceneRenderer::SetSkyboxIntensity(float intensity)
	{
		RenderManager::Submit([renderer = shared_from_this(), intensity](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_CubemapIntensity = glm::max(0.f, intensity);
		});
	}

	void SceneRenderer::SetUseSkyAsBackground(bool value)
	{
		RenderManager::Submit([renderer = shared_from_this(), value](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_bUseSkyAsBackground = value;
		});
	}

	void SceneRenderer::SetOptions(const SceneRendererSettings& options)
	{
		m_Options = options;
		
		const bool bTAAEnabled = m_Options.AA == AAMethod::TAA;
		m_Options.InternalState.bMotionBuffer = (m_Options.AO == AmbientOcclusion::GTAO) || bTAAEnabled || m_Options.MotionBlur.bEnable || m_Options.ScreenSpaceReflections.bEnable;
		m_Options.InternalState.bJitter = bTAAEnabled;
		m_Options.InternalState.bDepthHistory = m_Options.ScreenSpaceReflections.bEnable;
		m_Options.InternalState.bNormalHistory = m_Options.ScreenSpaceReflections.bEnable;
	}

	void SceneRenderer::SetViewportSize(const glm::uvec2 size)
	{
		if (m_Size == size)
			return;

		m_Size = size;

		RenderManager::Wait();
		RenderManager::SetImmediateDeletionMode(true);

		m_FinalImage->Resize({ m_Size, 1 });
		m_HDRRTImage->Resize({ m_Size, 1 });
		m_GBuffer.Resize({ m_Size, 1 });

		// Tasks
		m_RenderMeshesTask->OnResize(m_Size);
		m_RenderSkeletalMeshesTask->OnResize(m_Size);
		m_RenderSpritesTask->OnResize(m_Size);
		m_RenderDecalsTask->OnResize(m_Size);
		m_LightsManagerTask->OnResize(m_Size);
		m_RenderLinesTask->OnResize(m_Size);
		m_RenderTrianglesTask->OnResize(m_Size);
		m_RenderBillboardsTask->OnResize(m_Size);
		m_RenderUnlitTextTask->OnResize(m_Size);
		m_RenderLitTextTask->OnResize(m_Size);
		m_Text2DTask->OnResize(m_Size);
		m_Images2DTask->OnResize(m_Size);
		m_PBRPassTask->OnResize(m_Size);
		m_ShadowPassTask->OnResize(m_Size);
		m_SkyboxPassTask->OnResize(m_Size);
		m_PostProcessingPassTask->OnResize(m_Size);
		m_GridTask->OnResize(m_Size);
		m_TransparencyTask->OnResize(m_Size);
		m_DOFTask->OnResize(m_Size);
		m_ParticleTask->OnResize(m_Size);

		if (m_BloomTask)
			m_BloomTask->OnResize(m_Size);

		if (m_VolumetricTask)
			m_VolumetricTask->OnResize(m_Size);
		if (m_FogTask)
			m_FogTask->OnResize(m_Size);

		if (m_SSAOTask)
			m_SSAOTask->OnResize(m_Size);
		else if (m_GTAOTask)
			m_GTAOTask->OnResize(m_Size);

		if (m_TAATask)
			m_TAATask->OnResize(m_Size);

		if (m_MotionBlurTask)
			m_MotionBlurTask->OnResize(m_Size);

		if (m_ScreenSpaceReflectionsTask)
			m_ScreenSpaceReflectionsTask->OnResize(m_Size);

		RenderManager::SetImmediateDeletionMode(false);
		RenderManager::ReleasePendingResources();
		StagingManager::ReleaseBuffers();
	}

	void SceneRenderer::SetMeshesAnimationTransforms(std::unordered_map<uint32_t, std::vector<glm::mat4>>&& transforms)
	{
		RenderManager::Submit([renderer = shared_from_this(), transforms = std::move(transforms)](const Ref<CommandBuffer>&)
		{
			renderer->m_AnimationTransforms = std::move(transforms);
		});
	}

	void SceneRenderer::SetSkeletalParticleAnimationTransforms(std::unordered_map<GUID, std::unordered_map<GUID, std::vector<glm::mat4>>>&& transforms)
	{
		RenderManager::Submit([renderer = shared_from_this(), transforms = std::move(transforms)](const Ref<CommandBuffer>&)
		{
			renderer->m_SkeletalParticlesAnimationTransforms = std::move(transforms);
		});
	}

	void SceneRenderer::InitWithOptions()
	{
		auto& options = m_Options_RT;

		if (options.InternalState.bJitter)
		{
			if (!m_Jitter)
			{
				BufferSpecifications specs;
				specs.Size = sizeof(glm::vec2);
				specs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
				m_Jitter = Buffer::Create(specs, "Jitter");
			}
		}
		else
		{
			m_Jitter.reset();
		}

		m_GBuffer.InitOptional(options.InternalState, glm::uvec3(m_Size, 1u));
		m_PhotoLinearScale = CalculatePhotoLinearScale(options.PhotoLinearTonemappingParams, options.Gamma);
		m_GeometryManagerTask->InitWithOptions(options);
		m_RenderMeshesTask->InitWithOptions(options);
		m_RenderSkeletalMeshesTask->InitWithOptions(options);
		m_RenderSpritesTask->InitWithOptions(options);
		m_RenderDecalsTask->InitWithOptions(options);
		m_RenderBillboardsTask->InitWithOptions(options);
		m_RenderLitTextTask->InitWithOptions(options);
		m_Text2DTask->InitWithOptions(options);
		m_Images2DTask->InitWithOptions(options);
		m_PBRPassTask->InitWithOptions(options);
		m_RenderLinesTask->InitWithOptions(options);
		m_RenderTrianglesTask->InitWithOptions(options);
		m_PostProcessingPassTask->InitWithOptions(options);
		m_TransparencyTask->InitWithOptions(options);
		m_GridTask->InitWithOptions(options);
		m_ShadowPassTask->InitWithOptions(options);
		m_DOFTask->InitWithOptions(options);
		m_ParticleTask->InitWithOptions(options);

		InitOptionalTask<BloomPassTask>(m_BloomTask, options, options.BloomSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<SSAOTask>(m_SSAOTask, options, options.AO == AmbientOcclusion::SSAO, *this);
		InitOptionalTask<GTAOTask>(m_GTAOTask, options, options.AO == AmbientOcclusion::GTAO, *this);
		InitOptionalTask<TAATask>(m_TAATask, options, options.AA == AAMethod::TAA, *this);
		InitOptionalTask<VolumetricLightTask>(m_VolumetricTask, options, options.VolumetricSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<FogPassTask>(m_FogTask, options, options.FogSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<MotionBlurTask>(m_MotionBlurTask, options, options.MotionBlur.bEnable, *this);
		InitOptionalTask<ScreenSpaceReflectionsTask>(m_ScreenSpaceReflectionsTask, options, options.ScreenSpaceReflections.bEnable, *this);
	}

	void GBuffer::Init(const glm::uvec3& size)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = size;
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc;
		Depth = Image::Create(depthSpecs, "GBuffer_Depth");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = size;
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		Albedo = Image::Create(colorSpecs, "GBuffer_Albedo");

		ImageSpecifications normalSpecs;
		normalSpecs.Format = ImageFormat::R16G16B16A16_Float;
		normalSpecs.Layout = ImageLayoutType::RenderTarget;
		normalSpecs.Size = size;
		normalSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc;
		Geometry_Shading_Normals = Image::Create(normalSpecs, "GBuffer_Geometry_Shading_Normals");

		ImageSpecifications emissiveSpecs;
		emissiveSpecs.Format = ImageFormat::R11G11B10_Float;
		emissiveSpecs.Layout = ImageLayoutType::RenderTarget;
		emissiveSpecs.Size = size;
		emissiveSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		Emissive = Image::Create(emissiveSpecs, "GBuffer_Emissive");

		ImageSpecifications materialSpecs;
		materialSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		materialSpecs.Layout = ImageLayoutType::RenderTarget;
		materialSpecs.Size = size;
		materialSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		MaterialData = Image::Create(materialSpecs, "GBuffer_MaterialData");

		ImageSpecifications flagSpecs;
		flagSpecs.Format = ImageFormat::R8_UNorm;
		flagSpecs.Layout = ImageLayoutType::RenderTarget;
		flagSpecs.Size = size;
		flagSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		Flags = Image::Create(materialSpecs, "GBuffer_Flags");

		ImageSpecifications objectIDSpecs;
		objectIDSpecs.Format = ImageFormat::R32_SInt;
		objectIDSpecs.Layout = ImageLayoutType::RenderTarget;
		objectIDSpecs.Size = size;
		objectIDSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc;
		ObjectID = Image::Create(objectIDSpecs, "GBuffer_ObjectID");

		ImageSpecifications objectIDCopySpecs;
		objectIDCopySpecs.Format = ImageFormat::R32_SInt;
		objectIDCopySpecs.Size = size;
		objectIDCopySpecs.Usage = ImageUsage::TransferSrc | ImageUsage::TransferDst | ImageUsage::Sampled;
		objectIDCopySpecs.MemoryType = MemoryType::GpuToCpu;
		ObjectIDCopy = Image::Create(objectIDCopySpecs, "GBuffer_ObjectIDCopy");
	}
	
	void GBuffer::InitOptional(const SceneRendererInternalState& optional, const glm::uvec3& size)
	{
		if (optional.bMotionBuffer)
		{
			if (!Motion)
			{
				ImageSpecifications velocitySpecs;
				velocitySpecs.Format = ImageFormat::R16G16_Float;
				velocitySpecs.Size = size;
				velocitySpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
				Motion = Image::Create(velocitySpecs, "GBuffer_Motion");
			}
		}
		else
		{
			Motion.reset();
		}

		if (optional.bDepthHistory)
		{
			if (!DepthHistory)
			{
				ImageSpecifications specs;
				specs.Format = Depth->GetFormat();
				specs.Size = size;
				specs.Usage = Depth->GetUsage() | ImageUsage::TransferDst;
				specs.Layout = ImageReadAccess::PixelShaderRead;
				DepthHistory = Image::Create(specs, "GBuffer_DepthHistory");
			}
		}
		else
		{
			DepthHistory.reset();
		}

		if (optional.bNormalHistory)
		{
			if (!NormalsHistory)
			{
				ImageSpecifications specs;
				specs.Format = Geometry_Shading_Normals->GetFormat();
				specs.Size = size;
				specs.Usage = Geometry_Shading_Normals->GetUsage() | ImageUsage::TransferDst;
				specs.Layout = ImageReadAccess::PixelShaderRead;
				NormalsHistory = Image::Create(specs, "GBuffer_NormalsHistory");
			}
		}
		else
		{
			NormalsHistory.reset();
		}
	}
	
	void GBuffer::Resize(const glm::uvec3& size)
	{
		Albedo->Resize(size);
		MaterialData->Resize(size);
		Geometry_Shading_Normals->Resize(size);
		Emissive->Resize(size);
		ObjectID->Resize(size);
		if (ObjectIDCopy)
			ObjectIDCopy->Resize(size);
		Depth->Resize(size);
		Flags->Resize(size);
		if (Motion)
			Motion->Resize(size);
		
		const bool bNeedClear = DepthHistory || NormalsHistory;
		if (DepthHistory)
			DepthHistory->Resize(size);
		if (NormalsHistory)
			NormalsHistory->Resize(size);

		if (bNeedClear)
		{
			RenderManager::Submit([depth = DepthHistory, normals = NormalsHistory](const Ref<CommandBuffer>& cmd) mutable
			{
				constexpr glm::vec4 clearColor = glm::vec4(0.f);
				if (depth)
					cmd->ClearDepthStencilImage(depth, 0.f, 0, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
				if (normals)
					cmd->ClearColorImage(normals, clearColor, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			});
		}
	}
}
