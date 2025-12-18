#pragma once

#include <chrono>

namespace Eagle
{
	class Timer
	{
		using clock = std::chrono::high_resolution_clock;

	public:
		Timer()
			: m_Start(clock::now())
		{}

		void Restart()
		{
			m_Start = clock::now();
		}

		template<typename Precision = std::chrono::milliseconds>
		uint64_t GetDuration() const
		{
			return std::chrono::duration_cast<Precision>(clock::now() - m_Start).count();
		}

		uint64_t GetMilliseconds() const
		{
			return GetDuration<std::chrono::milliseconds>();
		}

		double GetSeconds() const
		{
			return GetDuration<std::chrono::milliseconds>() / 1000.0;
		}

	private:
		clock::time_point m_Start;
	};
}
