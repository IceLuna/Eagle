#include "egpch.h"
#include "ParticleSystemTask.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include <glm/gtc/packing.hpp>

#include "../../Eagle-Editor/assets/shaders/particle_system/common.h"

namespace Eagle
{
	namespace Utils
	{
		// For each component of v, returns -1 if the component is < 0, else 1
		static glm::vec2 SignNotZero(glm::vec2 v)
		{
			return glm::vec2(
				v.x >= 0.f ? 1.0f : -1.0f,
				v.y >= 0.f ? 1.0f : -1.0f
			);
		}

		// Packs a 3-component normal to 2 f16-channels using octahedron normals
		static uint32_t PackNormal(vec3 v)
		{
			float x = abs(v.x) + abs(v.y) + abs(v.z);
			v.x /= x;
			v.y /= x;

			if (v.z <= 0)
			{
				vec2 x = (vec2(1.0) - abs(vec2(v.y, v.x))) * SignNotZero(vec2(v.x, v.y));
				v.x = x.x;
				v.y = x.y;
			}

			return glm::packHalf2x16(v);
		}

		static uint32_t PackEmitterFlags(const ParticleEmitter& emitter)
		{
			uint32_t flags = 0;
			flags |= emitter.bExplode ? Emitter_Explode_Mask : 0;
			flags |= emitter.bApplyGravity ? Emitter_ApplyGravity_Mask : 0;
			flags |= emitter.bAlphaBlending ? Emitter_AlphaBlending_Mask : 0;
			flags |= emitter.bEmit ? Emitter_Enabled_Mask : 0;
			flags |= emitter.bAdditive ? Emitter_AdditiveBlending_Mask : 0;
			flags |= emitter.bBlendAnimation ? Emitter_BlendAnimation_Mask : 0;
			flags |= emitter.bDestroyImmediately ? Emitter_DestroyImmediately_Mask : 0;
			flags |= emitter.bFaceDirection ? Emitter_FaceDirection_Mask : 0;

			return flags;
		}

		static void ToGPUEmitter(const ParticleEmitter& emitter, const ParticleSystemTask::EmitterData& emitterData, const ParticleSystemTask::MeshEmitterData& meshEmitterData, Emitter& outData)
		{
			outData.TransformIndex = emitterData.TransformIndex;
			outData.AABBMin = emitter.VisibilityAABB.Min;
			outData.AABBMax = emitter.VisibilityAABB.Max;
			outData.CollisionType = uint32_t(emitter.CollisionMode);
			outData.EmissionShape = uint32_t(emitter.EmissionShape);
			outData.ColorStart = emitter.ColorStart;
			outData.ColorEnd = emitter.ColorEnd;
			outData.VelocityMin = emitter.VelocityMin;
			outData.VelocityMax = emitter.VelocityMax;
			outData.VelocityCoefStart = emitter.VelocityCoefStart;
			outData.VelocityCoefEnd = emitter.VelocityCoefEnd;
			outData.NumParticles = uint32_t(float(emitter.NumParticles) * emitter.NumParticlesRatio);
			outData.RotationZStart = glm::radians(emitter.RotationZStart);
			outData.RotationZEnd = glm::radians(emitter.RotationZEnd);
			outData.LoopCount = emitter.LoopCount;
			outData.SizeStart = emitter.SizeStart;
			outData.SizeEnd = emitter.SizeEnd;
			outData.RingRadius = emitter.RingRadius;
			outData.BouncinessMin = emitter.BouncinessMin;
			outData.ColliderSizeRatio = emitter.ColliderSizeRatio;
			outData.BouncinessMax = emitter.BouncinessMax;
			outData.RadialAcceleration = emitter.RadialAcceleration;
			outData.TangentialAcceleration = emitter.TangentialAcceleration;
			outData.RingThickness = emitter.RingThickness;
			outData.RadialAcceleration = emitter.RadialAcceleration;
			outData.LifetimeMin = emitter.LifetimeMin;
			outData.LifetimeMax = emitter.LifetimeMax;
			outData.TangentialAcceleration = emitter.TangentialAcceleration;
			outData.SphereRadius = emitter.SphereRadius;
			outData.BoxMin = emitter.BoxMin;
			outData.BoxMax = emitter.BoxMax;
			outData.Flags = PackEmitterFlags(emitter);
			outData.TextureIndex = emitter.Texture ? TextureSystem::AddTexture(emitter.Texture->GetTexture()) : 0u;
			outData.AnimationImagesNum = emitter.AnimationImagesNum;
			outData.AnimationSpeed = emitter.AnimationSpeed;
			outData.VertexOffset = meshEmitterData.VertexOffset;
			outData.IndexOffset = meshEmitterData.IndexOffset;
			outData.IndexCount = meshEmitterData.IndexCount;
			outData.NormalVelocityFactor = emitter.NormalVelocityFactor;
			outData.AnimationOffset = emitterData.AnimationOffset;

			if (outData.NumParticles == 0u)
			{
				// Disable emitter
				outData.Flags = outData.Flags & (~Emitter_Enabled_Mask);
			}

			// Needed so it spawns particles on the first update
			{
				const float spawnInterval = emitter.bExplode ? outData.LifetimeMax : outData.LifetimeMax / float(outData.NumParticles);
				outData.DeltaTime = spawnInterval;
				outData.WasExplode = emitter.bExplode ? 1u : 0u;
			}
			outData.IsVisible = 0u;
			outData.SpawnedSoFar = 0u;
			outData.LoopIteration = 0u;
		}
	
		static ParticleSystemTask::DecompositedTransform Decompose(const glm::mat4& mat)
		{
			const Transform tr = Math::DecomposeTransformMatrix(mat);

			ParticleSystemTask::DecompositedTransform decomposited;
			decomposited.ScaleX = tr.Scale3D.x;
			decomposited.ScaleY = tr.Scale3D.y;
			decomposited.RotationZ = tr.Rotation.EulerAngles().z;

			return decomposited;
		}
	
		template <typename MeshType, typename ParticleVertex>
		static void RebuildMeshData(const Ref<CommandBuffer>& cmd, std::unordered_map<Ref<MeshType>, ParticleSystemTask::MeshEmitterData>& meshDataMapping,
			std::vector<ParticleVertex>& vertices, std::vector<Index>& indices, Ref<Buffer>& vertexBuffer, Ref<Buffer>& indexBuffer)
		{
			vertices.clear();
			indices.clear();

			// Go through all emitter meshes and collect mesh data
			for (auto& [mesh, data] : meshDataMapping)
			{
				const auto& meshVertices = mesh->GetVertices();

				data.VertexOffset = (uint32_t)vertices.size();
				data.IndexOffset = (uint32_t)indices.size();
				data.IndexCount = 0u;

				for (const auto& vertex : meshVertices)
				{
					auto& newVertex = vertices.emplace_back();
					newVertex.Position = vertex.Position;
					newVertex.Normal = Utils::PackNormal(vertex.Normal);
					if constexpr (std::is_same<ParticleSystemTask::ParticleSkeletalMeshVertex, ParticleVertex>::value)
					{
						for (uint32_t i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
						{
							newVertex.Weights[i] = vertex.Weights[i];
							newVertex.BoneIDs[i] = vertex.BoneID[i];
						}
					}
				}

				const uint32_t indicesBuffersCount = mesh->GetMaterialSlotsCount();
				for (uint32_t i = 0; i < indicesBuffersCount; ++i)
				{
					const auto& meshIndices = mesh->GetIndices(i);
					data.IndexCount += (uint32_t)meshIndices.size();
					for (const auto& index : meshIndices)
					{
						indices.emplace_back(index);
					}
				}
			}

			// Upload mesh data to GPU
			{
				const size_t verticesSize = vertices.size() * sizeof(ParticleVertex);
				const size_t indicesSize = indices.size() * sizeof(Index);

				if (vertexBuffer->GetSize() < verticesSize)
				{
					vertexBuffer->Resize(verticesSize * 3u / 2u);
				}
				if (indexBuffer->GetSize() < indicesSize)
				{
					indexBuffer->Resize(indicesSize * 3u / 2u);
				}

				if (verticesSize > 0)
					cmd->Write(vertexBuffer, vertices.data(), verticesSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
				if (indicesSize > 0)
					cmd->Write(indexBuffer, indices.data(), indicesSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			}
		}
	}

	ParticleSystemTask::ParticleSystemTask(SceneRenderer& renderer)
		: RendererTask(renderer)
		, m_SortTranslucent(m_MaxParticles, true, true)
	{
		m_Size = m_Renderer.GetViewportSize();
		m_StaticMeshVertices.reserve(1024u);
		m_StaticMeshIndices.reserve(1024u);
		m_SkeletalMeshVertices.reserve(1024u);
		m_SkeletalMeshIndices.reserve(1024u);
		bSortOpaque = m_Renderer.GetOptions().bSortOpaqueParticles;

		InitSortOpaqueResources();
		InitPipelines();
		InitResources();
	}

	void ParticleSystemTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System");
		EG_CPU_TIMING_SCOPED("Particle System");

		Update(cmd);
		PreparePass(cmd);
		EmitPass(cmd);
		SimulatePass(cmd);

		if (bSortOpaque)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Particle System. Sort Opaque");
			EG_CPU_TIMING_SCOPED("Particle System. Sort Opaque");
			m_SortOpaque->RecordCommandBuffer(cmd, m_OpaqueDistancesBuffer, m_DrawArgs, offsetof(DrawIndirectArgs, InstanceCount), 0u, m_OpaqueIndicesToRender);
		}

		{
			EG_GPU_TIMING_SCOPED(cmd, "Particle System. Sort Translucent");
			EG_CPU_TIMING_SCOPED("Particle System. Sort Translucent");
			m_SortTranslucent.RecordCommandBuffer(cmd, m_TranslucentDistancesBuffer, m_DrawArgs, sizeof(DrawIndirectArgs) + offsetof(DrawIndirectArgs, InstanceCount), 0u, m_TranslucentIndicesToRender);
		}
		cmd->TransitionLayout(m_DrawArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument | BufferReadAccess::Uniform);

		RenderPass(cmd);

		m_PingPong = 1u - m_PingPong;
	}

	void ParticleSystemTask::OnResize(glm::uvec2 size)
	{
		m_Size = size;
		m_BillboardRender->Resize(size);
		m_BillboardRenderTranslucent->Resize(size);
	}

	void ParticleSystemTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (bSortOpaque == settings.bSortOpaqueParticles)
			return;

		bSortOpaque = settings.bSortOpaqueParticles;
		InitSortOpaqueResources();
	}

	void ParticleSystemTask::Update(const Ref<CommandBuffer>& cmd)
	{
		// 1. Update mesh data
		// 2. Check if GPU Emitters buffer is big enough and allocate enough memory if required
		// 3. Process emitters that need to be added
		// 4. Process emitters that need to be removed. Needs to be executed after Step 3 because an emitter might require one more update
		// 5. Process emitters that need to be updated
		// 6. Upload animation data to GPU
		// 7. Update transforms if required
		// 8. Check if GPU Particles buffer is big enough and allocate enough memory if required

		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Update");
		EG_CPU_TIMING_SCOPED("Particle System. Update");
		bool bEmittersChangedOrAdded = false;

		// Step 1
		if (bRebuildStaticMeshData || bRebuildSkeletalMeshData)
		{
			if (bRebuildStaticMeshData)
			{
				Utils::RebuildMeshData(cmd, m_StaticMeshDataMapping, m_StaticMeshVertices, m_StaticMeshIndices, m_StaticMeshVertexBuffer, m_StaticMeshIndexBuffer);
				bRebuildStaticMeshData = false;
			}
			if (bRebuildSkeletalMeshData)
			{
				Utils::RebuildMeshData(cmd, m_SkeletalMeshDataMapping, m_SkeletalMeshVertices, m_SkeletalMeshIndices, m_SkeletalMeshVertexBuffer, m_SkeletalMeshIndexBuffer);
				bRebuildSkeletalMeshData = false;
			}
		}

		// Step 2
		{
			const size_t numEmittersAfterUpdate = m_NumEmitters + m_EmittersToAdd.size();
			size_t currentSize = m_EmittersBuffer->GetSize();
			size_t newSize = numEmittersAfterUpdate * sizeof(Emitter);
			if (newSize > currentSize)
			{
				newSize = (newSize * 12) / 10; // Resize policy: increase by 20%
				BufferSpecifications specs = m_EmittersBuffer->GetSpecs();
				specs.Size = newSize;

				Ref<Buffer> newBuffer = Buffer::Create(specs, m_EmittersBuffer->GetDebugName());
				cmd->CopyBuffer(m_EmittersBuffer, newBuffer, 0, 0, m_NumEmitters * sizeof(Emitter));
				m_EmittersBuffer = std::move(newBuffer);
			}

			currentSize = m_EmittersSpawnCountBuffer->GetSize();
			newSize = numEmittersAfterUpdate * sizeof(uint32_t);
			if (newSize > currentSize)
			{
				newSize = (newSize * 12) / 10; // Resize policy: increase by 20%
				m_EmittersSpawnCountBuffer->Resize(newSize);
			}
		}

		// Step 3
		if (m_EmittersToAdd.size())
		{
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);

			const size_t count = m_EmittersToAdd.size();
			for (size_t i = 0; i < count; ++i)
			{
				const auto& emitterToAdd = m_EmittersToAdd[i].Emitter;
				auto& itEmitters = m_SystemToEmittersMapping.at(m_EmittersToAdd[i].SystemID);
				auto& emitterData = itEmitters.at(emitterToAdd);

				Emitter emitter;
				Utils::ToGPUEmitter(emitterToAdd, emitterData, GetEmitterMeshData(emitterToAdd), emitter);

				uint32_t insertIndex = m_NumEmitters;
				for (auto it = m_DeadEmitters.begin(); it != m_DeadEmitters.end(); ++it) // Find the first available slot
				{
					const auto& deadEmitter = *it;
					if (deadEmitter.IsDead())
					{
						const uint32_t emitterIndex = deadEmitter.Data.EmitterIndex;
						m_FreeTransformSlots.push_back(deadEmitter.Data.TransformIndex);
						m_DeadEmitters.erase(it);

						if (emitterIndex != s_InvalidEmitterIndex)
						{
							insertIndex = emitterIndex;
							break;
						}
					}
				}
				
				if (insertIndex == m_NumEmitters)
					m_NumEmitters++; // There were no free slots

				const size_t offset = insertIndex * sizeof(Emitter);
				cmd->WriteTransitionless(m_EmittersBuffer, &emitter, sizeof(Emitter), offset);

				emitterData.EmitterIndex = insertIndex;
			}
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

			m_EmittersToAdd.clear();

			bUpdateTransforms = true;
			bEmittersChangedOrAdded = true;
		}

		// Step 4
		if (m_EmittersToRemove.size())
		{
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);
			for (const auto& [emitter, removingData] : m_EmittersToRemove)
			{
				ParticleEmitter disabledEmitter = emitter;
				disabledEmitter.bEmit = false;

				const uint32_t flags = Utils::PackEmitterFlags(disabledEmitter);
				const size_t offset = removingData.EmitterIndex * sizeof(Emitter) + offsetof(Emitter, Flags);
				cmd->WriteTransitionless(m_EmittersBuffer, &flags, sizeof(uint32_t), offset);

				auto& dead = m_DeadEmitters.emplace_back();
				dead.Data = removingData;
				dead.TimeTillDead = emitter.bDestroyImmediately ? 0.f : emitter.LifetimeMax;
				dead.TimeOfDeath = std::chrono::high_resolution_clock::now();
			}
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

			m_EmittersToRemove.clear();
		}

		// Step 5
		if (m_EmittersToUpdate.size())
		{
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);

			for (const auto& [emitter, emitterData] : m_EmittersToUpdate)
			{
				Emitter gpuEmitter;
				Utils::ToGPUEmitter(emitter, emitterData, GetEmitterMeshData(emitter), gpuEmitter);

				const size_t sizeToUpdate = offsetof(Emitter, WorldPos); // We're updating the data before the 'WorldPos' because everything after is an internal state
				const size_t offset = emitterData.EmitterIndex * sizeof(Emitter);
				cmd->WriteTransitionless(m_EmittersBuffer, &gpuEmitter, sizeToUpdate, offset);
			}

			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

			m_EmittersToUpdate.clear();

			bUpdateTransforms = true;
			bEmittersChangedOrAdded = true;
		}

		// Step 6
		UpdateSkeletalAnimations(cmd);

		// Step 7
		if (bUpdateTransforms)
		{
			{
				const size_t size = m_Transforms.size() * sizeof(glm::mat4);
				if (size > m_TransformsBuffer->GetSize())
				{
					const size_t newSize = (size * 12) / 10; // Resize policy: increase by 20%
					m_TransformsBuffer->Resize(newSize);
				}
				cmd->Write(m_TransformsBuffer, m_Transforms.data(), size, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			}
			{
				const size_t size = m_DecompositedTransforms.size() * sizeof(DecompositedTransform);
				if (size > m_DecompositedTransformsBuffer->GetSize())
				{
					const size_t newSize = (size * 12) / 10; // Resize policy: increase by 20%
					m_DecompositedTransformsBuffer->Resize(newSize);
				}
				cmd->Write(m_DecompositedTransformsBuffer, m_DecompositedTransforms.data(), size, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			}
			bUpdateTransforms = false;
		}
		
		// Step 8
		if (bEmittersChangedOrAdded)
		{
			uint32_t maxParticles = 0;
			for (const auto& [_, emitters] : m_SystemToEmittersMapping)
				for (const auto& [emitter, _] : emitters)
					maxParticles += uint32_t(float(emitter.NumParticles) * emitter.NumParticlesRatio);

			if (maxParticles > m_MaxParticles)
			{
				SetMaxParticles(cmd, maxParticles);
			}
		}
	}

	void ParticleSystemTask::UpdateSkeletalAnimations(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Update skeletam mesh animations");
		EG_CPU_TIMING_SCOPED("Particle System. Update skeletam mesh animations");

		ParticleEmitter dummy;

		const auto& systemTransforms = m_Renderer.GetSkeletalParticleAnimationTransforms();
		m_AnimationTransforms.clear();

		cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);
		for (const auto& [systemID, perEmitterTransforms] : systemTransforms)
		{
			auto it = m_SystemToEmittersMapping.find(systemID);
			if (it == m_SystemToEmittersMapping.end())
				continue;

			auto& emittersData = it->second;
			for (const auto& [emitterID, transforms] : perEmitterTransforms)
			{
				dummy.ID = emitterID;
				auto it = emittersData.find(dummy);
				if (it == emittersData.end())
					continue;

				auto& emitterData = it->second;
				emitterData.AnimationOffset = uint32_t(m_AnimationTransforms.size());
				m_AnimationTransforms.insert(m_AnimationTransforms.end(), transforms.begin(), transforms.end());

				// We need to update animation offset because animation or skeletal mesh can change any time (which will invalidate offsets of other emitters)
				const size_t offset = emitterData.EmitterIndex * sizeof(Emitter) + offsetof(Emitter, AnimationOffset);
				cmd->WriteTransitionless(m_EmittersBuffer, &emitterData.AnimationOffset, sizeof(emitterData.AnimationOffset), offset);
			}
		}
		cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

		if (!m_AnimationTransforms.empty())
		{
			const size_t size = m_AnimationTransforms.size() * sizeof(glm::mat4);
			if (size > m_AnimationTransformsBuffer->GetSize())
			{
				const size_t newSize = (size * 12) / 10; // Resize policy: increase by 20%
				m_AnimationTransformsBuffer->Resize(newSize);
			}
			cmd->Write(m_AnimationTransformsBuffer, m_AnimationTransforms.data(), size, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		}
	}

	void ParticleSystemTask::PreparePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Prepare");
		EG_CPU_TIMING_SCOPED("Particle System. Prepare");

		struct PushData
		{
			glm::mat4 ViewProj;
			uint32_t PreSimIndex;
			uint32_t PostSimIndex;
			uint32_t NumEmitters;
			float DeltaTime;
			struct CullingFrustum
			{
				float near_right;
				float near_top;
				float near_plane;
				float far_plane;
			} Frustum;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewMatrix();
		pushData.PreSimIndex = m_PingPong;
		pushData.PostSimIndex = 1u - m_PingPong;
		pushData.NumEmitters = m_NumEmitters;
		pushData.DeltaTime = Application::Get().GetTimestep();

		const float tanFov = std::tan(0.5f * m_Renderer.GetFOV());
		const float nearPlane = m_Renderer.GetZNear();
		const float farPlane = m_Renderer.GetZFar();
		const float aspectRatio = float(m_Size.x) / m_Size.y;

		pushData.Frustum =
		{
			aspectRatio * nearPlane * tanFov,
			nearPlane * tanFov,
			-nearPlane,
			-farPlane,
		};

		m_PrepareData->SetBuffer(m_SystemData, 0, 0);
		m_PrepareData->SetBuffer(m_DrawArgs, 0, 1);
		m_PrepareData->SetBuffer(m_DispatchArgs, 0, 2);
		m_PrepareData->SetBuffer(m_EmittersBuffer, 0, 3);
		m_PrepareData->SetBuffer(m_EmittersSpawnCountBuffer, 0, 4);
		m_PrepareData->SetBuffer(m_TransformsBuffer, 0, 5);

		cmd->TransitionLayout(m_DrawArgs, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(m_DispatchArgs, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

		cmd->Dispatch(m_PrepareData, 1, 1, 1, &pushData);

		cmd->Barrier(m_SystemData);
		cmd->Barrier(m_DrawArgs);
		cmd->Barrier(m_EmittersBuffer);
		cmd->Barrier(m_EmittersSpawnCountBuffer);
		cmd->TransitionLayout(m_DispatchArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);
	}

	void ParticleSystemTask::EmitPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Emit");
		EG_CPU_TIMING_SCOPED("Particle System. Emit");

		struct PushData
		{
			uint32_t PreSimIndex;
			uint32_t FrameNumber;
			uint32_t NumEmitters;
			uint32_t MaxParticles;
		} pushData;

		pushData.PreSimIndex = m_PingPong;
		pushData.FrameNumber = (uint32_t)RenderManager::GetFrameNumber_RT();
		pushData.NumEmitters = m_NumEmitters;
		pushData.MaxParticles = m_MaxParticles;

		m_Emit->SetBuffer(m_SystemData, 0, 0);
		m_Emit->SetBuffer(m_ParticlesBuffer, 0, 1);
		m_Emit->SetBuffer(m_EmittersBuffer, 0, 2);
		m_Emit->SetBuffer(m_DeadIndices, 0, 3);
		m_Emit->SetBuffer(m_AliveIndices[m_PingPong], 0, 4);
		m_Emit->SetBuffer(m_EmittersSpawnCountBuffer, 0, 5);
		m_Emit->SetBuffer(m_TransformsBuffer, 0, 6);
		m_Emit->SetBuffer(m_StaticMeshVertexBuffer, 0, 7);
		m_Emit->SetBuffer(m_StaticMeshIndexBuffer, 0, 8);
		m_Emit->SetBuffer(m_SkeletalMeshVertexBuffer, 0, 9);
		m_Emit->SetBuffer(m_SkeletalMeshIndexBuffer, 0, 10);
		m_Emit->SetBuffer(m_AnimationTransformsBuffer, 0, 11);
		m_Emit->SetBuffer(m_DecompositedTransformsBuffer, 0, 12);

		cmd->DispatchIndirect(m_Emit, m_DispatchArgs, 0, &pushData);

		cmd->Barrier(m_SystemData);
		cmd->Barrier(m_ParticlesBuffer);
		cmd->Barrier(m_DeadIndices);
		cmd->Barrier(m_AliveIndices[m_PingPong]);
	}

	void ParticleSystemTask::SimulatePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Simulate");
		EG_CPU_TIMING_SCOPED("Particle System. Simulate");

		struct PushData
		{
			glm::mat4 ViewProj;

			glm::vec3 Gravity;
			float CameraNear;

			float CameraFar;
			float DeltaTime;
			uint32_t PreSimIndex;
			uint32_t PostSimIndex;
			uint32_t MaxParticles;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();
		pushData.Gravity = m_Renderer.GetGravity();
		pushData.CameraNear = m_Renderer.GetZNear();
		pushData.CameraFar = m_Renderer.GetZFar();
		pushData.DeltaTime = Application::Get().GetTimestep();
		pushData.PreSimIndex = m_PingPong;
		pushData.PostSimIndex = 1 - m_PingPong;
		pushData.MaxParticles = m_MaxParticles;

		auto& gbuffer = m_Renderer.GetGBuffer();

		m_Simulate->SetBuffer(m_SystemData, 0, 0);
		m_Simulate->SetBuffer(m_ParticlesBuffer, 0, 1);
		m_Simulate->SetBuffer(m_EmittersBuffer, 0, 2);
		m_Simulate->SetBuffer(m_DeadIndices, 0, 3);
		m_Simulate->SetBuffer(m_AliveIndices[m_PingPong], 0, 4);
		m_Simulate->SetBuffer(m_AliveIndices[1 - m_PingPong], 0, 5);
		m_Simulate->SetBuffer(m_TranslucentIndicesToRender, 0, 6);
		m_Simulate->SetBuffer(m_TranslucentDistancesBuffer, 0, 7);
		m_Simulate->SetBuffer(m_DrawArgs, 0, 8);
		m_Simulate->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 9);
		m_Simulate->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 10);
		m_Simulate->SetBuffer(m_Renderer.GetCameraBuffer(), 0, 11);
		m_Simulate->SetBuffer(m_OpaqueIndicesToRender, 0, 12);
		if (bSortOpaque)
		{
			m_Simulate->SetBuffer(m_OpaqueDistancesBuffer, 0, 13);
		}

		const ImageLayout oldDepthLayout = gbuffer.Depth->GetLayout();
		cmd->TransitionLayout(gbuffer.Depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);

		cmd->DispatchIndirect(m_Simulate, m_DispatchArgs, sizeof(DispatchIndirectArgs), &pushData);

		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
		cmd->Barrier(m_SystemData);
		cmd->Barrier(m_ParticlesBuffer);
		cmd->Barrier(m_OpaqueIndicesToRender);
		if (bSortOpaque)
		{
			cmd->Barrier(m_OpaqueDistancesBuffer);
		}
		cmd->Barrier(m_TranslucentIndicesToRender);
		cmd->Barrier(m_TranslucentDistancesBuffer);
		cmd->Barrier(m_DrawArgs);
	}

	void ParticleSystemTask::RenderPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Render");
		EG_CPU_TIMING_SCOPED("Particle System. Render");

		struct PushData
		{
			glm::mat4 View;
			glm::mat4 Proj;
		} pushData;
		pushData.View = m_Renderer.GetViewMatrix();
		pushData.Proj = m_Renderer.GetProjectionMatrix();

		m_BillboardRender->SetBuffer(m_ParticlesBuffer, 0, 0);
		m_BillboardRender->SetBuffer(m_OpaqueIndicesToRender, 0, 1);

		m_BillboardRenderTranslucent->SetBuffer(m_ParticlesBuffer, 0, 0);
		m_BillboardRenderTranslucent->SetBuffer(m_TranslucentIndicesToRender, 0, 1);
		m_BillboardRenderTranslucent->SetBuffer(m_DrawArgs, 0, 2);

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_BillboardRender->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), 1, 0);
			m_BillboardRenderTranslucent->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), 1, 0);
			m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		cmd->BeginGraphics(m_BillboardRender);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndirect(m_DrawArgs, 0, 1, sizeof(DrawIndirectArgs));
		cmd->EndGraphics();

		cmd->BeginGraphics(m_BillboardRenderTranslucent);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndirect(m_DrawArgs, sizeof(DrawIndirectArgs), 1, sizeof(DrawIndirectArgs));
		cmd->EndGraphics();
	}

	void ParticleSystemTask::SetMaxParticles(const Ref<CommandBuffer>& cmd, uint32_t maxParticles)
	{
		if (maxParticles <= m_MaxParticles)
		{
			EG_CORE_ASSERT(false);
			return;
		}

		const uint32_t oldMaxParticles = m_MaxParticles;
		m_MaxParticles = (maxParticles * 11) / 10; // Resize policy: increase by 10%
		const uint32_t newParticlesAmount = m_MaxParticles - oldMaxParticles;

		// Recreate resources
		{
			const size_t newParticlesSize = m_MaxParticles * sizeof(PackedParticle);
			BufferSpecifications specs = m_ParticlesBuffer->GetSpecs();
			specs.Size = newParticlesSize;
			Ref<Buffer> newParticlesBuffer = Buffer::Create(specs, m_ParticlesBuffer->GetDebugName());

			specs.Size = m_MaxParticles * sizeof(uint32_t);
			Ref<Buffer> aliveIndices0 = Buffer::Create(specs, m_AliveIndices[0]->GetDebugName());
			Ref<Buffer> aliveIndices1 = Buffer::Create(specs, m_AliveIndices[1]->GetDebugName());
			Ref<Buffer> deadIndices = Buffer::Create(specs, m_DeadIndices->GetDebugName());
			if (bSortOpaque)
			{
				m_OpaqueDistancesBuffer = Buffer::Create(specs, m_OpaqueDistancesBuffer->GetDebugName());
			}
			m_OpaqueIndicesToRender = Buffer::Create(specs, m_OpaqueIndicesToRender->GetDebugName());
			m_TranslucentIndicesToRender = Buffer::Create(specs, m_TranslucentIndicesToRender->GetDebugName());
			m_TranslucentDistancesBuffer = Buffer::Create(specs, m_TranslucentDistancesBuffer->GetDebugName());

			cmd->CopyBuffer(m_ParticlesBuffer, newParticlesBuffer, 0, 0, m_ParticlesBuffer->GetSize());
			cmd->CopyBuffer(m_AliveIndices[0], aliveIndices0, 0, 0, m_AliveIndices[0]->GetSize());
			cmd->CopyBuffer(m_AliveIndices[1], aliveIndices1, 0, 0, m_AliveIndices[1]->GetSize());
			cmd->CopyBuffer(m_DeadIndices, deadIndices, 0, newParticlesAmount * sizeof(uint32_t), m_DeadIndices->GetSize());

			m_ParticlesBuffer = std::move(newParticlesBuffer);
			m_AliveIndices[0] = std::move(aliveIndices0);
			m_AliveIndices[1] = std::move(aliveIndices1);
			m_DeadIndices = std::move(deadIndices);
		}

		// Update particles data
		{
			m_UpdateMaxParticles->SetBuffer(m_SystemData, 0, 0);
			m_UpdateMaxParticles->SetBuffer(m_DeadIndices, 0, 1);

			glm::uvec2 pushData = { newParticlesAmount, oldMaxParticles };

			constexpr uint32_t tileSize = 256;
			const uint32_t numGroup = CalcNumGroups(newParticlesAmount, tileSize);
			cmd->Dispatch(m_UpdateMaxParticles, numGroup, 1, 1, &pushData);
			cmd->Barrier(m_SystemData);
			cmd->Barrier(m_DeadIndices);
		}
	
		if (bSortOpaque)
			m_SortOpaque = MakeScope<SortTask>(m_MaxParticles, true, true);
		m_SortTranslucent = SortTask(m_MaxParticles, true, true);
	}

	void ParticleSystemTask::AddEmitterMeshData(const ParticleEmitter& emitter)
	{
		if (emitter.EmissionShape != ParticleEmitter::EmissionShapeType::Mesh || !emitter.MeshAsset)
			return;

		const AssetType meshType = emitter.MeshAsset->GetAssetType();
		const bool bStaticMesh = meshType == AssetType::StaticMesh;
		if (bStaticMesh)
		{
			auto mesh = Cast<AssetStaticMesh>(emitter.MeshAsset)->GetMesh();
			auto it = m_StaticMeshDataMapping.find(mesh);
			if (it != m_StaticMeshDataMapping.end())
			{
				it->second.UsageCounter++;
			}
			else
			{
				m_StaticMeshDataMapping.emplace(std::move(mesh), MeshEmitterData{});
				bRebuildStaticMeshData = true;
			}
		}
		else
		{
			auto mesh = Cast<AssetSkeletalMesh>(emitter.MeshAsset)->GetMesh();
			auto it = m_SkeletalMeshDataMapping.find(mesh);
			if (it != m_SkeletalMeshDataMapping.end())
			{
				it->second.UsageCounter++;
			}
			else
			{
				m_SkeletalMeshDataMapping.emplace(std::move(mesh), MeshEmitterData{});
				bRebuildSkeletalMeshData = true;
			}
		}
	}

	void ParticleSystemTask::RemoveEmitterMeshData(const ParticleEmitter& emitter)
	{
		if (emitter.EmissionShape != ParticleEmitter::EmissionShapeType::Mesh || !emitter.MeshAsset)
			return;

		const AssetType meshType = emitter.MeshAsset->GetAssetType();
		const bool bStaticMesh = meshType == AssetType::StaticMesh;
		if (bStaticMesh)
		{
			auto mesh = Cast<AssetStaticMesh>(emitter.MeshAsset)->GetMesh();
			auto it = m_StaticMeshDataMapping.find(mesh);
			if (it != m_StaticMeshDataMapping.end())
			{
				it->second.UsageCounter--;
				if (it->second.UsageCounter == 0)
				{
					m_StaticMeshDataMapping.erase(it);
					bRebuildStaticMeshData = true;
				}
			}
		}
		else
		{
			auto mesh = Cast<AssetSkeletalMesh>(emitter.MeshAsset)->GetMesh();
			auto it = m_SkeletalMeshDataMapping.find(mesh);
			if (it != m_SkeletalMeshDataMapping.end())
			{
				it->second.UsageCounter--;
				if (it->second.UsageCounter == 0)
				{
					m_SkeletalMeshDataMapping.erase(it);
					bRebuildSkeletalMeshData = true;
				}
			}
		}
	}

	ParticleSystemTask::MeshEmitterData ParticleSystemTask::GetEmitterMeshData(const ParticleEmitter& emitter)
	{
		if (emitter.EmissionShape != ParticleEmitter::EmissionShapeType::Mesh || !emitter.MeshAsset)
			return {};

		const AssetType meshType = emitter.MeshAsset->GetAssetType();
		const bool bStaticMesh = meshType == AssetType::StaticMesh;
		if (bStaticMesh)
		{
			auto mesh = Cast<AssetStaticMesh>(emitter.MeshAsset)->GetMesh();
			auto it = m_StaticMeshDataMapping.find(mesh);
			EG_CORE_ASSERT(it != m_StaticMeshDataMapping.end());
			return it->second;
		}
		else
		{
			auto mesh = Cast<AssetSkeletalMesh>(emitter.MeshAsset)->GetMesh();
			auto it = m_SkeletalMeshDataMapping.find(mesh);
			EG_CORE_ASSERT(it != m_SkeletalMeshDataMapping.end());
			return it->second;
		}
	}

	bool ParticleSystemTask::AddEmitter(const ParticleEmitter& emitter, const GUID& systemID, const glm::mat4& transform)
	{
		if (auto it = m_SystemToEmittersMapping.find(systemID); it != m_SystemToEmittersMapping.end())
		{
			const auto& emitters = it->second;
			if (emitters.find(emitter) != emitters.end())
			{
				// Trying to add already existing emitter
				EG_CORE_ASSERT(false); // Shouldn't really happen
				return false; // Already exists
			}
		}

		m_EmittersToAdd.emplace_back(AddingEmitterData{ emitter, systemID });

		uint32_t transformIndex = 0;
		if (m_FreeTransformSlots.empty())
		{
			transformIndex = (uint32_t)m_Transforms.size();
			m_Transforms.emplace_back();
			m_DecompositedTransforms.emplace_back();
		}
		else
		{
			transformIndex = m_FreeTransformSlots.back();
			m_FreeTransformSlots.pop_back();
		}
		m_Transforms[transformIndex] = transform * Math::ToTransformMatrix(emitter.RelativeTransform);
		m_DecompositedTransforms[transformIndex] = Utils::Decompose(m_Transforms[transformIndex]);
		auto& emitters = m_SystemToEmittersMapping[systemID];
		emitters[emitter] = EmitterData{ s_InvalidEmitterIndex, transformIndex, s_InvalidEmitterIndex }; // Emitter index will be set later
		AddEmitterMeshData(emitter);

		return true;
	}

	bool ParticleSystemTask::RemoveEmitter(const ParticleEmitter& emitter, const GUID& systemID, bool bForceImmediateRemoval)
	{
		auto itSystem = m_SystemToEmittersMapping.find(systemID);
		if (itSystem == m_SystemToEmittersMapping.end())
		{
			EG_CORE_ASSERT(false, "Non-existing system");
			return false; // Not found
		}

		auto& emitters = itSystem->second;
		auto it = emitters.find(emitter);
		if (it == emitters.end())
		{
			EG_CORE_ASSERT(false, "Trying to remove non-existing emitter");
			return false; // Not found
		}

		auto& data = m_EmittersToRemove.emplace_back();
		data.first = emitter;
		data.second.EmitterIndex = it->second.EmitterIndex;
		data.second.TransformIndex = it->second.TransformIndex;
		data.first.bDestroyImmediately |= bForceImmediateRemoval;
		RemoveEmitterMeshData(emitter);

		emitters.erase(it);

		return true;
	}

	void ParticleSystemTask::AddParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		struct SystemUpdateData
		{
			std::vector<ParticleEmitter> Emitters;
			glm::mat4 Transformation;
			GUID SystemID;
		};
		std::vector<SystemUpdateData> updateData;
		updateData.reserve(systems.size());
		for (const auto& system : systems)
		{
			const auto& asset = system->GetAsset();
			if (!asset)
				continue;

			auto& data = updateData.emplace_back();
			data.Emitters = asset->GetEmitters();
			data.Transformation = Math::ToTransformMatrix(system->GetWorldTransform());
			data.SystemID = system->GetSystemID();
		}

		if (updateData.empty())
			return;

		RenderManager::Submit([task = shared_from_this(), updateData = std::move(updateData)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& [emitters, transform, systemID] : updateData)
			{
				if (emitters.empty())
				{
					thisRef->m_SystemToEmittersMapping.emplace(systemID, std::unordered_map<ParticleEmitter, EmitterData>{});
					continue;
				}
				for (const auto& emitter : emitters)
				{
					thisRef->AddEmitter(emitter, systemID, transform);
				}
			}
		});
	}

	void ParticleSystemTask::UpdateParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		struct SystemUpdateData
		{
			std::vector<ParticleEmitter> Emitters;
			glm::mat4 Transformation;
			GUID SystemID;
		};
		std::vector<SystemUpdateData> updateData;
		updateData.reserve(systems.size());
		for (const auto& system : systems)
		{
			const auto& asset = system->GetAsset();
			if (!asset)
				continue;

			auto& data = updateData.emplace_back();
			data.Emitters = asset->GetEmitters();
			data.Transformation = Math::ToTransformMatrix(system->GetWorldTransform());
			data.SystemID = system->GetSystemID();
		}

		if (updateData.empty())
			return;

		RenderManager::Submit([task = shared_from_this(), updateData = std::move(updateData)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& [emitters, transform, systemID] : updateData)
			{
				auto itSystem = thisRef->m_SystemToEmittersMapping.find(systemID);
				if (itSystem == thisRef->m_SystemToEmittersMapping.end())
				{
					EG_CORE_ASSERT(false, "Trying to update non-existing system");
					continue;
				}
				auto systemEmitters = itSystem->second; // Intentional copy because `AddEmitter` and `RemoveEmitter` functions modify it

				// Remove emitters if not found in the new list
				for (const auto& [existingEmitter, _] : systemEmitters)
				{
					auto it2 = std::find(emitters.begin(), emitters.end(), existingEmitter);
					if (it2 == emitters.end()) // Old emitter isn't found in the new list, so remove it
						thisRef->RemoveEmitter(existingEmitter, systemID);
				}

				// Update or create emitters
				for (const auto& emitter : emitters)
				{
					if (systemEmitters.find(emitter) == systemEmitters.end())
					{
						thisRef->AddEmitter(emitter, systemID, transform); // New emitter isn't found in the old list, so add it
					}
					else
					{
						auto& existingEmitters = itSystem->second;
						auto it = existingEmitters.find(emitter);
						const auto& existingEmitter = it->first;
						{
							const EmitterData emitterData = it->second;

							// Check if should rebuild emitter mesh data
							const bool bMeshEmitter = existingEmitter.EmissionShape == ParticleEmitter::EmissionShapeType::Mesh;
							const bool bMeshEmitterChanged = (existingEmitter.EmissionShape != emitter.EmissionShape) ||
								(bMeshEmitter && existingEmitter.MeshAsset != emitter.MeshAsset);

							if (bMeshEmitterChanged)
							{
								thisRef->RemoveEmitterMeshData(existingEmitter);
								thisRef->AddEmitterMeshData(emitter);
							}

							// Update key
							existingEmitters.erase(it);
							existingEmitters.emplace(emitter, emitterData);

							// New emitter is found in the old list, so update its state
							thisRef->m_EmittersToUpdate.emplace_back(emitter, emitterData);
							thisRef->m_Transforms[emitterData.TransformIndex] = transform * Math::ToTransformMatrix(emitter.RelativeTransform);
							thisRef->m_DecompositedTransforms[emitterData.TransformIndex] = Utils::Decompose(thisRef->m_Transforms[emitterData.TransformIndex]);
						}
					}
				}
			}
		});
	}

	void ParticleSystemTask::RemoveParticleSystems(const std::unordered_set<GUID>& systems)
	{
		std::vector<GUID> removeData;
		removeData.reserve(systems.size());
		for (const auto& systemID : systems)
		{
			removeData.push_back(systemID);
		}

		if (removeData.empty())
			return;

		RenderManager::Submit([task = shared_from_this(), removeData = std::move(removeData)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& systemID : removeData)
			{
				auto it = thisRef->m_SystemToEmittersMapping.find(systemID);
				if (it == thisRef->m_SystemToEmittersMapping.end())
					continue;

				const auto& emitters = it->second;
				if (!emitters.empty())
				{
					auto copyEmitters = emitters;
					for (const auto& [emitter, _] : copyEmitters)
					{
						thisRef->RemoveEmitter(emitter, systemID);
					}
				}
				thisRef->m_SystemToEmittersMapping.erase(systemID);
			}
		});
	}

	void ParticleSystemTask::RemoveAllParticleSystems()
	{
		RenderManager::Submit([task = shared_from_this()](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& [systemID, emitters] : thisRef->m_SystemToEmittersMapping)
			{
				if (emitters.empty())
					continue;

				auto copyEmitters = emitters;
				for (const auto& [emitter, _] : copyEmitters)
				{
					thisRef->RemoveEmitter(emitter, systemID, true);
				}
			}
			thisRef->m_SystemToEmittersMapping.clear();
		});
	}

	void ParticleSystemTask::UpdateTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		struct UpdateTrData
		{
			glm::mat4 Transform;
			GUID EmitterID;
		};
		std::unordered_map<GUID, std::vector<UpdateTrData>> newTransforms;
		for (const auto& system : systems)
		{
			const auto& asset = system->GetAsset();
			if (!asset)
				continue;

			const auto& emitters = asset->GetEmitters();
			const glm::mat4 systemTr = Math::ToTransformMatrix(system->GetWorldTransform());
			auto& updateEmitters = newTransforms[system->GetSystemID()];
			for (const auto& emitter : emitters)
			{
				auto& data = updateEmitters.emplace_back();
				data.Transform = systemTr * Math::ToTransformMatrix(emitter.RelativeTransform);
				data.EmitterID = emitter.ID;
			}
		}

		if (newTransforms.empty())
			return;

		RenderManager::Submit([task = shared_from_this(), newTransforms = std::move(newTransforms)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);

			for (const auto& [systemID, datas] : newTransforms)
			{
				auto itSystem = thisRef->m_SystemToEmittersMapping.find(systemID);
				if (itSystem == thisRef->m_SystemToEmittersMapping.end())
					continue;

				for (const auto& [transform, emitterID] : datas)
				{
					ParticleEmitter dummy;
					dummy.ID = emitterID;

					auto& emitters = itSystem->second;
					auto it = emitters.find(dummy);
					if (it != emitters.end())
					{
						const uint32_t transformIndex = it->second.TransformIndex;
						thisRef->m_Transforms[transformIndex] = transform;
						thisRef->m_DecompositedTransforms[transformIndex] = Utils::Decompose(transform);
						thisRef->bUpdateTransforms = true;
					}
				}
			}
		});
	}

	void ParticleSystemTask::InitResources()
	{
		{
			BufferSpecifications specs{};
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferSrc | BufferUsage::TransferDst;

			specs.Size = m_MaxParticles * sizeof(PackedParticle);
			m_ParticlesBuffer = Buffer::Create(specs, "Particles");

			specs.Size = m_MaxParticles * sizeof(uint32_t);
			m_AliveIndices[0] = Buffer::Create(specs, "ParticleSystem_AliveIndices_Pre");
			m_AliveIndices[1] = Buffer::Create(specs, "ParticleSystem_AliveIndices_Post");
			m_DeadIndices = Buffer::Create(specs, "ParticleSystem_DeadIndices");
			m_OpaqueIndicesToRender = Buffer::Create(specs, "ParticleSystem_OpaqueIndicesToRender");
			m_TranslucentIndicesToRender = Buffer::Create(specs, "ParticleSystem_TranslucentIndicesToRender");
			m_TranslucentDistancesBuffer = Buffer::Create(specs, "ParticleSystem_TranslucentDistances");
			
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;
			specs.Size = m_MaxEmitters * sizeof(Emitter);
			m_EmittersBuffer = Buffer::Create(specs, "ParticleSystem_Emitters");

			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			specs.Size = m_MaxEmitters * sizeof(glm::mat4);
			m_TransformsBuffer = Buffer::Create(specs, "ParticleSystem_Transforms");
			m_AnimationTransformsBuffer = Buffer::Create(specs, "ParticleSystem_AnimationTransforms");

			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			specs.Size = m_MaxEmitters * sizeof(DecompositedTransform);
			m_DecompositedTransformsBuffer = Buffer::Create(specs, "ParticleSystem_DecompositedTransforms");

			specs.Size = m_MaxEmitters * sizeof(uint32_t);
			m_EmittersSpawnCountBuffer = Buffer::Create(specs, "ParticleSystem_EmittersSpawnCount");
		}

		{
			BufferSpecifications specs{};
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			specs.Size = sizeof(ParticleSystemData);
			m_SystemData = Buffer::Create(specs, "ParticleSystem_Data");
		}

		{
			BufferSpecifications specs{};
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer | BufferUsage::UniformBuffer;
			specs.Size = sizeof(DispatchIndirectArgs) * 2; // Emit args + Simulate args
			m_DispatchArgs = Buffer::Create(specs, "ParticleSystem_DispatchArgs");

			specs.Size = sizeof(DrawIndirectArgs) * 2; // Args for opaque + translucent passes
			m_DrawArgs = Buffer::Create(specs, "ParticleSystem_DrawArgs");
		}

		// Mesh data
		{
			BufferSpecifications specs{};
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			specs.Size = 1024u;
			m_StaticMeshVertexBuffer = Buffer::Create(specs, "ParticleSystem_StaticMeshVertices");
			m_StaticMeshIndexBuffer = Buffer::Create(specs, "ParticleSystem_StaticMeshIndices");
			m_SkeletalMeshVertexBuffer = Buffer::Create(specs, "ParticleSystem_SkeletalMeshVertices");
			m_SkeletalMeshIndexBuffer = Buffer::Create(specs, "ParticleSystem_SkeletalMeshIndices");
		}

		RenderManager::Submit([dataBuffer = m_SystemData, deadIndices = m_DeadIndices, maxParticles = m_MaxParticles](const Ref<CommandBuffer>& cmd) mutable
		{
			ParticleSystemData systemData(maxParticles);
			cmd->Write(dataBuffer, &systemData, sizeof(systemData), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

			std::vector<uint32_t> data(maxParticles);
			for (size_t i = 0; i < maxParticles; ++i)
				data[i] = uint32_t(i);
			cmd->Write(deadIndices, data.data(), data.size() * sizeof(uint32_t), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
		});
	}
	
	void ParticleSystemTask::InitPipelines()
	{
		// Compute pipelines
		{
			PipelineComputeState state{};
			
			state.ComputeShader = Shader::Create("particle_system/update_max_particles.comp", ShaderType::Compute);
			m_UpdateMaxParticles = PipelineCompute::Create(state);
			
			state.ComputeShader = Shader::Create("particle_system/prepare_data.comp", ShaderType::Compute);
			m_PrepareData = PipelineCompute::Create(state);
			
			state.ComputeShader = Shader::Create("particle_system/emit.comp", ShaderType::Compute);
			m_Emit = PipelineCompute::Create(state);
		}

		// Graphics pipelines
		{
			const auto& gBuffer = m_Renderer.GetGBuffer();
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
			colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
			colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = gBuffer.Depth;
			depthAttachment.bWriteDepth = false; // TODO: Should enable?
			depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;
			depthAttachment.ClearOperation = ClearOperation::Load;

			ShaderDefines transparentDefines;
			transparentDefines["EG_PARTICLE_BACK_TO_FRONT"] = "";
			transparentDefines["EG_BLEND"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("particle_system/particle2D.vert", ShaderType::Vertex, transparentDefines);
			state.FragmentShader = Shader::Create("particle_system/particle.frag", ShaderType::Fragment, transparentDefines);
			state.ColorAttachments.push_back(colorAttachment);
			state.DepthStencilAttachment = depthAttachment;
			// Note: Culling is disabled for "Facing Velocity" particles to render correctly.
			// TODO: Can we create a separate pipeline for such particles?
			// state.CullMode = CullMode::Front;

			if (m_BillboardRenderTranslucent)
				m_BillboardRenderTranslucent->SetState(state);
			else
				m_BillboardRenderTranslucent = PipelineGraphics::Create(state);

			state.VertexShader = Shader::Create("particle_system/particle2D.vert", ShaderType::Vertex);
			state.FragmentShader = Shader::Create("particle_system/particle.frag", ShaderType::Fragment);
			state.DepthStencilAttachment.bWriteDepth = true;
			state.ColorAttachments[0].bBlendEnabled = false;
			if (m_BillboardRender)
				m_BillboardRender->SetState(state);
			else
				m_BillboardRender = PipelineGraphics::Create(state);
		}
	}
	
	void ParticleSystemTask::InitSortOpaqueResources()
	{
		// Simulate pipeline
		{
			PipelineComputeState state{};
			ShaderDefines simulateDefs;
			if (bSortOpaque)
				simulateDefs["EG_SORT_OPAQUE"] = "";
			state.ComputeShader = Shader::Create("particle_system/simulate.comp", ShaderType::Compute, simulateDefs);
			if (m_Simulate)
				m_Simulate->SetState(state);
			else
				m_Simulate = PipelineCompute::Create(state);
		}

		if (bSortOpaque)
		{
			BufferSpecifications specs{};
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Usage = BufferUsage::StorageBuffer;
			specs.Size = m_MaxParticles * sizeof(uint32_t);
			m_OpaqueDistancesBuffer = Buffer::Create(specs, "ParticleSystem_OpaqueDistances");

			m_SortOpaque = MakeScope<SortTask>(m_MaxParticles, true, true);
		}
		else
		{
			m_SortOpaque.reset();
			m_OpaqueDistancesBuffer.reset();
		}
	}
}
