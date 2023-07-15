#include "egpch.h"
#include "RenderTextLitTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

namespace Eagle
{
	struct PushData
	{
		glm::mat4 ViewProj;
		glm::mat4 PrevViewProj;
	};

	RenderTextLitTask::RenderTextLitTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();
	}

	static void Draw(const Ref<CommandBuffer>& cmd, Ref<PipelineGraphics>& pipeline, const LitTextGeometryData& data, const PushData& pushData)
	{
		if (data.QuadVertices.empty())
			return;

		const uint32_t quadsCount = (uint32_t)(data.QuadVertices.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndexed(data.VertexBuffer, data.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void RenderTextLitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& data = m_Renderer.GetOpaqueLitTextData();
		const auto& notCastingShadowsData = m_Renderer.GetOpaqueLitNotCastingShadowTextData();

		if (data.QuadVertices.empty() && notCastingShadowsData.QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Render Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Text3D Lit");

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		m_Pipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_Pipeline->SetBuffer(m_Renderer.GetTextsPrevTransformBuffer(), 0, 1);
		}
		m_Pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		Draw(cmd, m_Pipeline, data, pushData);
		Draw(cmd, m_Pipeline, notCastingShadowsData, pushData);
	}

	void RenderTextLitTask::InitPipeline()
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
		state.VertexShader = Shader::Create("assets/shaders/text_lit.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("assets/shaders/text_lit.frag", ShaderType::Fragment, defines);
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
