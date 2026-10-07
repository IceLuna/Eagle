#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/Renderer/Tasks/SortTask.h"
#include "Eagle/Utils/Timer.h"

namespace Eagle
{
	class Image;
	class Buffer;
	class ParticleSystemComponent;

	class ParticleSystemTask : public RendererTask
	{
	public:
		ParticleSystemTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override;
		void InitWithOptions(const SceneRendererSettings& settings);

		void AddParticleSystem(const ParticleSystemComponent& system);
		void UpdateParticleSystem(const ParticleSystemComponent& system);
		void RemoveParticleSystem(const ParticleSystemComponent& system, bool bForceImmediateRemoval);
		void RemoveAllParticleSystems();
		void UpdateTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems);

		// Instead of decompositing it on GPU side per particle every frame, we'll send it there
		// Must match `DecompositedTransform` in particle_system/common.h.
		struct DecompositedTransform
		{
			// World rotation without scale
			glm::vec3 RotationColumn0 = glm::vec3(1.f, 0.f, 0.f);
			float RotationZ = 0.f; // Radians
			glm::vec3 RotationColumn1 = glm::vec3(0.f, 1.f, 0.f);
			float ScaleX = 0.f;
			glm::vec3 RotationColumn2 = glm::vec3(0.f, 0.f, 1.f);
			float ScaleY = 0.f;
		};

		struct EmitterData
		{
			uint32_t EmitterIndex = s_InvalidEmitterIndex; // index of the emitter inside of `m_EmittersBuffer`
			uint32_t TransformIndex = s_InvalidEmitterIndex; // index of the emitter inside of `m_Transforms`
			uint32_t AnimationOffset = s_InvalidEmitterIndex;

			bool IsEmitterIndexValid() const
			{
				return EmitterIndex != s_InvalidEmitterIndex;
			}
		};

		struct MeshEmitterData
		{
			uint32_t VertexOffset = 0u;
			uint32_t IndexOffset = 0u;
			uint32_t IndexCount = 0u;
			uint32_t UsageCounter = 1u; // If it reaches 0, mesh is removed from the mapping and mesh buffers are rebuilt
		};

		struct ParticleStaticMeshVertex
		{
			glm::vec3 Position = glm::vec3(0);
			uint32_t Normal = 0; // Packed. Used to set initial velocity.
		};

		struct ParticleSkeletalMeshVertex
		{
			glm::vec3 Position = glm::vec3(0);
			uint32_t Normal = 0; // Packed. Used to set initial velocity.
			uint16_t Weights[EG_MAX_BONES_PER_VERTEX] = { 0, 0, 0, 0 }; // unorm16
			uint16_t BoneID[EG_MAX_BONES_PER_VERTEX] = { 0, 0, 0, 0 };
		};

	private:
		bool AddEmitter(const ParticleEmitter& emitter, const GUID& systemID, const glm::mat4& transform);
		bool RemoveEmitter(const ParticleEmitter& emitter, const GUID& systemID, bool bForceImmediateRemoval = false);

		void InitResources();
		void InitPipelines();
		void InitSortOpaqueResources();

		void Update(const Ref<CommandBuffer>& cmd);
		void UpdateSkeletalAnimations(const Ref<CommandBuffer>& cmd);
		void PreparePass(const Ref<CommandBuffer>& cmd);
		void EmitPass(const Ref<CommandBuffer>& cmd);
		void SimulatePass(const Ref<CommandBuffer>& cmd);
		void RenderPass(const Ref<CommandBuffer>& cmd);

		void SetMaxParticles(const Ref<CommandBuffer>& cmd, uint32_t maxParticles);
		void ShrinkMaxParticles(const Ref<CommandBuffer>& cmd, uint32_t maxParticles);
		void GrowPoolIfParticlesWereDropped(const Ref<CommandBuffer>& cmd);
		void ReadBackDroppedParticles(const Ref<CommandBuffer>& cmd);
		void ResetGPUState(const Ref<CommandBuffer>& cmd);
		void ReclaimDeadEmitters();
		void UpdateMeshEmittersData(const Ref<CommandBuffer>& cmd);
		void WriteEmitterCurves(const Ref<CommandBuffer>& cmd, const ParticleEmitter& emitter, uint32_t emitterIndex);
		void WriteSubEmitterLinks(const Ref<CommandBuffer>& cmd, const GUID& systemID);

		void AddEmitterMeshData(const ParticleEmitter& emitter);
		void RemoveEmitterMeshData(const ParticleEmitter& emitter);
		MeshEmitterData GetEmitterMeshData(const ParticleEmitter& emitter);

		struct ModifyRequest
		{
			ParticleEmitter Emitter;
			GUID SystemID = GUID(0, 0);
			EmitterData Indices{};
			bool bDestroyImmediately = false;

			enum class RequestType
			{
				Add, Update, Remove
			} Type;
		};

		void HandleEmitter_Add_RT(const Ref<CommandBuffer>& cmd, const ModifyRequest& data);
		void HandleEmitter_Update_RT(const Ref<CommandBuffer>& cmd, const ModifyRequest& data);
		void HandleEmitter_Remove_RT(const Ref<CommandBuffer>& cmd, const ModifyRequest& data);

	private:
		struct ParticleSystemData
		{
			uint32_t AliveCount[2] = { 0, 0 };
			uint32_t EmitCount = 0;
			uint32_t SimulateCount = 0;
			uint32_t DeadCount = 0;
			uint32_t DroppedCount = 0;
			uint32_t EventSpawnBudget = 0;
			uint32_t Padding0 = 0;
			glm::mat4 CullingView = glm::mat4(1.f);
			glm::vec4 CullingFrustum = glm::vec4(0.f);

			ParticleSystemData(uint32_t deadCount) : DeadCount(deadCount) {}
		};

		// The prepare pass counts the particles it couldn't spawn because the pool was full (`ParticleSystemData::DroppedCount`).
		// The count is copied to a CPU-readable buffer every frame, and read `RendererConfig::FramesInFlight` frames later.
		// When particles were dropped, the pool grows (up to the budget)
		struct DroppedParticlesReadback
		{
			Ref<Buffer> ReadbackBuffer;
			uint32_t MaxParticlesAtSubmit = 0;
			bool bPending = false;
		};

		struct DeadEmitterData
		{
			Timer TimeOfDeath;
			uint32_t EmitterIndex = 0; // index of the emitter inside of `m_EmittersBuffer`
			uint32_t TransformIndex = 0;
			float TimeTillDead = 0.f; // In seconds

			bool IsDead() const
			{
				const double duration = TimeOfDeath.GetSeconds();
				if (duration >= TimeTillDead)
					return true;

				return false;
			}
		};

		ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<ParticleEmitter, EmitterData>> m_SystemToEmittersMapping; // Key - Particle system; Value - its emitters
		std::vector<ModifyRequest> m_ModifyRequestQueue;
		std::vector<DeadEmitterData> m_DeadEmitters; // Removed emitters whose particles might still be alive
		std::vector<uint32_t> m_FreeEmitterSlots; // Free slots inside of `m_EmittersBuffer`
		std::vector<uint32_t> m_EmitterGenerations; // Per slot of `m_EmittersBuffer`. Bumped every time a slot is (re)used

		std::vector<glm::mat4> m_Transforms;
		std::vector<DecompositedTransform> m_DecompositedTransforms;
		std::vector<uint32_t> m_FreeTransformSlots; // Free slots inside of `m_Transforms`

		Ref<Buffer> m_TransformsBuffer;
		Ref<Buffer> m_DecompositedTransformsBuffer;
		Ref<Buffer> m_ParticlesBuffer;
		Ref<Buffer> m_EmittersSpawnCountBuffer;
		Ref<Buffer> m_EmittersBuffer;
		Ref<Buffer> m_EmitterCurvesBuffer; // Over-lifetime values of every emitter slot, baked from its curves
		Ref<Buffer> m_SubEmitterLinksBuffer;
		Ref<Buffer> m_ParticleEventsBuffer; // Sub-emitter events
		Ref<Buffer> m_EventSpawnOffsetsBuffer;
		Ref<Buffer> m_AliveIndices[2]; // Pre/Post simulation
		Ref<Buffer> m_DeadIndices;
		Ref<Buffer> m_SystemData;
		Ref<Buffer> m_DispatchArgs;
		Ref<Buffer> m_DrawArgs;

		Ref<Buffer> m_OpaqueIndicesToRender;
		Ref<Buffer> m_OpaqueDistancesBuffer;
		Ref<Buffer> m_TranslucentIndicesToRender;
		Ref<Buffer> m_TranslucentDistancesBuffer;

		// For mesh emitters. TODO: Remove this when a bindless(global) mesh buffers are introduced, so that we don't have to duplicate it here
		std::vector<ParticleStaticMeshVertex> m_StaticMeshVertices;
		std::vector<Index> m_StaticMeshIndices;
		Ref<Buffer> m_StaticMeshVertexBuffer;
		Ref<Buffer> m_StaticMeshIndexBuffer;
		ankerl::unordered_dense::map<Ref<StaticMesh>, MeshEmitterData> m_StaticMeshDataMapping; // To avoid duplicating meshes in the memory
		bool bRebuildStaticMeshData = false;

		ankerl::unordered_dense::map<Ref<SkeletalMesh>, MeshEmitterData> m_SkeletalMeshDataMapping; // To avoid duplicating meshes in the memory
		ankerl::unordered_dense::set<GUID> m_ChangedSystemsTemp; // To avoid every frame allocations
		std::vector<ParticleSkeletalMeshVertex> m_SkeletalMeshVertices;
		std::vector<Index> m_SkeletalMeshIndices;
		Ref<Buffer> m_SkeletalMeshVertexBuffer;
		Ref<Buffer> m_SkeletalMeshIndexBuffer;
		std::vector<glm::mat4> m_AnimationTransforms;
		Ref<Buffer> m_AnimationTransformsBuffer;
		bool bRebuildSkeletalMeshData = false;

		Ref<PipelineCompute> m_UpdateMaxParticles;
		Ref<PipelineCompute> m_PrepareData;
		Ref<PipelineCompute> m_Emit;
		Ref<PipelineCompute> m_EmitEvents;
		Ref<PipelineCompute> m_Simulate;

		Ref<PipelineGraphics> m_BillboardRenderTranslucent;
		Ref<PipelineGraphics> m_BillboardRender;

		glm::uvec2 m_Size;
		uint32_t m_PingPong = 0; // Pre/Post simulation index
		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		bool bUpdateTransforms = false;
		bool bSortOpaque = false;
		bool bResetGPUState = false;
		bool bMaxParticlesBudgetChanged = false;
		bool bWarnedBudgetFull = false; // So that "the budget is full" is logged once per budget

		uint32_t m_NumEmitters = 0;
		uint32_t m_MaxParticlesBudget = 0;
		uint32_t m_MaxParticles = s_InitialMaxParticles; // Currently allocated. Grows on demand up to `m_MaxParticlesBudget`
		std::array<DroppedParticlesReadback, RendererConfig::FramesInFlight> m_DroppedReadbacks;
		uint32_t m_DroppedReadbackIndex = 0;
		uint32_t m_MaxEmitters = 100;

		Scope<SortTask> m_SortOpaque;
		SortTask m_SortTranslucent;

		constexpr static uint32_t s_InvalidEmitterIndex = uint32_t(-1);
		constexpr static uint32_t s_InitialMaxParticles = 100000u;
	};
}
