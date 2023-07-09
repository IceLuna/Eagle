#include "egpch.h"
#include "GeometryManagerTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"

#include "Eagle/Components/Components.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static constexpr size_t s_BaseVertexBufferSize = 1 * 1024 * 1024; // 1 MB
	static constexpr size_t s_BaseIndexBufferSize = 1 * 1024 * 1024; // 1 MB
	static constexpr size_t s_BaseMaterialBufferSize = 1024 * 1024; // 1 MB

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

	static void UploadIndexBuffer(const Ref<CommandBuffer>& cmd, Ref<Buffer>& buffer)
	{
		const size_t ibSize = buffer->GetSize();
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

		cmd->Write(buffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(buffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}

	GeometryManagerTask::GeometryManagerTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		// Create Mesh buffers
		{
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

			m_OpaqueMeshesData.VertexBuffer = Buffer::Create(vertexSpecs, "Meshes_VertexBuffer_Opaque");
			m_OpaqueMeshesData.InstanceBuffer = Buffer::Create(vertexSpecs, "Meshes_InstanceVertexBuffer_Opaque");
			m_OpaqueMeshesData.IndexBuffer = Buffer::Create(indexSpecs, "Meshes_IndexBuffer_Opaque");

			m_TranslucentMeshesData.VertexBuffer = Buffer::Create(vertexSpecs, "Meshes_VertexBuffer_Translucent");
			m_TranslucentMeshesData.InstanceBuffer = Buffer::Create(vertexSpecs, "Meshes_InstanceVertexBuffer_Translucent");
			m_TranslucentMeshesData.IndexBuffer = Buffer::Create(indexSpecs, "Meshes_IndexBuffer_Translucent");

			m_MeshesTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_TransformsBuffer");
		}

		// Create Sprite buffers
		{
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

			m_OpaqueSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Opaque");
			m_OpaqueSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Opaque");

			m_TranslucentSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Translucent");
			m_TranslucentSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Translucent");

			m_SpritesTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_TransformsBuffer");

			m_OpaqueSpritesData.QuadVertices.reserve(s_DefaultVerticesCount);
			m_TranslucentSpritesData.QuadVertices.reserve(s_DefaultVerticesCount);

			RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
			{
				UploadIndexBuffer(cmd, m_OpaqueSpritesData.IndexBuffer);
				UploadIndexBuffer(cmd, m_TranslucentSpritesData.IndexBuffer);
			});
		}
	}

	void GeometryManagerTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Process Geometry");
		EG_CPU_TIMING_SCOPED("Process Geometry");

		const bool bMaterialsChanged = MaterialSystem::HasChanged();

		// Meshes
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Meshes");
			EG_CPU_TIMING_SCOPED("Process Meshes");

			if (bUploadMeshes || bMaterialsChanged)
			{
				SortMeshes();
				{
					EG_GPU_TIMING_SCOPED(cmd, "3D Meshes. Upload vertex & index buffers");
					EG_CPU_TIMING_SCOPED("3D Meshes. Upload vertex & index buffers");
					UploadMeshes(cmd, m_OpaqueMeshesData, m_OpaqueMeshes);
					UploadMeshes(cmd, m_TranslucentMeshesData, m_TranslucentMeshes);
				}
				bUploadMeshes = false;
			}
			UploadMeshTransforms(cmd);
		}

		// Sprites
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Sprites");
			EG_CPU_TIMING_SCOPED("Process Sprites");

			if (bUploadSprites || bMaterialsChanged)
			{
				SortSprites();
				{
					EG_GPU_TIMING_SCOPED(cmd, "Sprites. Upload vertex & index buffers");
					EG_CPU_TIMING_SCOPED("Sprites. Upload vertex & index buffers");

					UploadSprites(cmd, m_OpaqueSpritesData);
					UploadSprites(cmd, m_TranslucentSpritesData);
				}
				bUploadSprites = false;
			}
			UploadSpriteTransforms(cmd);
		}
	}

	void GeometryManagerTask::SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty)
	{
		if (!bDirty)
			return;

		std::unordered_map<MeshKey, std::vector<MeshData>> tempMeshes;
		std::unordered_map<uint32_t, uint64_t> meshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)
		std::vector<glm::mat4> tempMeshTransforms;

		tempMeshes.reserve(meshes.size());
		tempMeshTransforms.reserve(meshes.size());
		meshTransformIndices.reserve(meshes.size());

		uint32_t meshIndex = 0;
		for (auto& comp : meshes)
		{
			const Ref<Eagle::StaticMesh>& staticMesh = comp->GetStaticMesh();
			if (!staticMesh || !staticMesh->IsValid())
				continue;

			const uint32_t meshID = comp->Parent.GetID();
			auto& instanceData = tempMeshes[{staticMesh, staticMesh->GetGUID(), comp->DoesCastShadows()}];
			auto& meshData = instanceData.emplace_back();
			meshData.Material = comp->GetMaterial();
			meshData.InstanceData.TransformIndex = meshIndex;
			meshData.InstanceData.ObjectID = meshID;
			// meshData.InstanceData.MaterialIndex is set later during the update

			tempMeshTransforms.push_back(Math::ToTransformMatrix(comp->GetWorldTransform()));
			meshTransformIndices.emplace(meshID, meshIndex);
			++meshIndex;
		}

		RenderManager::Submit([this, meshes = std::move(tempMeshes),
			transforms = std::move(tempMeshTransforms),
			transformIndices = std::move(meshTransformIndices)](Ref<CommandBuffer>&) mutable
			{
				m_Meshes = std::move(meshes);
				m_MeshTransforms = std::move(transforms);
				m_MeshTransformIndices = std::move(transformIndices);

				bUploadMeshes = true;
				bUploadMeshTransforms = true;
			});
	}
	
	void GeometryManagerTask::SetTransforms(const std::set<const StaticMeshComponent*>& meshes)
	{
		if (meshes.empty())
			return;

		struct Data
		{
			glm::mat4 TransformMatrix;
			uint32_t ID;
		};

		std::vector<Data> updateData;
		updateData.reserve(meshes.size());

		for (auto& mesh : meshes)
			updateData.push_back({ Math::ToTransformMatrix(mesh->GetWorldTransform()), mesh->Parent.GetID() });

		RenderManager::Submit([this, data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			for (auto& mesh : data)
			{
				auto it = m_MeshTransformIndices.find(mesh.ID);
				if (it != m_MeshTransformIndices.end())
				{
					m_MeshTransforms[it->second] = mesh.TransformMatrix;
					bUploadMeshTransforms = true;
				}
			}
		});
	}
	
	void GeometryManagerTask::SortMeshes()
	{
		EG_CPU_TIMING_SCOPED("Sort meshes based on Blend Mode");

		m_OpaqueMeshes.clear();
		m_TranslucentMeshes.clear();

		for (auto& [mesh, datas] : m_Meshes)
			for (auto& data : datas)
			{
				data.InstanceData.MaterialIndex = MaterialSystem::GetMaterialIndex(data.Material);
				switch (data.Material->GetBlendMode())
				{
					case Material::BlendMode::Opaque:
						m_OpaqueMeshes[mesh].push_back(data);
						break;
					case Material::BlendMode::Translucent:
						m_TranslucentMeshes[mesh].push_back(data);
						break;
				}
			}
	}

	void GeometryManagerTask::UploadMeshes(const Ref<CommandBuffer>& cmd, MeshGeometryData& meshData, const std::unordered_map<MeshKey, std::vector<MeshData>>& meshes)
	{
		if (meshes.empty())
			return;

		auto& vb  = meshData.VertexBuffer;
		auto& ivb = meshData.InstanceBuffer;
		auto& ib  = meshData.IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = 0;
		size_t currentIndexSize = 0;
		size_t meshesCount = 0;
		for (auto& [meshKey, datas] : meshes)
		{
			currentVertexSize += meshKey.Mesh->GetVerticesCount() * sizeof(Vertex);
			currentIndexSize += meshKey.Mesh->GetIndecesCount() * sizeof(Index);
			meshesCount += datas.size();
		}
		const size_t currentInstanceVertexSize = meshesCount * sizeof(PerInstanceData);

		if (currentVertexSize > vb->GetSize())
		{
			currentVertexSize = (currentVertexSize * 3) / 2;
			vb->Resize(currentVertexSize);
		}
		if (currentInstanceVertexSize > ivb->GetSize())
			vb->Resize((currentInstanceVertexSize * 3) / 2);
		if (currentIndexSize > ib->GetSize())
		{
			currentIndexSize = (currentIndexSize * 3) / 2;
			ib->Resize(currentIndexSize);
		}

		meshData.Vertices.clear();
		meshData.Indices.clear();
		meshData.InstanceVertices.clear();
		meshData.Vertices.reserve(currentVertexSize);
		meshData.InstanceVertices.reserve(currentInstanceVertexSize);
		meshData.Indices.reserve(currentIndexSize);

		for (auto& [meshKey, datas] : meshes)
		{
			const auto& meshVertices = meshKey.Mesh->GetVertices();
			const auto& meshIndices = meshKey.Mesh->GetIndeces();
			meshData.Vertices.insert(meshData.Vertices.end(), meshVertices.begin(), meshVertices.end());
			meshData.Indices.insert(meshData.Indices.end(), meshIndices.begin(), meshIndices.end());

			for (auto& data : datas)
				meshData.InstanceVertices.push_back(data.InstanceData);
		}

		cmd->Write(vb, meshData.Vertices.data(), meshData.Vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ivb, meshData.InstanceVertices.data(), currentInstanceVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ib, meshData.Indices.data(), meshData.Indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
		cmd->TransitionLayout(ivb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
		cmd->TransitionLayout(ib, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}
	
	void GeometryManagerTask::UploadMeshTransforms(const Ref<CommandBuffer>& cmd)
	{
		if (!bUploadMeshTransforms)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "3D Meshes. Upload Transforms buffer");
		EG_CPU_TIMING_SCOPED("3D Meshes. Upload Transforms buffer");

		bUploadMeshTransforms = false;

		const auto& transforms = m_MeshTransforms;
		auto& gpuBuffer = m_MeshesTransformsBuffer;

		const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
		if (currentBufferSize > gpuBuffer->GetSize())
		{
			gpuBuffer->Resize((currentBufferSize * 3) / 2);

			if (m_MeshesPrevTransformsBuffer)
				m_MeshesPrevTransformsBuffer.reset();
		}

		if (bMotionRequired && m_MeshesPrevTransformsBuffer)
			cmd->CopyBuffer(m_MeshesTransformsBuffer, m_MeshesPrevTransformsBuffer, 0, 0, m_MeshesTransformsBuffer->GetSize());

		cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(gpuBuffer);

		if (bMotionRequired && !m_MeshesPrevTransformsBuffer)
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_MeshesTransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_MeshesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_PrevTransformsBuffer");

			cmd->CopyBuffer(m_MeshesTransformsBuffer, m_MeshesPrevTransformsBuffer, 0, 0, m_MeshesTransformsBuffer->GetSize());
		}
	}

	// ---------- Sprites ----------
	void GeometryManagerTask::SortSprites()
	{
		EG_CPU_TIMING_SCOPED("Sort sprites based on Blend Mode");

		m_OpaqueSpritesData.QuadVertices.clear();
		m_TranslucentSpritesData.QuadVertices.clear();

		const size_t spritesCount = m_Sprites.size();
		for (size_t i = 0; i < spritesCount; ++i)
		{
			const auto& sprite = m_Sprites[i];
			switch (sprite.Material->GetBlendMode())
			{
				case Material::BlendMode::Opaque:
				{
					AddQuad(m_OpaqueSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], uint32_t(i));
					break;
				}
				case Material::BlendMode::Translucent:
				{
					AddQuad(m_TranslucentSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], uint32_t(i));
					break;
				}
				default: EG_CORE_ASSERT("Unknown blend mode!");
			}
		}
	}

	void GeometryManagerTask::UploadSprites(const Ref<CommandBuffer>& cmd, SpriteGeometryData& spritesData)
	{
		if (spritesData.QuadVertices.empty())
			return;

		auto& vb = spritesData.VertexBuffer;
		auto& ib = spritesData.IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = spritesData.QuadVertices.size() * sizeof(QuadVertex);
		const size_t currentIndexSize = (spritesData.QuadVertices.size() / 4) * (sizeof(Index) * 6);

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

		cmd->Write(vb, spritesData.QuadVertices.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void GeometryManagerTask::UploadSpriteTransforms(const Ref<CommandBuffer>& cmd)
	{
		if (!bUploadSpritesTransforms)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Sprites. Upload Transforms buffer");
		EG_CPU_TIMING_SCOPED("Sprites. Upload Transforms buffer");

		bUploadSpritesTransforms = false;

		const auto& transforms = m_SpriteTransforms;
		auto& gpuBuffer = m_SpritesTransformsBuffer;

		const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
		if (currentBufferSize > gpuBuffer->GetSize())
		{
			gpuBuffer->Resize((currentBufferSize * 3) / 2);

			if (m_SpritesPrevTransformsBuffer)
				m_SpritesPrevTransformsBuffer.reset();
		}

		if (bMotionRequired && m_SpritesPrevTransformsBuffer)
			cmd->CopyBuffer(m_SpritesTransformsBuffer, m_SpritesPrevTransformsBuffer, 0, 0, m_SpritesTransformsBuffer->GetSize());

		cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(gpuBuffer);

		if (bMotionRequired && !m_SpritesPrevTransformsBuffer)
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_MeshesTransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_SpritesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_PrevTransformsBuffer");

			cmd->CopyBuffer(m_SpritesTransformsBuffer, m_SpritesPrevTransformsBuffer, 0, 0, m_SpritesTransformsBuffer->GetSize());
		}
	}

	void GeometryManagerTask::SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<SpriteData> spritesData;
		std::unordered_map<uint32_t, uint64_t> tempTransformIndices; // EntityID -> uint64_t (index to m_Transforms)
		std::vector<glm::mat4> tempTransforms;

		spritesData.reserve(sprites.size());
		tempTransformIndices.reserve(sprites.size());
		tempTransforms.reserve(sprites.size());

		uint32_t spriteIndex = 0;
		for (auto& sprite : sprites)
		{
			auto& data = spritesData.emplace_back();
			data.Material = sprite->GetMaterial();
			data.EntityID = sprite->Parent.GetID();
			data.SubTexture = sprite->GetSubTexture();
			data.bSubTexture = sprite->IsSubTexture();

			tempTransformIndices.emplace(data.EntityID, spriteIndex);
			tempTransforms.emplace_back(Math::ToTransformMatrix(sprite->GetWorldTransform()));
			spriteIndex++;
		}

		RenderManager::Submit([this, sprites = std::move(spritesData),
							   transformIndices = std::move(tempTransformIndices),
							   transforms = std::move(tempTransforms)](Ref<CommandBuffer>& cmd) mutable
		{
			m_Sprites = std::move(sprites);
			m_SpriteTransformIndices = std::move(transformIndices);
			m_SpriteTransforms = std::move(transforms);

			bUploadSprites = true;
			bUploadSpritesTransforms = true;
		});
	}

	void GeometryManagerTask::SetTransforms(const std::set<const SpriteComponent*>& sprites)
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
			for (auto& sprite : data)
			{
				auto it = m_SpriteTransformIndices.find(sprite.ID);
				if (it != m_SpriteTransformIndices.end())
				{
					m_SpriteTransforms[it->second] = sprite.TransformMatrix;
					bUploadSpritesTransforms = true;
				}
			}
		});
	}

	void GeometryManagerTask::AddQuad(std::vector<QuadVertex>& vertices, const SpriteData& sprite, const glm::mat4& transform, uint32_t transformIndex)
	{
		const int entityID = (int)sprite.EntityID;

		if (sprite.bSubTexture)
			AddQuad(vertices, transform, sprite.SubTexture, sprite.Material, transformIndex, entityID);
		else
			AddQuad(vertices, transform, sprite.Material, transformIndex, entityID);
	}

	void GeometryManagerTask::AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<Material>& material, uint32_t transformIndex, int entityID)
	{
		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const glm::vec3 normal = glm::normalize(normalModel * s_QuadVertexNormal);
		const glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * s_QuadVertexNormal));
		const glm::vec3 invNormal = -normal;
		const glm::vec3 invWorldNormal = -worldNormal;

		const uint32_t materialIndex = MaterialSystem::GetMaterialIndex(material);

		size_t frontFaceVertexIndex = vertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex.TexCoords = s_TexCoords[i];
			vertex.EntityID = entityID;
			vertex.TransformIndex = transformIndex;
			vertex.MaterialIndex = materialIndex;
		}
		// Backface
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex = vertices[frontFaceVertexIndex++];
		}
	}

	void GeometryManagerTask::AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, uint32_t transformIndex, int entityID)
	{
		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const glm::vec3 normal = normalModel * s_QuadVertexNormal;
		const glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * s_QuadVertexNormal));
		const glm::vec3 invNormal = -normal;
		const glm::vec3 invWorldNormal = -worldNormal;

		const uint32_t albedoTextureIndex = TextureSystem::AddTexture(subtexture->GetTexture());
		const glm::vec2* spriteTexCoords = subtexture->GetTexCoords();

		const uint32_t materialIndex = MaterialSystem::GetMaterialIndex(material);

		size_t frontFaceVertexIndex = vertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex.TexCoords = spriteTexCoords[i];
			vertex.EntityID = entityID;
			vertex.TransformIndex = transformIndex;
			vertex.MaterialIndex = materialIndex;
		}
		// Backface
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex = vertices[frontFaceVertexIndex++];
		}
	}
}
