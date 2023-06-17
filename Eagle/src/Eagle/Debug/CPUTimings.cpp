#include "egpch.h"

#ifdef EG_CPU_TIMINGS

#include "CPUTimings.h"
#include "Eagle/Core/Application.h"

namespace Eagle
{
	static thread_local std::vector<CPUTiming*> s_TimingsStack;

	Eagle::CPUTiming::CPUTiming(const std::string_view name, bool bScoped)
		: m_bScoped(bScoped)
	{
		m_Data.Name = name;
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

		if (!s_TimingsStack.empty())
		{
			SetParent(s_TimingsStack.back());
		}
		s_TimingsStack.push_back(this);
	}

	void CPUTiming::End()
	{
		auto duration = std::chrono::high_resolution_clock::now() - m_StartTime;
		m_Data.Timing = float(std::chrono::duration_cast<std::chrono::duration<double>>(duration).count() * 1000.0);

		if (m_Parent)
		{
			m_Parent->m_Data.Children.push_back(m_Data);
		}
		else
		{
			Application::Get().AddCPUTiming(this);
		}
		s_TimingsStack.pop_back();
	}
}

#endif
