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
	static constexpr float s_QuadPosition = 0.5f;
	static constexpr glm::vec4 s_QuadVertexPosition[4] = { { -s_QuadPosition, -s_QuadPosition, 0.0f, 1.0f },
														   {  s_QuadPosition, -s_QuadPosition, 0.0f, 1.0f },
														   {  s_QuadPosition,  s_QuadPosition, 0.0f, 1.0f },
														   { -s_QuadPosition,  s_QuadPosition, 0.0f, 1.0f } };

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

	static constexpr size_t s_BaseMaterialBufferSize = 1024 * 1024; // 1 MB

	RenderSpritesTask::RenderSpritesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = m_Renderer.GetOptions_RT().OptionalGBuffers.bMotion;
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_BaseVertexBufferSize;
		vertexSpecs.Layout = BufferReadAccess::Vertex;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = s_BaseIndexBufferSize;
		indexSpecs.Layout = BufferReadAccess::Index;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		BufferSpecifications materialsBufferSpecs;
		materialsBufferSpecs.Size = s_BaseMaterialBufferSize;
		materialsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		materialsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications transformsBufferSpecs;
		transformsBufferSpecs.Size = sizeof(glm::mat4) * 100; // 100 transforms
		transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D");
		m_IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D");
		m_MaterialBuffer = Buffer::Create(materialsBufferSpecs, "Sprites_MaterialsBuffer");
		m_TransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_TransformsBuffer");

		m_QuadVertices.reserve(s_DefaultVerticesCount);
		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UploadIndexBuffer(cmd);
		});
	}

	void RenderSpritesTask::UpdateMaterials()
	{
		const size_t size = m_Materials.size();
		m_GPUMaterials.resize(size);

		for (size_t i = 0; i < size; ++i)
			m_GPUMaterials[i] = m_Materials[i];
	}

	void RenderSpritesTask::UploadMaterials(const Ref<CommandBuffer>& cmd)
	{
		if (m_GPUMaterials.empty())
			return;

		// Update GPU buffer
		const size_t materilBufferSize = m_MaterialBuffer->GetSize();
		const size_t materialDataSize = m_GPUMaterials.size() * sizeof(CPUMaterial);

		if (materialDataSize > materilBufferSize)
			m_MaterialBuffer->Resize((materilBufferSize * 3) / 2);

		cmd->Write(m_MaterialBuffer, m_GPUMaterials.data(), materialDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(m_MaterialBuffer);
	}

	void RenderSpritesTask::UploadTransforms(const Ref<CommandBuffer>& cmd)
	{
		if (!bUploadSpritesTransforms)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Sprites. Upload Transforms buffer");
		EG_CPU_TIMING_SCOPED("Sprites. Upload Transforms buffer");

		bUploadSpritesTransforms = false;

		const auto& transforms = m_Transforms;
		auto& gpuBuffer = m_TransformsBuffer;

		const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
		if (currentBufferSize > gpuBuffer->GetSize())
		{
			gpuBuffer->Resize((currentBufferSize * 3) / 2);

			if (m_PrevTransformsBuffer)
				m_PrevTransformsBuffer.reset();
		}

		if (bMotionRequired && m_PrevTransformsBuffer)
			cmd->CopyBuffer(m_TransformsBuffer, m_PrevTransformsBuffer, 0, 0, m_TransformsBuffer->GetSize());

		cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(gpuBuffer);

		if (bMotionRequired && !m_PrevTransformsBuffer)
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_TransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_PrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_PrevTransformsBuffer");

			cmd->CopyBuffer(m_TransformsBuffer, m_PrevTransformsBuffer, 0, 0, m_TransformsBuffer->GetSize());
		}
	}

	void RenderSpritesTask::SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<Ref<Material>> tempMaterials;
		std::vector<QuadVertex> tempVertices;
		std::vector<glm::mat4> tempTransforms;
		std::unordered_map<uint32_t, uint64_t> tempTransformIndices; // EntityID -> uint64_t (index to m_Transforms)

		tempVertices.reserve(sprites.size() * 8); // each sprite has 8 vertices, 4 - front face; 4 - back face
		tempMaterials.reserve(sprites.size());
		tempTransforms.reserve(sprites.size());
		tempTransformIndices.reserve(sprites.size());

		uint32_t spriteIndex = 0;
		for (auto& sprite : sprites)
		{
			AddQuad(tempVertices, tempMaterials, tempTransforms, *sprite);
			tempTransformIndices.emplace(sprite->Parent.GetID(), spriteIndex);
			spriteIndex++;
		}

		RenderManager::Submit([this, vertices = std::move(tempVertices),
			materials = std::move(tempMaterials),
			transforms = std::move(tempTransforms),
			transformIndices = std::move(tempTransformIndices)](Ref<CommandBuffer>& cmd) mutable
		{
			m_QuadVertices = std::move(vertices);
			m_Materials = std::move(materials);
			m_Transforms = std::move(transforms);
			m_TransformIndices = std::move(transformIndices);

			bUploadSprites = true;
			bUploadSpritesTransforms = true;
		});
	}

	void RenderSpritesTask::SetTransforms(const std::set<const SpriteComponent*>& sprites)
	{
		if (sprites.empty())
			return;

		struct Data
		{
			glm::mat4 TransformMatrix;
			uint32_t ID;
		};

		std::vector<Data> updateData;
		updateData.reserve(sprites.size());

		for (auto& sprite : sprites)
			updateData.push_back({ Math::ToTransformMatrix(sprite->GetWorldTransform()), sprite->Parent.GetID() });

		RenderManager::Submit([this, data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			for (auto& mesh : data)
			{
				auto it = m_TransformIndices.find(mesh.ID);
				if (it != m_TransformIndices.end())
				{
					m_Transforms[it->second] = mesh.TransformMatrix;
					bUploadSpritesTransforms = true;
				}
			}
		});
	}

	void RenderSpritesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_QuadVertices.empty())
			return;

		UpdateMaterials(); // No caching of materials yet, so need to update anyway
		UploadMaterials(cmd);
		UploadQuads(cmd);
		UploadTransforms(cmd);
		RenderSprites(cmd);
	}

	void RenderSpritesTask::RenderSprites(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Render Sprites");
		EG_GPU_TIMING_SCOPED(cmd, "Render Sprites");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrame;
		if (bTexturesDirty)
		{
			m_Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrame = texturesChangedFrame + 1;
		}
		m_Pipeline->SetBuffer(m_MaterialBuffer, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_Pipeline->SetBuffer(m_TransformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_Pipeline->SetBuffer(m_PrevTransformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}

		const uint32_t quadsCount = (uint32_t)(m_QuadVertices.size() / 4);
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
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
		if (!bUploadSprites)
			return;

		EG_CPU_TIMING_SCOPED("Upload Sprites");
		EG_GPU_TIMING_SCOPED(cmd, "Upload Sprites");

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = m_QuadVertices.size() * sizeof(QuadVertex);
		const size_t currentIndexSize = (m_QuadVertices.size() / 4) * (sizeof(Index) * 6);

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

		cmd->Write(vb, m_QuadVertices.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
		bUploadSprites = false;
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

		m_Pipeline = PipelineGraphics::Create(state);
	}

	void RenderSpritesTask::AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const SpriteComponent& sprite)
	{
		const int entityID = sprite.Parent.GetID();
		const auto& material = sprite.Material;

		if (sprite.IsSubTexture())
			AddQuad(vertices, materials, transforms, sprite.GetWorldTransform(), sprite.GetSubTexture(), material, (int)entityID);
		else
			AddQuad(vertices, materials, transforms, sprite.GetWorldTransform(), material, (int)entityID);
	}

	void RenderSpritesTask::AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const Transform& transform, const Ref<Material>& material, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
		AddQuad(vertices, materials, transforms, transformMatrix, material, entityID);
	}

	void RenderSpritesTask::AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const Transform& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, int entityID)
	{
		if (!subtexture || !(subtexture->GetTexture()))
			return;

		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
		AddQuad(vertices, materials, transforms, transformMatrix, subtexture, material, entityID);
	}

	void RenderSpritesTask::AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const glm::mat4& transform, const Ref<Material>& material, int entityID)
	{
		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const glm::vec3 normal = glm::normalize(normalModel * s_QuadVertexNormal);
		const glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * s_QuadVertexNormal));
		const glm::vec3 invNormal = -normal;
		const glm::vec3 invWorldNormal = -worldNormal;

		const uint32_t materialIndex = (uint32_t)materials.size();
		const uint32_t transformIndex = (uint32_t)transforms.size();
		materials.push_back(material);
		transforms.push_back(transform);

		size_t frontFaceVertexIndex = vertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex.TexCoords = s_TexCoords[i];
			vertex.EntityID = entityID;
			vertex.MaterialIndex = materialIndex;
			vertex.TransformIndex = transformIndex;
		}
		// Backface
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex = vertices[frontFaceVertexIndex++];
		}
	}

	void RenderSpritesTask::AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, int entityID)
	{
		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const glm::vec3 normal = normalModel * s_QuadVertexNormal;
		const glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * s_QuadVertexNormal));
		const glm::vec3 invNormal = -normal;
		const glm::vec3 invWorldNormal = -worldNormal;

		const uint32_t albedoTextureIndex = TextureSystem::AddTexture(subtexture->GetTexture());
		const glm::vec2* spriteTexCoords = subtexture->GetTexCoords();

		const uint32_t materialIndex = (uint32_t)materials.size();
		const uint32_t transformIndex = (uint32_t)transforms.size();
		materials.push_back(material);
		transforms.push_back(transform);

		size_t frontFaceVertexIndex = vertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex.TexCoords = spriteTexCoords[i];
			vertex.EntityID = entityID;
			vertex.MaterialIndex = materialIndex;
			vertex.TransformIndex = transformIndex;
		}
		// Backface
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex = vertices[frontFaceVertexIndex++];
		}
	}
}
