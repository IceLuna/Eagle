#pragma once

#include "Shader.h"
#include "Pipeline.h"

namespace Eagle
{
	inline uint32_t CalcNumGroups(uint32_t size, uint32_t groupSize)
	{
		return (size + groupSize - 1) / groupSize;
	}

	inline glm::uvec2 CalcNumGroups(glm::uvec2 size, uint32_t groupSize)
	{
		return (size + groupSize - 1u) / groupSize;
	}

	inline glm::uvec3 CalcNumGroups(glm::uvec3 size, uint32_t groupSize)
	{
		return (size + groupSize - 1u) / groupSize;
	}

	struct PipelineComputeState
	{
		Ref<Shader> ComputeShader;
		ShaderSpecializationInfo ComputeSpecializationInfo;
	};

	class PipelineCompute : virtual public Pipeline
	{
	public:
		virtual void SetState(const PipelineComputeState& state) = 0;
	
	protected:
		PipelineCompute(const PipelineComputeState& state) : m_State(state) {}

	public:
		const PipelineComputeState& GetState() const { return m_State; }
		static Ref<PipelineCompute> Create(const PipelineComputeState& state, const Ref<PipelineCompute>& parentPipeline = nullptr);

	protected:
		PipelineComputeState m_State;
	};
}
