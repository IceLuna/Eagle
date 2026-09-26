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
#include "Tasks/MSAATask.h"
#include "Tasks/FXAATask.h"
#include "Tasks/VolumetricLightTask.h"
#include "Tasks/DOFTask.h"
#include "Tasks/MotionBlurTask.h"
#include "Tasks/ScreenSpaceReflectionsTask.h"
#include "Tasks/ParticleSystemTask.h"
#include "Tasks/HZBTask.h"

#include "Eagle/Debug/CPUTimings.h" 
#include "Eagle/Debug/GPUTimings.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Camera/Camera.h"

namespace Eagle
{
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

		m_PickRequests.reserve(s_MaxPickRequests - 1);
		m_PickResults.reserve(s_MaxPickRequests);
		m_Options_RT = m_EffectiveOptions;

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
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferSrc | ImageUsage::TransferDst;
		colorSpecs.MipsCount = UINT_MAX;
		m_HDRRTImage = Image::Create(colorSpecs, "Renderer_HDR");

		m_GBuffer.Init({ m_Size, 1 });
		m_GBuffer.InitOptional(m_Options.InternalState, glm::uvec3(m_Size, 1u));
		// Create tasks
		m_SkinCacheTask = MakeRef<SkinCacheTask>(*this);
		m_FrustumCullingTask = MakeRef<FrustumCullingTask>(*this);
		m_LightCullingTask = MakeRef<LightCullingTask>(*this);
		m_RenderMeshesTask = MakeRef<RenderMeshesTask>(*this);
		m_RenderSkeletalMeshesTask = MakeRef<RenderSkeletalMeshesTask>(*this);
		m_RenderSpritesTask = MakeRef<RenderSpritesTask>(*this);
		m_RenderDecalsTask = MakeRef<RenderDecalsTask>(*this);
		m_LightsManagerTask = MakeRef<LightsManagerTask>(*this);
		m_GeometryManagerTask = MakeRef<GeometryManagerTask>(*this);
		m_RenderLinesTask = MakeRef<RenderLinesTask>(*this);
		m_RenderTrianglesTask = MakeRef<RenderTrianglesTask>(*this);
		m_RenderBillboardsTask = MakeRef<RenderBillboardsTask>(*this);
		m_RenderLitTextTask = MakeRef<RenderTextLitTask>(*this);
		m_RenderUnlitTextTask = MakeRef<RenderTextUnlitTask>(*this);
		m_PBRPassTask = MakeRef<PBRPassTask>(*this);
		m_ShadowPassTask = MakeRef<ShadowPassTask>(*this);
		m_SkyboxPassTask = MakeRef<SkyboxPassTask>(*this);
		m_PostProcessingPassTask = MakeRef<PostprocessingPassTask>(*this);
		m_GridTask = MakeRef<GridTask>(*this);
		m_TransparencyTask = MakeRef<TransparencyTask>(*this);
		m_Text2DTask = MakeRef<RenderText2DTask>(*this);
		m_Images2DTask = MakeRef<RenderImages2DTask>(*this);
		m_DOFTask = MakeRef<DOFTask>(*this);
		m_ParticleTask = MakeRef<ParticleSystemTask>(*this);
		m_ScreenSpaceShadows = MakeRef<ScreenSpaceShadowsTask>(*this);
		
		InitOptionalTask<BloomPassTask>(m_BloomTask, options, options.BloomSettings.bEnable, *this);
		InitOptionalTask<SSAOTask>(m_SSAOTask, options, options.AO == AmbientOcclusion::SSAO, *this);
		InitOptionalTask<GTAOTask>(m_GTAOTask, options, options.AO == AmbientOcclusion::GTAO, *this);
		InitOptionalTask<TAATask>(m_TAATask, options, options.AA == AAMethod::TAA, *this);
		InitOptionalTask<MSAATask>(m_MSAATask, options, options.AA == AAMethod::MSAA, *this);
		InitOptionalTask<FXAATask>(m_FXAATask, options, options.AA == AAMethod::FXAA, *this);
		InitOptionalTask<VolumetricLightTask>(m_VolumetricTask, options, options.VolumetricSettings.bEnable, *this);
		InitOptionalTask<FogPassTask>(m_FogTask, options, options.FogSettings.bEnable, *this);
		InitOptionalTask<MotionBlurTask>(m_MotionBlurTask, options, options.MotionBlur.bEnable, *this);
		InitOptionalTask<ScreenSpaceReflectionsTask>(m_ScreenSpaceReflectionsTask, options, options.ScreenSpaceReflections.bEnable, *this);
		InitOptionalTask<HZBTask>(m_HZBTask, options, m_Options.InternalState.bGenerateHZB, *this);

		InitWithOptions();
	}

	void SceneRenderer::Render(const Camera* camera, const glm::mat4& viewMat, glm::vec3 viewPosition, glm::vec3 viewDirection)
	{
		EG_ASSERT(camera);

		// Make sure the slot we're about to reuse has been consumed
		ResolveObjectPicking();

		// Object picking requests
		const uint32_t pickSlotIndex = RenderManager::GetCurrentFrameIndex_CPU();
		std::array<glm::ivec2, s_MaxPickRequests> pickCoords{};
		uint32_t pickCoordsCount = 0;
		if (!IsRuntime() || m_EffectiveOptions.bEnableObjectPicking)
		{
			const uint64_t frame = RenderManager::GetFrameNumber_CPU();
			std::erase_if(m_PickRequests, [frame](const PickRequest& r) { return frame - r.LastRequestedFrame > s_PickRequestLifetime; });

			if (m_MousePickCoord)
				pickCoords[pickCoordsCount++] = *m_MousePickCoord;
			for (const auto& request : m_PickRequests)
				pickCoords[pickCoordsCount++] = request.Coord;

			auto& slot = m_PickSlots[pickSlotIndex];
			slot.Coords = pickCoords;
			slot.CoordsCount = pickCoordsCount;
			slot.bHasMouse = m_MousePickCoord.has_value();
			slot.SubmittedFrame = frame;
			slot.bPending = true; // Even if empty, so that the resolve clears stale results (e.g. mouse left the viewport)
		}
		else
		{
			m_PickRequests.clear();
			m_PickResults.clear();
			m_MousePickResult.reset();
		}

		std::vector<glm::mat4> cameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> cameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);

		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			cameraCascadeProjections[i] = camera->GetCascadeProjection(i);
			cameraCascadeFarPlanes[i] = camera->GetCascadeFarPlane(i);
		}

		RenderManager::Submit([renderer = shared_from_this(), viewMat, proj = camera->GetProjection(), viewPosition, viewDirection, bRenderGrid = m_bGridEnabled, options = m_EffectiveOptions,
			cascadeProjections = std::move(cameraCascadeProjections), cascadeFarPlanes = std::move(cameraCascadeFarPlanes), shadowDistance = camera->GetShadowFarClip(),
			cascadesSmoothTransitionAlpha = camera->GetCascadesSmoothTransitionAlpha(), zNear = camera->GetPerspectiveNearClip(), zFar = camera->GetPerspectiveFarClip(),
			cameraFov = camera->GetPerspectiveVerticalFOV(), bProjectionFlipped = camera->IsProjectionFlipped(),
			pickCoords, pickCoordsCount, pickSlotIndex](const Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_bProjectionFlipped = bProjectionFlipped;
			renderer->m_ZNear = zNear;
			renderer->m_ZFar = zFar;
			renderer->m_CameraFOV = cameraFov;
			if (renderer->m_Options_RT != options)
			{
				renderer->m_Options_RT = options;
				renderer->InitWithOptions();
			}
			if (renderer->m_Options_RT.InternalState.bGenerateHZB)
			{
				if (!renderer->m_HZBTask)
				{
					renderer->m_HZBTask = MakeRef<HZBTask>(*renderer.get());
				}
			}
			else
			{
				renderer->m_HZBTask.reset();
			}
			renderer->m_Options_RT.InternalState.CascadesSmoothTransitionAlpha = cascadesSmoothTransitionAlpha;

			renderer->m_Stats[renderer->m_FrameIndex] = RenderStats();

			renderer->m_PrevView = renderer->m_CameraMatrices.View;
			renderer->m_PrevProjection = renderer->m_CameraMatrices.Proj;
			renderer->m_PrevViewProjection = renderer->m_CameraMatrices.ViewProj;

			renderer->m_CameraMatrices.PrevProjUnjittered = renderer->m_CameraMatrices.ProjUnjittered;
			renderer->m_CameraMatrices.ProjUnjittered = proj;
			renderer->m_CameraMatrices.PrevViewProjUnjittered = renderer->m_CameraMatrices.ViewProjUnjittered;
			renderer->m_CameraMatrices.ViewProjUnjittered = proj * viewMat;

			if (options.InternalState.bJitter)
			{
				// The range of numbers from Halton sequence is between 0 to 1.
				// In order to use these numbers as offset for jittering,
				// we need to adjust the range so that the positions are jittered both in positive and negative directions and are not jittered more than the size
				glm::vec2 jitter = RenderManager::GetHalton();
				jitter = (2.0f * (jitter - 0.5f) / glm::vec2(renderer->m_Size));

				proj[2][0] += jitter.x;
				proj[2][1] += jitter.y;
			}

			renderer->m_CameraMatrices.View = viewMat;
			renderer->m_CameraMatrices.Proj = proj;
			renderer->m_CameraMatrices.ViewProj = renderer->m_CameraMatrices.Proj * renderer->m_CameraMatrices.View;
			renderer->m_CameraMatrices.InvViewProj = glm::inverse(renderer->m_CameraMatrices.ViewProj);
			renderer->m_CameraMatrices.PrevViewProj = renderer->m_PrevViewProjection;
			renderer->m_CameraMatrices.InvProj = glm::inverse(renderer->m_CameraMatrices.Proj);
			renderer->m_CameraMatrices.PrevProj = renderer->m_PrevProjection;
			renderer->m_CameraMatrices.PrevView = renderer->m_PrevView;
			cmd->Write(renderer->m_CameraDataBuffer, &renderer->m_CameraMatrices, sizeof(CameraData), 0, renderer->m_CameraDataBuffer->GetLayout(), BufferReadAccess::Uniform);

			renderer->m_ViewPos = viewPosition;
			renderer->m_ViewDir = viewDirection;
			renderer->m_CameraCascadeProjections = std::move(cascadeProjections);
			renderer->m_CameraCascadeFarPlanes = std::move(cascadeFarPlanes);
			renderer->m_MaxShadowDistance = shadowDistance;

			if (renderer->m_bUseDebugCullingFrustum)
			{
				renderer->m_CullingData = renderer->m_DebugCullingData;
				renderer->m_bUseDebugCullingFrustum = false;
			}
			else
			{
				const auto& size = renderer->m_Size;
				const float aspectRatio = float(size.x) / size.y;
				renderer->m_CullingData.Frustum = CalculateFrustum(zNear, zFar, cameraFov, aspectRatio);
				renderer->m_CullingData.View = renderer->m_CameraMatrices.View;
				renderer->m_CullingData.Proj = renderer->m_CameraMatrices.Proj;
				renderer->m_CullingData.InvProj = renderer->m_CameraMatrices.InvProj;
				renderer->m_CullingData.Position = viewPosition;
			}

			cmd->TransitionLayout(renderer->m_FinalImage, renderer->m_FinalImage->GetLayout(), ImageLayoutType::RenderTarget);
			{
				EG_GPU_TIMING_SCOPED(cmd, "Clearing Render Targets");
				EG_CPU_TIMING_SCOPED("Clearing Render Targets");

				cmd->ClearColorImage(renderer->m_HDRRTImage, glm::vec4(0), renderer->m_HDRRTImage->GetLayout(), renderer->m_HDRRTImage->GetLayout());
				renderer->m_GBuffer.Clear(cmd);
			}

			renderer->m_LightsManagerTask->RecordCommandBuffer(cmd);
			renderer->m_GeometryManagerTask->RecordCommandBuffer(cmd);
			renderer->m_SkinCacheTask->RecordCommandBuffer(cmd);
			renderer->m_FrustumCullingTask->RecordCommandBuffer(cmd);
			renderer->m_RenderMeshesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderSpritesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderSkeletalMeshesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderLitTextTask->RecordCommandBuffer(cmd);
			renderer->m_RenderDecalsTask->RecordCommandBuffer(cmd);
			if (renderer->m_HZBTask)
				renderer->m_HZBTask->RecordCommandBuffer(cmd);

			renderer->m_LightCullingTask->RecordCommandBuffer(cmd);
			renderer->m_ShadowPassTask->RecordCommandBuffer(cmd);
			renderer->m_ScreenSpaceShadows->RecordCommandBuffer(cmd);

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

			renderer->m_Images2DTask->RecordCommandBuffer(cmd);
			renderer->m_Text2DTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.BloomSettings.bEnable)
				renderer->m_BloomTask->RecordCommandBuffer(cmd);
			renderer->m_PostProcessingPassTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.AA == AAMethod::FXAA)
				renderer->m_FXAATask->RecordCommandBuffer(cmd);
			else if (renderer->m_Options_RT.AA == AAMethod::MSAA)
				renderer->m_MSAATask->RecordCommandBuffer(cmd);

			if (bRenderGrid)
				renderer->m_GridTask->RecordCommandBuffer(cmd);

			cmd->TransitionLayout(renderer->m_FinalImage, renderer->m_FinalImage->GetLayout(), ImageReadAccess::PixelShaderRead);
			renderer->m_GBuffer.PrepareForReading(cmd);

			// Object picking: copy only the requested pixels instead of the whole image
			if (pickCoordsCount > 0)
			{
				EG_GPU_TIMING_SCOPED(cmd, "ObjectID readback");
				EG_CPU_TIMING_SCOPED("ObjectID readback");

				auto& readback = renderer->m_PickSlots[pickSlotIndex].Readback;
				if (!readback)
				{
					BufferSpecifications specs;
					specs.Size = s_MaxPickRequests * sizeof(int32_t);
					specs.Usage = BufferUsage::TransferDst;
					specs.MemoryType = MemoryType::GpuToCpu;
					readback = Buffer::Create(specs, "ObjectID_Readback");
				}

				const auto& objectID = renderer->m_GBuffer.ObjectID;
				const glm::ivec2 maxCoord = glm::ivec2(glm::uvec2(objectID->GetSize())) - 1;

				std::array<BufferImageCopy, s_MaxPickRequests> regions;
				for (uint32_t i = 0; i < pickCoordsCount; ++i)
				{
					regions[i].BufferOffset = i * sizeof(int32_t);
					regions[i].ImageOffset = glm::ivec3(glm::clamp(pickCoords[i], glm::ivec2(0), maxCoord), 0);
					regions[i].ImageExtent = glm::uvec3(1u);
				}

				const ImageLayout objectIDLayout = objectID->GetLayout();
				cmd->TransitionLayout(objectID, objectIDLayout, ImageReadAccess::CopySource);
				cmd->TransitionLayout(readback, readback->GetLayout(), BufferLayoutType::CopyDest);
				cmd->CopyImageToBuffer(objectID, readback, std::span<const BufferImageCopy>(regions.data(), pickCoordsCount));
				cmd->TransitionLayout(readback, BufferLayoutType::CopyDest, BufferReadAccess::Host);
				cmd->TransitionLayout(objectID, ImageReadAccess::CopySource, objectIDLayout);
			}

			if (renderer->m_bIsGame)
				RenderManager::SetPresentImage(renderer->m_FinalImage);

			renderer->m_Stats_MT = renderer->m_Stats[renderer->m_FrameIndex];
			renderer->m_FrameIndex = (renderer->m_FrameIndex + 1) % RendererConfig::FramesInFlight;
		});
	}

	void SceneRenderer::ResolveObjectPicking()
	{
		auto& slot = m_PickSlots[RenderManager::GetCurrentFrameIndex_CPU()];
		if (!slot.bPending)
			return;
		slot.bPending = false;

		if (RenderManager::GetFrameNumber_CPU() - slot.SubmittedFrame > RendererConfig::FramesInFlight)
			return;

		m_MousePickResult.reset();
		m_PickResults.clear();
		if (slot.CoordsCount == 0 || !slot.Readback)
			return;

		const int32_t* ids = (const int32_t*)slot.Readback->Map();

		uint32_t i = 0;
		if (slot.bHasMouse)
			m_MousePickResult = ids[i++];

		for (; i < slot.CoordsCount; ++i)
			m_PickResults.push_back({ slot.Coords[i], ids[i] });

		slot.Readback->Unmap();
	}

	bool SceneRenderer::GetObjectIDUnderMouse(int32_t& outObjectID) const
	{
		if (!m_MousePickResult)
			return false;

		outObjectID = *m_MousePickResult;
		return true;
	}

	bool SceneRenderer::GetObjectIDAt(glm::ivec2 coord, int32_t& outObjectID)
	{
		if (m_MousePickCoord && *m_MousePickCoord == coord)
			return GetObjectIDUnderMouse(outObjectID);

		const uint64_t frame = RenderManager::GetFrameNumber_CPU();
		auto requestIt = std::find_if(m_PickRequests.begin(), m_PickRequests.end(), [coord](const PickRequest& r) { return r.Coord == coord; });
		if (requestIt != m_PickRequests.end())
			requestIt->LastRequestedFrame = frame;
		else if (m_PickRequests.size() < s_MaxPickRequests - 1) // -1 for the mouse
			m_PickRequests.push_back({ coord, frame });

		auto resultIt = std::find_if(m_PickResults.begin(), m_PickResults.end(), [coord](const PickResult& r) { return r.Coord == coord; });
		if (resultIt == m_PickResults.end())
			return false;

		outObjectID = resultIt->ObjectID;
		return true;
	}

	void SceneRenderer::SetOutputImage(const Ref<Image>& image)
	{
		RenderManager::Submit([renderer = shared_from_this(), image](const Ref<CommandBuffer>& cmd) mutable
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
		RenderManager::Submit([renderer = shared_from_this(), cubemap](const Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_Cubemap = cubemap;
		});
	}

	void SceneRenderer::SetSkybox(const SkySettings& sky)
	{
		RenderManager::Submit([renderer = shared_from_this(), sky](const Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_Sky = sky;
		});
	}

	void SceneRenderer::SetSkyboxIntensity(float intensity)
	{
		RenderManager::Submit([renderer = shared_from_this(), intensity](const Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_CubemapIntensity = glm::max(0.f, intensity);
		});
	}

	void SceneRenderer::SetUseSkyAsBackground(bool value)
	{
		RenderManager::Submit([renderer = shared_from_this(), value](const Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_bUseSkyAsBackground = value;
		});
	}

	void SceneRenderer::SetOptions(const SceneRendererSettings& options)
	{
		m_Options = options;
		RecalculateOptions();
	}

	void SceneRenderer::SetPostProcessOverride(const GUID& owner, const PostProcessOverride& postProcess)
	{
		if (m_PostProcessOverrideOwner == owner && m_PostProcessOverride == postProcess)
			return;

		m_PostProcessOverrideOwner = owner;
		m_PostProcessOverride = postProcess;
		RecalculateOptions();
	}

	void SceneRenderer::ClearPostProcessOverride(const GUID& owner)
	{
		if (m_PostProcessOverrideOwner != owner)
			return;

		ClearPostProcessOverride();
	}

	void SceneRenderer::ClearPostProcessOverride()
	{
		if (m_PostProcessOverride.IsEmpty() && m_PostProcessOverrideOwner.IsNull())
			return;

		m_PostProcessOverride.Clear();
		m_PostProcessOverrideOwner = GUID(0, 0);
		RecalculateOptions();
	}

	void SceneRenderer::RecalculateOptions()
	{
		m_EffectiveOptions = m_Options;
		m_PostProcessOverride.ApplyTo(m_EffectiveOptions);

		const bool bTAAEnabled = m_EffectiveOptions.AA == AAMethod::TAA;
		m_EffectiveOptions.InternalState.bMotionBuffer = bTAAEnabled || m_EffectiveOptions.MotionBlur.bEnable || m_EffectiveOptions.ScreenSpaceReflections.bEnable;
		m_EffectiveOptions.InternalState.bJitter = bTAAEnabled;

		m_Options.InternalState = m_EffectiveOptions.InternalState;
	}

	void SceneRenderer::SetViewportSize(const glm::uvec2 size)
	{
		if (m_Size == size)
			return;

		RenderManager::Wait();
		RenderManager::SetImmediateDeletionMode(true);

		m_Size = size;

		m_FinalImage->Resize({ m_Size, 1 });
		m_HDRRTImage->Resize({ m_Size, 1 });
		m_GBuffer.Resize({ m_Size, 1 });

		// Tasks
		m_LightCullingTask->OnResize(m_Size);
		m_FrustumCullingTask->OnResize(m_Size);
		m_SkinCacheTask->OnResize(m_Size);
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
		m_ScreenSpaceShadows->OnResize(m_Size);
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

		if (m_MSAATask)
			m_MSAATask->OnResize(m_Size);

		if (m_FXAATask)
			m_FXAATask->OnResize(m_Size);

		if (m_MotionBlurTask)
			m_MotionBlurTask->OnResize(m_Size);

		if (m_ScreenSpaceReflectionsTask)
			m_ScreenSpaceReflectionsTask->OnResize(m_Size);

		if (m_HZBTask)
			m_HZBTask->OnResize(m_Size);

		RenderManager::SetImmediateDeletionMode(false);
		RenderManager::ReleasePendingResources();
		StagingManager::ReleaseBuffers();
	}

	void SceneRenderer::SetMeshesAnimationTransforms(ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>>&& transforms)
	{
		RenderManager::Submit([renderer = shared_from_this(), transforms = std::move(transforms)](const Ref<CommandBuffer>&)
		{
			renderer->m_AnimationTransforms = std::move(transforms);
		});
	}

	void SceneRenderer::SetSkeletalParticleAnimationTransforms(ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>>&& transforms)
	{
		RenderManager::Submit([renderer = shared_from_this(), transforms = std::move(transforms)](const Ref<CommandBuffer>&)
		{
			renderer->m_SkeletalParticlesAnimationTransforms = std::move(transforms);
		});
	}

	void SceneRenderer::SetDebugFrustumCulling(const glm::vec3& cameraPos, const glm::mat4& view, const Camera& camera, float aspectRatio)
	{
		const float fovY = camera.GetPerspectiveVerticalFOV();
		const float nearPlane = camera.GetPerspectiveNearClip();
		const float farPlane = camera.GetPerspectiveFarClip();

		CullingFrustumData data{};
		data.Frustum = CalculateFrustum(nearPlane, farPlane, fovY, aspectRatio);
		data.View = view;
		data.Proj = camera.GetProjection();
		data.InvProj = glm::inverse(data.Proj);
		data.Position = cameraPos;

		RenderManager::Submit([renderer = shared_from_this(), data](const Ref<CommandBuffer>&)
		{
			renderer->m_DebugCullingData = data;
			renderer->m_bUseDebugCullingFrustum = true;
		});
	}

	void SceneRenderer::InitWithOptions()
	{
		auto& options = m_Options_RT;

		m_GBuffer.InitOptional(options.InternalState, glm::uvec3(m_Size, 1u));
		m_PhotoLinearScale = CalculatePhotoLinearScale(options.PhotoLinearTonemappingParams, options.Gamma);
		m_GeometryManagerTask->InitWithOptions(options);
		m_FrustumCullingTask->InitWithOptions(options);
		m_LightsManagerTask->InitWithOptions(options);
		m_LightCullingTask->InitWithOptions(options);
		m_SkinCacheTask->InitWithOptions(options);
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
		m_ScreenSpaceShadows->InitWithOptions(options);
		m_DOFTask->InitWithOptions(options);
		m_ParticleTask->InitWithOptions(options);

		InitOptionalTask<BloomPassTask>(m_BloomTask, options, options.BloomSettings.bEnable, *this);
		InitOptionalTask<SSAOTask>(m_SSAOTask, options, options.AO == AmbientOcclusion::SSAO, *this);
		InitOptionalTask<GTAOTask>(m_GTAOTask, options, options.AO == AmbientOcclusion::GTAO, *this);
		InitOptionalTask<TAATask>(m_TAATask, options, options.AA == AAMethod::TAA, *this);
		InitOptionalTask<MSAATask>(m_MSAATask, options, options.AA == AAMethod::MSAA, *this);
		InitOptionalTask<FXAATask>(m_FXAATask, options, options.AA == AAMethod::FXAA, *this);
		InitOptionalTask<VolumetricLightTask>(m_VolumetricTask, options, options.VolumetricSettings.bEnable, *this);
		InitOptionalTask<FogPassTask>(m_FogTask, options, options.FogSettings.bEnable, *this);
		InitOptionalTask<MotionBlurTask>(m_MotionBlurTask, options, options.MotionBlur.bEnable, *this);
		InitOptionalTask<ScreenSpaceReflectionsTask>(m_ScreenSpaceReflectionsTask, options, options.ScreenSpaceReflections.bEnable, *this);
		InitOptionalTask<HZBTask>(m_HZBTask, options, options.InternalState.bGenerateHZB, *this);
	}

	void GBuffer::Init(const glm::uvec3& size)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = size;
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc;
		Depth = Image::Create(depthSpecs, "GBuffer_Depth");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = size;
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		Albedo = Image::Create(colorSpecs, "GBuffer_Albedo");

		ImageSpecifications normalSpecs;
		normalSpecs.Format = ImageFormat::R16G16B16A16_Float;
		normalSpecs.Layout = ImageLayoutType::RenderTarget;
		normalSpecs.Size = size;
		normalSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		Normals = Image::Create(normalSpecs, "GBuffer_Geometry_Shading_Normals");

		ImageSpecifications emissiveSpecs;
		emissiveSpecs.Format = ImageFormat::R11G11B10_Float;
		emissiveSpecs.Layout = ImageLayoutType::RenderTarget;
		emissiveSpecs.Size = size;
		emissiveSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		Emissive = Image::Create(emissiveSpecs, "GBuffer_Emissive");

		ImageSpecifications materialSpecs;
		materialSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		materialSpecs.Layout = ImageLayoutType::RenderTarget;
		materialSpecs.Size = size;
		materialSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		MaterialData = Image::Create(materialSpecs, "GBuffer_MaterialData");

		ImageSpecifications flagSpecs;
		flagSpecs.Format = ImageFormat::R8_UInt;
		flagSpecs.Layout = ImageLayoutType::RenderTarget;
		flagSpecs.Size = size;
		flagSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		Flags = Image::Create(flagSpecs, "GBuffer_Flags");

		ImageSpecifications objectIDSpecs;
		objectIDSpecs.Format = ImageFormat::R32_SInt;
		objectIDSpecs.Layout = ImageLayoutType::RenderTarget;
		objectIDSpecs.Size = size;
		objectIDSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
		ObjectID = Image::Create(objectIDSpecs, "GBuffer_ObjectID");
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
				velocitySpecs.Layout = ImageLayoutType::RenderTarget;
				velocitySpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
				Motion = Image::Create(velocitySpecs, "GBuffer_Motion");
			}
		}
		else
		{
			Motion.reset();
		}

		if (optional.bGenerateHZB)
		{
			if (!HZB)
			{
				ImageSpecifications specs{};
				specs.Format = ImageFormat::R32_Float;
				specs.Size = size;
				specs.MipsCount = UINT_MAX;
				specs.Usage = ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
				specs.Layout = ImageLayoutType::StorageImage;
				HZB = Image::Create(specs, "HZB");

				const uint32_t mipsCount = HZB->GetMipsCount();
				HZBSampler = Sampler::Create(FilterMode::Point, AddressMode::ClampToOpaqueBlack, CompareOperation::Never, 0.f, float(mipsCount - 1), 1.f);
			}
		}
		else
		{
			HZB.reset();
			HZBSampler.reset();
		}
	}
	
	void GBuffer::Resize(const glm::uvec3& size)
	{
		Albedo->Resize(size);
		MaterialData->Resize(size);
		Normals->Resize(size);
		Emissive->Resize(size);
		ObjectID->Resize(size);
		Depth->Resize(size);
		Flags->Resize(size);
		if (Motion)
			Motion->Resize(size);
		if (HZB)
		{
			HZB->Resize(size);

			const uint32_t mipsCount = HZB->GetMipsCount();
			HZBSampler = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, float(mipsCount - 1), 1.f);
		}
	}
	
	void GBuffer::Clear(const Ref<CommandBuffer>& cmd)
	{
		cmd->ClearDepthStencilImage(Depth, 0, 0, Depth->GetLayout(), ImageLayoutType::DepthStencilWrite);
		cmd->ClearColorImage(ObjectID, glm::uintBitsToFloat(glm::uvec4(-1)), ObjectID->GetLayout(), ImageLayoutType::RenderTarget);
		if (Motion)
			cmd->ClearColorImage(Motion, glm::vec4(0), Motion->GetLayout(), ImageLayoutType::RenderTarget);

#ifndef EG_RELEASE
		// Note: I think there's no need to clear these buffers.
		cmd->ClearColorImage(Albedo, glm::vec4(0), Albedo->GetLayout(), ImageLayoutType::RenderTarget);
		cmd->ClearColorImage(Normals, glm::vec4(0), Normals->GetLayout(), ImageLayoutType::RenderTarget);
		cmd->ClearColorImage(Emissive, glm::vec4(0), Emissive->GetLayout(), ImageLayoutType::RenderTarget);
		cmd->ClearColorImage(MaterialData, glm::vec4(0), MaterialData->GetLayout(), ImageLayoutType::RenderTarget);
		cmd->ClearColorImage(Flags, glm::vec4(0), Flags->GetLayout(), ImageLayoutType::RenderTarget);
#endif
	}
	
	void GBuffer::PrepareForReading(const Ref<CommandBuffer>& cmd)
	{
		cmd->TransitionLayout(Depth, Depth->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(ObjectID, ObjectID->GetLayout(), ImageReadAccess::PixelShaderRead);
		if (Motion)
			cmd->TransitionLayout(Motion, Motion->GetLayout(), ImageReadAccess::PixelShaderRead);

		cmd->TransitionLayout(Albedo, Albedo->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(Normals, Normals->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(Emissive, Emissive->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(MaterialData, MaterialData->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(Flags, Flags->GetLayout(), ImageReadAccess::PixelShaderRead);
	}
}
