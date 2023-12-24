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

#include "msdf-atlas-gen.h"

#include <codecvt>

// TODO: Test this functionality on heavy scenes and check if it's faster than uploading the whole buffer at once
#define EG_UPLOAD_ONLY_REQUIRED_TRANSFORMS 1

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

			if (i >= indices.size())
				break;

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

	static void UploadIndexBufferOneSided(const Ref<CommandBuffer>& cmd, Ref<Buffer>& buffer)
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

	static void UploadTransforms(const Ref<CommandBuffer>& cmd, const std::vector<glm::mat4>& transforms, Ref<Buffer>& transformsBuffer, Ref<Buffer>& prevTransformsBuffer,
		std::vector<uint64_t>& specificIndices, bool* bUploadTransforms, bool* bUploadSpecificTransforms, bool bMotionRequired, bool bTransformBufferGarbage, const char* debugName)
	{
		EG_GPU_TIMING_SCOPED(cmd, debugName);
		EG_CPU_TIMING_SCOPED(debugName);

		auto& gpuBuffer = transformsBuffer;
		auto& prevGpuBuffer = prevTransformsBuffer;

		if (!(*bUploadTransforms) && !(*bUploadSpecificTransforms))
		{
			if (bMotionRequired)
				cmd->CopyBuffer(gpuBuffer, prevGpuBuffer, 0, 0, gpuBuffer->GetSize());

			return;
		}

		if (transforms.empty())
		{
			*bUploadTransforms = false;
			*bUploadSpecificTransforms = false;
			specificIndices.clear();
			return;
		}

#if EG_UPLOAD_ONLY_REQUIRED_TRANSFORMS
		if (*bUploadTransforms)
#else
		if (*bUploadTransforms || *bUploadSpecificTransforms)
#endif
		{
			const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
			if (currentBufferSize > gpuBuffer->GetSize())
			{
				size_t newSize = (currentBufferSize * 3) / 2;
				gpuBuffer->Resize(newSize);
				bTransformBufferGarbage = true;
				if (prevGpuBuffer)
					prevGpuBuffer->Resize(newSize);
			}

			if (bMotionRequired && !bTransformBufferGarbage) // Copy old transforms but not if it's garbage
				cmd->CopyBuffer(gpuBuffer, prevGpuBuffer, 0, 0, gpuBuffer->GetSize());

			cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
			cmd->StorageBufferBarrier(gpuBuffer);

			if (bMotionRequired && bTransformBufferGarbage)
				cmd->CopyBuffer(gpuBuffer, prevGpuBuffer, 0, 0, gpuBuffer->GetSize());
		}
#if EG_UPLOAD_ONLY_REQUIRED_TRANSFORMS
		else
		{
			// If uploadind specific transforms, copy data to "Prev Transforms" and the update current transforms buffer
			if (*bUploadSpecificTransforms)
			{
				constexpr size_t uploadSize = sizeof(glm::mat4);
				if (bMotionRequired)
				{
					// Update prev buffer
					for (auto& index : specificIndices)
					{
						const size_t offset = index * uploadSize;
						cmd->CopyBuffer(gpuBuffer, prevGpuBuffer, offset, offset, uploadSize);
					}
				}

				// Update current buffer
				for (auto& index : specificIndices)
				{
					const size_t offset = index * uploadSize;
					cmd->Write(gpuBuffer, &transforms[index], uploadSize, offset, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
				}
			}
		}
#endif

		*bUploadTransforms = false;
		*bUploadSpecificTransforms = false;
		specificIndices.clear();
	}

	GeometryManagerTask::GeometryManagerTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = m_Renderer.GetOptions_RT().InternalState.bMotionBuffer;

		// Create Mesh buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = s_MeshesBaseVertexBufferSize;
			vertexSpecs.Layout = BufferReadAccess::Vertex;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = s_MeshesBaseIndexBufferSize;
			indexSpecs.Layout = BufferReadAccess::Index;
			indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

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

			m_MaskedMeshesData.VertexBuffer = Buffer::Create(vertexSpecs, "Meshes_VertexBuffer_Masked");
			m_MaskedMeshesData.InstanceBuffer = Buffer::Create(vertexSpecs, "Meshes_InstanceVertexBuffer_Masked");
			m_MaskedMeshesData.IndexBuffer = Buffer::Create(indexSpecs, "Meshes_IndexBuffer_Masked");

			m_MeshesTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_TransformsBuffer");
		}

		// Create Sprite buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = s_SpritesBaseVertexBufferSize;
			vertexSpecs.Layout = BufferReadAccess::Vertex;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = s_SpritesBaseIndexBufferSize;
			indexSpecs.Layout = BufferReadAccess::Index;
			indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = sizeof(glm::mat4) * 100; // 100 transforms
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;

			m_OpaqueSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Opaque");
			m_OpaqueSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Opaque");

			m_MaskedSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Masked");
			m_MaskedSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Masked");

			m_OpaqueNonShadowSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Opaque_NotCastingShadow");
			m_OpaqueNonShadowSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Opaque_NotCastingShadow");

			m_MaskedNonShadowSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Masked_NotCastingShadow");
			m_MaskedNonShadowSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Masked_NotCastingShadow");

			m_TranslucentSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Translucent");
			m_TranslucentSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Translucent");

			m_TranslucentNonShadowSpritesData.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Translucent_NotCastingShadow");
			m_TranslucentNonShadowSpritesData.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Translucent_NotCastingShadow");

			m_SpritesTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_TransformsBuffer");

			m_OpaqueSpritesData.QuadVertices.reserve(s_SpritesDefaultVerticesCount);
			m_OpaqueNonShadowSpritesData.QuadVertices.reserve(s_SpritesDefaultVerticesCount);
			m_MaskedSpritesData.QuadVertices.reserve(s_SpritesDefaultVerticesCount);
			m_MaskedNonShadowSpritesData.QuadVertices.reserve(s_SpritesDefaultVerticesCount);
			m_TranslucentSpritesData.QuadVertices.reserve(s_SpritesDefaultVerticesCount);
			m_TranslucentNonShadowSpritesData.QuadVertices.reserve(s_SpritesDefaultVerticesCount);

			RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
			{
				UploadIndexBuffer(cmd, m_OpaqueSpritesData.IndexBuffer);
				UploadIndexBuffer(cmd, m_OpaqueNonShadowSpritesData.IndexBuffer);
				UploadIndexBuffer(cmd, m_MaskedSpritesData.IndexBuffer);
				UploadIndexBuffer(cmd, m_MaskedNonShadowSpritesData.IndexBuffer);
				UploadIndexBuffer(cmd, m_TranslucentSpritesData.IndexBuffer);
				UploadIndexBuffer(cmd, m_TranslucentNonShadowSpritesData.IndexBuffer);
			});
		}
	
		// Create Text buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = s_LitTextBaseVertexBufferSize;
			vertexSpecs.Layout = BufferReadAccess::Vertex;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = s_LitTextBaseIndexBufferSize;
			indexSpecs.Layout = BufferReadAccess::Index;
			indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = sizeof(glm::mat4) * 100; // 100 transforms
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;

			m_OpaqueLitTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer_Opaque");
			m_OpaqueLitTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer_Opaque");
			m_OpaqueLitNonShadowTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer_Opaque_NotCastingShadow");
			m_OpaqueLitNonShadowTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer_Opaque_NotCastingShadow");

			m_MaskedLitTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer_Masked");
			m_MaskedLitTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer_Masked");
			m_MaskedLitNonShadowTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer_Masked_NotCastingShadow");
			m_MaskedLitNonShadowTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer_Masked_NotCastingShadow");

			m_TranslucentLitTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer_Translucent");
			m_TranslucentLitTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer_Translucent");
			m_TranslucentNonShadowLitTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer_Translucent_NotCastingShadow");
			m_TranslucentNonShadowLitTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer_Translucent_NotCastingShadow");

			vertexSpecs.Size = s_UnlitTextBaseVertexBufferSize;
			indexSpecs.Size = s_UnlitTextBaseIndexBufferSize;
			m_UnlitTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Unlit_VertexBuffer");
			m_UnlitTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Unlit_IndexBuffer");
			m_UnlitNonShadowTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Unlit_VertexBuffer_NotCastingShadow");
			m_UnlitNonShadowTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Unlit_IndexBuffer_NotCastingShadow");

			m_TextTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_TransformsBuffer");

			m_OpaqueLitTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_OpaqueLitNonShadowTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_MaskedLitTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_MaskedLitNonShadowTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_TranslucentLitTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_TranslucentNonShadowLitTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_UnlitTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);
			m_UnlitNonShadowTextData.QuadVertices.reserve(s_TextDefaultVerticesCount);

			RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
			{
				UploadIndexBuffer(cmd, m_OpaqueLitTextData.IndexBuffer);
				UploadIndexBuffer(cmd, m_OpaqueLitNonShadowTextData.IndexBuffer);
				UploadIndexBuffer(cmd, m_MaskedLitTextData.IndexBuffer);
				UploadIndexBuffer(cmd, m_MaskedLitNonShadowTextData.IndexBuffer);
				UploadIndexBuffer(cmd, m_TranslucentLitTextData.IndexBuffer);
				UploadIndexBuffer(cmd, m_TranslucentNonShadowLitTextData.IndexBuffer);
				UploadIndexBufferOneSided(cmd, m_UnlitTextData.IndexBuffer);
				UploadIndexBufferOneSided(cmd, m_UnlitNonShadowTextData.IndexBuffer);
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
					UploadMeshes(cmd, m_MaskedMeshesData, m_MaskedMeshes);
				}
			}
			const bool bTransformBufferGarbage = bUploadMeshes;
			UploadTransforms(cmd, m_MeshTransforms, m_MeshesTransformsBuffer, m_MeshesPrevTransformsBuffer, m_MeshUploadSpecificTransforms,
				&bUploadMeshTransforms, &bUploadMeshSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "3D Meshes. Upload Transforms buffer");

			bUploadMeshes = false;
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
					UploadSprites(cmd, m_OpaqueNonShadowSpritesData);
					UploadSprites(cmd, m_MaskedSpritesData);
					UploadSprites(cmd, m_MaskedNonShadowSpritesData);
					UploadSprites(cmd, m_TranslucentSpritesData);
					UploadSprites(cmd, m_TranslucentNonShadowSpritesData);
				}
			}
			const bool bTransformBufferGarbage = bUploadSprites;
			UploadTransforms(cmd, m_SpriteTransforms, m_SpritesTransformsBuffer, m_SpritesPrevTransformsBuffer, m_SpriteUploadSpecificTransforms,
				&bUploadSpritesTransforms, &bUploadSpritesSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Sprites. Upload Transforms buffer");
			
			bUploadSprites = false;
		}
	
		// Texts
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Texts");
			EG_CPU_TIMING_SCOPED("Process Texts");

			const bool bTextMaterialsChanged = false; // Texts have their own material type
			if (bUploadTextQuads || bTextMaterialsChanged)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Texts. Upload vertex & index buffers");
				EG_CPU_TIMING_SCOPED("Texts. Upload vertex & index buffers");

				UploadTexts(cmd, m_OpaqueLitTextData);
				UploadTexts(cmd, m_OpaqueLitNonShadowTextData);
				UploadTexts(cmd, m_MaskedLitTextData);
				UploadTexts(cmd, m_MaskedLitNonShadowTextData);
				UploadTexts(cmd, m_TranslucentLitTextData);
				UploadTexts(cmd, m_TranslucentNonShadowLitTextData);
				UploadTexts(cmd, m_UnlitTextData);
				UploadTexts(cmd, m_UnlitNonShadowTextData);
			}
			const bool bTransformBufferGarbage = bUploadTextQuads;
			UploadTransforms(cmd, m_TextTransforms, m_TextTransformsBuffer, m_TextPrevTransformsBuffer, m_TextUploadSpecificTransforms,
				&bUploadTextTransforms, &bUploadTextSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Texts. Upload Transforms buffer");

			bUploadTextQuads = false;
		}
	}

	void GeometryManagerTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (bMotionRequired == settings.InternalState.bMotionBuffer)
			return;

		bMotionRequired = settings.InternalState.bMotionBuffer;
		if (!bMotionRequired)
		{
			m_MeshesPrevTransformsBuffer.reset();
			m_SpritesPrevTransformsBuffer.reset();
			m_TextPrevTransformsBuffer.reset();
		}
		else
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_MeshesTransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_MeshesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_SpritesTransformsBuffer->GetSize();
			m_SpritesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_TextTransformsBuffer->GetSize();
			m_TextPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_PrevTransformsBuffer");
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
					m_MeshUploadSpecificTransforms.push_back(it->second);
					bUploadMeshSpecificTransforms = true;
				}
			}
		});
	}
	
	void GeometryManagerTask::SortMeshes()
	{
		EG_CPU_TIMING_SCOPED("Sort meshes based on Blend Mode");

		m_OpaqueMeshes.clear();
		m_TranslucentMeshes.clear();
		m_MaskedMeshes.clear();

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
					case Material::BlendMode::Masked:
						m_MaskedMeshes[mesh].push_back(data);
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

	// ---------- Sprites ----------
	void GeometryManagerTask::SortSprites()
	{
		EG_CPU_TIMING_SCOPED("Sort sprites based on Blend Mode");

		m_OpaqueSpritesData.QuadVertices.clear();
		m_OpaqueNonShadowSpritesData.QuadVertices.clear();
		m_MaskedSpritesData.QuadVertices.clear();
		m_MaskedNonShadowSpritesData.QuadVertices.clear();
		m_TranslucentSpritesData.QuadVertices.clear();
		m_TranslucentNonShadowSpritesData.QuadVertices.clear();

		const size_t spritesCount = m_Sprites.size();
		for (size_t i = 0; i < spritesCount; ++i)
		{
			const auto& sprite = m_Sprites[i];
			const uint32_t transformIndex = uint32_t(i);
			switch (sprite.Material->GetBlendMode())
			{
				case Material::BlendMode::Opaque:
				{
					if (sprite.bCastsShadows)
						AddQuad(m_OpaqueSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], transformIndex);
					else
						AddQuad(m_OpaqueNonShadowSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], transformIndex);
					break;
				}
				case Material::BlendMode::Translucent:
				{
					if (sprite.bCastsShadows)
						AddQuad(m_TranslucentSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], transformIndex);
					else
						AddQuad(m_TranslucentNonShadowSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], transformIndex);
					break;
				}
				case Material::BlendMode::Masked:
				{
					if (sprite.bCastsShadows)
						AddQuad(m_MaskedSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], transformIndex);
					else
						AddQuad(m_MaskedNonShadowSpritesData.QuadVertices, sprite, m_SpriteTransforms[i], transformIndex);
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
			data.bAtlas = sprite->IsAtlas();
			data.bCastsShadows = sprite->DoesCastShadows();
			if (data.bAtlas)
			{
				if (const auto& atlas = data.Material->GetAlbedoTexture())
				{
					const float textureWidth = (float)atlas->GetWidth();
					const float textureHeight = (float)atlas->GetHeight();

					const glm::vec2 coords = sprite->GetAtlasSpriteCoords();
					const glm::vec2 cellSize = sprite->GetAtlasSpriteSize();
					const glm::vec2 spriteSize = sprite->GetAtlasSpriteSizeCoef();

					glm::vec2 min = { (coords.x * cellSize.x) / textureWidth, (coords.y * cellSize.y) / textureHeight };
					glm::vec2 max = { ((coords.x + spriteSize.x) * cellSize.x) / textureWidth, ((coords.y + spriteSize.y) * cellSize.y) / textureHeight };

					data.AtlasSpriteUVs[0] = { min.x, max.y };
					data.AtlasSpriteUVs[1] = { max.x, max.y };
					data.AtlasSpriteUVs[2] = { max.x, min.y };
					data.AtlasSpriteUVs[3] = { min.x, min.y };
				}
			}

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
					m_SpriteUploadSpecificTransforms.push_back(it->second);
					bUploadSpritesSpecificTransforms = true;
				}
			}
		});
	}

	void GeometryManagerTask::AddQuad(std::vector<QuadVertex>& vertices, const SpriteData& sprite, const glm::mat4& transform, uint32_t transformIndex)
	{
		AddQuad(vertices, transform, sprite.Material, transformIndex, sprite.bAtlas ? sprite.AtlasSpriteUVs : s_TexCoords, (int)sprite.EntityID);
	}

	void GeometryManagerTask::AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<Material>& material, uint32_t transformIndex, const glm::vec2 UVs[4], int entityID)
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
			vertex.TexCoords = UVs[i];
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

	// --------- Texts ---------
	struct LitTextComponentData
	{
		glm::vec3 Albedo;
		float Roughness;
		glm::vec3 Emissive;
		float Metallness;
		std::u32string Text;
		Ref<Font> Font;
		int EntityID;
		float LineHeightOffset;
		float KerningOffset;
		float MaxWidth;
		float AO;
		float Opacity;
		float OpacityMask;
		uint32_t TransformIndex;
	};

	struct UnlitTextComponentData
	{
		glm::vec3 Color;
		std::u32string Text;
		Ref<Font> Font;
		int EntityID;
		float LineHeightOffset;
		float KerningOffset;
		float MaxWidth;
		uint32_t TransformIndex;
	};

	static std::u32string ToUTF32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

	static void ProcessLitComponents(const std::vector<LitTextComponentData>& textComponents, std::unordered_map<Ref<Texture2D>, uint32_t>& fontAtlases, LitTextGeometryData& geometryData, uint32_t& atlasCurrentIndex)
	{
		if (textComponents.empty())
			return;

		const size_t componentsCount = textComponents.size();
		for (size_t i = 0; i < componentsCount; ++i)
		{
			auto& component = textComponents[i];
			const auto& fontGeometry = component.Font->GetFontGeometry();
			const auto& metrics = fontGeometry->getMetrics();
			const auto& text = component.Text;
			const auto& atlas = component.Font->GetAtlas();
			uint32_t atlasIndex = atlasCurrentIndex;
			auto it = fontAtlases.find(atlas);
			if (it == fontAtlases.end())
			{
				if (fontAtlases.size() == RendererConfig::MaxTextures)
				{
					EG_CORE_CRITICAL("Not enough samplers to store all font atlases! Max supported fonts: {}", RendererConfig::MaxTextures);
					atlasIndex = 0;
				}
				else
					fontAtlases.emplace(atlas, atlasCurrentIndex++);
			}
			else
				atlasIndex = it->second;

			const double spaceAdvance = fontGeometry->getGlyph(' ')->getAdvance();
			std::vector<int> nextLines = Font::GetNextLines(metrics, fontGeometry, text, spaceAdvance,
				component.LineHeightOffset, component.KerningOffset, component.MaxWidth);

			{
				double x = 0.0;
				double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
				double y = 0.0;
				const uint32_t transformIndex = component.TransformIndex;

				const size_t textSize = text.size();
				for (int i = 0; i < textSize; i++)
				{
					char32_t character = text[i];
					if (character == '\n' || Font::NextLine(i, nextLines))
					{
						x = 0;
						y -= fsScale * metrics.lineHeight + component.LineHeightOffset;
						continue;
					}

					const bool bIsTab = character == '\t';
					if (character == ' ' || bIsTab)
					{
						character = ' '; // treat tabs as spaces
						double advance = spaceAdvance;
						if (i < textSize - 1)
						{
							char32_t nextCharacter = text[i + 1];
							if (nextCharacter == '\t')
								nextCharacter = ' ';
							fontGeometry->getAdvance(advance, character, nextCharacter);
						}

						// Tab is 4 spaces
						x += (fsScale * advance + component.KerningOffset) * (bIsTab ? 4.0 : 1.0);
						continue;
					}

					auto glyph = fontGeometry->getGlyph(character);
					if (!glyph)
						glyph = fontGeometry->getGlyph('?');
					if (!glyph)
						continue;

					double l, b, r, t;
					glyph->getQuadAtlasBounds(l, b, r, t);

					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);

					pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
					pl += x, pb += y, pr += x, pt += y;

					double texelWidth = 1. / atlas->GetWidth();
					double texelHeight = 1. / atlas->GetHeight();
					l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

					const size_t q1Index = geometryData.QuadVertices.size();
					{
						auto& q1 = geometryData.QuadVertices.emplace_back();
						q1.Position = glm::vec2(pl, pb);
						q1.AlbedoRoughness = glm::vec4(component.Albedo, component.Roughness);
						q1.EmissiveMetallness = glm::vec4(component.Emissive, component.Metallness);
						q1.AO = component.AO;
						q1.Opacity = component.Opacity;
						q1.OpacityMask = component.OpacityMask;
						q1.TexCoord = { l, b };
						q1.EntityID = component.EntityID;
						q1.AtlasIndex = atlasIndex;
						q1.TransformIndex = transformIndex;
					}

					const size_t q2Index = geometryData.QuadVertices.size();
					{
						auto& q2 = geometryData.QuadVertices.emplace_back();
						q2 = geometryData.QuadVertices[q1Index];
						q2.Position = glm::vec2(pl, pt);
						q2.TexCoord = { l, t };
					}

					const size_t q3Index = geometryData.QuadVertices.size();
					{
						auto& q3 = geometryData.QuadVertices.emplace_back();
						q3 = geometryData.QuadVertices[q1Index];
						q3.Position = glm::vec2(pr, pt);
						q3.TexCoord = { r, t };
					}


					const size_t q4Index = geometryData.QuadVertices.size();
					{
						auto& q4 = geometryData.QuadVertices.emplace_back();
						q4 = geometryData.QuadVertices[q1Index];
						q4.Position = glm::vec2(pr, pb);
						q4.TexCoord = { r, b };
					}

					// back face, they have NON inverted normals
					geometryData.QuadVertices.emplace_back() = geometryData.QuadVertices[q1Index];
					geometryData.QuadVertices.emplace_back() = geometryData.QuadVertices[q2Index];
					geometryData.QuadVertices.emplace_back() = geometryData.QuadVertices[q3Index];
					geometryData.QuadVertices.emplace_back() = geometryData.QuadVertices[q4Index];

					if (i + 1 < textSize)
					{
						double advance = glyph->getAdvance();
						fontGeometry->getAdvance(advance, character, text[i + 1]);
						x += fsScale * advance + component.KerningOffset;
					}
				}
			}
		}
	}

	static void ProcessUnlitComponents(const std::vector<UnlitTextComponentData>& textComponents, std::unordered_map<Ref<Texture2D>, uint32_t>& fontAtlases, UnlitTextGeometryData& geometryData, uint32_t& atlasCurrentIndex)
	{
		if (textComponents.empty())
			return;

		for (auto& component : textComponents)
		{
			const auto& fontGeometry = component.Font->GetFontGeometry();
			const auto& metrics = fontGeometry->getMetrics();
			const auto& text = component.Text;
			const auto& atlas = component.Font->GetAtlas();
			uint32_t atlasIndex = atlasCurrentIndex;
			auto it = fontAtlases.find(atlas);
			if (it == fontAtlases.end())
			{
				if (fontAtlases.size() == RendererConfig::MaxTextures) // Can't be more than EG_MAX_TEXTURES
				{
					EG_CORE_CRITICAL("Not enough samplers to store all font atlases! Max supported fonts: {}", RendererConfig::MaxTextures);
					atlasIndex = 0;
				}
				else
					fontAtlases.emplace(atlas, atlasCurrentIndex++);
			}
			else
				atlasIndex = it->second;

			const double spaceAdvance = fontGeometry->getGlyph(' ')->getAdvance();
			std::vector<int> nextLines = Font::GetNextLines(metrics, fontGeometry, text, spaceAdvance,
				component.LineHeightOffset, component.KerningOffset, component.MaxWidth);

			{
				double x = 0.0;
				double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
				double y = 0.0;
				const size_t textSize = text.size();
				for (int i = 0; i < textSize; i++)
				{
					char32_t character = text[i];
					if (character == '\n' || Font::NextLine(i, nextLines))
					{
						x = 0;
						y -= fsScale * metrics.lineHeight + component.LineHeightOffset;
						continue;
					}

					const bool bIsTab = character == '\t';
					if (character == ' ' || bIsTab)
					{
						character = ' '; // treat tabs as spaces
						double advance = spaceAdvance;
						if (i < textSize - 1)
						{
							char32_t nextCharacter = text[i + 1];
							if (nextCharacter == '\t')
								nextCharacter = ' ';
							fontGeometry->getAdvance(advance, character, nextCharacter);
						}

						// Tab is 4 spaces
						x += (fsScale * advance + component.KerningOffset) * (bIsTab ? 4.0 : 1.0);
						continue;
					}

					auto glyph = fontGeometry->getGlyph(character);
					if (!glyph)
						glyph = fontGeometry->getGlyph('?');
					if (!glyph)
						continue;

					double l, b, r, t;
					glyph->getQuadAtlasBounds(l, b, r, t);

					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);

					pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
					pl += x, pb += y, pr += x, pt += y;

					double texelWidth = 1. / atlas->GetWidth();
					double texelHeight = 1. / atlas->GetHeight();
					l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

					const size_t q1Index = geometryData.QuadVertices.size();
					{
						auto& q1 = geometryData.QuadVertices.emplace_back();
						q1.Position = glm::vec2(pl, pb);
						q1.Color = component.Color;
						q1.TexCoord = { l, b };
						q1.EntityID = component.EntityID;
						q1.AtlasIndex = atlasIndex;
						q1.TransformIndex = component.TransformIndex;
					}

					const size_t q2Index = geometryData.QuadVertices.size();
					{
						auto& q2 = geometryData.QuadVertices.emplace_back();
						q2 = geometryData.QuadVertices[q1Index];
						q2.Position = glm::vec2(pl, pt);
						q2.TexCoord = { l, t };
					}

					const size_t q3Index = geometryData.QuadVertices.size();
					{
						auto& q3 = geometryData.QuadVertices.emplace_back();
						q3 = geometryData.QuadVertices[q1Index];
						q3.Position = glm::vec2(pr, pt);
						q3.TexCoord = { r, t };
					}

					const size_t q4Index = geometryData.QuadVertices.size();
					{
						auto& q4 = geometryData.QuadVertices.emplace_back();
						q4 = geometryData.QuadVertices[q1Index];
						q4.Position = glm::vec2(pr, pb);
						q4.TexCoord = { r, b };
					}

					if (i + 1 < textSize)
					{
						double advance = glyph->getAdvance();
						fontGeometry->getAdvance(advance, character, text[i + 1]);
						x += fsScale * advance + component.KerningOffset;
					}
				}
			}
		}
	}

	void GeometryManagerTask::SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<LitTextComponentData> opaqueLitDatas;
		std::vector<LitTextComponentData> opaqueLitNotCastingShadowDatas;
		std::vector<LitTextComponentData> maskedLitDatas;
		std::vector<LitTextComponentData> maskedLitNotCastingShadowDatas;
		std::vector<LitTextComponentData> translucentLitDatas;
		std::vector<LitTextComponentData> translucentLitNotCastingShadowDatas;
		std::vector<UnlitTextComponentData> unlitDatas;
		std::vector<UnlitTextComponentData> unlitNotCastingShadowDatas;
		std::unordered_map<uint32_t, uint64_t> tempTransformsIndices; // EntityID -> uint64_t (index to m_TextTransformIndices)
		std::vector<glm::mat4> tempTransforms;

		opaqueLitDatas.reserve(texts.size());
		opaqueLitNotCastingShadowDatas.reserve(texts.size());
		maskedLitDatas.reserve(texts.size());
		maskedLitNotCastingShadowDatas.reserve(texts.size());
		translucentLitDatas.reserve(texts.size());
		translucentLitNotCastingShadowDatas.reserve(texts.size());
		unlitDatas.reserve(texts.size());
		unlitNotCastingShadowDatas.reserve(texts.size());
		tempTransforms.reserve(texts.size());
		tempTransformsIndices.reserve(texts.size());

		for (auto& text : texts)
		{
			if (!text->GetFont())
				continue;

			const uint32_t transformIndex = (uint32_t)tempTransforms.size();
			if (text->IsLit())
			{
				LitTextComponentData* data = nullptr;
				// Emplace into correct container
				switch (text->GetBlendMode())
				{
					case Material::BlendMode::Opaque:
						data = &(text->DoesCastShadows() ? opaqueLitDatas.emplace_back() : opaqueLitNotCastingShadowDatas.emplace_back());
						break;
					case Material::BlendMode::Translucent:
						data = &(text->DoesCastShadows() ? translucentLitDatas.emplace_back() : translucentLitNotCastingShadowDatas.emplace_back());
						break;
					case Material::BlendMode::Masked:
						data = &(text->DoesCastShadows() ? maskedLitDatas.emplace_back() : maskedLitNotCastingShadowDatas.emplace_back());
						break;

					default: EG_ASSERT(false);
				}

				data->Text = ToUTF32(text->GetText());
				data->Font = text->GetFont();
				data->Albedo = text->GetAlbedoColor();
				data->Emissive = text->GetEmissiveColor();
				data->Roughness = glm::max(EG_MIN_ROUGHNESS, text->GetRoughness());
				data->Metallness = text->GetMetallness();
				data->AO = text->GetAO();
				data->EntityID = text->Parent.GetID();
				data->LineHeightOffset = text->GetLineSpacing();
				data->KerningOffset = text->GetKerning();
				data->MaxWidth = text->GetMaxWidth();
				data->TransformIndex = transformIndex;
				data->Opacity = text->GetOpacity();
				data->OpacityMask = text->GetOpacityMask();
			}
			else
			{
				auto& data = text->DoesCastShadows() ? unlitDatas.emplace_back() : unlitNotCastingShadowDatas.emplace_back();
				data.TransformIndex = transformIndex;
				data.Text = ToUTF32(text->GetText());
				data.Font = text->GetFont();
				data.Color = text->GetColor();
				data.EntityID = text->Parent.GetID();
				data.LineHeightOffset = text->GetLineSpacing();
				data.KerningOffset = text->GetKerning();
				data.MaxWidth = text->GetMaxWidth();
			}
			tempTransformsIndices.emplace(text->Parent.GetID(), transformIndex);
			tempTransforms.emplace_back(Math::ToTransformMatrix(text->GetWorldTransform()));
		}

		RenderManager::Submit([this, opaqueTextComponents = std::move(opaqueLitDatas), opaqueNotCastingShadowsTextComponents = std::move(opaqueLitNotCastingShadowDatas),
			maskedTextComponents = std::move(maskedLitDatas), maskedNotCastingShadowsTextComponents = std::move(maskedLitNotCastingShadowDatas),
			translucentTextComponents = std::move(translucentLitDatas), translucentNotCastingShadowsTextComponents = std::move(translucentLitNotCastingShadowDatas),
			unlitNotCastingShadowsTextComponents = std::move(unlitNotCastingShadowDatas),
			unlitTextComponents = std::move(unlitDatas), transforms = std::move(tempTransforms), transformsIndices = std::move(tempTransformsIndices)](Ref<CommandBuffer>&) mutable
		{
			bUploadTextQuads = true;
			bUploadTextTransforms = true;

			m_OpaqueLitTextData.QuadVertices.clear();
			m_OpaqueLitNonShadowTextData.QuadVertices.clear();
			m_MaskedLitTextData.QuadVertices.clear();
			m_MaskedLitNonShadowTextData.QuadVertices.clear();
			m_TranslucentLitTextData.QuadVertices.clear();
			m_TranslucentNonShadowLitTextData.QuadVertices.clear();
			m_UnlitTextData.QuadVertices.clear();
			m_UnlitNonShadowTextData.QuadVertices.clear();

			m_FontAtlases.clear();
			m_Atlases.clear();
			m_TextTransforms = std::move(transforms);
			m_TextTransformIndices = std::move(transformsIndices);

			uint32_t atlasCurrentIndex = 0;
			ProcessLitComponents(opaqueTextComponents, m_FontAtlases, m_OpaqueLitTextData, atlasCurrentIndex);
			ProcessLitComponents(opaqueNotCastingShadowsTextComponents, m_FontAtlases, m_OpaqueLitNonShadowTextData, atlasCurrentIndex);
			ProcessLitComponents(maskedTextComponents, m_FontAtlases, m_MaskedLitTextData, atlasCurrentIndex);
			ProcessLitComponents(maskedNotCastingShadowsTextComponents, m_FontAtlases, m_MaskedLitNonShadowTextData, atlasCurrentIndex);
			ProcessLitComponents(translucentTextComponents, m_FontAtlases, m_TranslucentLitTextData, atlasCurrentIndex);
			ProcessLitComponents(translucentNotCastingShadowsTextComponents, m_FontAtlases, m_TranslucentNonShadowLitTextData, atlasCurrentIndex);
			ProcessUnlitComponents(unlitTextComponents, m_FontAtlases, m_UnlitTextData, atlasCurrentIndex);
			ProcessUnlitComponents(unlitNotCastingShadowsTextComponents, m_FontAtlases, m_UnlitNonShadowTextData, atlasCurrentIndex);

			m_Atlases.resize(atlasCurrentIndex);
			for (auto& atlas : m_FontAtlases)
				m_Atlases[atlas.second] = atlas.first;
		});
	}
	
	void GeometryManagerTask::SetTransforms(const std::set<const TextComponent*>& texts)
	{
		if (texts.empty())
			return;

		struct Data
		{
			glm::mat4 TransformMatrix;
			uint32_t ID;
		};

		std::vector<Data> updateData;
		updateData.reserve(texts.size());

		for (auto& text : texts)
			updateData.push_back({ Math::ToTransformMatrix(text->GetWorldTransform()), text->Parent.GetID() });

		RenderManager::Submit([this, data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			for (auto& text : data)
			{
				auto it = m_TextTransformIndices.find(text.ID);
				if (it != m_TextTransformIndices.end())
				{
					m_TextTransforms[it->second] = text.TransformMatrix;
					m_TextUploadSpecificTransforms.push_back(it->second);
					bUploadTextSpecificTransforms = true;
				}
			}
		});
	}

	void GeometryManagerTask::UploadTexts(const Ref<CommandBuffer>& cmd, LitTextGeometryData& textsData)
	{
		if (textsData.QuadVertices.empty())
			return;

		auto& vb = textsData.VertexBuffer;
		auto& ib = textsData.IndexBuffer;
		auto& quads = textsData.QuadVertices;
		using TextVertexType = LitTextQuadVertex;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = quads.size() * sizeof(TextVertexType);
		const size_t currentIndexSize = (quads.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(TextVertexType);
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

	void GeometryManagerTask::UploadTexts(const Ref<CommandBuffer>& cmd, UnlitTextGeometryData& textsData)
	{
		if (textsData.QuadVertices.empty())
			return;

		auto& vb = textsData.VertexBuffer;
		auto& ib = textsData.IndexBuffer;
		auto& quads = textsData.QuadVertices;
		using TextVertexType = UnlitTextQuadVertex;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = quads.size() * sizeof(TextVertexType);
		const size_t currentIndexSize = (quads.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(TextVertexType);
			newSize += alignment - (newSize % alignment);

			vb->Resize(newSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, ib->GetSize() * 3 / 2);
			constexpr size_t alignment = 6 * sizeof(Index);
			newSize += alignment - (newSize % alignment);

			ib->Resize(newSize);
			UploadIndexBufferOneSided(cmd, ib);
		}

		cmd->Write(vb, quads.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}
}
