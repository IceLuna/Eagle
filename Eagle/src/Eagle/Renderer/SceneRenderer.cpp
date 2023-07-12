#include "egpch.h"
#include "SceneRenderer.h"
#include "RenderManager.h"

#include "Tasks/BloomPassTask.h" 
#include "Tasks/SkyboxPassTask.h" 
#include "Tasks/PostprocessingPassTask.h" 
#include "Tasks/GridTask.h" 
#include "Tasks/TransparencyTask.h"
#include "Tasks/RenderMeshesTask.h"
#include "Tasks/RenderSpritesTask.h"

#include "Eagle/Debug/CPUTimings.h" 
#include "Eagle/Debug/GPUTimings.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Camera/Camera.h"

namespace Eagle
{
	SceneRenderer::SceneRenderer(const glm::uvec2 size, const SceneRendererSettings& options)
		: m_Size(size), m_Options(options)
	{
		ImageSpecifications finalColorSpecs;
		finalColorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		finalColorSpecs.Layout = ImageLayoutType::RenderTarget;
		finalColorSpecs.Size = { size.x, size.y, 1 };
		finalColorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		m_FinalImage = Image::Create(finalColorSpecs, "Renderer_FinalImage");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R32G32B32A32_Float;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { size.x, size.y, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		colorSpecs.MipsCount = UINT_MAX;
		m_HDRRTImage = Image::Create(colorSpecs, "Renderer_HDR_RT");

		m_GBuffer.Init({ m_Size, 1 });
		m_GBuffer.InitOptional(m_Options.OptionalGBuffers, glm::uvec3(m_Size, 1u));
		// Create tasks
		m_RenderMeshesTask = MakeScope<RenderMeshesTask>(*this);
		m_RenderSpritesTask = MakeScope<RenderSpritesTask>(*this);
		m_LightsManagerTask = MakeScope<LightsManagerTask>(*this);
		m_GeometryManagerTask = MakeScope<GeometryManagerTask>(*this);
		m_RenderLinesTask = MakeScope<RenderLinesTask>(*this);
		m_RenderBillboardsTask = MakeScope<RenderBillboardsTask>(*this, m_HDRRTImage);
		m_RenderLitTextTask = MakeScope<RenderTextLitTask>(*this);
		m_RenderUnlitTextTask = MakeScope<RenderTextUnlitTask>(*this, m_HDRRTImage);
		m_PBRPassTask = MakeScope<PBRPassTask>(*this, m_HDRRTImage);
		m_ShadowPassTask = MakeScope<ShadowPassTask>(*this);
		m_SkyboxPassTask = MakeScope<SkyboxPassTask>(*this, m_HDRRTImage);
		m_PostProcessingPassTask = MakeScope<PostprocessingPassTask>(*this, m_HDRRTImage, m_FinalImage);
		m_GridTask = MakeScope<GridTask>(*this, m_FinalImage);
		m_TransparencyTask = MakeScope<TransparencyTask>(*this);

		InitWithOptions();
	}

	void SceneRenderer::Render(const Camera* camera, const glm::mat4& viewMat, glm::vec3 viewPosition)
	{
		EG_ASSERT(camera);

		std::vector<glm::mat4> cameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> cameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);

		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			cameraCascadeProjections[i] = camera->GetCascadeProjection(i);
			cameraCascadeFarPlanes[i] = camera->GetCascadeFarPlane(i);
		}

		RenderManager::Submit([renderer = shared_from_this(), viewMat, proj = camera->GetProjection(), viewPosition, bRenderGrid = m_bGridEnabled,
			cascadeProjections = std::move(cameraCascadeProjections), cascadeFarPlanes = std::move(cameraCascadeFarPlanes), shadowDistance = camera->GetShadowFarClip()](Ref<CommandBuffer>& cmd) mutable
		{
			renderer->m_PrevView = renderer->m_View;
			renderer->m_PrevProjection = renderer->m_Projection;
			renderer->m_PrevViewProjection = renderer->m_ViewProjection;

			renderer->m_View = viewMat;
			renderer->m_Projection = proj;
			renderer->m_ViewProjection = renderer->m_Projection * renderer->m_View;
			renderer->m_ViewPos = viewPosition;
			renderer->m_CameraCascadeProjections = std::move(cascadeProjections);
			renderer->m_CameraCascadeFarPlanes = std::move(cascadeFarPlanes);
			renderer->m_MaxShadowDistance = shadowDistance;

			renderer->m_LightsManagerTask->RecordCommandBuffer(cmd);
			renderer->m_GeometryManagerTask->RecordCommandBuffer(cmd);
			renderer->m_RenderMeshesTask->RecordCommandBuffer(cmd);
			renderer->m_RenderSpritesTask->RecordCommandBuffer(cmd);
			renderer->m_ShadowPassTask->RecordCommandBuffer(cmd);
			renderer->m_RenderLitTextTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.AO == AmbientOcclusion::SSAO)
				renderer->m_SSAOTask->RecordCommandBuffer(cmd);
			else if (renderer->m_Options_RT.AO == AmbientOcclusion::GTAO)
				renderer->m_GTAOTask->RecordCommandBuffer(cmd);

			renderer->m_PBRPassTask->RecordCommandBuffer(cmd);
			renderer->m_RenderBillboardsTask->RecordCommandBuffer(cmd);
			renderer->m_RenderUnlitTextTask->RecordCommandBuffer(cmd);
			renderer->m_SkyboxPassTask->RecordCommandBuffer(cmd);
			renderer->m_RenderLinesTask->RecordCommandBuffer(cmd);
			renderer->m_TransparencyTask->RecordCommandBuffer(cmd);

			if (renderer->m_Options_RT.BloomSettings.bEnable)
				renderer->m_BloomTask->RecordCommandBuffer(cmd);
			renderer->m_PostProcessingPassTask->RecordCommandBuffer(cmd);

			if (bRenderGrid)
				renderer->m_GridTask->RecordCommandBuffer(cmd);
		});
	}

	void SceneRenderer::SetSkybox(const Ref<TextureCube>& cubemap)
	{
		RenderManager::Submit([this, cubemap](Ref<CommandBuffer>& cmd) mutable
		{
			m_Cubemap = cubemap;
		});
	}

	void SceneRenderer::SetOptions(const SceneRendererSettings& options)
	{
		m_Options = options;
		m_Options.OptionalGBuffers.bMotion = m_Options.AO == AmbientOcclusion::GTAO;

		RenderManager::Submit([this, options = m_Options](const Ref<CommandBuffer>& cmd)
		{
			if (m_Options_RT != options)
			{
				m_Options_RT = options;
				InitWithOptions();
			}
		});
	}

	void SceneRenderer::SetViewportSize(const glm::uvec2 size)
	{
		if (m_Size == size)
			return;

		m_Size = size;
		EG_EDITOR_TRACE("Viewport was resized: {}x{}", m_Size.x, m_Size.y);

		RenderManager::Wait();

		m_FinalImage->Resize({ m_Size, 1 });
		m_HDRRTImage->Resize({ m_Size, 1 });
		m_GBuffer.Resize({ m_Size, 1 });

		// Tasks
		m_RenderMeshesTask->OnResize(m_Size);
		m_RenderSpritesTask->OnResize(m_Size);
		m_LightsManagerTask->OnResize(m_Size);
		m_RenderLinesTask->OnResize(m_Size);
		m_RenderBillboardsTask->OnResize(m_Size);
		m_RenderUnlitTextTask->OnResize(m_Size);
		m_RenderLitTextTask->OnResize(m_Size);
		m_PBRPassTask->OnResize(m_Size);
		m_ShadowPassTask->OnResize(m_Size);
		m_SkyboxPassTask->OnResize(m_Size);
		m_PostProcessingPassTask->OnResize(m_Size);
		m_GridTask->OnResize(m_Size);
		m_TransparencyTask->OnResize(m_Size);

		if (m_Options.BloomSettings.bEnable)
			m_BloomTask->OnResize(m_Size);

		if (m_Options.AO == AmbientOcclusion::SSAO)
			m_SSAOTask->OnResize(m_Size);
		else if (m_Options.AO == AmbientOcclusion::GTAO)
			m_GTAOTask->OnResize(m_Size);

		RenderManager::ReleasePendingResources();
	}
	
	template <typename TaskClass, typename Task, typename... Args>
	static void InitOptionalTask(Scope<Task>& task, const SceneRendererSettings& settings, bool bEnabled, Args&&... args)
	{
		if (task)
		{
			if (!bEnabled)
				task.reset(); // Deallocating task
			else
				task->InitWithOptions(settings);
		}
		else if (bEnabled)
			task = MakeScope<TaskClass>(std::forward<Args>(args)...);
	}

	void SceneRenderer::InitWithOptions()
	{
		auto& options = m_Options_RT;
		m_GBuffer.InitOptional(options.OptionalGBuffers, glm::uvec3(m_Size, 1u));
		m_PhotoLinearScale = CalculatePhotoLinearScale(options.PhotoLinearTonemappingParams, options.Gamma);
		m_GeometryManagerTask->InitWithOptions(options);
		m_RenderMeshesTask->InitWithOptions(options);
		m_RenderSpritesTask->InitWithOptions(options);
		m_RenderLitTextTask->InitWithOptions(options);
		m_PBRPassTask->InitWithOptions(options);
		m_RenderLinesTask->InitWithOptions(options);
		m_PostProcessingPassTask->InitWithOptions(options);
		m_TransparencyTask->InitWithOptions(options);

		InitOptionalTask<BloomPassTask>(m_BloomTask, options, options.BloomSettings.bEnable, *this, m_HDRRTImage);
		InitOptionalTask<SSAOTask>(m_SSAOTask, options, options.AO == AmbientOcclusion::SSAO, *this);
		InitOptionalTask<GTAOTask>(m_GTAOTask, options, options.AO == AmbientOcclusion::GTAO, *this);
	}

	void GBuffer::Init(const glm::uvec3& size)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = size;
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;
		Depth = Image::Create(depthSpecs, "GBuffer_Depth");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = size;
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		AlbedoRoughness = Image::Create(colorSpecs, "GBuffer_Albedo_Roughness");

		ImageSpecifications normalSpecs;
		normalSpecs.Format = ImageFormat::R16G16B16A16_Float;
		normalSpecs.Layout = ImageLayoutType::RenderTarget;
		normalSpecs.Size = size;
		normalSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		Geometry_Shading_Normals = Image::Create(normalSpecs, "GBuffer_Geometry_Shading_Normals");

		ImageSpecifications emissiveSpecs;
		emissiveSpecs.Format = ImageFormat::R16G16B16A16_Float;
		emissiveSpecs.Layout = ImageLayoutType::RenderTarget;
		emissiveSpecs.Size = size;
		emissiveSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		Emissive = Image::Create(emissiveSpecs, "GBuffer_Emissive");

		ImageSpecifications materialSpecs;
		materialSpecs.Format = ImageFormat::R8G8_UNorm;
		materialSpecs.Layout = ImageLayoutType::RenderTarget;
		materialSpecs.Size = size;
		materialSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		MaterialData = Image::Create(materialSpecs, "GBuffer_MaterialData");

		ImageSpecifications objectIDSpecs;
		objectIDSpecs.Format = ImageFormat::R32_SInt;
		objectIDSpecs.Layout = ImageLayoutType::RenderTarget;
		objectIDSpecs.Size = size;
		objectIDSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc;
		ObjectID = Image::Create(objectIDSpecs, "GBuffer_ObjectID");
	}
	
	void GBuffer::InitOptional(const OptionalGBuffers& optional, const glm::uvec3& size)
	{
		if (optional.bMotion)
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
	}
}
