#include "egpch.h"
#include "ParticleSystemTask.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/particle_system/common.h"

namespace Eagle
{
	namespace Utils
	{
		uint32_t PackEmitterFlags(const ParticleEmitter& emitter)
		{
			uint32_t flags = 0;
			flags |= emitter.bOneShot ? Emitter_OneShot_Mask : 0;
			flags |= emitter.bExplode ? Emitter_Explode_Mask : 0;
			flags |= emitter.bApplyGravity ? Emitter_ApplyGravity_Mask : 0;
			flags |= emitter.bAlphaBlending ? Emitter_AlphaBlending_Mask : 0;
			flags |= emitter.bEmit ? Emitter_Enabled_Mask : 0;

			return flags;
		}

		void ToGPUEmitter(const ParticleEmitter& emitter, uint32_t transformIndex, Emitter& outData)
		{
			outData.TransformIndex = transformIndex;
			outData.AABBMin = emitter.VisibilityAABB.Min;
			outData.AABBMax = emitter.VisibilityAABB.Max;
			outData.CollisionType = uint32_t(emitter.CollisionMode);
			outData.EmissionShape = uint32_t(emitter.EmissionShape);
			outData.ColorStart = emitter.ColorStart;
			outData.ColorEnd = emitter.ColorEnd;
			outData.VelocityMin = emitter.VelocityMin;
			outData.VelocityMax= emitter.VelocityMax;
			outData.VelocityCoefStart = emitter.VelocityCoefStart;
			outData.VelocityCoefEnd = emitter.VelocityCoefEnd;
			outData.NumParticles = uint32_t(float(emitter.NumParticles) * emitter.NumParticlesRatio);
			outData.RotationZStart = glm::radians(emitter.RotationZStart);
			outData.RotationZEnd = glm::radians(emitter.RotationZEnd);
			outData.FastForwardTo = emitter.FastForwardTo;
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

			const float spawnInterval = emitter.bExplode ? outData.LifetimeMax : outData.LifetimeMax / float(outData.NumParticles);
			outData.DeltaTime = spawnInterval; // Needed so it spawns particles on the first update
			outData.IsVisible = 0u;
		}
	}

	ParticleSystemTask::ParticleSystemTask(SceneRenderer& renderer)
		: RendererTask(renderer)
		, m_Sort(m_MaxParticles, true, true)
	{
		m_Size = m_Renderer.GetViewportSize();
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

		{
			EG_GPU_TIMING_SCOPED(cmd, "Particle System. Sort");
			EG_CPU_TIMING_SCOPED("Particle System. Sort");
			m_Sort.RecordCommandBuffer(cmd, m_DistancesBuffer, m_DrawArgs, offsetof(DrawIndirectArgs, InstanceCount), 0u, m_IndicesToRender);
			cmd->TransitionLayout(m_DrawArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument | BufferReadAccess::Uniform);
		}

		RenderPass(cmd);

		m_PingPong = 1u - m_PingPong;
	}

	void ParticleSystemTask::OnResize(glm::uvec2 size)
	{
		m_Size = size;
		m_BillboardRender->Resize(size);
	}

	void ParticleSystemTask::Update(const Ref<CommandBuffer>& cmd)
	{
		// 1. Check if GPU Emitters buffer is big enough and allocate enough memory if required
		// 2. Process emitters that need to be removed
		// 3. Process emitters that need to be added
		// 4. Process emitters that need to be updated
		// 5. Update transforms if required
		// 6. Check if GPU Particles buffer is big enough and allocate enough memory if required

		bool bEmittersChangedOrAdded = false;

		// Step 1
		{
			const size_t numEmittersAfterUpdate = m_NumEmitters + m_EmittersToAdd.size() - m_EmittersToRemove.size();
			const size_t currentSize = m_EmittersBuffer->GetSize();
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
		}

		// Step 2
		if (m_EmittersToRemove.size())
		{
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);
			for (const auto& [emitter, emitterIndex] : m_EmittersToRemove)
			{
				ParticleEmitter disabledEmitter = emitter;
				disabledEmitter.bEmit = false;

				const uint32_t flags = Utils::PackEmitterFlags(disabledEmitter);
				const size_t offset = emitterIndex * sizeof(Emitter) + offsetof(Emitter, Flags);
				cmd->WriteTransitionless(m_EmittersBuffer, &flags, sizeof(uint32_t), offset);

				auto& dead = m_DeadEmitters.emplace_back();
				dead.EmitterIndex = emitterIndex;
				dead.TimeTillDead = emitter.LifetimeMax;
				dead.TimeOfDeath = std::chrono::high_resolution_clock::now();
			}
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

			m_EmittersToRemove.clear();
		}

		// Step 3
		if (m_EmittersToAdd.size())
		{
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);

			const uint32_t count = (uint32_t)m_EmittersToAdd.size();
			for (uint32_t i = 0; i < count; ++i)
			{
				Emitter emitter;
				const uint32_t transformIndex = m_EmitterTransformsMapping.at(m_EmittersToAdd[i].ID);
				Utils::ToGPUEmitter(m_EmittersToAdd[i], transformIndex, emitter);

				uint32_t insertIndex = m_NumEmitters;
				for (auto it = m_DeadEmitters.begin(); it != m_DeadEmitters.end(); ++it)
				{
					const auto& deadEmitter = *it;
					if (deadEmitter.IsDead())
					{
						insertIndex = deadEmitter.EmitterIndex;
						m_DeadEmitters.erase(it);
						break;
					}
				}
				
				if (insertIndex == m_NumEmitters)
					m_NumEmitters++; // There were no free slots

				const size_t offset = insertIndex * sizeof(Emitter);
				cmd->WriteTransitionless(m_EmittersBuffer, &emitter, sizeof(Emitter), offset);

				m_EmittersMapping.emplace(m_EmittersToAdd[i], insertIndex);
			}
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

			m_EmittersToAdd.clear();

			bUpdateTransforms = true;
			bEmittersChangedOrAdded = true;
		}

		// Step 4
		if (m_EmittersToUpdate.size())
		{
			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::StorageBuffer, BufferLayoutType::CopyDest);

			for (const auto& [emitter, emitterIndex] : m_EmittersToUpdate)
			{
				Emitter gpuEmitter;
				const uint32_t transformIndex = m_EmitterTransformsMapping.at(emitter.ID);
				Utils::ToGPUEmitter(emitter, transformIndex, gpuEmitter);

				const size_t sizeToUpdate = offsetof(Emitter, WorldPos); // We're updating the data before the 'WorldPos' because everything after is an internal state
				const size_t offset = emitterIndex * sizeof(Emitter);
				cmd->WriteTransitionless(m_EmittersBuffer, &gpuEmitter, sizeToUpdate, offset);
			}

			cmd->TransitionLayout(m_EmittersBuffer, BufferLayoutType::CopyDest, BufferLayoutType::StorageBuffer);

			m_EmittersToUpdate.clear();

			bUpdateTransforms = true;
			bEmittersChangedOrAdded = true;
		}

		// Step 5
		if (bUpdateTransforms)
		{
			const size_t size = m_Transforms.size() * sizeof(glm::mat4);
			if (size > m_TransformsBuffer->GetSize())
			{
				const size_t newSize = (size * 12) / 10; // Resize policy: increase by 20%
				m_TransformsBuffer->Resize(newSize);
			}
			cmd->Write(m_TransformsBuffer, m_Transforms.data(), size, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			bUpdateTransforms = false;
		}
		
		// Step 6
		if (bEmittersChangedOrAdded)
		{
			uint32_t maxParticles = 0;
			for (const auto& [emitter, _] : m_EmittersMapping)
				maxParticles += uint32_t(float(emitter.NumParticles) * emitter.NumParticlesRatio);

			if (maxParticles > m_MaxParticles)
			{
				SetMaxParticles(cmd, maxParticles);
			}
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
		} pushData;

		pushData.PreSimIndex = m_PingPong;
		pushData.FrameNumber = (uint32_t)RenderManager::GetFrameNumber_RT();
		pushData.NumEmitters = m_NumEmitters;

		m_Emit->SetBuffer(m_SystemData, 0, 0);
		m_Emit->SetBuffer(m_ParticlesBuffer, 0, 1);
		m_Emit->SetBuffer(m_EmittersBuffer, 0, 2);
		m_Emit->SetBuffer(m_DeadIndices, 0, 3);
		m_Emit->SetBuffer(m_AliveIndices[m_PingPong], 0, 4);
		m_Emit->SetBuffer(m_EmittersSpawnCountBuffer, 0, 5);
		m_Emit->SetBuffer(m_TransformsBuffer, 0, 6);

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

			glm::vec3 CameraPos;
			float CameraFar;

			float DeltaTime;
			uint32_t PreSimIndex;
			uint32_t PostSimIndex;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();
		pushData.Gravity = m_Renderer.GetGravity();
		pushData.CameraNear = m_Renderer.GetZNear();
		pushData.CameraPos = m_Renderer.GetViewPosition();
		pushData.CameraFar = m_Renderer.GetZFar();
		pushData.DeltaTime = Application::Get().GetTimestep();
		pushData.PreSimIndex = m_PingPong;
		pushData.PostSimIndex = 1 - m_PingPong;

		auto& gbuffer = m_Renderer.GetGBuffer();

		m_Simulate->SetBuffer(m_SystemData, 0, 0);
		m_Simulate->SetBuffer(m_ParticlesBuffer, 0, 1);
		m_Simulate->SetBuffer(m_EmittersBuffer, 0, 2);
		m_Simulate->SetBuffer(m_DeadIndices, 0, 3);
		m_Simulate->SetBuffer(m_AliveIndices[m_PingPong], 0, 4);
		m_Simulate->SetBuffer(m_AliveIndices[1 - m_PingPong], 0, 5);
		m_Simulate->SetBuffer(m_IndicesToRender, 0, 6);
		m_Simulate->SetBuffer(m_DistancesBuffer, 0, 7);
		m_Simulate->SetBuffer(m_DrawArgs, 0, 8);
		m_Simulate->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 9);
		m_Simulate->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 10);

		const ImageLayout oldDepthLayout = gbuffer.Depth->GetLayout();
		cmd->TransitionLayout(gbuffer.Depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);

		cmd->DispatchIndirect(m_Simulate, m_DispatchArgs, sizeof(DispatchIndirectArgs), &pushData);

		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
		cmd->Barrier(m_SystemData);
		cmd->Barrier(m_ParticlesBuffer);
		cmd->Barrier(m_IndicesToRender);
		cmd->Barrier(m_DistancesBuffer);
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
		m_BillboardRender->SetBuffer(m_IndicesToRender, 0, 1);
		m_BillboardRender->SetBuffer(m_DrawArgs, 0, 2);

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_BillboardRender->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), 1, 0);
			m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		cmd->BeginGraphics(m_BillboardRender);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndirect(m_DrawArgs, 0, 1, sizeof(DrawIndirectArgs));
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
			const size_t newParticlesSize = m_MaxParticles * sizeof(Particle);
			BufferSpecifications specs = m_ParticlesBuffer->GetSpecs();
			specs.Size = newParticlesSize;
			Ref<Buffer> newParticlesBuffer = Buffer::Create(specs, m_ParticlesBuffer->GetDebugName());

			specs.Size = m_MaxParticles * sizeof(uint32_t);
			Ref<Buffer> aliveIndices0 = Buffer::Create(specs, m_AliveIndices[0]->GetDebugName());
			Ref<Buffer> aliveIndices1 = Buffer::Create(specs, m_AliveIndices[1]->GetDebugName());
			Ref<Buffer> deadIndices = Buffer::Create(specs, m_DeadIndices->GetDebugName());
			m_IndicesToRender = Buffer::Create(specs, m_IndicesToRender->GetDebugName());
			m_DistancesBuffer = Buffer::Create(specs, m_DistancesBuffer->GetDebugName());

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
	
		m_Sort = SortTask(m_MaxParticles, true, true);
	}

	bool ParticleSystemTask::AddEmitter(const ParticleEmitter& emitter, const GUID& systemID, const glm::mat4& transform)
	{
		if (m_EmittersMapping.find(emitter) != m_EmittersMapping.end())
		{
			EG_CORE_ASSERT(false); // Shouldn't really happen
			return false; // Already exists
		}

		m_EmittersToAdd.emplace_back(emitter);
		uint32_t transformIndex = 0;
		if (m_FreeTransformSlots.empty())
		{
			transformIndex = (uint32_t)m_Transforms.size();
			m_Transforms.emplace_back();
		}
		else
		{
			transformIndex = m_FreeTransformSlots.back();
			m_FreeTransformSlots.pop_back();
		}
		m_EmitterTransformsMapping[emitter.ID] = transformIndex;
		m_Transforms[transformIndex] = transform * Math::ToTransformMatrix(emitter.RelativeTransform);
		m_SystemToEmittersMapping[systemID].emplace_back(emitter);

		return true;
	}

	bool ParticleSystemTask::RemoveEmitter(const ParticleEmitter& emitter, const GUID& systemID)
	{
		auto it = m_EmittersMapping.find(emitter);
		if (it == m_EmittersMapping.end())
		{
			EG_CORE_ASSERT(false); // Shouldn't really happen
			return false; // Not found
		}

		m_EmittersToRemove.emplace_back(std::pair{ emitter, it->second });
		auto transformIt = m_EmitterTransformsMapping.find(emitter.ID);
		EG_CORE_ASSERT(transformIt != m_EmitterTransformsMapping.end()); // Should never happen
		const uint32_t transformIndex = transformIt->second;
		m_FreeTransformSlots.push_back(transformIndex);
		m_EmitterTransformsMapping.erase(transformIt);
		m_EmittersMapping.erase(it);

		auto& systemEmitters = m_SystemToEmittersMapping[systemID];
		auto itEmitter = std::find(systemEmitters.begin(), systemEmitters.end(), emitter);
		systemEmitters.erase(itEmitter);

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
			auto& data = updateData.emplace_back();
			data.Emitters = system->Emitters;
			data.Transformation = Math::ToTransformMatrix(system->GetWorldTransform());
			data.SystemID = system->Parent.GetGUID();
		}

		RenderManager::Submit([task = shared_from_this(), updateData = std::move(updateData)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& [emitters, transform, systemID] : updateData)
			{
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
			auto& data = updateData.emplace_back();
			data.Emitters = system->Emitters;
			data.Transformation = Math::ToTransformMatrix(system->GetWorldTransform());
			data.SystemID = system->Parent.GetGUID();
		}

		RenderManager::Submit([task = shared_from_this(), updateData = std::move(updateData)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& [emitters, transform, systemID] : updateData)
			{
				// Intentional copy because `AddEmitter` and `RemoveEmitter` functions modify it
				auto systemEmitters = thisRef->m_SystemToEmittersMapping.at(systemID);

				// Remove emitters if not found in the new list
				for (const auto& currentEmitter : systemEmitters)
				{
					auto it = thisRef->m_EmittersMapping.find(currentEmitter);
					if (it == thisRef->m_EmittersMapping.end())
					{
						EG_CORE_ASSERT(false); // Shouldn't really happen
						continue;
					}

					auto it2 = std::find(emitters.begin(), emitters.end(), it->first);
					if (it2 == emitters.end()) // Old emitter isn't found in the new list, so remove it
						thisRef->RemoveEmitter(*it2, systemID);
				}

				// Update or create emitters
				for (const auto& emitter : emitters)
				{
					auto it = thisRef->m_EmittersMapping.find(emitter);
					if (it == thisRef->m_EmittersMapping.end())
					{
						EG_CORE_ASSERT(false); // Shouldn't really happen
						continue;
					}

					auto itEmitterInSystem = std::find(systemEmitters.begin(), systemEmitters.end(), emitter);
					if (itEmitterInSystem == systemEmitters.end())
					{
						thisRef->AddEmitter(emitter, systemID, transform); // New emitter isn't found in the old list, so add it
					}
					else
					{
						const uint32_t emitterIndex = it->second;
						// Update key
						thisRef->m_EmittersMapping.erase(it);
						thisRef->m_EmittersMapping.emplace(emitter, emitterIndex);

						*itEmitterInSystem = emitter;

						// New emitter is found in the old list, so update its state
						thisRef->m_EmittersToUpdate.emplace_back(emitter, emitterIndex);
						uint32_t transformIndex = thisRef->m_EmitterTransformsMapping.at(emitter.ID);
						thisRef->m_Transforms[transformIndex] = transform * Math::ToTransformMatrix(emitter.RelativeTransform);
					}
				}
			}
		});
	}

	void ParticleSystemTask::RemoveParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		struct SystemRemoveData
		{
			std::vector<ParticleEmitter> Emitters;
			GUID SystemID;
		};

		std::vector<SystemRemoveData> removeData;
		removeData.reserve(systems.size());
		for (const auto& system : systems)
		{
			auto& data = removeData.emplace_back();
			data.Emitters = system->Emitters;
			data.SystemID = system->Parent.GetGUID();
		}

		RenderManager::Submit([task = shared_from_this(), removeData = std::move(removeData)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);
			for (const auto& [emitters, systemID] : removeData)
			{
				for (const auto& emitter : emitters)
				{
					thisRef->RemoveEmitter(emitter, systemID);
				}
				thisRef->m_SystemToEmittersMapping.erase(systemID);
			}
		});
	}

	void ParticleSystemTask::UpdateTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems)
	{
		std::vector<std::pair<glm::mat4, GUID>> newTransforms;
		for (const auto& system : systems)
		{
			const glm::mat4 systemTr = Math::ToTransformMatrix(system->GetWorldTransform());
			for (const auto& emitter : system->Emitters)
			{
				auto& data = newTransforms.emplace_back();
				data.first = systemTr * Math::ToTransformMatrix(emitter.RelativeTransform);
				data.second = emitter.ID;
			}
		}

		RenderManager::Submit([task = shared_from_this(), newTransforms = std::move(newTransforms)](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<ParticleSystemTask>(task);

			for (const auto& [newTransform, emitterID] : newTransforms)
			{
				auto transformIt = thisRef->m_EmitterTransformsMapping.find(emitterID);
				if (transformIt != thisRef->m_EmitterTransformsMapping.end())
				{
					const uint32_t transformIndex = transformIt->second;
					thisRef->m_Transforms[transformIndex] = newTransform;
					thisRef->bUpdateTransforms = true;
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

			specs.Size = m_MaxParticles * sizeof(Particle);
			m_ParticlesBuffer = Buffer::Create(specs, "Particles");

			specs.Size = m_MaxParticles * sizeof(uint32_t);
			m_AliveIndices[0] = Buffer::Create(specs, "ParticleSystem_AliveIndices_Pre");
			m_AliveIndices[1] = Buffer::Create(specs, "ParticleSystem_AliveIndices_Post");
			m_DeadIndices = Buffer::Create(specs, "ParticleSystem_DeadIndices");
			m_IndicesToRender = Buffer::Create(specs, "ParticleSystem_IndicesToRender");
			m_DistancesBuffer = Buffer::Create(specs, "ParticleSystem_Distances");
			
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			specs.Size = m_MaxEmitters * sizeof(Emitter);
			m_EmittersBuffer = Buffer::Create(specs, "ParticleSystem_Emitters");

			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			specs.Size = m_MaxEmitters * sizeof(glm::mat4);
			m_TransformsBuffer = Buffer::Create(specs, "ParticleSystem_Transforms");

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

			specs.Size = sizeof(DrawIndirectArgs);
			m_DrawArgs = Buffer::Create(specs, "ParticleSystem_DrawArgs");
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
			
			state.ComputeShader = Shader::Create("particle_system/simulate.comp", ShaderType::Compute);
			m_Simulate = PipelineCompute::Create(state);
		}

		// Graphics pipelines with Alpha-blending
		{
			const auto& gBuffer = m_Renderer.GetGBuffer();
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

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = gBuffer.Depth;
			// depthAttachment.bWriteDepth = true; TODO: Should enable?
			depthAttachment.DepthCompareOp = CompareOperation::LessEqual;
			depthAttachment.ClearOperation = ClearOperation::Load;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("particle_system/particle2D.vert", ShaderType::Vertex);
			state.FragmentShader = Shader::Create("particle_system/particle.frag", ShaderType::Fragment);
			state.ColorAttachments.push_back(colorAttachment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;

			if (m_BillboardRender)
				m_BillboardRender->SetState(state);
			else
				m_BillboardRender = PipelineGraphics::Create(state);
		}
	}
}
