#include "egpch.h"
#include "RenderBillboardsTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/Image.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static constexpr glm::vec4 s_QuadVertexPosition[4] = { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, 0.5f, 0.0f, 1.0f }, { -0.5f, 0.5f, 0.0f, 1.0f } };
	static constexpr glm::vec2 s_TexCoords[4] = { {0.0f, 1.0f}, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f } };

	RenderBillboardsTask::RenderBillboardsTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
		: RendererTask(renderer)
		, m_ResultImage(renderTo)
	{
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_BaseBillboardVertexBufferSize;
		vertexSpecs.Layout = BufferReadAccess::Vertex;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = s_BaseBillboardIndexBufferSize;
		indexSpecs.Layout = BufferReadAccess::Index;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "Billboard_VertexBuffer");
		m_IndexBuffer = Buffer::Create(indexSpecs, "Billboard_IndexBuffer");
		m_Vertices.reserve(s_DefaultBillboardVerticesCount);

		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UpdateIndexBuffer(cmd);
		});
	}

	void RenderBillboardsTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Vertices.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Billboards");
		EG_CPU_TIMING_SCOPED("Billboards");

		UpdateBuffers(cmd);
		RenderBillboards(cmd);
	}

	void RenderBillboardsTask::UpdateBuffers(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Upload billboards buffers");
		EG_CPU_TIMING_SCOPED("Upload billboards buffers");

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = m_Vertices.size() * sizeof(BillboardVertex);
		const size_t currentIndexSize = (m_Vertices.size() / 4) * (sizeof(Index) * 6);

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;

		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			const size_t alignment = 4 * sizeof(BillboardVertex);
			newSize += alignment - (newSize % alignment);
			vb->Resize(newSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, ib->GetSize() * 3 / 2);
			const size_t alignment = 6 * sizeof(Index);
			newSize += alignment - (newSize % alignment);

			ib->Resize(newSize);
			UpdateIndexBuffer(cmd);
		}

		cmd->Write(vb, m_Vertices.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderBillboardsTask::UpdateIndexBuffer(const Ref<CommandBuffer>& cmd)
	{
		const size_t& ibSize = m_IndexBuffer->GetSize();
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

		cmd->Write(m_IndexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(m_IndexBuffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}

	void RenderBillboardsTask::RenderBillboards(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Billboards");
		EG_CPU_TIMING_SCOPED("Render Billboards");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrame = texturesChangedFrame + 1;
		}

		const uint32_t quadsCount = (uint32_t)(m_Vertices.size() / 4);

		const glm::mat4& proj = m_Renderer.GetProjectionMatrix();
		const float& gamma = m_Renderer.GetOptions_RT().Gamma;
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&proj, nullptr);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void RenderBillboardsTask::SetBillboards(const std::vector<const BillboardComponent*>& billboards)
	{
		EG_CPU_TIMING_SCOPED("Renderer. Set Billboards");

		struct BillboardData
		{
			Transform WorldTransform;
			uint32_t TextureIndex;
			uint32_t EntityID;
		};

		std::vector<BillboardData> tempData;
		tempData.reserve(billboards.size());
		for (auto& billboard : billboards)
		{
			if (!billboard->Texture)
				continue;

			auto& data = tempData.emplace_back();
			data.WorldTransform = billboard->GetWorldTransform();
			data.TextureIndex = TextureSystem::AddTexture(billboard->Texture);
			data.EntityID = billboard->Parent.GetID();
		}

		RenderManager::Submit([this, billboards = std::move(tempData)](Ref<CommandBuffer>& cmd)
		{
			m_Vertices.clear();
			m_Vertices.reserve(billboards.size() * 4);
			for (auto& billboard : billboards)
			{
				const Transform& worldTransform = billboard.WorldTransform;
				const glm::mat4 transform = Math::ToTransformMatrix(worldTransform);

				const auto& viewMatrix = m_Renderer.GetViewMatrix();
				glm::mat4 modelView = viewMatrix * transform;
				// Remove rotation, apply scaling
				modelView[0] = glm::vec4(worldTransform.Scale3D.x, 0, 0, 0);
				modelView[1] = glm::vec4(0, worldTransform.Scale3D.y, 0, 0);
				modelView[2] = glm::vec4(0, 0, worldTransform.Scale3D.z, 0);

				for (int i = 0; i < 4; ++i)
				{
					auto& vertex = m_Vertices.emplace_back();

					vertex.Position = modelView * s_QuadVertexPosition[i];
					vertex.TexCoord = s_TexCoords[i];
					vertex.TextureIndex = billboard.TextureIndex;
					vertex.EntityID = billboard.EntityID;
				}
			}
		});
	}

	void RenderBillboardsTask::AddAdditionalBillboard(const Transform& worldTransform, const Ref<Texture2D>& texture, int entityID)
	{
		if (!texture)
			return;

		const uint32_t textureIndex = TextureSystem::AddTexture(texture);
		RenderManager::Submit([this, worldTransform, textureIndex, entityID](Ref<CommandBuffer>& cmd)
		{
			const glm::mat4 transform = Math::ToTransformMatrix(worldTransform);

			const auto& viewMatrix = m_Renderer.GetViewMatrix();
			glm::mat4 modelView = viewMatrix * transform;

			// Remove rotation, apply scaling
			modelView[0] = glm::vec4(worldTransform.Scale3D.x, 0, 0, 0);
			modelView[1] = glm::vec4(0, worldTransform.Scale3D.y, 0, 0);
			modelView[2] = glm::vec4(0, 0, worldTransform.Scale3D.z, 0);

			for (int i = 0; i < 4; ++i)
			{
				auto& vertex = m_Vertices.emplace_back();

				vertex.Position = modelView * s_QuadVertexPosition[i];
				vertex.TexCoord = s_TexCoords[i];
				vertex.TextureIndex = textureIndex;
				vertex.EntityID = entityID;
			}
		});
	}
	
	void RenderBillboardsTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_ResultImage;
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

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/billboard.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/billboard.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
