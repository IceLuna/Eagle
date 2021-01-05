#pragma once

#include <chrono>

namespace Eagle
{
	template <typename Fn>
	class Timer
	{
	public:

		Timer(const char* name, Fn&& func)
		:	m_Name(name), m_Func(func), m_Stopped(false)
		{
			m_StartPoint = std::chrono::steady_clock::now();
		}

		~Timer()
		{
			if (!m_Stopped)
			{
				Stop();
			}
		}

		void Start()
		{
			m_Stopped = false;
			m_StartPoint = std::chrono::steady_clock::now();
		}

		void Stop()
		{
			auto end = std::chrono::steady_clock::now();
			m_Stopped = true;

			float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_StartPoint).count() * 0.001f;

			m_Func({m_Name, duration});
		}

	private:
		std::chrono::time_point<std::chrono::steady_clock> m_StartPoint;
		const char* m_Name;
		Fn m_Func;
		bool m_Stopped;
	};

}