#include "egpch.h"
#include "SkyboxPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	SkyboxPassTask::SkyboxPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
		: RendererTask(renderer)
		, m_FinalImage(renderTo)
	{
		InitPipeline();
	}

	void SkyboxPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& skybox = m_Renderer.GetSkybox();
		if (!skybox)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Skybox Pass");
		EG_CPU_TIMING_SCOPED("Skybox Pass");

		const glm::mat4 ViewProj = m_Renderer.GetProjectionMatrix() * glm::mat4(glm::mat3(m_Renderer.GetViewMatrix()));

		m_Pipeline->SetImageSampler(skybox->GetImage(), Sampler::BilinearSampler, 0, 0);
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&ViewProj[0][0], nullptr);
		cmd->Draw(36, 0);
		cmd->EndGraphics();
	}

	void SkyboxPassTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = m_FinalImage;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::LessEqual;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/skybox.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/skybox.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}