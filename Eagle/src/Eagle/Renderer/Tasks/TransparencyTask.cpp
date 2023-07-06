#include "egpch.h"
#include "TransparencyTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/Tasks/RenderMeshesTask.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	TransparencyTask::TransparencyTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const std::string layersString = std::to_string(m_Layers);
		m_TransparencyColorShader     = Shader::Create("assets/shaders/transparency.frag", ShaderType::Fragment, { {"EG_COLOR_PASS",     ""}, {"EG_OIT_LAYERS", layersString} });
		m_TransparencyDepthShader     = Shader::Create("assets/shaders/transparency.frag", ShaderType::Fragment, { {"EG_DEPTH_PASS",     ""}, {"EG_OIT_LAYERS", layersString} });
		m_TransparencyCompositeShader = Shader::Create("assets/shaders/transparency.frag", ShaderType::Fragment, { {"EG_COMPOSITE_PASS", ""}, {"EG_OIT_LAYERS", layersString} });

		const glm::uvec2 size = m_Renderer.GetViewportSize();
		constexpr size_t formatSize = GetImageFormatBPP(ImageFormat::R32_UInt) / 8u;
		BufferSpecifications specs;
		specs.Format = ImageFormat::R32_UInt;
		specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::TransferDst;
		specs.Size = size_t(size.x * size.y * m_Layers) * 2ull * formatSize;
		m_OITBuffer = Buffer::Create(specs, "OIT_Buffer");

		InitMeshPipelines();
		InitSpritesPipelines();
		InitCompositePipelines();
	}
	
	void TransparencyTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetTranslucentMeshes();
		const auto& vertices = m_Renderer.GetTranslucentSpritesData().QuadVertices;
		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);

		if (meshes.empty() && quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency");
		EG_CPU_TIMING_SCOPED("Transparency");

		{
			EG_GPU_TIMING_SCOPED(cmd, "Transparency. Clear Buffer");
			EG_CPU_TIMING_SCOPED("Transparency. Clear Buffer");

			cmd->TransitionLayout(m_OITBuffer, BufferLayoutType::Unknown, BufferLayoutType::CopyDest);
			cmd->FillBuffer(m_OITBuffer, 0xFFFFFFFFu, 0, m_OITBuffer->GetSize() / 2); // Fill all depth values
			cmd->TransitionLayout(m_OITBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);
		}

		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderMeshesDepth(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderSpritesDepth(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderMeshesColor(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderSpritesColor(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		CompositePass(cmd);
	}
	
	void TransparencyTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		return;

		uint32_t layers = 2;
		if (m_Layers == layers)
			return;

		m_Layers = layers;
		const glm::uvec2 size = m_Renderer.GetViewportSize();
		constexpr size_t formatSize = GetImageFormatBPP(ImageFormat::R32_UInt) / 8u;
		const size_t bufferSize = size_t(size.x * size.y * m_Layers) * 2ull * formatSize;
		m_OITBuffer->Resize(bufferSize);
	}

	void TransparencyTask::RenderMeshesDepth(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetTranslucentMeshes();

		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency_Meshes_Depth");
		EG_CPU_TIMING_SCOPED("Transparency_Meshes_Depth");

		const auto& meshesData = m_Renderer.GetTranslucentMeshesData();
		const auto& vb = meshesData.VertexBuffer;
		const auto& ivb = meshesData.InstanceBuffer;
		const auto& ib = meshesData.IndexBuffer;

		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MeshesTexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_MeshesDepthPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_MeshesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_MeshesTexturesUpdatedFrame = texturesChangedFrame + 1;
		}

		m_MeshesDepthPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, 1);
		m_MeshesDepthPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);

		cmd->BeginGraphics(m_MeshesDepthPipeline);
		cmd->SetGraphicsRootConstants(&viewProj[0][0], &viewportSize);

		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
			const uint32_t instanceCount = (uint32_t)datas.size();

			cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);

			firstIndex += indicesCount;
			vertexOffset += verticesCount;
			firstInstance += instanceCount;
		}
		cmd->EndGraphics();
	}

	void TransparencyTask::RenderSpritesDepth(const Ref<CommandBuffer>& cmd)
	{
		const auto& spritesData = m_Renderer.GetTranslucentSpritesData();
		const auto& vertices = spritesData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency_Sprites_Depth");
		EG_CPU_TIMING_SCOPED("Transparency_Sprites_Depth");

		const auto& vb = spritesData.VertexBuffer;
		const auto& ib = spritesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_SpritesTexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_SpritesDepthPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_SpritesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_SpritesTexturesUpdatedFrame = texturesChangedFrame + 1;
		}

		m_SpritesDepthPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, 1);
		m_SpritesDepthPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);

		cmd->BeginGraphics(m_SpritesDepthPipeline);
		cmd->SetGraphicsRootConstants(&viewProj[0][0], &viewportSize);
		cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}
	
	void TransparencyTask::RenderMeshesColor(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetTranslucentMeshes();

		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency_Meshes_Color");
		EG_CPU_TIMING_SCOPED("Transparency_Meshes_Color");

		const auto& meshesData = m_Renderer.GetTranslucentMeshesData();
		const auto& vb = meshesData.VertexBuffer;
		const auto& ivb = meshesData.InstanceBuffer;
		const auto& ib = meshesData.IndexBuffer;

		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		const auto& materials = m_Renderer.GetMeshMaterialsBuffer();
		m_MeshesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MeshesColorPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_MeshesColorPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);

		cmd->BeginGraphics(m_MeshesColorPipeline);
		cmd->SetGraphicsRootConstants(&viewProj[0][0], &viewportSize);

		uint32_t firstIndex = 0;
		uint32_t firstInstance = 0;
		uint32_t vertexOffset = 0;
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
			const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
			const uint32_t instanceCount = (uint32_t)datas.size();

			cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);

			firstIndex += indicesCount;
			vertexOffset += verticesCount;
			firstInstance += instanceCount;
		}
		cmd->EndGraphics();
	}

	void TransparencyTask::RenderSpritesColor(const Ref<CommandBuffer>& cmd)
	{
		const auto& spritesData = m_Renderer.GetTranslucentSpritesData();
		const auto& vertices = spritesData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency_Sprites_Color");
		EG_CPU_TIMING_SCOPED("Transparency_Sprites_Color");

		const auto& vb = spritesData.VertexBuffer;
		const auto& ib = spritesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		const auto& materials = m_Renderer.GetSpritesMaterialsBuffer();
		m_SpritesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_SpritesColorPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_SpritesColorPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);

		cmd->BeginGraphics(m_SpritesColorPipeline);
		cmd->SetGraphicsRootConstants(&viewProj[0][0], &viewportSize);
		cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void TransparencyTask::CompositePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Transparency_Composite");
		EG_CPU_TIMING_SCOPED("Transparency_Composite");

		m_CompositePipeline->SetBuffer(m_OITBuffer, 0, 0);

		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		cmd->StorageBufferBarrier(m_OITBuffer);

		cmd->BeginGraphics(m_CompositePipeline);
		cmd->SetGraphicsRootConstants(nullptr, &viewportSize);
		cmd->Draw(3, 0);
		cmd->EndGraphics();
	}

	void TransparencyTask::InitMeshPipelines()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetHDROutput();
		attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		attachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		attachment.ClearOperation = ClearOperation::Load;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::One;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/mesh_transparency.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::None; // TODO: Think
		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

		m_MeshesColorPipeline = PipelineGraphics::Create(state);

		state.FragmentShader = m_TransparencyDepthShader;
		m_MeshesDepthPipeline = PipelineGraphics::Create(state);
	}

	void TransparencyTask::InitSpritesPipelines()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetHDROutput();
		attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		attachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		attachment.ClearOperation = ClearOperation::Load;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::One;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/sprite_transparency.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_SpritesColorPipeline = PipelineGraphics::Create(state);

		state.FragmentShader = m_TransparencyDepthShader;
		m_SpritesDepthPipeline = PipelineGraphics::Create(state);
	}
	
	void TransparencyTask::InitCompositePipelines()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetHDROutput();
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::One;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/quad_tri.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyCompositeShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::None;

		m_CompositePipeline = PipelineGraphics::Create(state);
	}
}
