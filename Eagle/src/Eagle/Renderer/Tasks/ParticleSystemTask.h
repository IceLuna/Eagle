#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/Renderer/Tasks/SortTask.h"

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

		void AddParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems);
		void UpdateParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems);
		void RemoveParticleSystems(const std::unordered_set<const ParticleSystemComponent*>& systems);
		void UpdateTransforms(const std::unordered_set<const ParticleSystemComponent*>& systems);

	private:
		bool AddEmitter(const ParticleEmitter& emitter, const GUID& systemID, const glm::mat4& transform);
		bool RemoveEmitter(const ParticleEmitter& emitter, const GUID& systemID);

		void InitResources();
		void InitPipelines();

		void Update(const Ref<CommandBuffer>& cmd);
		void PreparePass(const Ref<CommandBuffer>& cmd);
		void EmitPass(const Ref<CommandBuffer>& cmd);
		void SimulatePass(const Ref<CommandBuffer>& cmd);
		void RenderPass(const Ref<CommandBuffer>& cmd);

		void SetMaxParticles(const Ref<CommandBuffer>& cmd, uint32_t maxParticles);

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
			uint32_t EmitterIndex = 0;
			std::chrono::high_resolution_clock::time_point TimeOfDeath;
			float TimeTillDead = 0.f; // In seconds

			bool IsDead() const
			{
				const auto now = std::chrono::high_resolution_clock::now();
				const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - TimeOfDeath).count() / 1000.f; // To seconds
				if (duration >= TimeTillDead)
					return true;

				return false;
			}
		};

		std::unordered_map<ParticleEmitter, uint32_t> m_EmittersMapping; // uint32_t - index of the emitter inside of `m_EmittersBuffer` 
		std::vector<ParticleEmitter> m_EmittersToAdd;
		std::vector<std::pair<ParticleEmitter, uint32_t>> m_EmittersToUpdate; // uint32_t - index of the emitter inside of `m_EmittersBuffer`
		std::vector<std::pair<ParticleEmitter, uint32_t>> m_EmittersToRemove; // uint32_t - index of the emitter inside of `m_EmittersBuffer`
		std::vector<DeadEmitterData> m_DeadEmitters;
		std::unordered_map<GUID, std::vector<ParticleEmitter>> m_SystemToEmittersMapping; // Key - Particle system; Value - its emitters.

		std::vector<glm::mat4> m_Transforms;
		std::unordered_map<GUID, uint32_t> m_EmitterTransformsMapping; // GUID - Emitter ID; uint32_t - index of the emitter inside of `m_Transforms` 
		std::vector<uint32_t> m_FreeTransformSlots; // Free slots inside of `m_Transforms`

		Ref<Buffer> m_TransformsBuffer;
		Ref<Buffer> m_ParticlesBuffer;
		Ref<Buffer> m_EmittersSpawnCountBuffer;
		Ref<Buffer> m_EmittersBuffer;
		Ref<Buffer> m_AliveIndices[2]; // Pre/Post simulation
		Ref<Buffer> m_IndicesToRender;
		Ref<Buffer> m_DeadIndices;
		Ref<Buffer> m_SystemData;
		Ref<Buffer> m_DispatchArgs;
		Ref<Buffer> m_DrawArgs;
		Ref<Buffer> m_DistancesBuffer;

		Ref<PipelineCompute> m_UpdateMaxParticles;
		Ref<PipelineCompute> m_PrepareData;
		Ref<PipelineCompute> m_Emit;
		Ref<PipelineCompute> m_Simulate;

		Ref<PipelineGraphics> m_BillboardRender; // Alpha-blending enabled

		glm::uvec2 m_Size;
		uint32_t m_PingPong = 0; // Pre/Post simulation index
		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		bool bUpdateTransforms = false;

		uint32_t m_NumEmitters = 0;
		uint32_t m_MaxParticles = 100000;
		uint32_t m_MaxEmitters = 100;

		SortTask m_Sort;
	};
}
