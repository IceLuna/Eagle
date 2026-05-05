#include "egpch.h"
#include "ThreadPool.h"

namespace Eagle
{
	ThreadPool::ThreadPool(std::string_view name, uint32_t numThreads, bool bRegisterPool)
		: m_ThreadPool(numThreads)
		, m_Name(name)
		, bRegister(bRegisterPool)
	{
		if (bRegister)
			Application::Get().AddThread(*this);
		SetName();
	}
	
	void ThreadPool::SetName()
	{
		// Setting the thread name, for example, for debugger
#ifdef EG_PLATFORM_WINDOWS
		const uint32_t threadsCount = (uint32_t)m_ThreadPool.get_thread_count();
		auto nativeHandles = m_ThreadPool.get_native_handles();
		if (threadsCount == 1)
		{
			const std::string name = m_Name.data();
			const std::wstring wideName(name.begin(), name.end());
			SetThreadDescription((HANDLE)nativeHandles[0], wideName.c_str());
		}
		else
		{
			for (uint32_t i = 0; i < threadsCount; ++i)
			{
				const std::string name = m_Name.data() + std::string(" #") + std::to_string(i);
				const std::wstring wideName(name.begin(), name.end());
				SetThreadDescription((HANDLE)nativeHandles[i], wideName.c_str());
			}
		}
#else
#error "Add support"
#endif
	}
}
