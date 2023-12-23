#include "egpch.h"
#include "ThreadPool.h"

namespace Eagle
{
	ThreadPool::ThreadPool(std::string_view name, uint32_t numThreads)
		: m_ThreadPool(numThreads)
		, m_Name(name)
	{
		Application::Get().AddThread(*this);
		SetName();
	}
	
	void ThreadPool::SetName()
	{
		// Setting the thread name, for example, for debugger
#ifdef EG_PLATFORM_WINDOWS
		auto& threads = m_ThreadPool.get_threads();
		const uint32_t threadsCount = m_ThreadPool.get_thread_count();
		if (threadsCount == 1)
		{
			auto& thread = threads[0];
			const std::string name = m_Name.data();
			const std::wstring wideName(name.begin(), name.end());
			SetThreadDescription((HANDLE)thread.native_handle(), wideName.c_str());
		}
		else
		{
			for (uint32_t i = 0; i < threadsCount; ++i)
			{
				auto& thread = threads[i];
				const std::string name = m_Name.data() + std::string(" #") + std::to_string(i);
				const std::wstring wideName(name.begin(), name.end());
				SetThreadDescription((HANDLE)thread.native_handle(), wideName.c_str());
			}
		}
#endif
	}
}
