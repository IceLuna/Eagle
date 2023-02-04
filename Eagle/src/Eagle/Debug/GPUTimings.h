#pragma once

#include "Eagle/Core/Core.h"

#ifdef EG_GPU_TIMINGS

namespace Eagle
{
	class RHIGPUTiming
	{
	public:
		virtual ~RHIGPUTiming() = default;

		float GetTiming() const { return m_Timing; }

		virtual void* GetQueryPoolHandle() = 0;
		virtual void QueryTiming(uint32_t frameInFlight) = 0;

		static Ref<RHIGPUTiming> Create();

	public:
		bool bIsUsed = true;

	protected:
		float m_Timing = 0.f;
	};

	class CommandBuffer;

	class GPUTimings
	{
	public:
		GPUTimings(Ref<CommandBuffer>& cmd, const std::string_view name, bool bScoped);
		~GPUTimings();

		void Start();
		void End();

	private:
		Ref<CommandBuffer> m_Cmd;
		Ref<RHIGPUTiming> m_GPUTiming;
		std::string_view m_Name;
		uint32_t m_FrameIndex = uint32_t(-1);
		bool m_bStarted = false;
		bool m_bScoped = false;
	};
}

#define EG_GPU_TIMING_SCOPED(cmd, name) GPUTimings debug_timing##line(cmd, name, true)

#else

#define EG_GPU_TIMING_SCOPED(...)

#endif
