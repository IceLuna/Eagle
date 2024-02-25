#include "egpch.h"
#include "RenderSkeletalMeshesTask.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/AnimationSystem.h"

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
		auto& meshes = m_Renderer.GetAllSkeletalMeshes();
		if (meshes.empty())
			return;

		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderSkeletalMeshesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.Image = gbuffer.AlbedoRoughness;
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
		depthAttachment.DepthCompareOp = CompareOperation::Less;

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

	void RenderSkeletalMeshesTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Render Opaque Skeletal Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_OpaquePipeline->SetBufferArray(m_Renderer.GetAnimationTransformsBuffers(), 3, 0);

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_OpaquePipeline->SetBuffer(m_Renderer.GetSkeletalMeshPrevTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
			m_OpaquePipeline->SetBufferArray(m_Renderer.GetAnimationPrevTransformsBuffers(), 4, 0);
		}
		if (bJitter)
			m_OpaquePipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		cmd->BeginGraphics(m_OpaquePipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);

		auto& stats = m_Renderer.GetStats();
		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		const auto& meshes = m_Renderer.GetOpaqueSkeletalMeshes();
		const auto& meshesData = m_Renderer.GetOpaqueSkeletalMeshesData();
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndices().size();
			const uint32_t instanceCount = (uint32_t)datas.size();

			stats.Indeces += indicesCount;
			stats.Vertices += verticesCount;
			++stats.DrawCalls;

			cmd->DrawIndexedInstanced(meshesData.VertexBuffer, meshesData.IndexBuffer, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, meshesData.InstanceBuffer);

			firstIndex += indicesCount;
			vertexOffset += verticesCount;
			firstInstance += instanceCount;
		}

		cmd->EndGraphics();
	}

	void RenderSkeletalMeshesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Render Masked Skeletal Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_MaskedPipeline->SetBufferArray(m_Renderer.GetAnimationTransformsBuffers(), 3, 0);

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_MaskedPipeline->SetBuffer(m_Renderer.GetSkeletalMeshPrevTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
			m_MaskedPipeline->SetBufferArray(m_Renderer.GetAnimationPrevTransformsBuffers(), 4, 0);
		}
		if (bJitter)
			m_MaskedPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		cmd->BeginGraphics(m_MaskedPipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);

		auto& stats = m_Renderer.GetStats();
		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		const auto& meshes = m_Renderer.GetMaskedSkeletalMeshes();
		const auto& meshesData = m_Renderer.GetMaskedSkeletalMeshesData();
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndices().size();
			const uint32_t instanceCount = (uint32_t)datas.size();

			stats.Indeces += indicesCount;
			stats.Vertices += verticesCount;
			++stats.DrawCalls;

			cmd->DrawIndexedInstanced(meshesData.VertexBuffer, meshesData.IndexBuffer, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, meshesData.InstanceBuffer);

			firstIndex += indicesCount;
			vertexOffset += verticesCount;
			firstInstance += instanceCount;
		}

		cmd->EndGraphics();
	}
}
