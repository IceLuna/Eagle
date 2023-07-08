#include "egpch.h"
#include "TransparencyTask.h"

#include "Eagle/Renderer/RenderManager.h"
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
		m_Layers = m_Renderer.GetOptions_RT().TransparencyLayers;
		const std::string layersString = std::to_string(m_Layers);

		m_PBRShaderDefines = m_Renderer.GetPBRShaderDefines();
		auto defines = m_PBRShaderDefines;
		defines["EG_OIT_LAYERS"] = layersString;

		m_TransparencyColorShader     = Shader::Create("assets/shaders/transparency/transparency_color.frag", ShaderType::Fragment, defines);
		m_TransparencyDepthShader     = Shader::Create("assets/shaders/transparency/transparency.frag", ShaderType::Fragment, { {"EG_DEPTH_PASS",     ""}, {"EG_OIT_LAYERS", layersString} });
		m_TransparencyCompositeShader = Shader::Create("assets/shaders/transparency/transparency.frag", ShaderType::Fragment, { {"EG_COMPOSITE_PASS", ""}, {"EG_OIT_LAYERS", layersString} });

		InitOITBuffer();
		InitMeshPipelines();
		InitSpritesPipelines();
		InitCompositePipelines();
		InitEntityIDPipelines();
	}
	
	void TransparencyTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetTranslucentMeshes();
		const auto& vertices = m_Renderer.GetTranslucentSpritesData().QuadVertices;

		if (meshes.empty() && vertices.empty())
		{
			m_OITBuffer.reset(); // Release buffers since it's not needed
			return;
		}

		if (!m_OITBuffer)
			InitOITBuffer();

		EG_GPU_TIMING_SCOPED(cmd, "Transparency");
		EG_CPU_TIMING_SCOPED("Transparency");

		{
			EG_GPU_TIMING_SCOPED(cmd, "Transparency. Clear Buffer");
			EG_CPU_TIMING_SCOPED("Transparency. Clear Buffer");

			// Fill all depth values
			size_t bytesToClear = m_OITBuffer->GetSize() / 2;
			bytesToClear += 4ull - (bytesToClear % 4ull);

			cmd->TransitionLayout(m_OITBuffer, BufferLayoutType::Unknown, BufferLayoutType::CopyDest);
			cmd->FillBuffer(m_OITBuffer, 0xFFFFFFFFu, 0, bytesToClear);
			cmd->TransitionLayout(m_OITBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);
		}

		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderMeshesDepth(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderSpritesDepth(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		// Prepare data for color passes
		{
			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
			if (bTexturesDirty)
			{
				m_SpritesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
				m_MeshesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
				m_TexturesUpdatedFrame = texturesChangedFrame + 1;
			}

			const auto& iblTexture = m_Renderer.GetSkybox();
			const bool bHasIrradiance = iblTexture.operator bool();
			const auto& ibl = bHasIrradiance ? iblTexture : RenderManager::GetDummyIBL();
			m_ColorPushData.Size = m_Renderer.GetViewportSize();
			m_ColorPushData.CameraPos = m_Renderer.GetViewPosition();
			m_ColorPushData.MaxReflectionLOD = float(ibl->GetPrefilterImage()->GetMipsCount() - 1);
			m_ColorPushData.MaxShadowDistance2 = m_Renderer.GetShadowMaxDistance() * m_Renderer.GetShadowMaxDistance();

			if (m_PBRShaderDefines != m_Renderer.GetPBRShaderDefines())
			{
				const std::string layersString = std::to_string(m_Layers);
				m_PBRShaderDefines = m_Renderer.GetPBRShaderDefines();
				auto defines = m_PBRShaderDefines;
				defines["EG_OIT_LAYERS"] = layersString;
				m_TransparencyColorShader->SetDefines(defines);
				m_TransparencyColorShader->Reload();
			}
		}

		bool bTest = vertices.empty();
		RenderMeshesColor(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		RenderSpritesColor(cmd);
		cmd->StorageBufferBarrier(m_OITBuffer);

		CompositePass(cmd);
		RenderEntityIDs(cmd);
	}

	static void ReloadShader(Ref<Shader>& shader, const std::string& layers)
	{
		auto defines = shader->GetDefines();
		defines["EG_OIT_LAYERS"] = layers;
		shader->SetDefines(defines);
		shader->Reload();
	}
	
	void TransparencyTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (m_Layers == settings.TransparencyLayers)
			return;

		m_Layers = settings.TransparencyLayers;
		const std::string layersString = std::to_string(m_Layers);

		ReloadShader(m_TransparencyColorShader, layersString);
		ReloadShader(m_TransparencyDepthShader, layersString);
		ReloadShader(m_TransparencyCompositeShader, layersString);

		const glm::uvec2 size = m_Renderer.GetViewportSize();
		constexpr size_t formatSize = GetImageFormatBPP(ImageFormat::R32_UInt) / 8u;
		const size_t bufferSize = size_t(size.x * size.y * m_Layers) * s_Stride;
		m_OITBuffer->Resize(bufferSize);
	}

	void TransparencyTask::RenderMeshesDepth(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetTranslucentMeshes();

		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Meshes. Depth");
		EG_CPU_TIMING_SCOPED("Transparency. Meshes. Depth");

		const auto& meshesData = m_Renderer.GetTranslucentMeshesData();
		const auto& vb = meshesData.VertexBuffer;
		const auto& ivb = meshesData.InstanceBuffer;
		const auto& ib = meshesData.IndexBuffer;

		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		m_MeshesDepthPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, 0);
		m_MeshesDepthPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, 1);

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

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Sprites. Depth");
		EG_CPU_TIMING_SCOPED("Transparency. Sprites. Depth");

		const auto& vb = spritesData.VertexBuffer;
		const auto& ib = spritesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		m_SpritesDepthPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, 0);
		m_SpritesDepthPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, 1);

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

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Meshes. Color");
		EG_CPU_TIMING_SCOPED("Transparency. Meshes. Color");

		const auto& meshesData = m_Renderer.GetTranslucentMeshesData();
		const auto& vb = meshesData.VertexBuffer;
		const auto& ivb = meshesData.InstanceBuffer;
		const auto& ib = meshesData.IndexBuffer;

		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const glm::mat4& viewProj = m_Renderer.GetViewProjection();

		const auto& materials = m_Renderer.GetMeshMaterialsBuffer();
		m_MeshesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MeshesColorPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_MeshesColorPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetCameraBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		
		const auto& iblTexture = m_Renderer.GetSkybox();
		const bool bHasIrradiance = iblTexture.operator bool();
		const auto& ibl = bHasIrradiance ? iblTexture : RenderManager::GetDummyIBL();
		
		const Ref<Image>& smDistribution = m_Renderer.GetSMDistribution();
		const Ref<Image>& smDistributionToUse = smDistribution.operator bool() ? smDistribution : RenderManager::GetDummyImage3D();
		
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_MeshesColorPipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 1);
		m_MeshesColorPipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 2);
		m_MeshesColorPipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 3);
		m_MeshesColorPipeline->SetImageSampler(smDistributionToUse, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 4);
		
		uint32_t binding = EG_BINDING_DIRECTIONAL_LIGHT + 5;
		m_MeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, binding);
		binding += EG_CASCADES_COUNT;
		m_MeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), EG_SCENE_SET, binding);
		binding += EG_MAX_LIGHT_SHADOW_MAPS;
		m_MeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), EG_SCENE_SET, binding);

		cmd->BeginGraphics(m_MeshesColorPipeline);
		cmd->SetGraphicsRootConstants(&viewProj[0][0], &m_ColorPushData);

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

		if (vertices.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Sprites. Color");
		EG_CPU_TIMING_SCOPED("Transparency. Sprites. Color");

		const auto& vb = spritesData.VertexBuffer;
		const auto& ib = spritesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::mat4& viewProj = m_Renderer.GetViewProjection();

		const auto& materials = m_Renderer.GetSpritesMaterialsBuffer();
		m_SpritesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_SpritesColorPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_SpritesColorPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetCameraBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);

		const auto& iblTexture = m_Renderer.GetSkybox();
		const bool bHasIrradiance = iblTexture.operator bool();
		const auto& ibl = bHasIrradiance ? iblTexture : RenderManager::GetDummyIBL();
		
		const Ref<Image>& smDistribution = m_Renderer.GetSMDistribution();
		const Ref<Image>& smDistributionToUse = smDistribution.operator bool() ? smDistribution : RenderManager::GetDummyImage3D();
		
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_SpritesColorPipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 1);
		m_SpritesColorPipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 2);
		m_SpritesColorPipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 3);
		m_SpritesColorPipeline->SetImageSampler(smDistributionToUse, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 4);
		
		uint32_t binding = EG_BINDING_DIRECTIONAL_LIGHT + 5;
		m_SpritesColorPipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, binding);
		binding += EG_CASCADES_COUNT;
		m_SpritesColorPipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), EG_SCENE_SET, binding);
		binding += EG_MAX_LIGHT_SHADOW_MAPS;
		m_SpritesColorPipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), EG_SCENE_SET, binding);

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		cmd->BeginGraphics(m_SpritesColorPipeline);
		cmd->SetGraphicsRootConstants(&viewProj[0][0], &m_ColorPushData);
		cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void TransparencyTask::CompositePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Composite");
		EG_CPU_TIMING_SCOPED("Transparency. Composite");

		m_CompositePipeline->SetBuffer(m_OITBuffer, 0, 0);

		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		cmd->StorageBufferBarrier(m_OITBuffer);

		cmd->BeginGraphics(m_CompositePipeline);
		cmd->SetGraphicsRootConstants(nullptr, &viewportSize);
		cmd->Draw(3, 0);
		cmd->EndGraphics();
	}

	void TransparencyTask::RenderEntityIDs(const Ref<CommandBuffer>& cmd)
	{
		const glm::mat4& viewProj = m_Renderer.GetViewProjection();

		// Meshes
		{
			auto& meshes = m_Renderer.GetTranslucentMeshes();
			if (!meshes.empty())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Transparency. Meshes Entity IDs");
				EG_CPU_TIMING_SCOPED("Transparency. Meshes Entity IDs");

				const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
				m_MeshesEntityIDPipeline->SetBuffer(transformsBuffer, 0, 0);

				const auto& meshesData = m_Renderer.GetTranslucentMeshesData();
				const auto& vb = meshesData.VertexBuffer;
				const auto& ivb = meshesData.InstanceBuffer;
				const auto& ib = meshesData.IndexBuffer;

				cmd->BeginGraphics(m_MeshesEntityIDPipeline);
				cmd->SetGraphicsRootConstants(&viewProj[0][0], nullptr);

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
		}

		// Sprites
		{
			const auto& spritesData = m_Renderer.GetTranslucentSpritesData();
			const auto& vertices = spritesData.QuadVertices;

			if (!vertices.empty())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Transparency. Sprites Entity IDs");
				EG_CPU_TIMING_SCOPED("Transparency. Sprites Entity IDs");

				const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();
				m_SpritesEntityIDPipeline->SetBuffer(transformsBuffer, 0, 0);

				const auto& vb = spritesData.VertexBuffer;
				const auto& ib = spritesData.IndexBuffer;
				const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
				cmd->BeginGraphics(m_SpritesEntityIDPipeline);
				cmd->SetGraphicsRootConstants(&viewProj[0][0], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
		}
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
		state.VertexShader = Shader::Create("assets/shaders/transparency/mesh_transparency_color.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;
		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

		m_MeshesColorPipeline = PipelineGraphics::Create(state);

		state.VertexShader = Shader::Create("assets/shaders/transparency/mesh_transparency_depth.vert", ShaderType::Vertex);
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
		state.VertexShader = Shader::Create("assets/shaders/transparency/sprite_transparency_color.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_SpritesColorPipeline = PipelineGraphics::Create(state);

		state.VertexShader = Shader::Create("assets/shaders/transparency/sprite_transparency_depth.vert", ShaderType::Vertex);
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
	
	void TransparencyTask::InitEntityIDPipelines()
	{
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = m_Renderer.GetGBuffer().ObjectID;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/transparency/sprite_transparency_entityID.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/transparency/transparency_entityID.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_SpritesEntityIDPipeline = PipelineGraphics::Create(state);

		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/transparency/mesh_transparency_entityID.vert", ShaderType::Vertex);
		m_MeshesEntityIDPipeline = PipelineGraphics::Create(state);
	}
	
	void TransparencyTask::InitOITBuffer()
	{
		const glm::uvec2 size = m_Renderer.GetViewportSize();
		BufferSpecifications specs;
		specs.Format = ImageFormat::R32_UInt;
		specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::TransferDst;
		specs.Size = size_t(size.x * size.y * m_Layers) * s_Stride;
		m_OITBuffer = Buffer::Create(specs, "OIT_Buffer");
	}
}
