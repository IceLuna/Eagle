#include "egpch.h"
#include "RenderImages2DTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static constexpr glm::vec4 s_QuadVertexPosition[4] = {
		{ 0.f, 0.f, 0.0f, 1.0f },
		{ 1.f, 0.f, 0.0f, 1.0f },
		{ 1.f, 1.f, 0.0f, 1.0f },
		{ 0.f, 1.f, 0.0f, 1.0f }
	};

	static void UploadIndexBuffer(const Ref<CommandBuffer>& cmd, Ref<Buffer>& buffer)
	{
		const size_t& ibSize = buffer->GetSize();
		uint32_t offset = 0;
		std::vector<Index> indices(ibSize / sizeof(Index));
		for (size_t i = 0; i < indices.size(); i += 6)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
		}

		cmd->Write(buffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(buffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}

	RenderImages2DTask::RenderImages2DTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_BaseVertexBufferSize;
		vertexSpecs.Layout = BufferReadAccess::Vertex;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = s_BaseIndexBufferSize;
		indexSpecs.Layout = BufferReadAccess::Index;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "Images2D_VertexBuffer");
		m_IndexBuffer = Buffer::Create(indexSpecs, "Images2D_IndexBuffer");

		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UploadIndexBuffer(cmd, m_IndexBuffer);
		});
	}
	
	void RenderImages2DTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Quads.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Images 2D");
		EG_CPU_TIMING_SCOPED("Images 2D");

		Upload(cmd);
		Render(cmd);
		bUpdate = false;
	}

	void RenderImages2DTask::Upload(const Ref<CommandBuffer>& cmd)
	{
		if (!bUpdate)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Upload Images 2D");
		EG_CPU_TIMING_SCOPED("Upload Images 2D");

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;
		auto& quads = m_Quads;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = quads.size() * sizeof(QuadVertex);
		const size_t currentIndexSize = (quads.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(QuadVertex);
			newSize += alignment - (newSize % alignment);

			vb->Resize(newSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, ib->GetSize() * 3 / 2);
			constexpr size_t alignment = 6 * sizeof(Index);
			newSize += alignment - (newSize % alignment);

			ib->Resize(newSize);
			UploadIndexBuffer(cmd, ib);
		}

		cmd->Write(vb, quads.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderImages2DTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Images 2D");
		EG_CPU_TIMING_SCOPED("Render Images 2D");

		const auto& options = m_Renderer.GetOptions_RT();
		const bool bObjectPickingEnabled = options.bEnableObjectPicking && options.bEnable2DObjectPicking;
		auto& pipeline = (m_Renderer.IsRuntime() && !bObjectPickingEnabled) ? m_PipelineNoEntityID : m_Pipeline;
		pipeline->SetTextureArray(m_Textures, 0, 0);

		const uint32_t quadsCount = (uint32_t)(m_Quads.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();

		auto& stats = m_Renderer.GetStats2D();
		++stats.DrawCalls;
		stats.QuadCount += quadsCount;
	}

	void RenderImages2DTask::OnResize(glm::uvec2 size)
	{
		m_Pipeline->Resize(size.x, size.y);
		m_PipelineNoEntityID->Resize(size.x, size.y);
	}

	uint32_t RenderImages2DTask::ProcessImages(const std::vector<Image2DComponentData>& components)
	{
		if (components.empty())
			return 0;

		uint32_t textureCurrentIndex = 0u;
		for (auto& component : components)
		{
			const auto& texture = component.Texture;
			uint32_t textureIndex = textureCurrentIndex;

			auto it = m_TexturesMap.find(texture);
			if (it == m_TexturesMap.end())
			{
				if (m_TexturesMap.size() == RendererConfig::MaxTextures) // Can't be more than EG_MAX_TEXTURES
				{
					EG_CORE_CRITICAL("Not enough samplers to store all 2D textures! Max supported textures: {}", RendererConfig::MaxTextures);
					textureIndex = 0;
				}
				else
					m_TexturesMap.emplace(texture, textureCurrentIndex++);
			}
			else
				textureIndex = it->second;

			{
				const glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(component.Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
				const glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(component.Scale, 1.f));
				const glm::mat4 transform = scaleMat * rotate;

				constexpr glm::vec4 s_TexCoords[4] = {
					glm::vec4(0.f, 1.f, 0.f, 1.f),
					glm::vec4(1.f, 1.f, 0.f, 1.f),
					glm::vec4(1.f, 0.f, 0.f, 1.f),
					glm::vec4(0.f, 0.f, 0.f, 1.f)
				};

				for (int i = 0; i < 4; ++i)
				{
					auto& q1 = m_Quads.emplace_back();
					q1.Tint = component.Tint;
					q1.TextureIndex = textureIndex;
					q1.Position = glm::vec2(transform * s_TexCoords[i]) + component.Pos;
					q1.EntityID = component.EntityID;
					q1.Opacity = component.Opacity;
				}
			}
		}

		return textureCurrentIndex;
	}

	void RenderImages2DTask::SetImages(const std::vector<const Image2DComponent*>& images, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<Image2DComponentData> datas;
		datas.reserve(images.size());

		for (auto& image : images)
		{
			const auto& textureAsset = image->GetTextureAsset();
			if (!textureAsset)
				continue;

			const auto& texture = textureAsset->GetTexture();
			if (!texture)
				continue;

			auto& data = datas.emplace_back();
			data.Texture = texture;
			data.Tint = image->GetTint();
			data.Pos = image->GetPosition();
			data.Scale = image->GetScale();
			data.Rotation = image->GetRotation();
			data.EntityID = image->Parent.GetID();
			data.Opacity = image->GetOpacity();
		}

		RenderManager::Submit([components = std::move(datas), this](const Ref<CommandBuffer>&)
		{
			bUpdate = true;
			m_Quads.clear();
			m_Textures.clear();
			m_TexturesMap.clear();

			const uint32_t texturesCount = ProcessImages(components);
			m_Textures.resize(texturesCount);
			for (auto& [texture, index] : m_TexturesMap)
				m_Textures[index] = texture;
		});
	}

	void RenderImages2DTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetHDROutput();
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
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
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		ShaderDefines noObjectIDDefine = { {"EG_NO_OBJECT_ID", ""} };
		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/image2D.vert", ShaderType::Vertex, noObjectIDDefine);
		state.FragmentShader = Shader::Create("assets/shaders/image2D.frag", ShaderType::Fragment, noObjectIDDefine);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Back;

		if (m_PipelineNoEntityID)
			m_PipelineNoEntityID->SetState(state);
		else
			m_PipelineNoEntityID = PipelineGraphics::Create(state);

		state.VertexShader = Shader::Create("assets/shaders/image2D.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("assets/shaders/image2D.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
