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

		struct Data
		{
			std::string_view Name;
			mutable float Timing = 0.f;
			mutable std::vector<Data> Children;

			bool operator< (const Data& other) const
			{
				return Name < other.Name;
			}
		};

		std::string_view GetName() const { return m_Data.Name; }
		float GetTiming() const { return m_Data.Timing; }
		const std::vector<Data>& GetChildren() const { return m_Data.Children; }
		const Data& GetData() const { return m_Data; }

	private:
		void SetParent(CPUTiming* parent)
		{
			m_Parent = parent;
		}

		const CPUTiming* GetParent() const { return m_Parent; }

	private:
		CPUTiming* m_Parent = nullptr;

		std::chrono::high_resolution_clock::time_point m_StartTime;

		Data m_Data;

		bool m_bScoped = false;
	};
}

#define EG_CPU_TIMING_SCOPED(name) CPUTiming EG_CONCAT(cpu_debug_timing, __LINE__)(name, true)

#else

#define EG_CPU_TIMING_SCOPED(...)

#endif
