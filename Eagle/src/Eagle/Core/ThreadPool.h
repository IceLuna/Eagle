#pragma once

#include "BS_thread_pool.hpp"
#include "Application.h"

namespace Eagle
{
	class ThreadPool
	{
	public:
		ThreadPool(std::string_view name, uint32_t numThreads = 0);

		~ThreadPool() noexcept
		{
			Application::Get().RemoveThread(*this);
		}

		BS::thread_pool* operator->()
		{
			return &m_ThreadPool;
		}

		const BS::thread_pool* operator->() const
		{
			return &m_ThreadPool;
		}
		
		std::string_view GetName() const { return m_Name; }

	private:
		void SetName();

	private:
		BS::thread_pool m_ThreadPool;
		std::string_view m_Name;
	};
}
