#include "egpch.h"
#include "GeometryManagerTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Animation/AnimationSystem.h"

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

			cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			cmd->StorageBufferBarrier(gpuBuffer);

			if (bMotionRequired && bTransformBufferGarbage)
				cmd->CopyBuffer(gpuBuffer, prevGpuBuffer, 0, 0, gpuBuffer->GetSize());
		}
#if EG_UPLOAD_ONLY_REQUIRED_TRANSFORMS
		else
		{
			// If uploading specific transforms, copy data to "Prev Transforms" and the update current transforms buffer
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

		// Create Skeletal Mesh buffers
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

			m_OpaqueSkeletalMeshesData.VertexBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_VertexBuffer_Opaque");
			m_OpaqueSkeletalMeshesData.InstanceBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_InstanceVertexBuffer_Opaque");
			m_OpaqueSkeletalMeshesData.IndexBuffer = Buffer::Create(indexSpecs, "SkeletalMeshes_IndexBuffer_Opaque");

			m_TranslucentSkeletalMeshesData.VertexBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_VertexBuffer_Translucent");
			m_TranslucentSkeletalMeshesData.InstanceBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_InstanceVertexBuffer_Translucent");
			m_TranslucentSkeletalMeshesData.IndexBuffer = Buffer::Create(indexSpecs, "SkeletalMeshes_IndexBuffer_Translucent");

			m_MaskedSkeletalMeshesData.VertexBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_VertexBuffer_Masked");
			m_MaskedSkeletalMeshesData.InstanceBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_InstanceVertexBuffer_Masked");
			m_MaskedSkeletalMeshesData.IndexBuffer = Buffer::Create(indexSpecs, "SkeletalMeshes_IndexBuffer_Masked");

			m_SkeletalMeshesTransformsBuffer = Buffer::Create(transformsBufferSpecs, "SkeletalMeshes_TransformsBuffer");
		}

		// Create Sprite buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = 1; // Used 1 so that we don't allocate a lot of data here, but rather do it as needed
			vertexSpecs.Layout = BufferReadAccess::Vertex;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = 1; // Used 1 so that we don't allocate a lot of data here, but rather do it as needed
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
		}
	
		// Create Text buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = 1; // Used 1 so that we don't allocate a lot of data here, but rather do it as needed
			vertexSpecs.Layout = BufferReadAccess::Vertex;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = 1; // Used 1 so that we don't allocate a lot of data here, but rather do it as needed
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

			m_UnlitTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Unlit_VertexBuffer");
			m_UnlitTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Unlit_IndexBuffer");
			m_UnlitNonShadowTextData.VertexBuffer = Buffer::Create(vertexSpecs, "Text_Unlit_VertexBuffer_NotCastingShadow");
			m_UnlitNonShadowTextData.IndexBuffer = Buffer::Create(indexSpecs, "Text_Unlit_IndexBuffer_NotCastingShadow");

			m_TextTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_TransformsBuffer");
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
					EG_GPU_TIMING_SCOPED(cmd, "Static Meshes. Upload vertex & index buffers");
					EG_CPU_TIMING_SCOPED("Static Meshes. Upload vertex & index buffers");
					UploadMeshes(cmd, m_OpaqueMeshesData, m_OpaqueMeshes);
					UploadMeshes(cmd, m_TranslucentMeshesData, m_TranslucentMeshes);
					UploadMeshes(cmd, m_MaskedMeshesData, m_MaskedMeshes);
				}
			}
			const bool bTransformBufferGarbage = bUploadMeshes;
			UploadTransforms(cmd, m_MeshTransforms, m_MeshesTransformsBuffer, m_MeshesPrevTransformsBuffer, m_MeshUploadSpecificTransforms,
				&bUploadMeshTransforms, &bUploadMeshSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Static Meshes. Upload Transforms buffer");

			bUploadMeshes = false;
		}

		// Skeletal Meshes
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Skeletal Meshes");
			EG_CPU_TIMING_SCOPED("Process Skeletal Meshes");

			if (bUploadSkeletalMeshes || bMaterialsChanged)
			{
				SortSkeletalMeshes();
				{
					EG_GPU_TIMING_SCOPED(cmd, "Skeletal Meshes. Upload vertex & index buffers");
					EG_CPU_TIMING_SCOPED("Skeletal Meshes. Upload vertex & index buffers");
					UploadSkeletalMeshes(cmd, m_OpaqueSkeletalMeshesData, m_OpaqueSkeletalMeshes);
					UploadSkeletalMeshes(cmd, m_TranslucentSkeletalMeshesData, m_TranslucentSkeletalMeshes);
					UploadSkeletalMeshes(cmd, m_MaskedSkeletalMeshesData, m_MaskedSkeletalMeshes);
				}
			}
			const bool bTransformBufferGarbage = bUploadSkeletalMeshes;
			UploadTransforms(cmd, m_SkeletalMeshTransforms, m_SkeletalMeshesTransformsBuffer, m_SkeletalMeshesPrevTransformsBuffer, m_SkeletalMeshUploadSpecificTransforms,
				&bUploadSkeletalMeshTransforms, &bUploadSkeletalMeshSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Skeletal Meshes. Upload Transforms buffer");

			UploadAnimationTransforms(cmd, bTransformBufferGarbage);

			bUploadSkeletalMeshes = false;
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

			if (bUploadTextQuads || bMaterialsChanged)
			{
				if (!bUploadTextQuads)
					SortLitTexts(); // Required only when materials were changed. Because they're already sorted if `bUploadTextQuads` == true

				EG_GPU_TIMING_SCOPED(cmd, "Texts. Upload vertex & index buffers");
				EG_CPU_TIMING_SCOPED("Texts. Upload vertex & index buffers");

				UploadTexts(cmd, m_OpaqueLitTextData);
				UploadTexts(cmd, m_OpaqueLitNonShadowTextData);
				UploadTexts(cmd, m_MaskedLitTextData);
				UploadTexts(cmd, m_MaskedLitNonShadowTextData);
				UploadTexts(cmd, m_TranslucentLitTextData);
				UploadTexts(cmd, m_TranslucentNonShadowLitTextData);

				if (bUploadTextQuads) // Don't need to reupload if just materials have changes since unlit ones don't have materials
				{
					UploadTexts(cmd, m_UnlitTextData);
					UploadTexts(cmd, m_UnlitNonShadowTextData);
				}
			}
			const bool bTransformBufferGarbage = bUploadTextQuads;
			UploadTransforms(cmd, m_TextTransforms, m_TextTransformsBuffer, m_TextPrevTransformsBuffer, m_TextUploadSpecificTransforms,
				&bUploadTextTransforms, &bUploadTextSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Texts. Upload Transforms buffer");

			bUploadTextQuads = false;
		}
	}

	void GeometryManagerTask::UploadAnimationTransforms(const Ref<CommandBuffer>& cmd, bool bTransformsGarbage)
	{
		auto& animTransforms = m_AnimationTransforms;
		auto& animTransformsBuffers = m_AnimationTransformsBuffers;
		auto& prevAnimTransformsBuffers = m_AnimationPrevTransformsBuffers;

		// Upload anim transforms
		{
			EG_GPU_TIMING_SCOPED(cmd, "3D Skeletal Meshes. Process and upload animations");
			EG_CPU_TIMING_SCOPED("3D Skeletal Meshes. Process and upload animations");

			const auto& finalAnimTransforms = m_Renderer.GetMeshesAnimationTransforms();
			for (auto& [meshKey, meshData] : m_SkeletalMeshes)
			{
				const auto& mesh = meshKey.Mesh;
				for (auto& data : meshData.Datas)
				{
					// It doesn't matter which index we take, since `AnimTransformIndex` and `ObjectID` are going to be the same
					const auto& instanceData = data.InstanceDatas[0];
					auto& transforms = animTransforms[instanceData.AnimTransformIndex];
					auto it = finalAnimTransforms.find(instanceData.ObjectID);
					EG_ASSERT(it != finalAnimTransforms.end());
					transforms = it->second;

					bool bGarbage = bTransformsGarbage;

					auto& animTransformsBuffer = animTransformsBuffers[instanceData.AnimTransformIndex];
					const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
					if (!animTransformsBuffer || (animTransformsBuffer == Buffer::Dummy))
					{
						BufferSpecifications transformsBufferSpecs;
						transformsBufferSpecs.Size = currentBufferSize;
						transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
						transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;
						animTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_AnimationTransformsBuffer_#" + std::to_string(instanceData.AnimTransformIndex));
						bGarbage = true;
					}
					else if (currentBufferSize > animTransformsBuffer->GetSize())
					{
						size_t newSize = (currentBufferSize * 3) / 2;
						animTransformsBuffer->Resize(newSize);
						bGarbage = true;
					}

					Ref<Buffer>* prevAnimTransformsBuffer = nullptr;
					if (bMotionRequired)
					{
						auto& prevAnimTransformsBufferRef = prevAnimTransformsBuffers[instanceData.AnimTransformIndex];
						if (!prevAnimTransformsBufferRef || (prevAnimTransformsBufferRef == Buffer::Dummy))
						{
							BufferSpecifications transformsBufferSpecs;
							transformsBufferSpecs.Size = currentBufferSize;
							transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
							transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
							prevAnimTransformsBufferRef = Buffer::Create(transformsBufferSpecs, "Meshes_AnimationPrevTransformsBuffer_#" + std::to_string(instanceData.AnimTransformIndex));
							bGarbage = true;
						}
						else if (currentBufferSize > prevAnimTransformsBufferRef->GetSize())
						{
							size_t newSize = (currentBufferSize * 3) / 2;
							prevAnimTransformsBufferRef->Resize(newSize);
							bGarbage = true;
						}
						prevAnimTransformsBuffer = &prevAnimTransformsBufferRef;
					}

					if (bMotionRequired && !bGarbage)
					{
						EG_CORE_ASSERT(animTransformsBuffer->GetSize() == (*prevAnimTransformsBuffer)->GetSize());
						cmd->CopyBuffer(animTransformsBuffer, *prevAnimTransformsBuffer, 0, 0, (*prevAnimTransformsBuffer)->GetSize());
					}

					cmd->Write(animTransformsBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

					if (bMotionRequired && bGarbage)
						cmd->Write(*prevAnimTransformsBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

					cmd->StorageBufferBarrier(animTransformsBuffer);
					if (bMotionRequired)
						cmd->StorageBufferBarrier(*prevAnimTransformsBuffer);
				}
			}
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
			m_SkeletalMeshesPrevTransformsBuffer.reset();
			m_SpritesPrevTransformsBuffer.reset();
			m_TextPrevTransformsBuffer.reset();
			m_AnimationPrevTransformsBuffers.clear();
		}
		else
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_MeshesTransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_MeshesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_SkeletalMeshesTransformsBuffer->GetSize();
			m_SkeletalMeshesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "SkeletalMeshes_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_SpritesTransformsBuffer->GetSize();
			m_SpritesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_TextTransformsBuffer->GetSize();
			m_TextPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_PrevTransformsBuffer");

			m_AnimationPrevTransformsBuffers.resize(m_AnimationTransformsBuffers.size());
		}
	}

	// ---------- Static Meshes ----------
	void GeometryManagerTask::SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty)
	{
		if (!bDirty)
			return;

		std::unordered_map<MeshKey, MeshDatas> tempMeshes;
		std::unordered_map<uint32_t, uint64_t> meshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)
		std::vector<glm::mat4> tempMeshTransforms;

		tempMeshes.reserve(meshes.size());
		tempMeshTransforms.reserve(meshes.size());
		meshTransformIndices.reserve(meshes.size());

		uint32_t meshIndex = 0;
		for (auto& comp : meshes)
		{
			const auto& meshAsset = comp->GetMeshAsset();
			if (!meshAsset)
				continue;

			const Ref<Eagle::StaticMesh>& staticMesh = meshAsset->GetMesh();
			if (!staticMesh || !staticMesh->IsValid())
				continue;

			const uint32_t materialsCount = comp->GetMaterialsSlotsCount();
			const uint32_t meshID = comp->Parent.GetID();
			auto& instanceData = tempMeshes[{staticMesh, meshAsset->GetGUID(), comp->DoesCastShadows()}];
			auto& meshData = instanceData.Datas.emplace_back();
			for (uint32_t i = 0; i < materialsCount; ++i)
			{
				const auto& materialAsset = comp->GetMaterialAsset(i);
				meshData.Materials.push_back(materialAsset ? materialAsset->GetMaterial() : nullptr);
				auto& meshInstanceData = meshData.InstanceDatas.emplace_back();
				meshInstanceData.TransformIndex = meshIndex;
				meshInstanceData.ObjectID = meshID;
				// meshInstanceData.MaterialIndex is set later during the update
			}

			tempMeshTransforms.push_back(Math::ToTransformMatrix(comp->GetWorldTransform()));
			meshTransformIndices.emplace(meshID, meshIndex);
			++meshIndex;
		}

		RenderManager::Submit([task = shared_from_this(), meshes = std::move(tempMeshes),
			transforms = std::move(tempMeshTransforms),
			transformIndices = std::move(meshTransformIndices)](Ref<CommandBuffer>&) mutable
			{
				auto thisRef = Cast<GeometryManagerTask>(task);
				thisRef->m_Meshes = std::move(meshes);
				thisRef->m_MeshTransforms = std::move(transforms);
				thisRef->m_MeshTransformIndices = std::move(transformIndices);

				thisRef->bUploadMeshes = true;
				thisRef->bUploadMeshTransforms = true;
			});
	}
	
	void GeometryManagerTask::SetTransforms(const std::unordered_set<const StaticMeshComponent*>& meshes)
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

		RenderManager::Submit([task = shared_from_this(), data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			for (auto& mesh : data)
			{
				auto it = thisRef->m_MeshTransformIndices.find(mesh.ID);
				if (it != thisRef->m_MeshTransformIndices.end())
				{
					thisRef->m_MeshTransforms[it->second] = mesh.TransformMatrix;
					thisRef->m_MeshUploadSpecificTransforms.push_back(it->second);
					thisRef->bUploadMeshSpecificTransforms = true;
				}
			}
		});
	}
	
	void GeometryManagerTask::SortMeshes()
	{
		EG_CPU_TIMING_SCOPED("Sort static meshes based on Blend Mode");

		m_OpaqueMeshes.clear();
		m_TranslucentMeshes.clear();
		m_MaskedMeshes.clear();

		for (auto& [mesh, datas] : m_Meshes)
			for (auto& data : datas.Datas)
			{
				const size_t materialsCount = data.Materials.size();
				for (size_t i = 0; i < materialsCount; ++i)
					data.InstanceDatas[i].MaterialIndex = MaterialSystem::GetMaterialIndex(data.Materials[i]);

				struct MeshDataPerBlendMode
				{
					MeshData Data;
					std::vector<uint32_t> MaterialSlots;
				};
				std::array<MeshDataPerBlendMode, Material::MaxBlendModes> datasPerBlendMode;
				for (size_t i = 0; i < materialsCount; ++i)
				{
					const Material::BlendMode blendMode = data.Materials[i] ? data.Materials[i]->GetBlendMode() : Material::BlendMode::Opaque;
					auto& perBlendData = datasPerBlendMode[uint32_t(blendMode)];
					perBlendData.Data.InstanceDatas.push_back(data.InstanceDatas[i]);
					perBlendData.MaterialSlots.push_back(uint32_t(i));
				}

				for (uint32_t i = 0; i < Material::MaxBlendModes; ++i)
				{
					const Material::BlendMode blendMode = Material::BlendMode(i);
					if (datasPerBlendMode[i].Data.InstanceDatas.size())
					{
						switch (blendMode)
						{
							case Material::BlendMode::Opaque:
							{
								auto& meshes = m_OpaqueMeshes[mesh];
								meshes.Datas.push_back(data);
								meshes.MaterialSlots = std::move(datasPerBlendMode[i].MaterialSlots);
								break;
							}
							case Material::BlendMode::Translucent:
							{
								auto& meshes = m_TranslucentMeshes[mesh];
								meshes.Datas.push_back(data);
								meshes.MaterialSlots = std::move(datasPerBlendMode[i].MaterialSlots);
								break;
							}
							case Material::BlendMode::Masked:
							{
								auto& meshes = m_MaskedMeshes[mesh];
								meshes.Datas.push_back(data);
								meshes.MaterialSlots = std::move(datasPerBlendMode[i].MaterialSlots);
								break;
							}
						}
					}
				}
			}
	}

	void GeometryManagerTask::UploadMeshes(const Ref<CommandBuffer>& cmd, MeshGeometryData& meshData, const std::unordered_map<MeshKey, MeshDatas>& meshes)
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
			const uint32_t materialsCount = meshKey.Mesh->GetMaterialSlotsCount();
			currentVertexSize += meshKey.Mesh->GetVerticesCount() * sizeof(Vertex);
			for (uint32_t i = 0; i < materialsCount; ++i)
				currentIndexSize += meshKey.Mesh->GetIndicesCount(i) * sizeof(Index);
			meshesCount += datas.Datas.size() * materialsCount;
		}
		const size_t currentInstanceVertexSize = meshesCount * sizeof(PerInstanceData);

		if (currentVertexSize > vb->GetSize())
			vb->Resize((currentVertexSize * 3) / 2);
		if (currentInstanceVertexSize > ivb->GetSize())
			ivb->Resize((currentInstanceVertexSize * 3) / 2);
		if (currentIndexSize > ib->GetSize())
			ib->Resize((currentIndexSize * 3) / 2);

		meshData.Vertices.clear();
		meshData.Indices.clear();
		meshData.InstanceVertices.clear();
		meshData.Vertices.reserve(currentVertexSize / sizeof(Vertex));
		meshData.InstanceVertices.reserve(currentInstanceVertexSize / sizeof(PerInstanceData));
		meshData.Indices.reserve(currentIndexSize / sizeof(Index));

		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t materialsCount = meshKey.Mesh->GetMaterialSlotsCount();
			const auto& meshVertices = meshKey.Mesh->GetVertices();
			meshData.Vertices.insert(meshData.Vertices.end(), meshVertices.begin(), meshVertices.end());

			for (uint32_t i = 0; i < materialsCount; ++i)
			{
				const auto& meshIndices = meshKey.Mesh->GetIndices(i);
				meshData.Indices.insert(meshData.Indices.end(), meshIndices.begin(), meshIndices.end());
			}

			// Iterate over every mesh in the batch.
			// Append instance data in the pattern of `Structure of Arrays`.
			// For example, [0, 0, 0, 1, 1, 1] rather than [0, 1, 0, 1, 0, 1]
			for (uint32_t i = 0; i < materialsCount; ++i)
				for (auto& data : datas.Datas)
					meshData.InstanceVertices.push_back(data.InstanceDatas[i]);
		}

		cmd->Write(vb, meshData.Vertices.data(), meshData.Vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ivb, meshData.InstanceVertices.data(), currentInstanceVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ib, meshData.Indices.data(), meshData.Indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->Barrier(vb);
		cmd->Barrier(ivb);
		cmd->Barrier(ib);
	}

	// ---------- Skeletal Meshes ----------
	void GeometryManagerTask::SetSkeletalMeshes(const std::vector<SkeletalMeshComponent*>& meshes, bool bDirty)
	{
		if (!bDirty)
			return;

		std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas> tempMeshes;
		std::unordered_map<uint32_t, uint64_t> meshTransformIndices; // EntityID -> uint64_t (index to m_SkeletalMeshTransforms)
		std::vector<glm::mat4> tempMeshTransforms;

		tempMeshes.reserve(meshes.size());
		tempMeshTransforms.reserve(meshes.size());
		meshTransformIndices.reserve(meshes.size());

		uint32_t meshIndex = 0;
		for (auto& comp : meshes)
		{
			const auto& meshAsset = comp->GetMeshAsset();
			if (!meshAsset)
				continue;

			const Ref<SkeletalMesh>& skeletalMesh = meshAsset->GetMesh();
			if (!skeletalMesh || !skeletalMesh->IsValid())
				continue;

			const uint32_t materialsCount = comp->GetMaterialsSlotsCount();
			const uint32_t meshID = comp->Parent.GetID();
			auto& instanceData = tempMeshes[{skeletalMesh, meshAsset->GetGUID(), comp->DoesCastShadows()}];
			auto& meshData = instanceData.Datas.emplace_back();
			for (uint32_t i = 0; i < materialsCount; ++i)
			{
				const auto& materialAsset = comp->GetMaterialAsset(i);
				meshData.Materials.push_back(materialAsset ? materialAsset->GetMaterial() : nullptr);
				auto& instanceData = meshData.InstanceDatas.emplace_back();
				instanceData.TransformIndex = meshIndex;
				instanceData.ObjectID = meshID;
				// instanceData.MaterialIndex is set later during the update
				// instanceData.AnimTransformIndex is set later during the update
			}

			tempMeshTransforms.push_back(Math::ToTransformMatrix(comp->GetWorldTransform()));
			meshTransformIndices.emplace(meshID, meshIndex);
			++meshIndex;
		}

		RenderManager::Submit([task = shared_from_this(), meshes = std::move(tempMeshes),
			transforms = std::move(tempMeshTransforms),
			transformIndices = std::move(meshTransformIndices)](Ref<CommandBuffer>&) mutable
			{
				auto thisRef = Cast<GeometryManagerTask>(task);
				thisRef->m_SkeletalMeshes = std::move(meshes);
				thisRef->m_SkeletalMeshTransforms = std::move(transforms);
				thisRef->m_SkeletalMeshTransformIndices = std::move(transformIndices);

				thisRef->bUploadSkeletalMeshes = true;
				thisRef->bUploadSkeletalMeshTransforms = true;
			});
	}

	void GeometryManagerTask::SetTransforms(const std::unordered_set<const SkeletalMeshComponent*>& meshes)
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

		RenderManager::Submit([task = shared_from_this(), data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			for (auto& mesh : data)
			{
				auto it = thisRef->m_SkeletalMeshTransformIndices.find(mesh.ID);
				if (it != thisRef->m_SkeletalMeshTransformIndices.end())
				{
					thisRef->m_SkeletalMeshTransforms[it->second] = mesh.TransformMatrix;
					thisRef->m_SkeletalMeshUploadSpecificTransforms.push_back(it->second);
					thisRef->bUploadSkeletalMeshSpecificTransforms = true;
				}
			}
		});
	}

	void GeometryManagerTask::SortSkeletalMeshes()
	{
		EG_CPU_TIMING_SCOPED("Sort skeletal meshes based on Blend Mode");

		m_OpaqueSkeletalMeshes.clear();
		m_TranslucentSkeletalMeshes.clear();
		m_MaskedSkeletalMeshes.clear();
		uint32_t animationsCount = 0u;

		for (auto& [mesh, datas] : m_SkeletalMeshes)
			for (auto& data : datas.Datas)
			{
				const size_t materialsCount = data.Materials.size();
				for (size_t i = 0; i < materialsCount; ++i)
				{
					data.InstanceDatas[i].AnimTransformIndex = animationsCount++;
					data.InstanceDatas[i].MaterialIndex = MaterialSystem::GetMaterialIndex(data.Materials[i]);
				}

				struct MeshDataPerBlendMode
				{
					SkeletalMeshData Data;
					std::vector<uint32_t> MaterialSlots;
				};
				std::array<MeshDataPerBlendMode, Material::MaxBlendModes> datasPerBlendMode;
				for (size_t i = 0; i < materialsCount; ++i)
				{
					const Material::BlendMode blendMode = data.Materials[i] ? data.Materials[i]->GetBlendMode() : Material::BlendMode::Opaque;
					auto& perBlendData = datasPerBlendMode[uint32_t(blendMode)];
					perBlendData.Data.InstanceDatas.push_back(data.InstanceDatas[i]);
					perBlendData.MaterialSlots.push_back(uint32_t(i));
				}

				for (uint32_t i = 0; i < Material::MaxBlendModes; ++i)
				{
					const Material::BlendMode blendMode = Material::BlendMode(i);
					if (datasPerBlendMode[i].Data.InstanceDatas.size())
					{
						switch (blendMode)
						{
							case Material::BlendMode::Opaque:
							{
								auto& meshes = m_OpaqueSkeletalMeshes[mesh];
								meshes.Datas.push_back(data);
								meshes.MaterialSlots = std::move(datasPerBlendMode[i].MaterialSlots);
								break;
							}
							case Material::BlendMode::Translucent:
							{
								auto& meshes = m_TranslucentSkeletalMeshes[mesh];
								meshes.Datas.push_back(data);
								meshes.MaterialSlots = std::move(datasPerBlendMode[i].MaterialSlots);
								break;
							}
							case Material::BlendMode::Masked:
							{
								auto& meshes = m_MaskedSkeletalMeshes[mesh];
								meshes.Datas.push_back(data);
								meshes.MaterialSlots = std::move(datasPerBlendMode[i].MaterialSlots);
								break;
							}
						}
					}
				}
			}

		m_AnimationTransforms.resize(animationsCount);
		if (m_AnimationTransformsBuffers.size() < animationsCount)
		{
			m_AnimationTransformsBuffers.resize(animationsCount);
			if (bMotionRequired)
				m_AnimationTransformsBuffers.resize(animationsCount);
		}
		else
		{
			// Set unused buffers to Dummy. It's required to update descriptors that point to unused buffers
			for (size_t i = animationsCount; i < m_AnimationTransformsBuffers.size(); ++i)
			{
				m_AnimationTransformsBuffers[i] = Buffer::Dummy;
				if (bMotionRequired)
					m_AnimationPrevTransformsBuffers[i] = Buffer::Dummy;
			}
		}
		
		if (!bMotionRequired)
			m_AnimationPrevTransformsBuffers.clear();
	}

	void GeometryManagerTask::UploadSkeletalMeshes(const Ref<CommandBuffer>& cmd, SkeletalMeshGeometryData& meshData, const std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas>& meshes)
	{
		if (meshes.empty())
			return;

		auto& vb = meshData.VertexBuffer;
		auto& ivb = meshData.InstanceBuffer;
		auto& ib = meshData.IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = 0;
		size_t currentIndexSize = 0;
		size_t meshesCount = 0;
		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t materialsCount = meshKey.Mesh->GetMaterialSlotsCount();
			currentVertexSize += meshKey.Mesh->GetVerticesCount() * sizeof(SkeletalVertex);
			for (uint32_t i = 0; i < materialsCount; ++i)
				currentIndexSize += meshKey.Mesh->GetIndicesCount(i) * sizeof(Index);
			meshesCount += datas.Datas.size() * materialsCount;
		}
		const size_t currentInstanceVertexSize = meshesCount * sizeof(SkeletalPerInstanceData);

		if (currentVertexSize > vb->GetSize())
			vb->Resize((currentVertexSize * 3) / 2);
		if (currentInstanceVertexSize > ivb->GetSize())
			ivb->Resize((currentInstanceVertexSize * 3) / 2);
		if (currentIndexSize > ib->GetSize())
			ib->Resize((currentIndexSize * 3) / 2);

		meshData.Vertices.clear();
		meshData.Indices.clear();
		meshData.InstanceVertices.clear();
		meshData.Vertices.reserve(currentVertexSize/ sizeof(SkeletalVertex));
		meshData.InstanceVertices.reserve(currentInstanceVertexSize / sizeof(SkeletalPerInstanceData));
		meshData.Indices.reserve(currentIndexSize / sizeof(Index));

		for (auto& [meshKey, datas] : meshes)
		{
			const uint32_t materialsCount = meshKey.Mesh->GetMaterialSlotsCount();
			const auto& meshVertices = meshKey.Mesh->GetVertices();
			meshData.Vertices.insert(meshData.Vertices.end(), meshVertices.begin(), meshVertices.end());

			for (uint32_t i = 0; i < materialsCount; ++i)
			{
				const auto& meshIndices = meshKey.Mesh->GetIndices(i);
				meshData.Indices.insert(meshData.Indices.end(), meshIndices.begin(), meshIndices.end());
			}

			// Iterate over every mesh in the batch.
			// Append instance data in the pattern of `Structure of Arrays`.
			// For example, [0, 0, 0, 1, 1, 1] rather than [0, 1, 0, 1, 0, 1]
			for (uint32_t i = 0; i < materialsCount; ++i)
				for (auto& data : datas.Datas)
					meshData.InstanceVertices.push_back(data.InstanceDatas[i]);
		}

		cmd->Write(vb, meshData.Vertices.data(), meshData.Vertices.size() * sizeof(SkeletalVertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ivb, meshData.InstanceVertices.data(), currentInstanceVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ib, meshData.Indices.data(), meshData.Indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->Barrier(vb);
		cmd->Barrier(ivb);
		cmd->Barrier(ib);
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
			const Material::BlendMode blendMode = sprite.Material ? sprite.Material->GetBlendMode() : Material::BlendMode::Opaque;
			switch (blendMode)
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
			const auto& materialAsset = sprite->GetMaterialAsset();

			auto& data = spritesData.emplace_back();
			data.Material = materialAsset ? materialAsset->GetMaterial() : nullptr;
			data.EntityID = sprite->Parent.GetID();
			data.bAtlas = sprite->IsAtlas();
			data.bCastsShadows = sprite->DoesCastShadows();
			if (data.bAtlas && data.Material)
			{
				if (const auto& asset = data.Material->GetAlbedoAsset())
				{
					const auto& atlas = asset->GetTexture();
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

		RenderManager::Submit([task = shared_from_this(), sprites = std::move(spritesData),
							   transformIndices = std::move(tempTransformIndices),
							   transforms = std::move(tempTransforms)](Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			thisRef->m_Sprites = std::move(sprites);
			thisRef->m_SpriteTransformIndices = std::move(transformIndices);
			thisRef->m_SpriteTransforms = std::move(transforms);

			thisRef->bUploadSprites = true;
			thisRef->bUploadSpritesTransforms = true;
		});
	}

	void GeometryManagerTask::SetTransforms(const std::unordered_set<const SpriteComponent*>& sprites)
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

		RenderManager::Submit([task = shared_from_this(), data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			for (auto& sprite : data)
			{
				auto it = thisRef->m_SpriteTransformIndices.find(sprite.ID);
				if (it != thisRef->m_SpriteTransformIndices.end())
				{
					thisRef->m_SpriteTransforms[it->second] = sprite.TransformMatrix;
					thisRef->m_SpriteUploadSpecificTransforms.push_back(it->second);
					thisRef->bUploadSpritesSpecificTransforms = true;
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
		Ref<Material> Material;
		std::u32string Text;
		Ref<Font> Font;
		int EntityID;
		float LineHeightOffset;
		float KerningOffset;
		float MaxWidth;
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

	static void ProcessLitComponents(const std::vector<LitTextComponentData>& textComponents, std::unordered_map<Ref<Texture2D>, uint32_t>& fontAtlases, LitTextGeometryData& geometryData,
		std::unordered_map<uint32_t, Ref<Material>>& textMaterials, uint32_t& atlasCurrentIndex)
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
				const uint32_t materialIndex = MaterialSystem::GetMaterialIndex(component.Material);

				textMaterials[materialIndex] = component.Material;

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
						q1.MaterialIndex = materialIndex;
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
			const auto& asset = text->GetFontAsset();
			if (!asset)
				continue;

			const uint32_t transformIndex = (uint32_t)tempTransforms.size();
			if (text->IsLit())
			{
				const auto& materialAsset = text->GetMaterialAsset();
				Ref<Material> material = materialAsset ? materialAsset->GetMaterial() : nullptr;
				LitTextComponentData* data = nullptr;
				Material::BlendMode blendMode = material ? material->GetBlendMode() : Material::BlendMode::Opaque;
				// Emplace into correct container
				switch (blendMode)
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

				data->Material = std::move(material);
				data->Text = ToUTF32(text->GetText());
				data->Font = asset->GetFont();
				data->EntityID = text->Parent.GetID();
				data->LineHeightOffset = text->GetLineSpacing();
				data->KerningOffset = text->GetKerning();
				data->MaxWidth = text->GetMaxWidth();
				data->TransformIndex = transformIndex;
			}
			else
			{
				auto& data = text->DoesCastShadows() ? unlitDatas.emplace_back() : unlitNotCastingShadowDatas.emplace_back();
				data.TransformIndex = transformIndex;
				data.Text = ToUTF32(text->GetText());
				data.Font = asset->GetFont();
				data.Color = text->GetColor();
				data.EntityID = text->Parent.GetID();
				data.LineHeightOffset = text->GetLineSpacing();
				data.KerningOffset = text->GetKerning();
				data.MaxWidth = text->GetMaxWidth();
			}
			tempTransformsIndices.emplace(text->Parent.GetID(), transformIndex);
			tempTransforms.emplace_back(Math::ToTransformMatrix(text->GetWorldTransform()));
		}

		RenderManager::Submit([task = shared_from_this(), opaqueTextComponents = std::move(opaqueLitDatas), opaqueNotCastingShadowsTextComponents = std::move(opaqueLitNotCastingShadowDatas),
			maskedTextComponents = std::move(maskedLitDatas), maskedNotCastingShadowsTextComponents = std::move(maskedLitNotCastingShadowDatas),
			translucentTextComponents = std::move(translucentLitDatas), translucentNotCastingShadowsTextComponents = std::move(translucentLitNotCastingShadowDatas),
			unlitNotCastingShadowsTextComponents = std::move(unlitNotCastingShadowDatas),
			unlitTextComponents = std::move(unlitDatas), transforms = std::move(tempTransforms), transformsIndices = std::move(tempTransformsIndices)](Ref<CommandBuffer>&) mutable
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			thisRef->bUploadTextQuads = true;
			thisRef->bUploadTextTransforms = true;

			thisRef->m_OpaqueLitTextData.QuadVertices.clear();
			thisRef->m_OpaqueLitNonShadowTextData.QuadVertices.clear();
			thisRef->m_MaskedLitTextData.QuadVertices.clear();
			thisRef->m_MaskedLitNonShadowTextData.QuadVertices.clear();
			thisRef->m_TranslucentLitTextData.QuadVertices.clear();
			thisRef->m_TranslucentNonShadowLitTextData.QuadVertices.clear();
			thisRef->m_UnlitTextData.QuadVertices.clear();
			thisRef->m_UnlitNonShadowTextData.QuadVertices.clear();
			thisRef->m_TextMaterials.clear();

			thisRef->m_FontAtlases.clear();
			thisRef->m_Atlases.clear();
			thisRef->m_TextTransforms = std::move(transforms);
			thisRef->m_TextTransformIndices = std::move(transformsIndices);

			uint32_t atlasCurrentIndex = 0;
			ProcessLitComponents(opaqueTextComponents, thisRef->m_FontAtlases, thisRef->m_OpaqueLitTextData, thisRef->m_TextMaterials, atlasCurrentIndex);
			ProcessLitComponents(opaqueNotCastingShadowsTextComponents, thisRef->m_FontAtlases, thisRef->m_OpaqueLitNonShadowTextData, thisRef->m_TextMaterials, atlasCurrentIndex);
			ProcessLitComponents(maskedTextComponents, thisRef->m_FontAtlases, thisRef->m_MaskedLitTextData, thisRef->m_TextMaterials, atlasCurrentIndex);
			ProcessLitComponents(maskedNotCastingShadowsTextComponents, thisRef->m_FontAtlases, thisRef->m_MaskedLitNonShadowTextData, thisRef->m_TextMaterials, atlasCurrentIndex);
			ProcessLitComponents(translucentTextComponents, thisRef->m_FontAtlases, thisRef->m_TranslucentLitTextData, thisRef->m_TextMaterials, atlasCurrentIndex);
			ProcessLitComponents(translucentNotCastingShadowsTextComponents, thisRef->m_FontAtlases, thisRef->m_TranslucentNonShadowLitTextData, thisRef->m_TextMaterials, atlasCurrentIndex);
			ProcessUnlitComponents(unlitTextComponents, thisRef->m_FontAtlases, thisRef->m_UnlitTextData, atlasCurrentIndex);
			ProcessUnlitComponents(unlitNotCastingShadowsTextComponents, thisRef->m_FontAtlases, thisRef->m_UnlitNonShadowTextData, atlasCurrentIndex);

			thisRef->m_Atlases.resize(atlasCurrentIndex);
			for (auto& atlas : thisRef->m_FontAtlases)
				thisRef->m_Atlases[atlas.second] = atlas.first;
		});
	}
	
	void GeometryManagerTask::SetTransforms(const std::unordered_set<const TextComponent*>& texts)
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

		RenderManager::Submit([task = shared_from_this(), data = std::move(updateData)](Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			for (auto& text : data)
			{
				auto it = thisRef->m_TextTransformIndices.find(text.ID);
				if (it != thisRef->m_TextTransformIndices.end())
				{
					thisRef->m_TextTransforms[it->second] = text.TransformMatrix;
					thisRef->m_TextUploadSpecificTransforms.push_back(it->second);
					thisRef->bUploadTextSpecificTransforms = true;
				}
			}
		});
	}

	static void FillByBlendMode(std::vector<LitTextQuadVertex>& newOpaqueData, std::vector<LitTextQuadVertex>& newMaskedData, std::vector<LitTextQuadVertex>& newTranslucentData,
		const LitTextGeometryData& data, const std::unordered_map<uint32_t, Ref<Material>>& materials)
	{
		for (const auto& vertex : data.QuadVertices)
		{
			auto it = materials.find(vertex.MaterialIndex);
			EG_CORE_ASSERT(it != materials.end());
			const auto& material = it->second;
			const Material::BlendMode blend = material->GetBlendMode();

			switch (blend)
			{
			case Material::BlendMode::Opaque:
				newOpaqueData.push_back(vertex);
				break;
			case Material::BlendMode::Masked:
				newMaskedData.push_back(vertex);
				break;
			case Material::BlendMode::Translucent:
				newTranslucentData.push_back(vertex);
				break;
			}
		}
	}

	void GeometryManagerTask::SortLitTexts()
	{
		std::vector<LitTextQuadVertex> newOpaqueData;
		newOpaqueData.reserve(m_OpaqueLitTextData.QuadVertices.size());
		std::vector<LitTextQuadVertex> newMaskedData;
		newMaskedData.reserve(m_MaskedLitTextData.QuadVertices.size());
		std::vector<LitTextQuadVertex> newTranslucentData;
		newTranslucentData.reserve(m_TranslucentLitTextData.QuadVertices.size());

		{
			newOpaqueData.clear();
			newMaskedData.clear();
			newTranslucentData.clear();

			FillByBlendMode(newOpaqueData, newMaskedData, newTranslucentData, m_OpaqueLitTextData, m_TextMaterials);
			FillByBlendMode(newOpaqueData, newMaskedData, newTranslucentData, m_MaskedLitTextData, m_TextMaterials);
			FillByBlendMode(newOpaqueData, newMaskedData, newTranslucentData, m_TranslucentLitTextData, m_TextMaterials);

			m_OpaqueLitTextData.QuadVertices = std::move(newOpaqueData);
			m_MaskedLitTextData.QuadVertices = std::move(newMaskedData);
			m_TranslucentLitTextData.QuadVertices = std::move(newTranslucentData);
		}

		{
			newOpaqueData.clear();
			newMaskedData.clear();
			newTranslucentData.clear();

			FillByBlendMode(newOpaqueData, newMaskedData, newTranslucentData, m_OpaqueLitNonShadowTextData, m_TextMaterials);
			FillByBlendMode(newOpaqueData, newMaskedData, newTranslucentData, m_MaskedLitNonShadowTextData, m_TextMaterials);
			FillByBlendMode(newOpaqueData, newMaskedData, newTranslucentData, m_TranslucentNonShadowLitTextData, m_TextMaterials);

			m_OpaqueLitNonShadowTextData.QuadVertices = std::move(newOpaqueData);
			m_MaskedLitNonShadowTextData.QuadVertices = std::move(newMaskedData);
			m_TranslucentNonShadowLitTextData.QuadVertices = std::move(newTranslucentData);
		}
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
