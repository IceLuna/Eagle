#include "egpch.h"

#ifdef EG_GPU_TIMINGS

#include "CPUTimings.h"
#include "Eagle/Core/Application.h"

namespace Eagle
{
	Eagle::CPUTiming::CPUTiming(const std::string_view name, bool bScoped)
		: m_Name(name), m_bScoped(bScoped)
	{
		Start();
	}

	CPUTiming::~CPUTiming()
	{
		if (m_bScoped)
			End();
	}

	void CPUTiming::Start()
	{
		m_StartTime = std::chrono::high_resolution_clock::now();
	}

	void CPUTiming::End()
	{
		auto duration = std::chrono::high_resolution_clock::now() - m_StartTime;
		double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count() * 1000.0;
		Application::Get().AddCPUTiming(m_Name, float(elapsed));
	}
}

#endif
