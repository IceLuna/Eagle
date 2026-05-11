#include "egpch.h"
#include "MSAATask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"

#include "RenderTextLitTask.h"
#include "RenderSpritesTask.h"
#include "RenderMeshesTask.h"
#include "RenderSkeletalMeshesTask.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static SamplesCount MSAASamples2Samples(MSAASamples msaa)
	{
		switch (msaa)
		{
		case MSAASamples::x2: return SamplesCount::Samples2;
		case MSAASamples::x4: return SamplesCount::Samples4;
		case MSAASamples::x8: return SamplesCount::Samples8;
		default:
			EG_CORE_ASSERT(false);
			return SamplesCount::Samples1;
		}
	}

	MSAATask::MSAATask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& options = renderer.GetOptions_RT();
		m_Samples = options.MSAAParams.Samples;
		bJitter = options.InternalState.bJitter;

		m_MeshOpaqueDrawShader = Shader::Create("msaa/msaa_draw.frag", ShaderType::Fragment);
		m_MeshMaskedDrawShader = Shader::Create("msaa/msaa_draw.frag", ShaderType::Fragment, { {"EG_MASKED", "" }});

		CreateMSAATextures();
		InitPipelines();
	}
	
	void MSAATask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Deferred MSAA");
		EG_CPU_TIMING_SCOPED("Deferred MSAA");

		cmd->ClearDepthStencilImage(m_MSAADepth, 0.0, 0, m_MSAADepth->GetLayout(), ImageLayoutType::DepthStencilWrite);

		RenderSprites(cmd);
		RenderStaticMeshes(cmd);
		RenderSkeletalMeshes(cmd);

		// Resolve pass
		{
			EG_GPU_TIMING_SCOPED(cmd, "Deferred MSAA. Resolve AA");
			EG_CPU_TIMING_SCOPED("Deferred MSAA. Resolve AA");

			struct PushConstants
			{
				glm::vec3 CameraPos = glm::vec3(0);
				float EdgeThreshold = 0.25f;
				glm::ivec2 Size = glm::ivec2{0};
				float NearPlane = 0;
				float FarPlane = 0;
				uint32_t VisualizeEdges = 0;
			} pushData;

			const auto& output = m_Renderer.GetHDROutput();
			const auto& depth = m_Renderer.GetGBuffer().Depth;
			const auto& normals = m_Renderer.GetGBuffer().Normals;

			const float nearPlane = m_Renderer.GetZNear();
			const float farPlane = m_Renderer.GetZFar();
			const glm::vec2 viewportSize = output->GetSize();
			const auto& options = m_Renderer.GetOptions_RT().MSAAParams;

			pushData.CameraPos = m_Renderer.GetViewPosition();
			pushData.EdgeThreshold = options.EdgeThreshold;
			pushData.Size = viewportSize;
			pushData.VisualizeEdges = options.bVisualizeEdges ? 1u : 0u;
			pushData.NearPlane = m_Renderer.GetZNear();
			pushData.FarPlane = m_Renderer.GetZFar();

			const ImageLayout outputLayout = output->GetLayout();
			const ImageLayout depthLayout = depth->GetLayout();
			const ImageLayout normalsLayout = normals->GetLayout();

			cmd->TransitionLayout(output, outputLayout, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(depth, depthLayout, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(normals, normalsLayout, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(m_MSAADepth, ImageLayoutType::DepthStencilWrite, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(m_MSAANormals, ImageLayoutType::RenderTarget, ImageReadAccess::PixelShaderRead);

			m_ApplyMSAAPipeline->SetImage(output, 0, 0);
			m_ApplyMSAAPipeline->SetImageSampler(output, Sampler::BilinearSamplerClamp, 0, 1);
			m_ApplyMSAAPipeline->SetImageSampler(m_MSAADepth, Sampler::PointSampler, 0, 2);
			m_ApplyMSAAPipeline->SetImageSampler(m_MSAANormals, Sampler::PointSampler, 0, 3);
			m_ApplyMSAAPipeline->SetImageSampler(depth, Sampler::PointSampler, 0, 4);
			m_ApplyMSAAPipeline->SetImageSampler(normals, Sampler::PointSampler, 0, 5);
			m_ApplyMSAAPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 6);

			const glm::uvec3 groupSize = m_ApplyMSAAPipeline->GetWorkGroupSize();
			const glm::uvec2 numGroups = CalcNumGroups(viewportSize, groupSize);
			cmd->Dispatch(m_ApplyMSAAPipeline, numGroups.x, numGroups.y, 1, &pushData);

			cmd->TransitionLayout(output, ImageLayoutType::StorageImage, outputLayout);
			cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, depthLayout);
			cmd->TransitionLayout(normals, ImageReadAccess::PixelShaderRead, normalsLayout);
			cmd->TransitionLayout(m_MSAADepth, ImageReadAccess::PixelShaderRead, ImageLayoutType::DepthStencilWrite);
			cmd->TransitionLayout(m_MSAANormals, ImageReadAccess::PixelShaderRead, ImageLayoutType::RenderTarget);
		}
	}

	void MSAATask::OnResize(glm::uvec2 size)
	{
		m_MSAADepth->Resize(glm::uvec3(size, 1u));
		m_MSAANormals->Resize(glm::uvec3(size, 1u));

		m_SpritesPipeline->Resize(size.x, size.y);
		m_MaskedSpritesPipeline->Resize(size.x, size.y);

		m_StaticMeshesPipeline->Resize(size.x, size.y);
		m_MaskedStaticMeshesPipeline->Resize(size.x, size.y);

		m_SkeletalMeshesPipeline->Resize(size.x, size.y);
		m_MaskedSkeletalMeshesPipeline->Resize(size.x, size.y);
	}

	void MSAATask::RenderSprites(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Deferred MSAA. Sprites depth pass");
		EG_GPU_TIMING_SCOPED(cmd, "Deferred MSAA. Sprites depth pass");

		auto bindResources = [this](const Ref<PipelineGraphics>& pipeline, bool bTexturesDirty)
		{
			if (bTexturesDirty)
			{
				pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(m_Renderer.GetSpritesTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);
			if (bJitter)
				pipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);
		};

		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		auto& stats = m_Renderer.GetStats();
		const auto& vp = m_Renderer.GetViewProjection();
		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_SpritesTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];

		bindResources(m_SpritesPipeline, bTexturesDirty);
		bindResources(m_MaskedSpritesPipeline, bTexturesDirty);

		if (!singleSided.Opaque.IsEmpty() || !doubleSided.Opaque.IsEmpty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderSpritesTask::Draw(cmd, m_SpritesPipeline, singleSided.Opaque, glm::value_ptr(vp), stats);

			cmd->SetGraphicsCullMode(CullMode::None);
			RenderSpritesTask::Draw(cmd, m_SpritesPipeline, doubleSided.Opaque, glm::value_ptr(vp), stats);
		}

		if (!singleSided.Masked.IsEmpty() || !doubleSided.Masked.IsEmpty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderSpritesTask::Draw(cmd, m_MaskedSpritesPipeline, singleSided.Masked, glm::value_ptr(vp), stats);

			cmd->SetGraphicsCullMode(CullMode::None);
			RenderSpritesTask::Draw(cmd, m_MaskedSpritesPipeline, doubleSided.Masked, glm::value_ptr(vp), stats);
		}

		if (bTexturesDirty)
		{
			m_SpritesTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
	}

	void MSAATask::RenderStaticMeshes(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Deferred MSAA. Static Meshes depth pass");
		EG_CPU_TIMING_SCOPED("Deferred MSAA. Static Meshes depth pass");

		auto bindResources = [this](const Ref<PipelineGraphics>& pipeline, bool bTexturesDirty)
		{
			if (bTexturesDirty)
			{
				pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			}

			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(m_Renderer.GetMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

			if (bJitter)
				pipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);
		};

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& culledMeshes = m_Renderer.GetCulledStaticMeshes();
		const auto& ivb = culledMeshes.InstanceBuffer;
		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		auto& stats = m_Renderer.GetStats();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MeshesTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];

		bindResources(m_StaticMeshesPipeline, bTexturesDirty);
		bindResources(m_MaskedStaticMeshesPipeline, bTexturesDirty);

		RenderMeshesTask::DrawCulled(cmd, m_StaticMeshesPipeline, buffers, culledMeshes, MaterialBlendMode::Opaque, stats, glm::value_ptr(viewProj));
		RenderMeshesTask::DrawCulled(cmd, m_MaskedStaticMeshesPipeline, buffers, culledMeshes, MaterialBlendMode::Masked, stats, glm::value_ptr(viewProj));

		if (bTexturesDirty)
		{
			m_MeshesTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
	}

	void MSAATask::RenderSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Deferred MSAA. Skeletal Meshes depth pass");
		EG_CPU_TIMING_SCOPED("Deferred MSAA. Skeletal Meshes depth pass");

		auto bindResources = [this](const Ref<PipelineGraphics>& pipeline, const Ref<Buffer>& ivb, bool bTexturesDirty)
		{
			if (bTexturesDirty)
			{
				pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			}

			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(m_Renderer.GetSkinnedVertices(), EG_PERSISTENT_SET, EG_BINDING_MAX);
			pipeline->SetBuffer(ivb, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
			pipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
			pipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 3);
			if (bJitter)
				pipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);
		};

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& culledMeshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = culledMeshes.InstanceBuffer;
		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_SkeletalMeshesTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		auto& stats = m_Renderer.GetStats();

		bindResources(m_SkeletalMeshesPipeline, ivb, bTexturesDirty);
		bindResources(m_MaskedSkeletalMeshesPipeline, ivb, bTexturesDirty);

		RenderSkeletalMeshesTask::DrawCulled(cmd, m_SkeletalMeshesPipeline, buffers, culledMeshes, MaterialBlendMode::Opaque, stats);
		RenderSkeletalMeshesTask::DrawCulled(cmd, m_MaskedSkeletalMeshesPipeline, buffers, culledMeshes, MaterialBlendMode::Masked, stats);

		if (bTexturesDirty)
		{
			m_SkeletalMeshesTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
	}

	void MSAATask::CreateMSAATextures()
	{
		const SamplesCount samples = MSAASamples2Samples(m_Samples);
		// Depth
		{
			ImageSpecifications depthSpecs;
			depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
			depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
			depthSpecs.Size = m_MSAADepth ? m_MSAADepth->GetSize() : glm::uvec3(m_Renderer.GetViewportSize(), 1u);
			depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
			depthSpecs.SamplesCount = samples;
			m_MSAADepth = Image::Create(depthSpecs, "MSAA_Depth");
		}

		// Normals
		{
			ImageSpecifications normalSpecs;
			normalSpecs.Format = ImageFormat::R16G16_Float;
			normalSpecs.Layout = ImageLayoutType::RenderTarget;
			normalSpecs.Size = m_MSAANormals ? m_MSAANormals->GetSize() : glm::uvec3(m_Renderer.GetViewportSize(), 1u);
			normalSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
			normalSpecs.SamplesCount = samples;
			m_MSAANormals = Image::Create(normalSpecs, "MSAA_Normals");
		}
	}

	void MSAATask::InitPipelines()
	{
		InitSpritesPipeline();
		InitStaticPipeline();
		InitSkeletalPipeline();
		InitApplyPipeline();
	}

	void MSAATask::InitSpritesPipeline()
	{
		ColorAttachment normalsAttachment;
		normalsAttachment.ClearOperation = ClearOperation::Load;
		normalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.Image = m_MSAANormals;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_MSAADepth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

		ShaderDefines defines;
		defines["EG_BACKFACE_FLIP_NORMAL"] = "";
		if (bJitter)
			defines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(normalsAttachment);
		state.VertexShader = Shader::Create("msaa/msaa_sprite.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("msaa/msaa_draw.frag", ShaderType::Fragment, defines);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_SpritesPipeline)
			m_SpritesPipeline->SetState(state);
		else
			m_SpritesPipeline = PipelineGraphics::Create(state);

		defines["EG_MASKED"] = "";
		state.VertexShader = Shader::Create("msaa/msaa_sprite.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("msaa/msaa_draw.frag", ShaderType::Fragment, defines);
		state.bEnableAlphaToCoverage = true;

		if (m_MaskedSpritesPipeline)
			m_MaskedSpritesPipeline->SetState(state);
		else
			m_MaskedSpritesPipeline = PipelineGraphics::Create(state);
	}

	void MSAATask::InitStaticPipeline()
	{
		ColorAttachment normalsAttachment;
		normalsAttachment.ClearOperation = ClearOperation::Load;
		normalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.Image = m_MSAANormals;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_MSAADepth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

		ShaderDefines defines;
		if (bJitter)
			defines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(normalsAttachment);
		state.VertexShader = Shader::Create("msaa/msaa_mesh.vert", ShaderType::Vertex, defines);
		state.FragmentShader = m_MeshOpaqueDrawShader;
		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_StaticMeshesPipeline)
			m_StaticMeshesPipeline->SetState(state);
		else
			m_StaticMeshesPipeline = PipelineGraphics::Create(state);

		defines["EG_MASKED"] = "";
		state.VertexShader = Shader::Create("msaa/msaa_mesh.vert", ShaderType::Vertex, defines);
		state.FragmentShader = m_MeshMaskedDrawShader;
		state.bEnableAlphaToCoverage = true;

		if (m_MaskedStaticMeshesPipeline)
			m_MaskedStaticMeshesPipeline->SetState(state);
		else
			m_MaskedStaticMeshesPipeline = PipelineGraphics::Create(state);
	}

	void MSAATask::InitSkeletalPipeline()
	{
		ColorAttachment normalsAttachment;
		normalsAttachment.ClearOperation = ClearOperation::Load;
		normalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.Image = m_MSAANormals;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_MSAADepth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

		ShaderDefines defines;
		if (bJitter)
			defines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(normalsAttachment);
		state.VertexShader = Shader::Create("msaa/msaa_mesh_skeletal.vert", ShaderType::Vertex, defines);
		state.FragmentShader = m_MeshOpaqueDrawShader;
		state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_SkeletalMeshesPipeline)
			m_SkeletalMeshesPipeline->SetState(state);
		else
			m_SkeletalMeshesPipeline = PipelineGraphics::Create(state);

		defines["EG_MASKED"] = "";
		state.VertexShader = Shader::Create("msaa/msaa_mesh_skeletal.vert", ShaderType::Vertex, defines);
		state.FragmentShader = m_MeshMaskedDrawShader;
		state.bEnableAlphaToCoverage = true;

		if (m_MaskedSkeletalMeshesPipeline)
			m_MaskedSkeletalMeshesPipeline->SetState(state);
		else
			m_MaskedSkeletalMeshesPipeline = PipelineGraphics::Create(state);
	}
	
	void MSAATask::InitApplyPipeline()
	{
		ShaderDefines defines{};
		defines["EG_MSAA_SAMPLES"] = std::to_string(uint32_t(m_Samples));

		PipelineComputeState state{};
		state.ComputeShader = Shader::Create("msaa/msaa_resolve.comp", ShaderType::Compute, defines);
		m_ApplyMSAAPipeline = PipelineCompute::Create(state);
	}
}
