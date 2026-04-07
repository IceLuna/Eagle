#include "egpch.h"
#include "RenderSkeletalMeshesTask.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Animation/AnimationSystem.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	RenderSkeletalMeshesTask::RenderSkeletalMeshesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = renderer.GetOptions_RT().InternalState.bMotionBuffer;
		bJitter = renderer.GetOptions_RT().InternalState.bJitter;
		InitPipeline();
	}

	void RenderSkeletalMeshesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderSkeletalMeshesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment geometry_shading_NormalsAttachment;
		geometry_shading_NormalsAttachment.Image = gbuffer.Geometry_Shading_Normals;
		geometry_shading_NormalsAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment materialAttachment;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment flagsAttachment;
		flagsAttachment.Image = gbuffer.Flags;
		flagsAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		flagsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		flagsAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;

		ShaderDefines vertexDefines;
		ShaderDefines fragmentDefines;
		if (bMotionRequired)
		{
			vertexDefines["EG_MOTION"] = "";
			fragmentDefines["EG_MOTION"] = "";
		}
		if (bJitter)
			vertexDefines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("mesh_skeletal.vert", ShaderType::Vertex, vertexDefines);
		state.FragmentShader = Shader::Create("mesh.frag", ShaderType::Fragment, fragmentDefines);

		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometry_shading_NormalsAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(flagsAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (bMotionRequired)
		{
			ColorAttachment velocityAttachment;
			velocityAttachment.Image = gbuffer.Motion;
			velocityAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			velocityAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			velocityAttachment.ClearOperation = ClearOperation::Load;
			state.ColorAttachments.push_back(velocityAttachment);
		}

		state.PerInstanceAttribs = PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

		fragmentDefines["EG_MASKED"] = "";
		state.FragmentShader = Shader::Create("mesh.frag", ShaderType::Fragment, fragmentDefines);

		if (m_MaskedPipeline)
			m_MaskedPipeline->SetState(state);
		else
			m_MaskedPipeline = PipelineGraphics::Create(state);
	}

	void RenderSkeletalMeshesTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const std::vector<MeshDrawData>& meshes, const MeshGeometryData<SkeletalVertex>& buffers,
		RenderStats& stats, const DataBufferView& vertexPushData, const Ref<Framebuffer>& framebuffer)
	{
		// We're manually fetching VB & IVB data, so set the to null for the draw calls
		static Ref<Buffer> nullBuffer = nullptr;

		if (framebuffer)
			cmd->BeginGraphics(pipeline, framebuffer);
		else
			cmd->BeginGraphics(pipeline);

		struct PushData
		{
			uint32_t VertexCount = 0;
			uint32_t InstanceOffset = 0;
			uint32_t VerticesOffset = 0;
		};
		PushData pushData;

		constexpr size_t pushConstantsMaxSize = 128;
		EG_CORE_ASSERT((pushConstantsMaxSize - vertexPushData.Size) >= sizeof(PushData)); // We need to have enough space to hold PushData
		uint8_t pushConstants[pushConstantsMaxSize];
		if (vertexPushData.Size > 0)
		{
			memcpy_s(pushConstants, pushConstantsMaxSize, vertexPushData.Data, vertexPushData.Size);
		}

		const size_t dstOffset = vertexPushData.Size;
		for (const auto& data : meshes)
		{
			pushData.VertexCount = data.VerticesCount;
			pushData.VerticesOffset = data.SkinnedVertexOffset;

			for (const auto& matRenderData : data.PerMaterialData)
			{
				const uint32_t indicesCount = matRenderData.IndexCount;
				const uint32_t firstIndex = matRenderData.FirstIndex;
				const uint32_t instanceCount = matRenderData.InstanceCount;
				const uint32_t firstInstance = matRenderData.FirstInstance;
				if (instanceCount > 0)
				{
					pushData.InstanceOffset = firstInstance;

					memcpy_s(pushConstants + dstOffset, pushConstantsMaxSize - dstOffset, &pushData, sizeof(PushData));
					cmd->SetGraphicsRootConstants(pushConstants, nullptr);
					cmd->DrawIndexedInstanced(nullBuffer, buffers.IndexBuffer, indicesCount, firstIndex, 0, instanceCount, firstInstance, nullBuffer);
					++stats.DrawCalls;
				}
			}
		}

		cmd->EndGraphics();
	}

	void RenderSkeletalMeshesTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetSkeletalMeshesDrawData().Opaque;
		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Render Opaque Skeletal Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& vb = m_Renderer.GetSkinnedVertices();

		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_OpaquePipeline->SetBuffer(vb, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_OpaquePipeline->SetBuffer(buffers.InstanceBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 3);

		if (bMotionRequired)
		{
			m_OpaquePipeline->SetBuffer(m_Renderer.GetPrevSkinnedVerticesPositions(), EG_PERSISTENT_SET, EG_BINDING_MAX + 4);
		}
		if (bJitter)
			m_OpaquePipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		auto& stats = m_Renderer.GetStats();
		Draw(cmd, m_OpaquePipeline, meshes, buffers, stats);
	}

	void RenderSkeletalMeshesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetSkeletalMeshesDrawData().Masked;
		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Render Masked Skeletal Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& vb = m_Renderer.GetSkinnedVertices();

		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_MaskedPipeline->SetBuffer(vb, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_MaskedPipeline->SetBuffer(buffers.InstanceBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 3);

		if (bMotionRequired)
		{
			m_MaskedPipeline->SetBuffer(m_Renderer.GetPrevSkinnedVerticesPositions(), EG_PERSISTENT_SET, EG_BINDING_MAX + 4);
		}
		if (bJitter)
			m_MaskedPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		auto& stats = m_Renderer.GetStats();
		Draw(cmd, m_MaskedPipeline, meshes, buffers, stats);
	}
}
