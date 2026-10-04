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
			flags |= emitter.IsSkeletalMeshUsed() ? Emitter_SkeletalMesh_Mask : 0;
			flags |= emitter.VelocitySpace == ParticleEmitter::VelocitySpaceType::World ? Emitter_WorldSpaceVelocity_Mask : 0;

			return flags;
		}

		static void ToGPUEmitter(const ParticleEmitter& emitter, const ParticleSystemTask::EmitterData& emitterData, const ParticleSystemTask::MeshEmitterData& meshEmitterData, uint32_t generation, Emitter& outData)
		{
			outData.TransformIndex = emitterData.TransformIndex;
			outData.AABBMin = emitter.VisibilityAABB.Min;
			outData.AABBMax = emitter.VisibilityAABB.Max;
			outData.CollisionType = uint32_t(emitter.CollisionMode);
			outData.EmissionShape = uint32_t(emitter.EmissionShape);
			outData.VelocityMin = emitter.VelocityMin;
			outData.VelocityMax = emitter.VelocityMax;
			outData.SpawnRate = std::min(emitter.SpawnRate, ParticleEmitter::MaxSpawnRate);
			outData.LoopDuration = emitter.LoopDuration;
			outData.LoopCount = emitter.LoopCount;
			outData.RingRadius = emitter.RingRadius;
			outData.BouncinessMin = emitter.BouncinessMin;
			outData.ColliderSizeRatio = emitter.ColliderSizeRatio;
			outData.BouncinessMax = emitter.BouncinessMax;
			outData.RadialAcceleration = emitter.RadialAcceleration;
			outData.TangentialAcceleration = emitter.TangentialAcceleration;
			outData.RingThickness = emitter.RingThickness;
			outData.LifetimeMin = emitter.LifetimeMin;
			outData.LifetimeMax = emitter.LifetimeMax;
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
			outData.Generation = generation;
			outData.InternalFlags = 0u;
			outData.LoopIteration = 0u;

			// Disable emitter if it's useless
			if (outData.SpawnRate == 0u || outData.LoopDuration <= 0.f)
			{
				outData.Flags = outData.Flags & (~Emitter_Enabled_Mask);
			}

			// Needed so it spawns particles on the first update
			{
				const float spawnInterval = emitter.bExplode ? outData.LoopDuration : 1.f / float(outData.SpawnRate);
				outData.SpawnIntervalTimer = spawnInterval;
				Emitter_SetWasExplode(outData, emitter.bExplode);
			}
			Emitter_SetIsVisible(outData, false);
			outData.DeltaTime = emitter.bExplode ? outData.LoopDuration : 0.f;

			// Fast-forward is applied by the first `prepare_data` pass after the emitter is added.
			// Particles older than `LifetimeMax` are dead, so longer times only matter for loop counting.
			{
				float fastForwardTime = glm::max(emitter.FastForwardTo, 0.f);
				const float lifetimeMax = glm::max(emitter.LifetimeMax, 0.f);
				if (outData.LoopCount == 0u)
					fastForwardTime = glm::min(fastForwardTime, lifetimeMax + outData.LoopDuration);
				else
					fastForwardTime = glm::min(fastForwardTime, outData.LoopCount * outData.LoopDuration + lifetimeMax); // Everything is finished by then
				outData.FastForwardTime = fastForwardTime;
			}
		}
	
		// Time step of the particle simulation. Long frames/stutters are clamped so that a single huge step doesn't spawn a bunch of particles
		static float GetSimulationDeltaTime()
		{
			constexpr float maxDeltaTime = 0.1f;
			return std::min(float(Application::Get().GetTimestep()), maxDeltaTime);
		}

		static constexpr size_t s_EmitterCurvesSize = size_t(EmitterCurve_Count) * EmitterCurve_SamplesCount * sizeof(glm::vec4); // Per emitter slot

		// The shaders blend linearly between neighbouring samples
		static void BakeEmitterCurves(const ParticleEmitter& emitter, std::vector<glm::vec4>& outSamples)
		{
			constexpr uint32_t samplesCount = EmitterCurve_SamplesCount;
			for (uint32_t i = 0; i < samplesCount; ++i)
			{
				const float lifeAlpha = float(i) / float(samplesCount - 1u);

				// Smooth/cubic curves can overshoot
				const glm::vec4 color = emitter.Color.Evaluate(lifeAlpha);
				const float intensity = glm::max(emitter.ColorIntensity.Evaluate(lifeAlpha), 0.f);
				const glm::vec3 rgb = glm::max(glm::vec3(color) * intensity, glm::vec3(0.f));

				outSamples[EmitterCurve_Color * samplesCount + i] = glm::vec4(rgb, glm::clamp(color.a, 0.f, 1.f));
				outSamples[EmitterCurve_SizeRotation * samplesCount + i] = glm::vec4(emitter.Size.Evaluate(lifeAlpha), glm::radians(emitter.RotationZ.Evaluate(lifeAlpha)), 0.f);
				outSamples[EmitterCurve_VelocityCoef * samplesCount + i] = glm::vec4(emitter.VelocityCoef.Evaluate(lifeAlpha), 0.f);
			}
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
		static void RebuildMeshData(const Ref<CommandBuffer>& cmd, ankerl::unordered_dense::map<Ref<MeshType>, ParticleSystemTask::MeshEmitterData>& meshDataMapping,
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
							newVertex.BoneID[i] = vertex.BoneID[i];
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
					cmd->Write(vertexBuffer, vertices.data(), verticesSize, 0, vertexBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
				if (indicesSize > 0)
					cmd->Write(indexBuffer, indices.data(), indicesSize, 0, indexBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
			}
		}

		static uint32_t ClampParticlesBudget(uint32_t budget)
		{
			return std::max(budget, SceneRendererSettings::MinParticlesBudget);
		}
	}

	ParticleSystemTask::ParticleSystemTask(SceneRenderer& renderer)
		: RendererTask(renderer)
		, m_MaxParticlesBudget(Utils::ClampParticlesBudget(renderer.GetOptions().MaxParticlesBudget))
		, m_MaxParticles(std::min(s_InitialMaxParticles, m_MaxParticlesBudget))
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

		// Keep running while removed emitters may still have alive particles.
		// Otherwise those particles would freeze and resume once a new system is added
		if (m_SystemToEmittersMapping.empty() && m_DeadEmitters.empty())
			return;

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
		cmd->TransitionLayout(m_DrawArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);

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
		const uint32_t budget = Utils::ClampParticlesBudget(settings.MaxParticlesBudget);
		if (budget != m_MaxParticlesBudget)
		{
			m_MaxParticlesBudget = budget;
			bMaxParticlesBudgetChanged = true;
		}

		if (bSortOpaque == settings.bSortOpaqueParticles)
			return;

		bSortOpaque = settings.bSortOpaqueParticles;
		InitSortOpaqueResources();
	}

	void ParticleSystemTask::HandleEmitter_Add_RT(const Ref<CommandBuffer>& cmd, const ModifyRequest& data)
	{
		const auto& emitterToAdd = data.Emitter;

		// `RemoveEmitter` cancels pending `Add` requests, so this shouldn't happen. But if it does, don't leak the transform slot
		auto itEmitters = m_SystemToEmittersMapping.find(data.SystemID);
		if (itEmitters == m_SystemToEmittersMapping.end())
		{
			m_FreeTransformSlots.push_back(data.Indices.TransformIndex);
			return;
		}
		auto itEmitter = itEmitters->second.find(emitterToAdd);
		if (itEmitter == itEmitters->second.end())
		{
			m_FreeTransformSlots.push_back(data.Indices.TransformIndex);
			return;
		}

		uint32_t insertIndex = 0u;
		if (m_FreeEmitterSlots.empty())
		{
			insertIndex = m_NumEmitters++; // There are no free slots
			if (insertIndex > Emitter_MaxIndex)
			{
				EG_CORE_ERROR("Too many emitters. Number of emitters: {}. Max limit: {}", m_NumEmitters, Emitter_MaxIndex + 1u);
				EG_CORE_ASSERT(false, "Too many emitters");
				--m_NumEmitters;
				return;
			}
		}
		else
		{
			insertIndex = m_FreeEmitterSlots.back();
			m_FreeEmitterSlots.pop_back();
		}

		// Bump the slot's generation so that particles of the previous owner of this slot die instead of inheriting this emitter
		if (insertIndex >= m_EmitterGenerations.size())
			m_EmitterGenerations.resize(insertIndex + 1u, 0u);
		const uint32_t generation = (m_EmitterGenerations[insertIndex] + 1u) & Emitter_GenerationMask;
		m_EmitterGenerations[insertIndex] = generation;

		auto& emitterData = itEmitter->second;
		emitterData = data.Indices;
		emitterData.EmitterIndex = insertIndex;

		Emitter emitter;
		Utils::ToGPUEmitter(emitterToAdd, emitterData, GetEmitterMeshData(emitterToAdd), generation, emitter);

		const size_t offset = insertIndex * sizeof(Emitter);
		cmd->Write(m_EmittersBuffer, &emitter, sizeof(Emitter), offset, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
		WriteEmitterCurves(cmd, emitterToAdd, insertIndex);
	}

	void ParticleSystemTask::HandleEmitter_Update_RT(const Ref<CommandBuffer>& cmd, const ModifyRequest& data)
	{
		const auto& emitterToUpdate = data.Emitter;
		auto itEmitters = m_SystemToEmittersMapping.find(data.SystemID);
		if (itEmitters == m_SystemToEmittersMapping.end())
			return;

		auto itEmitter = itEmitters->second.find(emitterToUpdate);
		if (itEmitter == itEmitters->second.end() || !itEmitter->second.IsEmitterIndexValid())
			return;

		const auto& emitterData = itEmitter->second;
		const uint32_t generation = m_EmitterGenerations[emitterData.EmitterIndex];

		Emitter gpuEmitter;
		Utils::ToGPUEmitter(data.Emitter, emitterData, GetEmitterMeshData(data.Emitter), generation, gpuEmitter);

		const size_t sizeToUpdate = offsetof(Emitter, WorldPos); // We're updating the data before the 'WorldPos' because everything after is an internal state
		const size_t offset = emitterData.EmitterIndex * sizeof(Emitter);
		cmd->Write(m_EmittersBuffer, &gpuEmitter, sizeToUpdate, offset, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
		WriteEmitterCurves(cmd, data.Emitter, emitterData.EmitterIndex);
	}

	void ParticleSystemTask::WriteEmitterCurves(const Ref<CommandBuffer>& cmd, const ParticleEmitter& emitter, uint32_t emitterIndex)
	{
		std::vector<glm::vec4> samples(size_t(EmitterCurve_Count) * EmitterCurve_SamplesCount);
		Utils::BakeEmitterCurves(emitter, samples);

		const size_t offset = size_t(emitterIndex) * Utils::s_EmitterCurvesSize;
		cmd->Write(m_EmitterCurvesBuffer, samples.data(), Utils::s_EmitterCurvesSize, offset, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
	}

	void ParticleSystemTask::HandleEmitter_Remove_RT(const Ref<CommandBuffer>& cmd, const ModifyRequest& data)
	{
		// `RemoveEmitter` should always provide a valid index
		const EmitterData& emitterData = data.Indices;
		if (!emitterData.IsEmitterIndexValid())
		{
			EG_CORE_ASSERT(false, "Trying to remove an emitter that was never added");
			return;
		}

		const bool bDestroyImmediately = data.bDestroyImmediately || data.Emitter.bDestroyImmediately;

		// Unless `bDestroyImmediately` is set, removal is postponed.
		// We disable it to let all particles to finish simulation, and only then we remove it.
		ParticleEmitter disabledEmitter = data.Emitter;
		disabledEmitter.bEmit = false;
		disabledEmitter.bDestroyImmediately = bDestroyImmediately;

		const uint32_t flags = Utils::PackEmitterFlags(disabledEmitter);
		const size_t offset = emitterData.EmitterIndex * sizeof(Emitter) + offsetof(Emitter, Flags);
		cmd->Write(m_EmittersBuffer, &flags, sizeof(uint32_t), offset, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);

		auto& dead = m_DeadEmitters.emplace_back();
		dead.EmitterIndex = emitterData.EmitterIndex;
		dead.TransformIndex = emitterData.TransformIndex;
		// Even if the slot gets reused too early, the generation check makes the old particles die instead of using the new emitter
		dead.TimeTillDead = bDestroyImmediately ? 0.f : data.Emitter.LifetimeMax;
	}

	void ParticleSystemTask::ReclaimDeadEmitters()
	{
		for (size_t i = 0; i < m_DeadEmitters.size();)
		{
			const DeadEmitterData& dead = m_DeadEmitters[i];
			if (dead.IsDead())
			{
				m_FreeEmitterSlots.push_back(dead.EmitterIndex);
				m_FreeTransformSlots.push_back(dead.TransformIndex);

				m_DeadEmitters[i] = m_DeadEmitters.back();
				m_DeadEmitters.pop_back();
			}
			else
			{
				++i;
			}
		}
	}

	void ParticleSystemTask::ResetGPUState(const Ref<CommandBuffer>& cmd)
	{
		ParticleSystemData systemData(m_MaxParticles);
		cmd->Write(m_SystemData, &systemData, sizeof(systemData), 0, m_SystemData->GetLayout(), BufferLayoutType::StorageBuffer);

		std::vector<uint32_t> deadIndices(m_MaxParticles);
		for (uint32_t i = 0; i < m_MaxParticles; ++i)
			deadIndices[i] = i;
		cmd->Write(m_DeadIndices, deadIndices.data(), deadIndices.size() * sizeof(uint32_t), 0, m_DeadIndices->GetLayout(), BufferLayoutType::StorageBuffer);
	}

	void ParticleSystemTask::UpdateMeshEmittersData(const Ref<CommandBuffer>& cmd)
	{
		// Rebuilding mesh buffers can move every mesh, so all mesh emitters need new offsets
		struct MeshData
		{
			uint32_t VertexOffset;
			uint32_t IndexOffset;
			uint32_t IndexCount;
		};
		// Keep in sync with `Emitter`
		static_assert(offsetof(Emitter, IndexOffset) == offsetof(Emitter, VertexOffset) + sizeof(uint32_t));
		static_assert(offsetof(Emitter, IndexCount) == offsetof(Emitter, IndexOffset) + sizeof(uint32_t));

		bool bTransitioned = false;
		for (const auto& [_, emitters] : m_SystemToEmittersMapping)
		{
			for (const auto& [emitter, emitterData] : emitters)
			{
				if (emitter.EmissionShape != ParticleEmitter::EmissionShapeType::Mesh || !emitter.MeshAsset || !emitterData.IsEmitterIndexValid())
					continue;

				if (!bTransitioned)
				{
					cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);
					bTransitioned = true;
				}

				const MeshEmitterData meshEmitterData = GetEmitterMeshData(emitter);
				const MeshData data{ meshEmitterData.VertexOffset, meshEmitterData.IndexOffset, meshEmitterData.IndexCount };
				const size_t offset = emitterData.EmitterIndex * sizeof(Emitter) + offsetof(Emitter, VertexOffset);
				cmd->WriteTransitionless(m_EmittersBuffer, &data, sizeof(MeshData), offset);
			}
		}

		if (bTransitioned)
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);
	}

	void ParticleSystemTask::Update(const Ref<CommandBuffer>& cmd)
	{
		// 0. Reset GPU state if requested, apply a new particles budget & reclaim slots of dead emitters
		// 1. Update mesh data
		// 2. Check if GPU Emitters buffer is big enough and allocate enough memory if required
		// 3. Process emitters that need to be added/removed/updated. Update mesh offsets of existing emitters if mesh data was rebuilt
		// 4. Upload animation data to GPU
		// 5. Update transforms if required
		// 6. Check if GPU Particles buffer is big enough and allocate enough memory if required

		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Update");
		EG_CPU_TIMING_SCOPED("Particle System. Update");
		bool bRecalculateMaxParticles = false;

		// Step 0
		if (bResetGPUState)
		{
			ResetGPUState(cmd);
			bResetGPUState = false;
		}
		if (bMaxParticlesBudgetChanged)
		{
			bMaxParticlesBudgetChanged = false;
			if (m_MaxParticles > m_MaxParticlesBudget)
				ShrinkMaxParticles(cmd, m_MaxParticlesBudget);
			bRecalculateMaxParticles = true;
		}
		ReclaimDeadEmitters();

		// Step 1
		const bool bMeshDataRebuilt = bRebuildStaticMeshData || bRebuildSkeletalMeshData;
		if (bMeshDataRebuilt)
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
			size_t numEmittersAfterUpdate = m_NumEmitters;
			for (const auto& request : m_ModifyRequestQueue)
			{
				if (request.Type == ModifyRequest::RequestType::Add)
					numEmittersAfterUpdate++;
			}
			size_t currentSize = m_EmittersBuffer->GetSize();
			size_t newSize = numEmittersAfterUpdate * sizeof(Emitter);
			if (newSize > currentSize)
			{
				newSize = (newSize * 12) / 10; // Resize policy: increase by 20%
				BufferSpecifications specs = m_EmittersBuffer->GetSpecs();
				specs.Size = newSize;

				Ref<Buffer> newBuffer = Buffer::Create(specs, m_EmittersBuffer->GetDebugName());
				if (m_NumEmitters > 0)
					cmd->CopyBuffer(m_EmittersBuffer, newBuffer, 0, 0, m_NumEmitters * sizeof(Emitter));
				m_EmittersBuffer = std::move(newBuffer);
			}

			currentSize = m_EmitterCurvesBuffer->GetSize();
			newSize = numEmittersAfterUpdate * Utils::s_EmitterCurvesSize;
			if (newSize > currentSize)
			{
				newSize = (newSize * 12) / 10; // Resize policy: increase by 20%
				BufferSpecifications specs = m_EmitterCurvesBuffer->GetSpecs();
				specs.Size = newSize;

				Ref<Buffer> newBuffer = Buffer::Create(specs, m_EmitterCurvesBuffer->GetDebugName());
				if (m_NumEmitters > 0)
					cmd->CopyBuffer(m_EmitterCurvesBuffer, newBuffer, 0, 0, m_NumEmitters * Utils::s_EmitterCurvesSize);
				m_EmitterCurvesBuffer = std::move(newBuffer);
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
		if (!m_ModifyRequestQueue.empty())
		{
			for (const auto& request : m_ModifyRequestQueue)
			{
				switch (request.Type)
				{
				case ModifyRequest::RequestType::Add:
					HandleEmitter_Add_RT(cmd, request);
					bUpdateTransforms = true;
					bRecalculateMaxParticles = true;
					break;
				case ModifyRequest::RequestType::Update:
					HandleEmitter_Update_RT(cmd, request);
					bUpdateTransforms = true;
					bRecalculateMaxParticles = true;
					break;
				case ModifyRequest::RequestType::Remove:
					HandleEmitter_Remove_RT(cmd, request);
					break;
				default:
					EG_CORE_ASSERT(false);
				}
			}
			m_ModifyRequestQueue.clear();
		}
		if (bMeshDataRebuilt)
			UpdateMeshEmittersData(cmd);

		// Step 4
		UpdateSkeletalAnimations(cmd);

		// Step 5
		if (bUpdateTransforms)
		{
			{
				const size_t size = m_Transforms.size() * sizeof(glm::mat4);
				if (size > m_TransformsBuffer->GetSize())
				{
					const size_t newSize = (size * 12) / 10; // Resize policy: increase by 20%
					m_TransformsBuffer->Resize(newSize);
				}
				cmd->Write(m_TransformsBuffer, m_Transforms.data(), size, 0, m_TransformsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
			}
			{
				const size_t size = m_DecompositedTransforms.size() * sizeof(DecompositedTransform);
				if (size > m_DecompositedTransformsBuffer->GetSize())
				{
					const size_t newSize = (size * 12) / 10; // Resize policy: increase by 20%
					m_DecompositedTransformsBuffer->Resize(newSize);
				}
				cmd->Write(m_DecompositedTransformsBuffer, m_DecompositedTransforms.data(), size, 0, m_DecompositedTransformsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
			}
			bUpdateTransforms = false;
		}
		
		// Step 6
		if (bRecalculateMaxParticles)
		{
			auto toCount = [](double value) -> uint64_t
			{
				if (value <= 0.0)
					return 0;

				// Arbitrary capped at 2^40, which is far above any budget
				return uint64_t(glm::min(glm::ceil(value), double(1ull << 40)));
			};

			uint64_t maxParticles = 0;
			for (const auto& [_, emitters] : m_SystemToEmittersMapping)
			{
				for (const auto& [emitter, _] : emitters)
				{
					const uint32_t spawnRate = glm::min(emitter.SpawnRate, ParticleEmitter::MaxSpawnRate);
					if (emitter.bExplode)
					{
						double coef = double(emitter.LifetimeMax) / double(emitter.LoopDuration);
						if (coef <= 0.0 || glm::isinf(coef) || glm::isnan(coef))
							coef = 1.0;
						maxParticles += toCount(double(spawnRate) * glm::ceil(coef));
					}
					else
						maxParticles += toCount(double(spawnRate) * double(emitter.LifetimeMax));
				}
			}

			if (maxParticles > m_MaxParticlesBudget)
			{
				EG_CORE_WARN("[ParticleSystem] Emitters need {} particles, but the budget is {}. Some emitters will spawn fewer particles", maxParticles, m_MaxParticlesBudget);
				maxParticles = m_MaxParticlesBudget;
			}

			if (maxParticles > m_MaxParticles)
			{
				SetMaxParticles(cmd, uint32_t(maxParticles));
			}
		}
	}

	void ParticleSystemTask::UpdateSkeletalAnimations(const Ref<CommandBuffer>& cmd)
	{
		const auto& systemTransforms = m_Renderer.GetSkeletalParticleAnimationTransforms_RT();
		m_AnimationTransforms.clear();

		if (systemTransforms.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Update skeletam mesh animations");
		EG_CPU_TIMING_SCOPED("Particle System. Update skeletam mesh animations");

		cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);
		for (const auto& [systemID, perEmitterTransforms] : systemTransforms)
		{
			auto it = m_SystemToEmittersMapping.find(systemID);
			if (it == m_SystemToEmittersMapping.end())
				continue;

			auto& emittersData = it->second;
			for (const auto& [emitterID, transforms] : perEmitterTransforms)
			{
				ParticleEmitter dummy;
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
			cmd->Write(m_AnimationTransformsBuffer, m_AnimationTransforms.data(), size, 0, m_AnimationTransformsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		}
	}

	void ParticleSystemTask::PreparePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Prepare");
		EG_CPU_TIMING_SCOPED("Particle System. Prepare");

		struct PushData
		{
			glm::mat4 View;
			uint32_t PreSimIndex;
			uint32_t PostSimIndex;
			uint32_t NumEmitters;
			float DeltaTime;
			uint32_t MaxParticles;
			CullingFrustum Frustum;
		} pushData;

		const auto& cullingData = m_Renderer.GetCullingFrustumData();
		pushData.View = cullingData.View;
		pushData.PreSimIndex = m_PingPong;
		pushData.PostSimIndex = 1u - m_PingPong;
		pushData.NumEmitters = m_NumEmitters;
		pushData.DeltaTime = Utils::GetSimulationDeltaTime();
		pushData.MaxParticles = m_MaxParticles;
		pushData.Frustum = cullingData.Frustum;

		m_PrepareData->SetBuffer(m_SystemData, 0, 0);
		m_PrepareData->SetBuffer(m_DrawArgs, 0, 1);
		m_PrepareData->SetBuffer(m_DispatchArgs, 0, 2);
		m_PrepareData->SetBuffer(m_EmittersBuffer, 0, 3);
		m_PrepareData->SetBuffer(m_EmittersSpawnCountBuffer, 0, 4);
		m_PrepareData->SetBuffer(m_TransformsBuffer, 0, 5);

		cmd->TransitionLayout(m_DrawArgs, m_DrawArgs->GetLayout(), BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(m_DispatchArgs, m_DispatchArgs->GetLayout(), BufferLayoutType::StorageBuffer);

		// Note: this pipeline is designed with num groups of (1, 1, 1) in mind.
		// If this ever changes, the shader logic needs to be revisited. At least handling of available slots
		cmd->Dispatch(m_PrepareData, 1, 1, 1, &pushData);

		cmd->Barrier(m_SystemData);
		cmd->Barrier(m_DrawArgs);
		cmd->Barrier(m_EmittersBuffer);
		cmd->Barrier(m_EmittersSpawnCountBuffer);
		cmd->TransitionLayout(m_DispatchArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
	}

	void ParticleSystemTask::EmitPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Particle System. Emit");
		EG_CPU_TIMING_SCOPED("Particle System. Emit");

		struct PushData
		{
			glm::vec3 Gravity;
			uint32_t PreSimIndex;
			uint32_t FrameNumber;
			uint32_t NumEmitters;
			uint32_t MaxParticles;
		} pushData;

		pushData.PreSimIndex = m_PingPong;
		pushData.FrameNumber = (uint32_t)RenderManager::GetFrameNumber_RT();
		pushData.NumEmitters = m_NumEmitters;
		pushData.MaxParticles = m_MaxParticles;
		pushData.Gravity = m_Renderer.GetGravity();

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
		m_Emit->SetBuffer(m_EmitterCurvesBuffer, 0, 13);

		cmd->DispatchIndirect(m_Emit, m_DispatchArgs, 0, &pushData);

		cmd->Barrier(m_SystemData);
		cmd->Barrier(m_ParticlesBuffer);
		cmd->Barrier(m_DeadIndices);
		cmd->Barrier(m_AliveIndices[m_PingPong]);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
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
		pushData.DeltaTime = Utils::GetSimulationDeltaTime();
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
		m_Simulate->SetImageSampler(gbuffer.Normals, Sampler::PointSamplerClamp, 0, 10);
		m_Simulate->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 11);
		m_Simulate->SetBuffer(m_TransformsBuffer, 0, 12);
		m_Simulate->SetBuffer(m_EmitterCurvesBuffer, 0, 13);
		m_Simulate->SetBuffer(m_OpaqueIndicesToRender, 0, 14);
		if (bSortOpaque)
		{
			m_Simulate->SetBuffer(m_OpaqueDistancesBuffer, 0, 15);
		}

		const ImageLayout oldDepthLayout = gbuffer.Depth->GetLayout();
		const ImageLayout oldNormalsLayout = gbuffer.Normals->GetLayout();
		cmd->TransitionLayout(gbuffer.Depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gbuffer.Normals, oldNormalsLayout, ImageReadAccess::PixelShaderRead);

		cmd->DispatchIndirect(m_Simulate, m_DispatchArgs, sizeof(DispatchIndirectArgs), &pushData);

		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
		cmd->TransitionLayout(gbuffer.Normals, ImageReadAccess::PixelShaderRead, oldNormalsLayout);
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
		cmd->Barrier(m_AliveIndices[1 - m_PingPong]);
		cmd->Barrier(m_DeadIndices);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
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

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			const auto& images = TextureSystem::GetImages();
			const auto& samplers = TextureSystem::GetSamplers();

			m_BillboardRender->SetImageSamplerArray(images, samplers, EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_BillboardRenderTranslucent->SetImageSamplerArray(images, samplers, EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		auto& stats = m_Renderer.GetStats();

		cmd->BeginGraphics(m_BillboardRender);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndirect(m_DrawArgs, 0, 1, sizeof(DrawIndirectArgs));
		cmd->EndGraphics();
		++stats.DrawCalls;

		cmd->BeginGraphics(m_BillboardRenderTranslucent);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndirect(m_DrawArgs, sizeof(DrawIndirectArgs), 1, sizeof(DrawIndirectArgs));
		cmd->EndGraphics();
		++stats.DrawCalls;
	}

	void ParticleSystemTask::SetMaxParticles(const Ref<CommandBuffer>& cmd, uint32_t maxParticles)
	{
		if (maxParticles <= m_MaxParticles)
		{
			EG_CORE_ASSERT(false);
			return;
		}

		const uint32_t oldMaxParticles = m_MaxParticles;
		// Resize policy: increase by 10%, but not past the budget
		m_MaxParticles = (uint32_t)glm::min(uint64_t(maxParticles) * 11u / 10u, uint64_t(m_MaxParticlesBudget));
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

			const glm::uvec3 groupSize = m_UpdateMaxParticles->GetWorkGroupSize();
			const uint32_t numGroups = CalcNumGroups(newParticlesAmount, groupSize).x;
			cmd->Dispatch(m_UpdateMaxParticles, numGroups, 1, 1, &pushData);
			cmd->Barrier(m_SystemData);
			cmd->Barrier(m_DeadIndices);
		}
	
		if (bSortOpaque)
			m_SortOpaque = MakeScope<SortTask>(m_MaxParticles, true, true);
		m_SortTranslucent = SortTask(m_MaxParticles, true, true);
	}

	void ParticleSystemTask::ShrinkMaxParticles(const Ref<CommandBuffer>& cmd, uint32_t maxParticles)
	{
		if (maxParticles >= m_MaxParticles)
		{
			EG_CORE_ASSERT(false);
			return;
		}

		// Alive particles can be anywhere in the buffers, so they can't be kept without compacting them.
		// Buffers are recreated at the new size, and all particles are killed
		m_MaxParticles = maxParticles;

		const auto recreate = [](Ref<Buffer>& buffer, size_t size)
		{
			BufferSpecifications specs = buffer->GetSpecs();
			specs.Size = size;
			buffer = Buffer::Create(specs, buffer->GetDebugName());
		};
		const size_t indicesSize = size_t(m_MaxParticles) * sizeof(uint32_t);
		recreate(m_ParticlesBuffer, size_t(m_MaxParticles) * sizeof(PackedParticle));
		recreate(m_AliveIndices[0], indicesSize);
		recreate(m_AliveIndices[1], indicesSize);
		recreate(m_DeadIndices, indicesSize);
		recreate(m_OpaqueIndicesToRender, indicesSize);
		recreate(m_TranslucentIndicesToRender, indicesSize);
		recreate(m_TranslucentDistancesBuffer, indicesSize);
		if (bSortOpaque)
		{
			recreate(m_OpaqueDistancesBuffer, indicesSize);
			m_SortOpaque = MakeScope<SortTask>(m_MaxParticles, true, true);
		}
		m_SortTranslucent = SortTask(m_MaxParticles, true, true);

		ResetGPUState(cmd);
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
				EG_CORE_WARN("Trying to add already existing emitter");
				return false;
			}
		}

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

		auto& request = m_ModifyRequestQueue.emplace_back();
		request.Emitter = emitter;
		request.SystemID = systemID;
		request.Type = ModifyRequest::RequestType::Add;
		request.Indices = EmitterData{ s_InvalidEmitterIndex, transformIndex, s_InvalidEmitterIndex }; // Emitter index will be set later

		m_Transforms[transformIndex] = transform * Math::ToTransformMatrix(emitter.RelativeTransform);
		m_DecompositedTransforms[transformIndex] = Utils::Decompose(m_Transforms[transformIndex]);
		auto& emitters = m_SystemToEmittersMapping[systemID];
		emitters[emitter] = request.Indices;
		AddEmitterMeshData(emitter);

		return true;
	}

	bool ParticleSystemTask::RemoveEmitter(const ParticleEmitter& emitter, const GUID& systemID, bool bForceImmediateRemoval)
	{
		auto itSystem = m_SystemToEmittersMapping.find(systemID);
		if (itSystem == m_SystemToEmittersMapping.end())
		{
			EG_CORE_WARN("Trying to remove non-existing emitter");
			return false; // Not found
		}

		auto& emitters = itSystem->second;
		auto it = emitters.find(emitter);
		if (it == emitters.end())
		{
			EG_CORE_ASSERT(false, "Trying to remove non-existing emitter");
			return false; // Not found
		}

		if (!it->second.IsEmitterIndexValid())
		{
			// Its `Add` request hasn't been processed yet, so nothing exists on the GPU.
			// Cancel pending requests of this emitter (except removals of a previous instance with the same ID) and release its transform slot
			m_ModifyRequestQueue.erase(std::remove_if(m_ModifyRequestQueue.begin(), m_ModifyRequestQueue.end(),
				[&systemID, &emitter](const ModifyRequest& request)
				{
					return request.Type != ModifyRequest::RequestType::Remove && request.SystemID == systemID && request.Emitter == emitter;
				}), m_ModifyRequestQueue.end());

			m_FreeTransformSlots.push_back(it->second.TransformIndex);
			RemoveEmitterMeshData(it->first);
			emitters.erase(it);
			return true;
		}

		// Note: `it->first` and `emitter` might differ, so we should use `it->first` since the system used its state
		auto& request = m_ModifyRequestQueue.emplace_back();
		request.Emitter = it->first;
		request.SystemID = systemID;
		request.Type = ModifyRequest::RequestType::Remove;
		request.bDestroyImmediately = bForceImmediateRemoval;
		request.Indices = it->second;
		RemoveEmitterMeshData(it->first);

		emitters.erase(it);

		return true;
	}

	void ParticleSystemTask::AddParticleSystem(const ParticleSystemComponent& system)
	{
		struct SystemUpdateData
		{
			std::vector<ParticleEmitter> Emitters;
			glm::mat4 Transformation;
			GUID SystemID;
		};

		const auto& asset = system.GetAsset();
		if (!asset)
			return;

		SystemUpdateData data{};
		data.Emitters = asset->GetEmitters();
		data.Transformation = Math::ToTransformMatrix(system.GetWorldTransform());
		data.SystemID = system.GetSystemID();

		RenderManager::Submit([task = shared_from_this(), updateData = std::move(data)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			const auto& emitters = updateData.Emitters;
			if (emitters.empty())
			{
				thisRef->m_SystemToEmittersMapping.emplace(updateData.SystemID, ankerl::unordered_dense::map<ParticleEmitter, EmitterData>{});
			}
			else
			{
				for (const auto& emitter : emitters)
				{
					thisRef->AddEmitter(emitter, updateData.SystemID, updateData.Transformation);
				}
			}
		});
	}

	void ParticleSystemTask::UpdateParticleSystem(const ParticleSystemComponent& system)
	{
		struct SystemUpdateData
		{
			std::vector<ParticleEmitter> Emitters;
			glm::mat4 Transformation;
			GUID SystemID;
		};

		const auto& asset = system.GetAsset();
		if (!asset)
			return;

		SystemUpdateData data{};
		data.Emitters = asset->GetEmitters();
		data.Transformation = Math::ToTransformMatrix(system.GetWorldTransform());
		data.SystemID = system.GetSystemID();

		RenderManager::Submit([task = shared_from_this(), updateData = std::move(data)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			
			auto itSystem = thisRef->m_SystemToEmittersMapping.find(updateData.SystemID);
			if (itSystem == thisRef->m_SystemToEmittersMapping.end())
			{
				// Trying to update non-existing system
				return;
			}

			const auto& emitters = updateData.Emitters;
			auto systemEmitters = itSystem->second; // Intentional copy because `AddEmitter` and `RemoveEmitter` functions modify it

			// Remove emitters if not found in the new list
			for (const auto& [existingEmitter, _] : systemEmitters)
			{
				auto it2 = std::find(emitters.begin(), emitters.end(), existingEmitter);
				if (it2 == emitters.end()) // Old emitter isn't found in the new list, so remove it
					thisRef->RemoveEmitter(existingEmitter, updateData.SystemID);
			}

			// Update or create emitters
			for (const auto& emitter : emitters)
			{
				if (systemEmitters.find(emitter) == systemEmitters.end())
				{
					thisRef->AddEmitter(emitter, updateData.SystemID, updateData.Transformation); // New emitter isn't found in the old list, so add it
				}
				else
				{
					auto& existingEmitters = itSystem->second;
					auto it = existingEmitters.find(emitter);
					const auto& existingEmitter = it->first;
					{
						EmitterData emitterData = it->second;
						emitterData.AnimationOffset = s_InvalidEmitterIndex; // Reset it in case mesh is changed from SK to SM

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
						auto& request = thisRef->m_ModifyRequestQueue.emplace_back();
						request.Emitter = emitter;
						request.SystemID = updateData.SystemID;
						request.Type = ModifyRequest::RequestType::Update;

						thisRef->m_Transforms[emitterData.TransformIndex] = updateData.Transformation * Math::ToTransformMatrix(emitter.RelativeTransform);
						thisRef->m_DecompositedTransforms[emitterData.TransformIndex] = Utils::Decompose(thisRef->m_Transforms[emitterData.TransformIndex]);
					}
				}
			}
		});
	}

	void ParticleSystemTask::RemoveParticleSystem(const ParticleSystemComponent& system, bool bForceImmediateRemoval)
	{
		RenderManager::Submit([task = shared_from_this(), systemID = system.GetSystemID(), bForceImmediateRemoval](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			auto it = thisRef->m_SystemToEmittersMapping.find(systemID);
			if (it == thisRef->m_SystemToEmittersMapping.end())
				return;

			const auto& emitters = it->second;
			if (!emitters.empty())
			{
				auto copyEmitters = emitters;
				for (const auto& [emitter, _] : copyEmitters)
				{
					thisRef->RemoveEmitter(emitter, systemID, bForceImmediateRemoval);
				}
			}
			thisRef->m_SystemToEmittersMapping.erase(systemID);
		});
	}

	void ParticleSystemTask::RemoveAllParticleSystems()
	{
		RenderManager::Submit([task = shared_from_this()](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);

			// Note: `m_EmitterGenerations` is intentionally kept
			thisRef->m_SystemToEmittersMapping.clear();
			thisRef->m_ModifyRequestQueue.clear();
			thisRef->m_DeadEmitters.clear();
			thisRef->m_FreeEmitterSlots.clear();
			thisRef->m_NumEmitters = 0;

			thisRef->m_Transforms.clear();
			thisRef->m_DecompositedTransforms.clear();
			thisRef->m_FreeTransformSlots.clear();

			thisRef->m_StaticMeshDataMapping.clear();
			thisRef->m_SkeletalMeshDataMapping.clear();
			thisRef->bRebuildStaticMeshData = true;
			thisRef->bRebuildSkeletalMeshData = true;

			thisRef->bResetGPUState = true;
		});
	}

	void ParticleSystemTask::UpdateTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		struct UpdateTrData
		{
			glm::mat4 Transform;
			GUID EmitterID;
		};
		ankerl::unordered_dense::map<GUID, std::vector<UpdateTrData>> newTransforms;
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

			specs.Size = m_MaxEmitters * Utils::s_EmitterCurvesSize;
			m_EmitterCurvesBuffer = Buffer::Create(specs, "ParticleSystem_EmitterCurves");

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
			cmd->Write(dataBuffer, &systemData, sizeof(systemData), 0, dataBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

			std::vector<uint32_t> data(maxParticles);
			for (size_t i = 0; i < maxParticles; ++i)
				data[i] = uint32_t(i);
			cmd->Write(deadIndices, data.data(), data.size() * sizeof(uint32_t), 0, deadIndices->GetLayout(), BufferLayoutType::StorageBuffer);
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
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
