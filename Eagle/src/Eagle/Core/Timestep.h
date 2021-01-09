#pragma once

#include <thread>
#include <chrono>

namespace Eagle
{
	class Timestep
	{
	public:
		Timestep(float time = 0.f)
			: m_Time(time) {}

		operator float() const { return m_Time; }

		inline float GetSeconds() const { return m_Time; }
		inline float GetMilliseconds() const { return m_Time * 1000.f; }

	private:
		float m_Time;
	};

	template<typename Fn>
	class DelayCall
	{
	public:
		
		DelayCall(Fn&& fn, uint32_t ms) : m_Fn(std::move(fn)), m_Ms(ms)
		{
			m_Stop = new bool;
			*m_Stop = false;

			std::thread delayCall = std::thread([] (Fn fn, uint32_t ms, bool* stop)
			{
				std::chrono::milliseconds chronoMS(ms);
				std::this_thread::sleep_for(chronoMS);
				if (!(*stop))
				{
					fn();
				}
				delete stop;
			}, fn, ms, m_Stop);

			delayCall.detach();
		}

		inline void Stop() { *m_Stop = true; }

	private:
		Fn m_Fn;
		float m_Ms;
		bool* m_Stop;
	};

}