#include "egpch.h"
#include "RenderSpritesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Core/Transform.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
	static constexpr glm::vec4 s_QuadVertexPosition[4] = { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, 0.5f, 0.0f, 1.0f }, { -0.5f, 0.5f, 0.0f, 1.0f } };
	static constexpr glm::vec4 s_QuadVertexNormal = { 0.0f,  0.0f, 1.0f, 0.0f };

	static constexpr glm::vec2 s_TexCoords[4] = { {0.0f, 1.0f}, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f } };
	static constexpr glm::vec3 Edge1 = s_QuadVertexPosition[1] - s_QuadVertexPosition[0];
	static constexpr glm::vec3 Edge2 = s_QuadVertexPosition[2] - s_QuadVertexPosition[0];
	static constexpr glm::vec2 DeltaUV1 = s_TexCoords[1] - s_TexCoords[0];
	static constexpr glm::vec2 DeltaUV2 = s_TexCoords[2] - s_TexCoords[0];
	static constexpr float f = 1.0f / (DeltaUV1.x * DeltaUV2.y - DeltaUV2.x * DeltaUV1.y);

	static constexpr glm::vec4 s_Tangent = glm::vec4(f * (DeltaUV2.y * Edge1.x - DeltaUV1.y * Edge2.x),
		f * (DeltaUV2.y * Edge1.y - DeltaUV1.y * Edge2.y),
		f * (DeltaUV2.y * Edge1.z - DeltaUV1.y * Edge2.z),
		0.f);

	static constexpr glm::vec4 s_Bitangent = glm::vec4(f * (-DeltaUV2.x * Edge1.x + DeltaUV1.x * Edge2.x),
		f * (-DeltaUV2.x * Edge1.y + DeltaUV1.x * Edge2.y),
		f * (-DeltaUV2.x * Edge1.z + DeltaUV1.x * Edge2.z),
		0.f);

	RenderSpritesTask::RendererMaterial::RendererMaterial(const Ref<Material>& material)
		: TintColor(material->TintColor), TilingFactor(material->TilingFactor)
	{}

	RenderSpritesTask::RendererMaterial& RenderSpritesTask::RendererMaterial::operator=(const Ref<Texture2D>& texture)
	{
		uint32_t albedoTextureIndex = TextureSystem::AddTexture(texture);
		PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

		return *this;
	}

	RenderSpritesTask::RendererMaterial& RenderSpritesTask::RendererMaterial::operator=(const Ref<Material>& material)
	{
		TintColor = material->TintColor;
		TilingFactor = material->TilingFactor;

		uint32_t albedoTextureIndex = TextureSystem::AddTexture(material->GetAlbedoTexture());
		uint32_t metallnessTextureIndex = TextureSystem::AddTexture(material->GetMetallnessTexture());
		uint32_t normalTextureIndex = TextureSystem::AddTexture(material->GetNormalTexture());
		uint32_t roughnessTextureIndex = TextureSystem::AddTexture(material->GetRoughnessTexture());
		uint32_t aoTextureIndex = TextureSystem::AddTexture(material->GetAOTexture());

		PackedTextureIndices = PackedTextureIndices2 = 0;
		PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
		PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
		PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

		PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
		PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

		return *this;
	}

	RenderSpritesTask::RenderSpritesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_BaseVertexBufferSize;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = s_BaseIndexBufferSize;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D");
		m_IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D");

		m_QuadVertices.reserve(s_DefaultVerticesCount);
		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UploadIndexBuffer(cmd);
		});
	}

	void RenderSpritesTask::SetSprites(const std::vector<const SpriteComponent*>& sprites)
	{
		m_QuadVertices.clear();
		m_QuadVertices.reserve(sprites.size() * 8); // each sprite has 8 vertices, 4 - front face; 4 - back face

		for (auto& sprite : sprites)
			AddQuad(*sprite);
	}

	void RenderSpritesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_QuadVertices.empty())
			return;

		UploadQuads(cmd);
		Render(cmd);
	}

	void RenderSpritesTask::Render(const Ref<CommandBuffer>& cmd)
	{
		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrame = texturesChangedFrame + 1;
		}

		const uint32_t quadsCount = (uint32_t)(m_QuadVertices.size() / 4);
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&m_Renderer.GetViewProjection()[0][0], nullptr);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void RenderSpritesTask::UploadIndexBuffer(const Ref<CommandBuffer>& cmd)
	{
		const size_t ibSize = m_IndexBuffer->GetSize();
		uint32_t offset = 0;
		std::vector<Index> indices(ibSize / sizeof(Index));
		for (size_t i = 0; i < indices.size();)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
			i += 6;

			indices[i + 0] = offset + 2;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 0;

			indices[i + 3] = offset + 0;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 2;

			offset += 4;
			i += 6;
		}

		cmd->Write(m_IndexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(m_IndexBuffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}

	void RenderSpritesTask::UploadQuads(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Renderer. Upload Quads");
		EG_GPU_TIMING_SCOPED(cmd, "Upload Quads");

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = m_QuadVertices.size() * sizeof(QuadVertex);
		size_t currentIndexSize = (m_QuadVertices.size() / 4) * (sizeof(Index) * 6);

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
			UploadIndexBuffer(cmd);
		}

		cmd->Write(vb, m_QuadVertices.data(), m_QuadVertices.size() * sizeof(QuadVertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderSpritesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = false;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = gbuffer.Albedo;

		ColorAttachment geometryNormalAttachment;
		geometryNormalAttachment.bClearEnabled = false;
		geometryNormalAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.Image = gbuffer.GeometryNormal;

		ColorAttachment shadingNormalAttachment;
		shadingNormalAttachment.bClearEnabled = false;
		shadingNormalAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.Image = gbuffer.ShadingNormal;

		ColorAttachment materialAttachment;
		materialAttachment.bClearEnabled = false;
		materialAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.Image = gbuffer.MaterialData;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.bClearEnabled = false;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.Image = gbuffer.ObjectID;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bClearEnabled = false;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/sprite.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("assets/shaders/sprite.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometryNormalAttachment);
		state.ColorAttachments.push_back(shadingNormalAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}

	void RenderSpritesTask::AddQuad(const SpriteComponent& sprite)
	{
		const int entityID = sprite.Parent.GetID();
		const auto& material = sprite.Material;

		if (sprite.bSubTexture)
			AddQuad(sprite.GetWorldTransform(), sprite.SubTexture, { material->TintColor, 1.f, material->TilingFactor }, (int)entityID);
		else
			AddQuad(sprite.GetWorldTransform(), material, (int)entityID);
	}

	void RenderSpritesTask::AddQuad(const Transform& transform, const Ref<Material>& material, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
		AddQuad(transformMatrix, material, entityID);
	}

	void RenderSpritesTask::AddQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const SubTextureProps& textureProps, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
		AddQuad(transformMatrix, subtexture, textureProps, entityID);
	}

	void RenderSpritesTask::AddQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID)
	{
		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const glm::vec3 normal = normalModel * s_QuadVertexNormal;
		const glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * s_QuadVertexNormal));
		const glm::vec3 invNormal = -normal;
		const glm::vec3 invWorldNormal = -worldNormal;

		size_t frontFaceVertexIndex = m_QuadVertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = m_QuadVertices.emplace_back();
			vertex.Position = transform * s_QuadVertexPosition[i];
			vertex.Normal = normal;
			vertex.WorldTangent   = glm::normalize(glm::vec3(transform * s_Tangent));
			vertex.WorldBitangent = glm::normalize(glm::vec3(transform * s_Bitangent));
			vertex.WorldNormal = worldNormal;
			vertex.TexCoord = s_TexCoords[i];
			vertex.EntityID = entityID;
			vertex.Material = material;
		}

		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = m_QuadVertices.emplace_back();
			vertex = m_QuadVertices[frontFaceVertexIndex++];
			vertex.Normal = invNormal;
			vertex.WorldNormal = invWorldNormal;
		}
	}

	void RenderSpritesTask::AddQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const SubTextureProps& textureProps, int entityID)
	{
		if (!subtexture)
			return;

		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const glm::vec3 normal = normalModel * s_QuadVertexNormal;
		const glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * s_QuadVertexNormal));
		const glm::vec3 invNormal = -normal;
		const glm::vec3 invWorldNormal = -worldNormal;

		const uint32_t albedoTextureIndex = TextureSystem::AddTexture(subtexture->GetTexture());
		const glm::vec2* spriteTexCoords = subtexture->GetTexCoords();

		size_t frontFaceVertexIndex = m_QuadVertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = m_QuadVertices.emplace_back();

			vertex.Position = transform * s_QuadVertexPosition[i];
			vertex.Normal = normal;
			vertex.WorldTangent   = glm::normalize(glm::vec3(transform * s_Tangent));
			vertex.WorldBitangent = glm::normalize(glm::vec3(transform * s_Bitangent));
			vertex.WorldNormal = worldNormal;
			vertex.TexCoord = spriteTexCoords[i];
			vertex.EntityID = entityID;

			vertex.Material.TintColor = textureProps.TintColor;
			vertex.Material.TilingFactor = textureProps.TilingFactor;
			vertex.Material.PackedTextureIndices = albedoTextureIndex;
		}
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = m_QuadVertices.emplace_back();
			vertex = m_QuadVertices[frontFaceVertexIndex++];
			vertex.Normal = invNormal;
			vertex.WorldNormal = invWorldNormal;
		}
	}
}
