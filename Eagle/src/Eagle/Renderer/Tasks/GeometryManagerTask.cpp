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

// TODO v0.7: Test this functionality on heavy scenes and check if it's faster than uploading the whole buffer at once
#define EG_UPLOAD_ONLY_REQUIRED_TRANSFORMS 1

namespace Eagle
{
	struct DrawDataInsertIndices
	{
		uint32_t Global = UINT_MAX;
		uint32_t ShadowCasting = UINT_MAX;

		bool IsValid() const { return Global != UINT_MAX; }
	};

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

	namespace Utils
	{
		static void UploadIndexBuffer(const Ref<CommandBuffer>& cmd, const Ref<Buffer>& buffer)
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

			cmd->Write(buffer, indices.data(), ibSize, 0, buffer->GetLayout(), BufferReadAccess::Index);
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

				cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, gpuBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
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

			* bUploadTransforms = false;
			*bUploadSpecificTransforms = false;
			specificIndices.clear();
		}

		[[nodiscard]] static DrawDataInsertIndices AddDrawData(MeshesDrawLists& data, const MeshDrawData& meshData, Material::BlendMode blendMode,
			const MeshDrawData::MaterialData& matData, bool bNewMaterialSlot, bool bCastsShadows, bool bDoubleSided, const DrawDataInsertIndices& dataIndices)
		{
			auto& drawLists = bDoubleSided ? data.DoubleSided : data.SingleSided;

			std::vector<MeshDrawData>* allDatas = nullptr;
			std::vector<MeshDrawData>* shadowCastingDatas = nullptr;
			switch (blendMode)
			{
				case Material::BlendMode::Opaque:
				{
					allDatas = &drawLists.Opaque;
					shadowCastingDatas = &drawLists.ShadowCastingOpaque;
					break;
				}
				case Material::BlendMode::Masked:
				{
					allDatas = &drawLists.Masked;
					shadowCastingDatas = &drawLists.ShadowCastingMasked;
					break;
				}
				case Material::BlendMode::Translucent:
				{
					allDatas = &drawLists.Translucent;
					shadowCastingDatas = &drawLists.ShadowCastingTranslucent;
					break;
				}
				default:
				{
					EG_CORE_ASSERT(false);
					return {};
				}
			}

			// In order to prevent duplication of the same mesh (shadow casting and non-casting ones) being added to the `allDatas` list,
			// `dataIndex` is used to update the draw data (increment instance count), instead of adding a new draw command and partially breaking instancing
			DrawDataInsertIndices insertionIndices = dataIndices;
			if (!dataIndices.IsValid())
			{
				insertionIndices.Global = (int32_t)allDatas->size();
				allDatas->emplace_back(meshData).PerMaterialData.push_back(matData);
				if (bCastsShadows)
				{
					insertionIndices.ShadowCasting = (int32_t)shadowCastingDatas->size();
					shadowCastingDatas->emplace_back(meshData).PerMaterialData.push_back(matData);
				}
			}
			else
			{
				auto& opaque = (*allDatas)[insertionIndices.Global];
				if (bNewMaterialSlot)
				{
					opaque.PerMaterialData.push_back(matData);
					if (bCastsShadows)
					{
						auto& opaqueShadow = (*shadowCastingDatas)[insertionIndices.ShadowCasting];
						opaqueShadow.PerMaterialData.push_back(matData);
					}
				}
				else
				{
					opaque.PerMaterialData.back().InstanceCount += matData.InstanceCount;
					if (bCastsShadows)
					{
						auto& opaqueShadow = (*shadowCastingDatas)[insertionIndices.ShadowCasting];
						opaqueShadow.PerMaterialData.back().InstanceCount += matData.InstanceCount;
					}
				}
			}

			return insertionIndices;
		}

		template <typename MeshType, typename MeshesMapType>
		static void ProcessInstances2(const MeshesMapType& meshes, MeshesDrawLists* drawList, std::vector<PerInstanceData>* ivb)
		{
			struct InstanceKey
			{
				MeshIndicesData Indices;
				uint32_t VertexOffset = 0;
				uint32_t VerticesCount = 0;
				uint32_t InstanceCount = 0;
				uint32_t SkinnedVertexOffset = 0;

				Ref<MeshType> Mesh;
				Material::BlendMode BlendMode = Material::BlendMode::Opaque;
				uint32_t MaterialSlot = 0;
				bool bCastsShadows = false;
				bool bDoubleSided = false;

				bool operator< (const InstanceKey& other) const
				{
					if (Mesh != other.Mesh)
						return Mesh < other.Mesh;

					if (BlendMode != other.BlendMode)
						return BlendMode < other.BlendMode;

					if (bDoubleSided != other.bDoubleSided)
						return bDoubleSided < other.bDoubleSided; // Single sided first

					if (MaterialSlot != other.MaterialSlot)
						return MaterialSlot < other.MaterialSlot;

					return bCastsShadows > other.bCastsShadows; // Shadow casters first
				}
			};

			std::map<InstanceKey, std::vector<PerInstanceData>> instancesDatas;

			uint32_t skinnedVerticesOffset = 0;
			for (const auto& [meshKey, instances] : meshes)
			{
				const auto& mesh = meshKey.Mesh;
				const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
				const uint32_t instanceCount = (uint32_t)instances.size();

				InstanceKey instanceKey{};
				instanceKey.Mesh = mesh;
				instanceKey.VertexOffset = meshKey.VerticesOffset;
				instanceKey.VerticesCount = meshKey.VerticesCount;
				instanceKey.InstanceCount = instanceCount;
				instanceKey.SkinnedVertexOffset = skinnedVerticesOffset;
				skinnedVerticesOffset += meshKey.VerticesCount * instanceCount;

				for (const auto& instance : instances)
				{
					instanceKey.bCastsShadows = instance.bCastsShadows;
					for (uint32_t i = 0; i < materialsCount; ++i)
					{
						const Material::BlendMode blendMode = instance.Materials[i] ? instance.Materials[i]->GetBlendMode() : Material::BlendMode::Opaque;
						const bool bDoubleSided = instance.Materials[i] ? instance.Materials[i]->IsDoubleSided() : false;
						instanceKey.BlendMode = blendMode;
						instanceKey.bDoubleSided = bDoubleSided;
						instanceKey.MaterialSlot = i;
						instanceKey.Indices = meshKey.PerMaterialIndices[i];

						auto& instancesData = instancesDatas[instanceKey];
						instancesData.push_back(instance.SubMeshData[i]);
					}
				}
			}

			Ref<MeshType> lastMesh = nullptr;
			uint32_t lastBlendMode = UINT_MAX;
			uint32_t lastMaterialSlot = UINT_MAX;
			bool lastDoubleSided = false;
			DrawDataInsertIndices insertionIndices = {};
			uint32_t currentOffset = 0;
			for (const auto& [instanceKey, instances] : instancesDatas)
			{
				if (lastMesh != instanceKey.Mesh || lastBlendMode != uint32_t(instanceKey.BlendMode) || lastDoubleSided != instanceKey.bDoubleSided)
				{
					insertionIndices = {};
					lastMesh = instanceKey.Mesh;
					lastBlendMode = uint32_t(instanceKey.BlendMode);
				}

				const bool bNewMatSlot = lastMaterialSlot != instanceKey.MaterialSlot;
				lastMaterialSlot = instanceKey.MaterialSlot;

				ivb->insert(ivb->end(), instances.begin(), instances.end());

				const uint32_t instanceCount = (uint32_t)instances.size();
				MeshDrawData drawData{};
				drawData.VertexOffset = instanceKey.VertexOffset;
				drawData.VerticesCount = instanceKey.VerticesCount;
				drawData.InstanceCount = instanceKey.InstanceCount;
				drawData.SkinnedVertexOffset = instanceKey.SkinnedVertexOffset;

				MeshDrawData::MaterialData matData{};
				matData.InstanceCount = instanceCount;
				matData.FirstIndex = instanceKey.Indices.FirstIndex;
				matData.IndexCount = instanceKey.Indices.IndicesCount;
				matData.FirstInstance = currentOffset;

				insertionIndices = Utils::AddDrawData(*drawList, drawData, instanceKey.BlendMode, matData, bNewMatSlot, instanceKey.bCastsShadows, instanceKey.bDoubleSided, insertionIndices);

				currentOffset += matData.InstanceCount;
			}
		}

		static std::vector<MeshDrawData>& GetDrawData(MeshesDrawLists& data, Material::BlendMode blendMode, bool bShadowCastingOnly)
		{
			auto& opaque      = bShadowCastingOnly ? data.SingleSided.ShadowCastingOpaque      : data.SingleSided.Opaque;
			auto& translucent = bShadowCastingOnly ? data.SingleSided.ShadowCastingTranslucent : data.SingleSided.Translucent;
			auto& masked      = bShadowCastingOnly ? data.SingleSided.ShadowCastingMasked      : data.SingleSided.Masked;

			switch (blendMode)
			{
				case Material::BlendMode::Opaque: return opaque;
				case Material::BlendMode::Translucent: return translucent;
				case Material::BlendMode::Masked: return masked;
				default:
					EG_CORE_ASSERT(false);
					return opaque;
			}
		}

		template <typename VertexType, typename MeshesMapType>
		static void UploadMeshes(const Ref<CommandBuffer>& cmd, MeshGeometryData<VertexType>& buffers, MeshesMapType& meshes)
		{
			auto& vb = buffers.VertexBuffer;
			auto& ib = buffers.IndexBuffer;

			// Reserving enough space to hold Vertex & Index data
			size_t currentVertexSize = 0;
			size_t currentIndexSize = 0;
			uint32_t vertexOffset = 0;
			size_t meshesCount = 0;
			for (auto& [meshKey, instances] : meshes)
			{
				auto& mesh = meshKey.Mesh;
				const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
				const uint32_t verticesCount = (uint32_t)mesh->GetVerticesCount();

				meshKey.VerticesCount = verticesCount;
				meshKey.VerticesOffset = vertexOffset;

				currentVertexSize += verticesCount * sizeof(VertexType);
				currentIndexSize += mesh->GetTotalIndicesCount() * sizeof(Index);
				meshesCount += instances.size() * materialsCount;
				vertexOffset += verticesCount;
			}

			if (currentVertexSize > vb->GetSize())
				vb->Resize((currentVertexSize * 3) / 2);
			if (currentIndexSize > ib->GetSize())
				ib->Resize((currentIndexSize * 3) / 2);

			buffers.Vertices.clear();
			buffers.Indices.clear();
			buffers.Vertices.reserve(currentVertexSize / sizeof(VertexType));
			buffers.Indices.reserve(currentIndexSize / sizeof(Index));

			uint32_t firstIndex = 0u;
			for (auto& [meshKey, instances] : meshes)
			{
				auto& mesh = meshKey.Mesh;
				const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
				const auto& meshVertices = mesh->GetVertices();
				buffers.Vertices.insert(buffers.Vertices.end(), meshVertices.begin(), meshVertices.end());

				meshKey.PerMaterialIndices.resize(materialsCount);
				for (uint32_t i = 0; i < materialsCount; ++i)
				{
					const auto& meshIndices = mesh->GetIndices(i);
					const uint32_t indicesCount = (uint32_t)meshIndices.size();
					buffers.Indices.insert(buffers.Indices.end(), meshIndices.begin(), meshIndices.end());

					auto& indicesData = meshKey.PerMaterialIndices[i];
					indicesData.IndicesCount = indicesCount;
					indicesData.FirstIndex = firstIndex;
					firstIndex += indicesCount;
				}
			}

			cmd->Write(vb, buffers.Vertices.data(), buffers.Vertices.size() * sizeof(VertexType), 0, vb->GetLayout(), BufferReadAccess::Vertex);
			cmd->Write(ib, buffers.Indices.data(), buffers.Indices.size() * sizeof(Index), 0, ib->GetLayout(), BufferReadAccess::Index);
		}

		// Fill up ivb so that the same blend mode instances are adjacent in memory.
		// Also, shadow casting instances of the blend mode come first.
		// For example: Opaque_CastingShadow_#0, Opaque_CastingShadow_#1, Opaque_NotCastingShadow_#2, ..., Opaque_NotCastingShadow_#N
		// This pattern allows us to build two draw lists: one for passes that care only about shadow casting meshes (Shadow pass),
		// and the other list for passes that don't care about it (Base pass).
		// So, with the above example, shadow pass draw list will have `Instance Count = 2`, but the base pass will have `Instance Count = N`.
		template <typename MeshesMap, typename PerInstanceDataType>
		static void ProcessInstances(const MeshesMap& meshes, MeshesDrawLists* drawList, std::vector<PerInstanceDataType>* ivb)
		{
			struct MeshCounters
			{
				std::array<uint32_t, Material::MaxBlendModes> Offset = { 0 };
				std::array<uint32_t, Material::MaxBlendModes> ShadowCasting = { 0 };
				std::array<uint32_t, Material::MaxBlendModes> NonShadowCasting = { 0 };
			};

			uint32_t totalInstances = 0u;
			std::vector<MeshCounters> offsets;
			offsets.reserve(meshes.size());
			{
				// First, count the instances by types so that we can calculate final offsets correctly
				for (const auto& [meshKey, instances] : meshes)
				{
					MeshCounters& offset = offsets.emplace_back();
					const auto& mesh = meshKey.Mesh;
					const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
					for (auto& instance : instances)
					{
						for (uint32_t i = 0; i < materialsCount; ++i)
						{
							const Material::BlendMode blendMode = instance.Materials[i] ? instance.Materials[i]->GetBlendMode() : Material::BlendMode::Opaque;
							// Count instances
							instance.bCastsShadows ? offset.ShadowCasting[uint32_t(blendMode)]++ : offset.NonShadowCasting[uint32_t(blendMode)]++;
						}
					}

					totalInstances += materialsCount * uint32_t(instances.size());
				}

				// Calculate the final offsets
				uint32_t currentOffset = 0;
				for (uint32_t i = 0; i < Material::MaxBlendModes; ++i)
				{
					for (auto& offset : offsets)
					{
						const uint32_t shadowCastingInstances = offset.ShadowCasting[i];
						const uint32_t nonShadowCastingInstances = offset.NonShadowCasting[i];

						offset.Offset[i] = currentOffset;
						offset.ShadowCasting[i] = offset.Offset[i]; // Shadow casting go first
						offset.NonShadowCasting[i] = offset.Offset[i] + shadowCastingInstances;

						currentOffset += shadowCastingInstances + nonShadowCastingInstances;
					}
				}
			}
			ivb->resize(totalInstances);

			constexpr uint32_t buckets = 2; // Separating shadow casting and non shadow casting instances
			constexpr uint8_t shadowCastingIdx = 0;
			constexpr uint8_t allInstancesIdx = 1;
			uint32_t skinnedVerticesOffset = 0;
			uint32_t meshIdx = 0;
			for (const auto& [meshKey, instances] : meshes)
			{
				const auto& mesh = meshKey.Mesh;
				const uint32_t instanceCount = (uint32_t)instances.size();
				const uint32_t materialsCount = mesh->GetMaterialSlotsCount();

				// Bucket at `shadowCastingIdx` will contain draw data just for shadow casting instances.
				// Bucket at `nonShadowCastingIdx` will contain draw data for all instances
				std::array<bool, Material::MaxBlendModes> hasAnyInstances[buckets] = { { false }, { false } };
				std::array<MeshDrawData, Material::MaxBlendModes> drawDatas[buckets] = { {}, {} };

				for (uint32_t b = 0; b < buckets; ++b)
				{
					for (size_t i = 0; i < Material::MaxBlendModes; ++i)
					{
						drawDatas[b][i].SkinnedVertexOffset = skinnedVerticesOffset;
						drawDatas[b][i].VertexOffset = meshKey.VerticesOffset;
						drawDatas[b][i].VerticesCount = meshKey.VerticesCount;
						drawDatas[b][i].InstanceCount = instanceCount;
						// Allocated as required.
						// drawDatas[i].PerMaterialData.resize(materialsCount);
					}
				}
				skinnedVerticesOffset += meshKey.VerticesCount * instanceCount;

				// Iterate over every mesh in the batch.
				// Append instance data in the pattern of `Structure of Arrays`.
				// For example, [0, 0, 0, 1, 1, 1] rather than [0, 1, 0, 1, 0, 1]
				for (uint32_t i = 0; i < materialsCount; ++i)
				{
					std::array<uint32_t, Material::MaxBlendModes> instancesPerBlendMode = { 0 };
					for (auto& instance : instances)
					{
						const Material::BlendMode blendMode = instance.Materials[i] ? instance.Materials[i]->GetBlendMode() : Material::BlendMode::Opaque;
						const uint32_t blendModeIdx = uint32_t(blendMode);
						instancesPerBlendMode[blendModeIdx]++;

						auto& allInstancesDrawData = drawDatas[allInstancesIdx][blendModeIdx];
						if (allInstancesDrawData.PerMaterialData.size() != materialsCount)
							allInstancesDrawData.PerMaterialData.resize(materialsCount);

						auto& perMaterialData = allInstancesDrawData.PerMaterialData[i];
						const uint32_t offset = offsets[meshIdx].Offset[blendModeIdx];
						if (perMaterialData.InstanceCount == 0)
						{
							perMaterialData.IndexCount = meshKey.PerMaterialIndices[i].IndicesCount;
							perMaterialData.FirstIndex = meshKey.PerMaterialIndices[i].FirstIndex;
							perMaterialData.FirstInstance = offset;
							hasAnyInstances[allInstancesIdx][blendModeIdx] = true;
						}
						if (instance.bCastsShadows)
						{
							auto& shadowCastingInstancesDrawData = drawDatas[shadowCastingIdx][blendModeIdx];

							if (shadowCastingInstancesDrawData.PerMaterialData.size() != materialsCount)
								shadowCastingInstancesDrawData.PerMaterialData.resize(materialsCount);
							
							auto& perMaterialData = shadowCastingInstancesDrawData.PerMaterialData[i];
							if (perMaterialData.InstanceCount == 0)
							{
								perMaterialData.IndexCount = meshKey.PerMaterialIndices[i].IndicesCount;
								perMaterialData.FirstIndex = meshKey.PerMaterialIndices[i].FirstIndex;
								perMaterialData.FirstInstance = offset;
								hasAnyInstances[shadowCastingIdx][blendModeIdx] = true;
							}
							perMaterialData.InstanceCount++;
						}

						uint32_t& insertionIdx = instance.bCastsShadows ? offsets[meshIdx].ShadowCasting[blendModeIdx] : offsets[meshIdx].NonShadowCasting[blendModeIdx];
						(*ivb)[insertionIdx++] = instance.SubMeshData[i];
						perMaterialData.InstanceCount++;
					}

					for (uint32_t blendMode = 0; blendMode < Material::MaxBlendModes; ++blendMode)
					{
						offsets[meshIdx].Offset[blendMode] += instancesPerBlendMode[blendMode];
					}
				}

				for (uint32_t b = 0; b < buckets; ++b)
				{
					const bool bShadowCasting = b == shadowCastingIdx;
					for (size_t i = 0; i < Material::MaxBlendModes; ++i)
					{
						if (hasAnyInstances[b][i])
						{
							Utils::GetDrawData(*drawList, Material::BlendMode(i), bShadowCasting).emplace_back(std::move(drawDatas[b][i]));
						}
					}
				}
				meshIdx++;
			}
		}
	}

	GeometryManagerTask::GeometryManagerTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
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

			m_StaticMeshesBuffers.VertexBuffer = Buffer::Create(vertexSpecs, "StaticMeshes_VertexBuffer");
			m_StaticMeshesBuffers.InstanceBuffer = Buffer::Create(vertexSpecs, "StaticMeshes_InstanceVertexBuffer");
			m_StaticMeshesBuffers.IndexBuffer = Buffer::Create(indexSpecs, "StaticMeshes_IndexBuffer");
			m_StaticMeshesBuffers.TransformsBuffer = Buffer::Create(transformsBufferSpecs, "StaticMeshes_TransformsBuffer");
		}

		// Create Skeletal Mesh buffers
		{
			BufferSpecifications vertexSpecs;
			vertexSpecs.Size = s_MeshesBaseVertexBufferSize;
			vertexSpecs.Layout = BufferReadAccess::Vertex;
			vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst | BufferUsage::StorageBuffer;

			BufferSpecifications indexSpecs;
			indexSpecs.Size = s_MeshesBaseIndexBufferSize;
			indexSpecs.Layout = BufferReadAccess::Index;
			indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = sizeof(glm::mat4) * 100; // 100 transforms
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;

			m_SkeletalMeshesBuffers.VertexBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_VertexBuffer");
			m_SkeletalMeshesBuffers.InstanceBuffer = Buffer::Create(vertexSpecs, "SkeletalMeshes_InstanceVertexBuffer");
			m_SkeletalMeshesBuffers.IndexBuffer = Buffer::Create(indexSpecs, "SkeletalMeshes_IndexBuffer");
			m_SkeletalMeshesBuffers.TransformsBuffer = Buffer::Create(transformsBufferSpecs, "SkeletalMeshes_TransformsBuffer");
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

			m_SingleSidedSprites.Init(vertexSpecs, indexSpecs);
			m_DoubleSidedSprites.Init(vertexSpecs, indexSpecs);

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

			m_SingleSidedTexts.Init(vertexSpecs, indexSpecs);
			m_DoubleSidedTexts.Init(vertexSpecs, indexSpecs);

			constexpr bool bOpaqueOnly = true;
			m_SingleSidedUnlitTexts.Init(vertexSpecs, indexSpecs, bOpaqueOnly);
			m_DoubleSidedUnlitTexts.Init(vertexSpecs, indexSpecs, bOpaqueOnly);

			m_TextTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_TransformsBuffer");
		}
	
		InitWithOptions(m_Renderer.GetOptions());
	}

	void GeometryManagerTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Process Geometry");
		EG_CPU_TIMING_SCOPED("Process Geometry");

		// If it changed, we need to re-sort meshes
		const bool bRenderingModeChanged = MaterialSystem::HasRenderingModeChanged();

		// Meshes
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Meshes");
			EG_CPU_TIMING_SCOPED("Process Meshes");

			if (bUploadMeshes || bRenderingModeChanged)
			{
				if (bUploadMeshes)
					UploadStaticMeshes(cmd);
				SortMeshes(cmd);
			}
			const bool bTransformBufferGarbage = bUploadMeshes;
			Utils::UploadTransforms(cmd, m_MeshTransforms, m_StaticMeshesBuffers.TransformsBuffer, m_StaticMeshesBuffers.PrevTransformsBuffer, m_MeshUploadSpecificTransforms,
				&bUploadMeshTransforms, &bUploadMeshSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Static Meshes. Upload Transforms buffer");

			bUploadMeshes = false;
		}

		// Skeletal Meshes
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Skeletal Meshes");
			EG_CPU_TIMING_SCOPED("Process Skeletal Meshes");

			if (bUploadSkeletalMeshes || bRenderingModeChanged)
			{
				if (bUploadSkeletalMeshes)
					UploadSkeletalMeshes(cmd);
				SortSkeletalMeshes(cmd);
			}
			// Note: we're not copying/storing prev transforms buffer here. It's handled in a more optimal way inside SkinCacheTask by just copying last frame's skinned vertices
			const bool bTransformBufferGarbage = bUploadSkeletalMeshes;
			Utils::UploadTransforms(cmd, m_SkeletalMeshTransforms, m_SkeletalMeshesBuffers.TransformsBuffer, m_SkeletalMeshesBuffers.PrevTransformsBuffer, m_SkeletalMeshUploadSpecificTransforms,
				&bUploadSkeletalMeshTransforms, &bUploadSkeletalMeshSpecificTransforms, false, bTransformBufferGarbage, "Skeletal Meshes. Upload Transforms buffer");

			UploadAnimationTransforms(cmd);

			bUploadSkeletalMeshes = false;
		}

		// Sprites
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Sprites");
			EG_CPU_TIMING_SCOPED("Process Sprites");

			if (bUploadSprites || bRenderingModeChanged)
			{
				SortSprites();
				{
					EG_GPU_TIMING_SCOPED(cmd, "Sprites. Upload vertex & index buffers");
					EG_CPU_TIMING_SCOPED("Sprites. Upload vertex & index buffers");

					UploadSprites(cmd, m_SingleSidedSprites);
					UploadSprites(cmd, m_DoubleSidedSprites);
				}
			}
			const bool bTransformBufferGarbage = bUploadSprites;
			Utils::UploadTransforms(cmd, m_SpriteTransforms, m_SpritesTransformsBuffer, m_SpritesPrevTransformsBuffer, m_SpriteUploadSpecificTransforms,
				&bUploadSpritesTransforms, &bUploadSpritesSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Sprites. Upload Transforms buffer");
			
			bUploadSprites = false;
		}
	
		// Texts
		{
			EG_GPU_TIMING_SCOPED(cmd, "Process Texts");
			EG_CPU_TIMING_SCOPED("Process Texts");

			if (bUploadTextQuads || bRenderingModeChanged)
			{
				SortTexts();
				{
					EG_GPU_TIMING_SCOPED(cmd, "Sprites. Upload vertex & index buffers");
					EG_CPU_TIMING_SCOPED("Sprites. Upload vertex & index buffers");

					UploadTexts(cmd, m_SingleSidedTexts);
					UploadTexts(cmd, m_DoubleSidedTexts);
					UploadTexts(cmd, m_SingleSidedUnlitTexts);
					UploadTexts(cmd, m_DoubleSidedUnlitTexts);
				}
			}
			const bool bTransformBufferGarbage = bUploadTextQuads;
			Utils::UploadTransforms(cmd, m_TextTransforms, m_TextTransformsBuffer, m_TextPrevTransformsBuffer, m_TextUploadSpecificTransforms,
				&bUploadTextTransforms, &bUploadTextSpecificTransforms, bMotionRequired, bTransformBufferGarbage, "Texts. Upload Transforms buffer");

			bUploadTextQuads = false;
		}
	}

	void GeometryManagerTask::UploadAnimationTransforms(const Ref<CommandBuffer>& cmd)
	{
		auto& animTransforms = m_AnimationTransforms;
		auto& animTransformsBuffers = m_AnimationTransformsBuffers;

		// Upload anim transforms
		{
			EG_GPU_TIMING_SCOPED(cmd, "Skeletal Meshes. Process and upload animations");
			EG_CPU_TIMING_SCOPED("Skeletal Meshes. Process and upload animations");

			const auto& finalAnimTransforms = m_Renderer.GetMeshesAnimationTransforms_RT();
			for (auto& [meshKey, instances] : m_SkeletalMeshes)
			{
				const auto& mesh = meshKey.Mesh;
				for (auto& instance : instances)
				{
					// It doesn't matter which submesh index we take, since `TransformIndex` and `ObjectID` are going to be the same
					const auto& instanceData = instance.SubMeshData[0];
					const uint32_t animIndex = instanceData.PackedTransformIndex & (~EG_RECEIVES_DECALS_MASK);
					auto& transforms = animTransforms[animIndex];
					auto it = finalAnimTransforms.find(instanceData.ObjectID);
					EG_ASSERT(it != finalAnimTransforms.end());
					transforms = it->second;

					auto& animTransformsBuffer = animTransformsBuffers[animIndex];
					const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
					if (!animTransformsBuffer || (animTransformsBuffer == Buffer::Dummy))
					{
						BufferSpecifications transformsBufferSpecs;
						transformsBufferSpecs.Size = currentBufferSize;
						transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
						transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;
						animTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_AnimationTransformsBuffer_#" + std::to_string(animIndex));
					}
					else if (currentBufferSize > animTransformsBuffer->GetSize())
					{
						size_t newSize = (currentBufferSize * 3) / 2;
						animTransformsBuffer->Resize(newSize);
					}

					cmd->Write(animTransformsBuffer, transforms.data(), currentBufferSize, 0, animTransformsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
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
			m_StaticMeshesBuffers.PrevTransformsBuffer.reset();
			m_SpritesPrevTransformsBuffer.reset();
			m_TextPrevTransformsBuffer.reset();
		}
		else
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_StaticMeshesBuffers.TransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_StaticMeshesBuffers.PrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Meshes_PrevTransformsBuffer");

			// Note: we're not storing prev transforms buffer. It's handled in a more optimal way inside SkinCacheTask by just copying last frame's skinned vertices
			// transformsBufferSpecs.Size = m_SkeletalMeshesBuffers.TransformsBuffer->GetSize();
			// m_SkeletalMeshesBuffers.PrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "SkeletalMeshes_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_SpritesTransformsBuffer->GetSize();
			m_SpritesPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Sprites_PrevTransformsBuffer");

			transformsBufferSpecs.Size = m_TextTransformsBuffer->GetSize();
			m_TextPrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_PrevTransformsBuffer");
		}
	}

	// ---------- Static Meshes ----------
	void GeometryManagerTask::SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty)
	{
		if (!bDirty)
			return;

		StaticMeshesMap tempMeshes;
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

			const Ref<StaticMesh>& staticMesh = meshAsset->GetMesh();
			if (!staticMesh || !staticMesh->IsValid())
				continue;

			const bool bCastsShadows = comp->DoesCastShadows();
			const bool bReceivesDecals = comp->DoesReceiveDecals();
			const uint32_t materialsCount = comp->GetMaterialsSlotsCount();
			const uint32_t meshID = comp->Parent.GetID();

			auto& instances = tempMeshes[{ staticMesh }];
			auto& instance = instances.emplace_back();
			instance.SubMeshData.reserve(materialsCount);
			instance.bCastsShadows = bCastsShadows;

			for (uint32_t i = 0; i < materialsCount; ++i)
			{
				const auto& materialAsset = comp->GetMaterialAsset(i);
				instance.Materials.push_back(materialAsset ? materialAsset->GetMaterial() : nullptr);
				auto& subInstanceData = instance.SubMeshData.emplace_back();
				subInstanceData.PackedTransformIndex = meshIndex | (bReceivesDecals ? EG_RECEIVES_DECALS_MASK : 0u);
				subInstanceData.ObjectID = meshID;
				subInstanceData.MaterialIndex = MaterialSystem::GetMaterialIndex(instance.Materials[i]);
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
				thisRef->m_StaticMeshes = std::move(meshes);
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
	
	void GeometryManagerTask::SortMeshes(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Sort static meshes based on Blend Mode");
		EG_GPU_TIMING_SCOPED(cmd, "Static Meshes. Upload instance vertex buffer");

		auto& ivbData = m_StaticMeshesBuffers.InstanceVertices;
		ivbData.clear();
		m_StaticMeshesDrawData.Clear();

		Utils::ProcessInstances2<StaticMesh>(m_StaticMeshes, &m_StaticMeshesDrawData, &ivbData);
		//Utils::ProcessInstances(m_StaticMeshes, &m_StaticMeshesDrawData, &ivbData);

		if (!ivbData.empty())
		{
			const size_t currentInstanceVertexSize = ivbData.size() * sizeof(PerInstanceData);

			auto& ivb = m_StaticMeshesBuffers.InstanceBuffer;
			if (currentInstanceVertexSize > ivb->GetSize())
				ivb->Resize((currentInstanceVertexSize * 3) / 2);

			cmd->Write(ivb, ivbData.data(), currentInstanceVertexSize, 0, ivb->GetLayout(), BufferReadAccess::Vertex);
		}
	}

	void GeometryManagerTask::UploadStaticMeshes(const Ref<CommandBuffer>& cmd)
	{
		if (m_StaticMeshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Static Meshes. Upload vertex & index buffers");
		EG_CPU_TIMING_SCOPED("Static Meshes. Upload vertex & index buffers");

		Utils::UploadMeshes(cmd, m_StaticMeshesBuffers, m_StaticMeshes);
	}

	// ---------- Skeletal Meshes ----------
	void GeometryManagerTask::SetSkeletalMeshes(const std::vector<SkeletalMeshComponent*>& meshes, bool bDirty)
	{
		if (!bDirty)
			return;

		SkeletalMeshesMap tempMeshes;
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

			const bool bCastsShadows = comp->DoesCastShadows();
			const bool bReceivesDecals = comp->DoesReceiveDecals();
			const uint32_t materialsCount = comp->GetMaterialsSlotsCount();
			const uint32_t meshID = comp->Parent.GetID();

			auto& instances = tempMeshes[{ skeletalMesh }];
			auto& instance = instances.emplace_back();
			instance.SubMeshData.reserve(materialsCount);
			instance.bCastsShadows = bCastsShadows;

			for (uint32_t i = 0; i < materialsCount; ++i)
			{
				const auto& materialAsset = comp->GetMaterialAsset(i);
				instance.Materials.push_back(materialAsset ? materialAsset->GetMaterial() : nullptr);
				auto& subInstanceData = instance.SubMeshData.emplace_back();
				subInstanceData.PackedTransformIndex = meshIndex | (bReceivesDecals ? EG_RECEIVES_DECALS_MASK : 0u);
				subInstanceData.ObjectID = meshID;
				subInstanceData.MaterialIndex = MaterialSystem::GetMaterialIndex(instance.Materials[i]);
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

	void GeometryManagerTask::SortSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Sort skeletal meshes based on Blend Mode");
		EG_GPU_TIMING_SCOPED(cmd, "Skeletal Meshes. Upload instance vertex buffer");

		auto& ivbData = m_SkeletalMeshesBuffers.InstanceVertices;
		ivbData.clear();
		m_SkeletalMeshesDrawData.Clear();

		Utils::ProcessInstances2<SkeletalMesh>(m_SkeletalMeshes, &m_SkeletalMeshesDrawData, &ivbData);
		//Utils::ProcessInstances(m_SkeletalMeshes, &m_SkeletalMeshesDrawData, &ivbData);

		if (!ivbData.empty())
		{
			const size_t currentInstanceVertexSize = ivbData.size() * sizeof(PerInstanceData);

			auto& ivb = m_SkeletalMeshesBuffers.InstanceBuffer;
			if (currentInstanceVertexSize > ivb->GetSize())
				ivb->Resize((currentInstanceVertexSize * 3) / 2);

			cmd->Write(ivb, ivbData.data(), currentInstanceVertexSize, 0, ivb->GetLayout(), BufferLayoutType::StorageBuffer);
		}

		const uint32_t animationsCount = (uint32_t)m_SkeletalMeshTransforms.size();
		m_AnimationTransforms.resize(animationsCount);
		if (m_AnimationTransformsBuffers.size() < animationsCount)
		{
			m_AnimationTransformsBuffers.resize(animationsCount);
		}
		else
		{
			// Set unused buffers to Dummy. It's required to update descriptors that point to unused buffers
			for (size_t i = animationsCount; i < m_AnimationTransformsBuffers.size(); ++i)
			{
				m_AnimationTransformsBuffers[i] = Buffer::Dummy;
			}
		}
	}

	void GeometryManagerTask::UploadSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		if (m_SkeletalMeshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Skeletal Meshes. Upload vertex & index buffers");
		EG_CPU_TIMING_SCOPED("Skeletal Meshes. Upload vertex & index buffers");

		Utils::UploadMeshes(cmd, m_SkeletalMeshesBuffers, m_SkeletalMeshes);
	}

	// ---------- Sprites ----------
	void GeometryManagerTask::SortSprites()
	{
		EG_CPU_TIMING_SCOPED("Sort sprites based on Blend Mode");

		m_SingleSidedSprites.Clear();
		m_DoubleSidedSprites.Clear();

		const size_t spritesCount = m_Sprites.size();
		for (size_t i = 0; i < spritesCount; ++i)
		{
			const auto& sprite = m_Sprites[i];
			const uint32_t transformIndex = uint32_t(i);
			const uint32_t transformIndexPacked = transformIndex | (sprite.bReceivesDecals ? (1 << 31) : 0u);
			const Material::BlendMode blendMode = sprite.Material ? sprite.Material->GetBlendMode() : Material::BlendMode::Opaque;
			const bool bDoubleSided = sprite.Material ? sprite.Material->IsDoubleSided() : false;
			auto& spritesData = bDoubleSided ? m_DoubleSidedSprites : m_SingleSidedSprites;
			switch (blendMode)
			{
				case Material::BlendMode::Opaque:
				{
					if (sprite.bCastsShadows)
						AddQuad(spritesData.Opaque.ShadowCastingQuads.QuadVertices, sprite, m_SpriteTransforms[i], transformIndexPacked);
					else
						AddQuad(spritesData.Opaque.NonShadowQuads.QuadVertices, sprite, m_SpriteTransforms[i], transformIndexPacked);
					break;
				}
				case Material::BlendMode::Translucent:
				{
					if (sprite.bCastsShadows)
						AddQuad(spritesData.Translucent.ShadowCastingQuads.QuadVertices, sprite, m_SpriteTransforms[i], transformIndexPacked);
					else
						AddQuad(spritesData.Translucent.NonShadowQuads.QuadVertices, sprite, m_SpriteTransforms[i], transformIndexPacked);
					break;
				}
				case Material::BlendMode::Masked:
				{
					if (sprite.bCastsShadows)
						AddQuad(spritesData.Masked.ShadowCastingQuads.QuadVertices, sprite, m_SpriteTransforms[i], transformIndexPacked);
					else
						AddQuad(spritesData.Masked.NonShadowQuads.QuadVertices, sprite, m_SpriteTransforms[i], transformIndexPacked);
					break;
				}
				default: EG_CORE_ASSERT("Unknown blend mode!");
			}
		}
	}

	void GeometryManagerTask::UploadSprites(const Ref<CommandBuffer>& cmd, const SpriteGeometryData& spritesData)
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
			Utils::UploadIndexBuffer(cmd, ib);
		}

		cmd->Write(vb, spritesData.QuadVertices.data(), currentVertexSize, 0, vb->GetLayout(), BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void GeometryManagerTask::UploadSprites(const Ref<CommandBuffer>& cmd, const QuadsRenderData<SpriteGeometryData>& spritesData)
	{
		UploadSprites(cmd, spritesData.Opaque.ShadowCastingQuads);
		UploadSprites(cmd, spritesData.Opaque.NonShadowQuads);
		UploadSprites(cmd, spritesData.Masked.ShadowCastingQuads);
		UploadSprites(cmd, spritesData.Masked.NonShadowQuads);
		UploadSprites(cmd, spritesData.Translucent.ShadowCastingQuads);
		UploadSprites(cmd, spritesData.Translucent.NonShadowQuads);
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
			data.bReceivesDecals = sprite->DoesReceiveDecals();
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
		const uint32_t materialIndex = MaterialSystem::GetMaterialIndex(material);
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = vertices.emplace_back();
			vertex.TexCoords = UVs[i];
			vertex.EntityID = entityID;
			vertex.TransformIndex = transformIndex;
			vertex.MaterialIndex = materialIndex;
		}
	}

	// --------- Texts ---------
	template <typename TextDataType, typename VertexType>
	static void ProcessTextData(const TextDataType& component, std::unordered_map<Ref<Texture2D>, uint32_t>& fontAtlases,
		std::vector<VertexType>& vertices, uint32_t& atlasCurrentIndex)
	{
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

				const size_t q1Index = vertices.size();
				if constexpr (std::is_same_v<TextDataType, LitTextData>)
				{
					auto& q1 = vertices.emplace_back();
					q1.Position = glm::vec2(pl, pb);
					q1.MaterialIndex = component.MaterialIndex;
					q1.TexCoord = { l, b };
					q1.EntityID = component.EntityID;
					q1.AtlasIndex = atlasIndex;
					q1.TransformIndex = transformIndex;
				}
				else
				{
					auto& q1 = vertices.emplace_back();
					q1.Position = glm::vec2(pl, pb);
					q1.Color = component.Color;
					q1.TexCoord = { l, b };
					q1.EntityID = component.EntityID;
					q1.AtlasIndex = atlasIndex;
					q1.TransformIndex = component.TransformIndex;
				}

				const size_t q2Index = vertices.size();
				{
					auto& q2 = vertices.emplace_back();
					q2 = vertices[q1Index];
					q2.Position = glm::vec2(pr, pb);
					q2.TexCoord = { r, b };
				}

				const size_t q3Index = vertices.size();
				{
					auto& q3 = vertices.emplace_back();
					q3 = vertices[q1Index];
					q3.Position = glm::vec2(pr, pt);
					q3.TexCoord = { r, t };
				}

				const size_t q4Index = vertices.size();
				{
					auto& q4 = vertices.emplace_back();
					q4 = vertices[q1Index];
					q4.Position = glm::vec2(pl, pt);
					q4.TexCoord = { l, t };
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

	static void ProcessUnlitComponents(const std::vector<UnlitTextData>& textComponents, std::unordered_map<Ref<Texture2D>, uint32_t>& fontAtlases, UnlitTextGeometryData& geometryData, uint32_t& atlasCurrentIndex)
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

		std::vector<LitTextData> litTexts;
		std::vector<UnlitTextData> unlitTexts;
		std::unordered_map<uint32_t, uint64_t> tempTransformsIndices; // EntityID -> uint64_t (index to m_TextTransformIndices)
		std::vector<glm::mat4> tempTransforms;

		litTexts.reserve(texts.size());
		unlitTexts.reserve(texts.size());
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
				LitTextData& data = litTexts.emplace_back();
				data.Material = std::move(material);
				data.Text = Utils::ToUTF32(text->GetText());
				data.Font = asset->GetFont();
				data.EntityID = text->Parent.GetID();
				data.LineHeightOffset = text->GetLineSpacing();
				data.KerningOffset = text->GetKerning();
				data.MaxWidth = text->GetMaxWidth();
				data.TransformIndex = transformIndex | (text->DoesReceiveDecals() ? (1 << 31) : 0u);
				data.MaterialIndex = MaterialSystem::GetMaterialIndex(data.Material);
				data.bCastsShadows = text->DoesCastShadows();
			}
			else
			{
				auto& data = unlitTexts.emplace_back();
				data.TransformIndex = transformIndex;
				data.Text = Utils::ToUTF32(text->GetText());
				data.Font = asset->GetFont();
				data.Color = text->GetColor();
				data.EntityID = text->Parent.GetID();
				data.LineHeightOffset = text->GetLineSpacing();
				data.KerningOffset = text->GetKerning();
				data.MaxWidth = text->GetMaxWidth();
				data.bCastsShadows = text->DoesCastShadows();
				data.bDoubleSided = text->IsDoubleSided();
			}
			tempTransformsIndices.emplace(text->Parent.GetID(), transformIndex);
			tempTransforms.emplace_back(Math::ToTransformMatrix(text->GetWorldTransform()));
		}

		RenderManager::Submit([task = shared_from_this(), litTextComponents = std::move(litTexts), unlitTextComponents = std::move(unlitTexts),
			transforms = std::move(tempTransforms), transformsIndices = std::move(tempTransformsIndices)](Ref<CommandBuffer>&) mutable
		{
			auto thisRef = Cast<GeometryManagerTask>(task);
			thisRef->bUploadTextQuads = true;
			thisRef->bUploadTextTransforms = true;

			thisRef->m_SingleSidedTexts.Clear();
			thisRef->m_DoubleSidedTexts.Clear();
			thisRef->m_SingleSidedUnlitTexts.Clear();
			thisRef->m_DoubleSidedUnlitTexts.Clear();

			thisRef->m_TextTransforms = std::move(transforms);
			thisRef->m_TextTransformIndices = std::move(transformsIndices);
			thisRef->m_LitTexts = std::move(litTextComponents);
			thisRef->m_UnlitTexts = std::move(unlitTextComponents);
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

	void GeometryManagerTask::SortTexts()
	{
		EG_CPU_TIMING_SCOPED("Sort lit texts based on Blend Mode");

		m_SingleSidedTexts.Clear();
		m_DoubleSidedTexts.Clear();
		m_FontAtlases.clear();
		m_Atlases.clear();

		uint32_t atlasCurrentIndex = 0;
		{
			const size_t textsCount = m_LitTexts.size();
			for (size_t i = 0; i < textsCount; ++i)
			{
				const auto& text = m_LitTexts[i];
				const Material::BlendMode blendMode = text.Material ? text.Material->GetBlendMode() : Material::BlendMode::Opaque;
				const bool bDoubleSided = text.Material ? text.Material->IsDoubleSided() : false;
				auto& textsData = bDoubleSided ? m_DoubleSidedTexts : m_SingleSidedTexts;

				switch (blendMode)
				{
					case Material::BlendMode::Opaque:
					{
						if (text.bCastsShadows)
							ProcessTextData(text, m_FontAtlases, textsData.Opaque.ShadowCastingQuads.QuadVertices, atlasCurrentIndex);
						else
							ProcessTextData(text, m_FontAtlases, textsData.Opaque.NonShadowQuads.QuadVertices, atlasCurrentIndex);
						break;
					}
					case Material::BlendMode::Translucent:
					{
						if (text.bCastsShadows)
							ProcessTextData(text, m_FontAtlases, textsData.Translucent.ShadowCastingQuads.QuadVertices, atlasCurrentIndex);
						else
							ProcessTextData(text, m_FontAtlases, textsData.Translucent.NonShadowQuads.QuadVertices, atlasCurrentIndex);
						break;
					}
					case Material::BlendMode::Masked:
					{
						if (text.bCastsShadows)
							ProcessTextData(text, m_FontAtlases, textsData.Masked.ShadowCastingQuads.QuadVertices, atlasCurrentIndex);
						else
							ProcessTextData(text, m_FontAtlases, textsData.Masked.NonShadowQuads.QuadVertices, atlasCurrentIndex);
						break;
					}
					default: EG_CORE_ASSERT("Unknown blend mode!");
				}
			}
		}

		{
			const size_t textsCount = m_UnlitTexts.size();
			for (size_t i = 0; i < textsCount; ++i)
			{
				const auto& text = m_UnlitTexts[i];
				auto& textsData = text.bDoubleSided ? m_DoubleSidedUnlitTexts : m_SingleSidedUnlitTexts;
				if (text.bCastsShadows)
					ProcessTextData(text, m_FontAtlases, textsData.Opaque.ShadowCastingQuads.QuadVertices, atlasCurrentIndex);
				else
					ProcessTextData(text, m_FontAtlases, textsData.Opaque.NonShadowQuads.QuadVertices, atlasCurrentIndex);
			}
		}

		m_Atlases.resize(atlasCurrentIndex);
		for (auto& atlas : m_FontAtlases)
			m_Atlases[atlas.second] = atlas.first;
	}

	void GeometryManagerTask::UploadTexts(const Ref<CommandBuffer>& cmd, const QuadsRenderData<LitTextGeometryData>& textsData)
	{
		UploadTexts(cmd, textsData.Opaque.ShadowCastingQuads);
		UploadTexts(cmd, textsData.Opaque.NonShadowQuads);
		UploadTexts(cmd, textsData.Masked.ShadowCastingQuads);
		UploadTexts(cmd, textsData.Masked.NonShadowQuads);
		UploadTexts(cmd, textsData.Translucent.ShadowCastingQuads);
		UploadTexts(cmd, textsData.Translucent.NonShadowQuads);
	}

	void GeometryManagerTask::UploadTexts(const Ref<CommandBuffer>& cmd, const LitTextGeometryData& textsData)
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
			Utils::UploadIndexBuffer(cmd, ib);
		}

		cmd->Write(vb, quads.data(), currentVertexSize, 0, vb->GetLayout(), BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void GeometryManagerTask::UploadTexts(const Ref<CommandBuffer>& cmd, const QuadsRenderData<UnlitTextGeometryData>& textsData)
	{
		UploadTexts(cmd, textsData.Opaque.ShadowCastingQuads);
		UploadTexts(cmd, textsData.Opaque.NonShadowQuads);
		UploadTexts(cmd, textsData.Masked.ShadowCastingQuads);
		UploadTexts(cmd, textsData.Masked.NonShadowQuads);
		UploadTexts(cmd, textsData.Translucent.ShadowCastingQuads);
		UploadTexts(cmd, textsData.Translucent.NonShadowQuads);
	}

	void GeometryManagerTask::UploadTexts(const Ref<CommandBuffer>& cmd, const UnlitTextGeometryData& textsData)
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
			Utils::UploadIndexBuffer(cmd, ib);
		}

		cmd->Write(vb, quads.data(), currentVertexSize, 0, vb->GetLayout(), BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}
}
