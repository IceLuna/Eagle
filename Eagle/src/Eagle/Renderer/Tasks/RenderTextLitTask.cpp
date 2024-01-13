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
		bMotionRequired = m_Renderer.GetOptions_RT().InternalState.bMotionBuffer;
		bJitter = m_Renderer.GetOptions_RT().InternalState.bJitter;
		InitPipeline();
	}

	static void Draw(const Ref<CommandBuffer>& cmd, Ref<PipelineGraphics>& pipeline, const LitTextGeometryData& data, const PushData& pushData, SceneRenderer::Statistics2D& stats)
	{
		if (data.QuadVertices.empty())
			return;

		const uint32_t quadsCount = (uint32_t)(data.QuadVertices.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndexed(data.VertexBuffer, data.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
		++stats.DrawCalls;
		stats.QuadCount += quadsCount;
	}

	void RenderTextLitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderTextLitTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		const auto& data = m_Renderer.GetOpaqueLitTextData();
		const auto& notCastingShadowsData = m_Renderer.GetOpaqueLitNotCastingShadowTextData();

		if (data.QuadVertices.empty() && notCastingShadowsData.QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Render Opaque Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Text3D Lit");

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		m_OpaquePipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_OpaquePipeline->SetBuffer(m_Renderer.GetTextsPrevTransformBuffer(), 0, 1);
		}
		m_OpaquePipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		Draw(cmd, m_OpaquePipeline, data, pushData, m_Renderer.GetStats2D());
		Draw(cmd, m_OpaquePipeline, notCastingShadowsData, pushData, m_Renderer.GetStats2D());
	}

	void RenderTextLitTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		const auto& data = m_Renderer.GetMaskedLitTextData();
		const auto& notCastingShadowsData = m_Renderer.GetMaskedLitNotCastingShadowTextData();

		if (data.QuadVertices.empty() && notCastingShadowsData.QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Render Masked Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Text3D Lit");

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		m_MaskedPipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_MaskedPipeline->SetBuffer(m_Renderer.GetTextsPrevTransformBuffer(), 0, 1);
		}
		m_MaskedPipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		Draw(cmd, m_MaskedPipeline, data, pushData, m_Renderer.GetStats2D());
		Draw(cmd, m_MaskedPipeline, notCastingShadowsData, pushData, m_Renderer.GetStats2D());
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
		state.VertexShader = Shader::Create(Application::GetCorePath() / "assets/shaders/text_lit.vert", ShaderType::Vertex, vertexDefines);
		state.FragmentShader = Shader::Create(Application::GetCorePath() / "assets/shaders/text_lit.frag", ShaderType::Fragment, fragmentDefines);
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
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

		fragmentDefines["EG_MASKED"] = "";
		state.FragmentShader = Shader::Create(Application::GetCorePath() / "assets/shaders/text_lit.frag", ShaderType::Fragment, fragmentDefines);
		if (m_MaskedPipeline)
			m_MaskedPipeline->SetState(state);
		else
			m_MaskedPipeline = PipelineGraphics::Create(state);
	}
}
