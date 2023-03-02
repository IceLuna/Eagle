#pragma once

#include "Eagle/Core/Core.h"

#ifdef EG_CPU_TIMINGS

namespace Eagle
{
	class CPUTiming
	{
	public:
		CPUTiming(const std::string_view name, bool bScoped);
		~CPUTiming();

		void Start();
		void End();

	private:
		std::chrono::high_resolution_clock::time_point m_StartTime;
		std::string_view m_Name;
		bool m_bScoped = false;
	};
}

#define EG_CPU_TIMING_SCOPED(name) CPUTiming EG_CONCAT(cpu_debug_timing, __LINE__)(name, true)

#else

#define EG_CPU_TIMING_SCOPED(...)

#endif
