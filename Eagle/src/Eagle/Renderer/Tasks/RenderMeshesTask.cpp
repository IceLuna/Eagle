#include "egpch.h"
#include "RenderMeshesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	RenderMeshesTask::RenderMeshesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = renderer.GetOptions_RT().InternalState.bMotionBuffer;
		bJitter = renderer.GetOptions_RT().InternalState.bJitter;
		InitPipeline();
	}

	void RenderMeshesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& opaqueMeshes = m_Renderer.GetOpaqueMeshes();
		const auto& maskedMeshes = m_Renderer.GetMaskedMeshes();
		if (opaqueMeshes.empty())
		{
			// Just to clear images & transition layouts
			cmd->BeginGraphics(m_OpaquePipeline);
			cmd->EndGraphics();
		}
		else
			RenderOpaque(cmd);
		
		if (!maskedMeshes.empty())
			RenderMasked(cmd);
	}

	void RenderMeshesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment geometry_shading_NormalsAttachment;
		geometry_shading_NormalsAttachment.Image = gbuffer.Geometry_Shading_Normals;
		geometry_shading_NormalsAttachment.InitialLayout = ImageLayoutType::Unknown;
		geometry_shading_NormalsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.InitialLayout = ImageLayoutType::Unknown;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment materialAttachment;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.InitialLayout = ImageLayoutType::Unknown;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment flagsAttachment;
		flagsAttachment.Image = gbuffer.Flags;
		flagsAttachment.InitialLayout = ImageLayoutType::Unknown;
		flagsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		flagsAttachment.ClearOperation = ClearOperation::Clear;

		constexpr int objectIDClearColorUint = -1;
		const float objectIDClearColor = *(float*)(&objectIDClearColorUint);
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::Unknown;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Clear;
		objectIDAttachment.ClearColor = glm::vec4{ objectIDClearColor };

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Clear;
		depthAttachment.DepthClearValue = 0.f;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

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
		state.VertexShader = Shader::Create("mesh.vert", ShaderType::Vertex, vertexDefines);
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
			velocityAttachment.InitialLayout = ImageLayoutType::Unknown;
			velocityAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			velocityAttachment.ClearOperation = ClearOperation::Clear;
			state.ColorAttachments.push_back(velocityAttachment);
		}

		state.PerInstanceAttribs = PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

		// Attachments of masked pipeline must be loaded
		for (auto& attachment : state.ColorAttachments)
		{
			attachment.ClearOperation = ClearOperation::Load;
			attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		}
		state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
		state.DepthStencilAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;

		fragmentDefines["EG_MASKED"] = "";
		state.FragmentShader = Shader::Create("mesh.frag", ShaderType::Fragment, fragmentDefines);

		if (m_MaskedPipeline)
			m_MaskedPipeline->SetState(state);
		else
			m_MaskedPipeline = PipelineGraphics::Create(state);
	}
	
	void RenderMeshesTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Meshes");
		EG_CPU_TIMING_SCOPED("Render Opaque Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_OpaquePipeline->SetBuffer(m_Renderer.GetMeshPrevTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		if (bJitter)
			m_OpaquePipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		cmd->BeginGraphics(m_OpaquePipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);

		auto& stats = m_Renderer.GetStats();
		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		const auto& meshes = m_Renderer.GetOpaqueMeshes();
		const auto& meshesData = m_Renderer.GetOpaqueMeshesData();
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t instanceCount = (uint32_t)datas.Instances.size();

			stats.Vertices += verticesCount;

			const auto& slotsToRender = datas.MaterialSlots;
			for (const uint32_t matSlot : slotsToRender)
			{
				const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndices(matSlot).size();
				const uint32_t indicesOffset = (uint32_t)meshKey.Mesh->GetIndicesOffset(matSlot);
				cmd->DrawIndexedInstanced(meshesData.VertexBuffer, meshesData.IndexBuffer, indicesCount, firstIndex + indicesOffset, vertexOffset, instanceCount, firstInstance + instanceCount * matSlot, meshesData.InstanceBuffer);

				stats.Indeces += indicesCount;
				++stats.DrawCalls;
			}
			firstInstance += instanceCount * meshKey.Mesh->GetMaterialSlotsCount();
			firstIndex += (uint32_t)meshKey.Mesh->GetTotalIndicesCount();

			vertexOffset += verticesCount;
		}

		cmd->EndGraphics();
	}

	void RenderMeshesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Meshes");
		EG_CPU_TIMING_SCOPED("Render Masked Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_MaskedPipeline->SetBuffer(m_Renderer.GetMeshPrevTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		if (bJitter)
			m_MaskedPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		cmd->BeginGraphics(m_MaskedPipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);

		auto& stats = m_Renderer.GetStats();
		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		const auto& meshes = m_Renderer.GetMaskedMeshes();
		const auto& meshesData = m_Renderer.GetMaskedMeshesData();
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t instanceCount = (uint32_t)datas.Instances.size();

			stats.Vertices += verticesCount;

			const auto& slotsToRender = datas.MaterialSlots;
			for (const uint32_t matSlot : slotsToRender)
			{
				const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndices(matSlot).size();
				const uint32_t indicesOffset = (uint32_t)meshKey.Mesh->GetIndicesOffset(matSlot);
				cmd->DrawIndexedInstanced(meshesData.VertexBuffer, meshesData.IndexBuffer, indicesCount, firstIndex + indicesOffset, vertexOffset, instanceCount, firstInstance + instanceCount * matSlot, meshesData.InstanceBuffer);

				stats.Indeces += indicesCount;
				++stats.DrawCalls;
			}
			firstInstance += instanceCount * meshKey.Mesh->GetMaterialSlotsCount();
			firstIndex += (uint32_t)meshKey.Mesh->GetTotalIndicesCount();

			vertexOffset += verticesCount;
		}

		cmd->EndGraphics();
	}
}
