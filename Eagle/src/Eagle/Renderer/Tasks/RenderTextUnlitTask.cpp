#include "egpch.h"
#include "RenderTextUnlitTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

#include <codecvt>

namespace Eagle
{
	RenderTextUnlitTask::RenderTextUnlitTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = m_Renderer.GetOptions_RT().InternalState.bMotionBuffer;
		InitPipeline();
	}

	void RenderTextUnlitTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (bMotionRequired == settings.InternalState.bMotionBuffer)
			return;

		bMotionRequired = settings.InternalState.bMotionBuffer;
		InitPipeline();
	}

	void RenderTextUnlitTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const QuadsRenderData<UnlitTextGeometryData>::BlendModeGeomType& data, const void* pushData, RenderStats& stats)
	{
		if (data.IsEmpty())
			return;

		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(pushData, nullptr);

		uint32_t quadsCount = (uint32_t)(data.ShadowCastingQuads.QuadVertices.size() / 4);
		if (quadsCount > 0)
		{
			cmd->DrawIndexed(data.ShadowCastingQuads.VertexBuffer, data.ShadowCastingQuads.IndexBuffer, quadsCount * 6, 0, 0);
			++stats.DrawCalls;
		}
		quadsCount = (uint32_t)(data.NonShadowQuads.QuadVertices.size() / 4);
		if (quadsCount > 0)
		{
			cmd->DrawIndexed(data.NonShadowQuads.VertexBuffer, data.NonShadowQuads.IndexBuffer, quadsCount * 6, 0, 0);
			++stats.DrawCalls;
		}

		cmd->EndGraphics();
	}

	void RenderTextUnlitTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const UnlitTextGeometryData& data, const void* pushData, RenderStats& stats, const Ref<Framebuffer>& fb)
	{
		const uint32_t quadsCount = (uint32_t)(data.QuadVertices.size() / 4);
		if (quadsCount == 0)
			return;

		if (fb)
			cmd->BeginGraphics(pipeline, fb);
		else
			cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(pushData, nullptr);
		cmd->DrawIndexed(data.VertexBuffer, data.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
		++stats.DrawCalls;
	}

	void RenderTextUnlitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedUnlitTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedUnlitTextsRenderData();

		const bool bHasOpaque = !singleSided.Opaque.IsEmpty() || !doubleSided.Opaque.IsEmpty();
		const bool bHasTranslucent = !singleSided.Translucent.IsEmpty() || !doubleSided.Translucent.IsEmpty();
		if (!bHasOpaque && !bHasTranslucent)
			return;

		EG_CPU_TIMING_SCOPED("Render Text3D Unlit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Text3D Unlit");

		const auto& vp = m_Renderer.GetViewProjection();
		auto& stats = m_Renderer.GetStats();

		if (bHasOpaque)
		{
			m_Pipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
			if (bMotionRequired)
			{
				m_Pipeline->SetBuffer(m_Renderer.GetTextsPrevTransformBuffer(), 0, 1);
				m_Pipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 2);
			}
			m_Pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			cmd->SetGraphicsCullMode(CullMode::Back);
			Draw(cmd, m_Pipeline, singleSided.Opaque, glm::value_ptr(vp), stats);

			cmd->SetGraphicsCullMode(CullMode::None);
			Draw(cmd, m_Pipeline, doubleSided.Opaque, glm::value_ptr(vp), stats);
		}

		if (bHasTranslucent)
		{
			m_TranslucentPipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
			m_TranslucentPipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			cmd->SetGraphicsCullMode(CullMode::Back);
			Draw(cmd, m_TranslucentPipeline, singleSided.Translucent, glm::value_ptr(vp), stats);

			cmd->SetGraphicsCullMode(CullMode::None);
			Draw(cmd, m_TranslucentPipeline, doubleSided.Translucent, glm::value_ptr(vp), stats);
		}
	}

	void RenderTextUnlitTask::OnResize(glm::uvec2 size)
	{
		m_Pipeline->Resize(size.x, size.y);
		m_TranslucentPipeline->Resize(size.x, size.y);
	}

	void RenderTextUnlitTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetHDROutput();
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = m_Renderer.GetGBuffer().ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;
		depthAttachment.ClearOperation = ClearOperation::Load;

		ShaderDefines defines;
		if (bMotionRequired)
			defines["EG_MOTION"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("text/text.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("text/text.frag", ShaderType::Fragment, defines);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (bMotionRequired)
		{
			ColorAttachment velocityAttachment;
			velocityAttachment.Image = m_Renderer.GetGBuffer().Motion;
			velocityAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			velocityAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			velocityAttachment.ClearOperation = ClearOperation::Load;
			state.ColorAttachments.push_back(velocityAttachment);
		}
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);

		ShaderDefines translucentDefines;
		translucentDefines["EG_TRANSLUCENT"] = "";
		state.VertexShader = Shader::Create("text/text.vert", ShaderType::Vertex, translucentDefines);
		state.FragmentShader = Shader::Create("text/text.frag", ShaderType::Fragment, translucentDefines);
		state.ColorAttachments.resize(2); // Color & object ID
		state.DepthStencilAttachment.bWriteDepth = false;

		if (m_TranslucentPipeline)
			m_TranslucentPipeline->SetState(state);
		else
			m_TranslucentPipeline = PipelineGraphics::Create(state);
	}
}
