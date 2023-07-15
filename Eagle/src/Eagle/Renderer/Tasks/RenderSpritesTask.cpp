#include "egpch.h"
#include "RenderSpritesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	struct PushData
	{
		glm::mat4 ViewProj;
		glm::mat4 PrevViewProj;
	};

	RenderSpritesTask::RenderSpritesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = m_Renderer.GetOptions_RT().OptionalGBuffers.bMotion;
		InitPipeline();
	}

	static void Draw(const Ref<CommandBuffer>& cmd, Ref<PipelineGraphics>& pipeline, const SpriteGeometryData& spritesData, const PushData& pushData)
	{
		if (spritesData.QuadVertices.empty())
			return;

		const uint32_t quadsCount = (uint32_t)(spritesData.QuadVertices.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndexed(spritesData.VertexBuffer, spritesData.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void RenderSpritesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& spritesData = m_Renderer.GetOpaqueSpritesData();
		const auto& notCastingShadowspritesData = m_Renderer.GetOpaqueNotCastingShadowSpriteData();

		if (spritesData.QuadVertices.empty() && notCastingShadowspritesData.QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Render Sprites");
		EG_GPU_TIMING_SCOPED(cmd, "Render Sprites");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrame = texturesChangedFrame + 1;
		}
		m_Pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_Pipeline->SetBuffer(m_Renderer.GetSpritesTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_Pipeline->SetBuffer(m_Renderer.GetSpritesPrevTransformBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}

		Draw(cmd, m_Pipeline, spritesData, pushData);
		Draw(cmd, m_Pipeline, notCastingShadowspritesData, pushData);
	}

	void RenderSpritesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = gbuffer.AlbedoRoughness;

		ColorAttachment geometry_shading_NormalsAttachment;
		geometry_shading_NormalsAttachment.ClearOperation = ClearOperation::Load;
		geometry_shading_NormalsAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.Image = gbuffer.Geometry_Shading_Normals;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.ClearOperation = ClearOperation::Load;
		emissiveAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.Image = gbuffer.Emissive;

		ColorAttachment materialAttachment;
		materialAttachment.ClearOperation = ClearOperation::Load;
		materialAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.Image = gbuffer.MaterialData;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.ClearOperation = ClearOperation::Load;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.Image = gbuffer.ObjectID;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		ShaderDefines defines;
		if (bMotionRequired)
			defines["EG_MOTION"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/sprite.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("assets/shaders/sprite.frag", ShaderType::Fragment, defines);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometry_shading_NormalsAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
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
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
