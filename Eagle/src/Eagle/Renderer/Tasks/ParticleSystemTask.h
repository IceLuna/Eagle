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
		void RemoveParticleSystem(const ParticleSystemComponent& system);
		void RemoveAllParticleSystems();
		void UpdateTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems);

		// We need to apply scale.XY and rotation.Z only, so instead of decompositing it on GPU side, we'll send it there
		struct DecompositedTransform
		{
			float ScaleX = 0.f;
			float ScaleY = 0.f;
			float RotationZ = 0.f; // Radians
			float Padding0 = 0.f;
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

			ParticleSystemData(uint32_t deadCount) : DeadCount(deadCount) {}
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
		std::vector<DeadEmitterData> m_DeadEmitters;

		std::vector<glm::mat4> m_Transforms;
		std::vector<DecompositedTransform> m_DecompositedTransforms;
		std::vector<uint32_t> m_FreeTransformSlots; // Free slots inside of `m_Transforms`

		Ref<Buffer> m_TransformsBuffer;
		Ref<Buffer> m_DecompositedTransformsBuffer;
		Ref<Buffer> m_ParticlesBuffer;
		Ref<Buffer> m_EmittersSpawnCountBuffer;
		Ref<Buffer> m_EmittersBuffer;
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
		Ref<PipelineCompute> m_Simulate;

		Ref<PipelineGraphics> m_BillboardRenderTranslucent;
		Ref<PipelineGraphics> m_BillboardRender;

		glm::uvec2 m_Size;
		uint32_t m_PingPong = 0; // Pre/Post simulation index
		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		bool bUpdateTransforms = false;
		bool bSortOpaque = false;

		uint32_t m_NumEmitters = 0;
		uint32_t m_MaxParticles = 100000;
		uint32_t m_MaxEmitters = 100;

		Scope<SortTask> m_SortOpaque;
		SortTask m_SortTranslucent;

		constexpr static uint32_t s_InvalidEmitterIndex = uint32_t(-1);
	};
}
