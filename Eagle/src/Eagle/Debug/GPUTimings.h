#pragma once

#include "Eagle/Core/Core.h"

#ifdef EG_GPU_TIMINGS

namespace Eagle
{
	class RHIGPUTiming
	{
	public:
		static constexpr uint32_t InvalidQuery = uint32_t(-1);

		virtual ~RHIGPUTiming()
		{
			Destroy();
		}

		// Averaged over the last frames. If it's used several times in a frame, it's the sum of all of them
		float GetTiming() const { return m_Timing; }

		std::chrono::steady_clock::time_point GetLastUsedTime() const { return m_LastUsedTime; }

		virtual void* GetQueryPoolHandle() = 0;

		// Returns the index of the query pair to write the start and end timestamps to, or `InvalidQuery` if there's no free one.
		// In that case, `Update` grows the pool once it reads that frame, and the next frames are measured fully
		uint32_t AllocateQuery(uint32_t frameIndex)
		{
			const uint32_t use = m_UsedQueries[frameIndex]++;
			return use < m_QueriesPerFrame ? frameIndex * m_QueriesPerFrame + use : InvalidQuery;
		}

		// Reads the queries that `frameIndex` wrote and adds them to the average. The GPU must be done with that frame
		void Update(uint32_t frameIndex, std::chrono::steady_clock::time_point now);

		static Ref<RHIGPUTiming> Create(const std::string_view name);

		void SetParent(RHIGPUTiming* parent);
		const RHIGPUTiming* GetParent() const { return m_Parent; }

		const std::vector <RHIGPUTiming*>& GetChildren() const { return m_Children; }
		const std::string_view GetName() const { return m_Name; }

		void Destroy()
		{
			if (m_Parent)
			{
				m_Parent->RemoveChild(this);
				m_Parent = nullptr;
			}

			for (auto& child : m_Children)
				child->m_Parent = nullptr;
			m_Children.clear();
		}

	protected:
		RHIGPUTiming();

		virtual void RecreatePool() = 0;

		// Sums the durations of the first `count` query pairs of `frameIndex`.
		// Returns false if they aren't available
		virtual bool ReadTimings(uint32_t frameIndex, uint32_t count, float& outTiming) = 0;

		void AddChild(RHIGPUTiming* timing)
		{
			m_Children.push_back(timing);
		}

		void RemoveChild(const RHIGPUTiming* timing)
		{
			auto it = std::find(m_Children.begin(), m_Children.end(), timing);
			if (it != m_Children.end())
				m_Children.erase(it);
		}

	protected:
		std::string_view m_Name;
		RHIGPUTiming* m_Parent = nullptr;
		std::vector <RHIGPUTiming*> m_Children;
		std::vector<uint32_t> m_UsedQueries; // How many times it was used in each frame in flight
		std::chrono::steady_clock::time_point m_LastUsedTime;
		uint32_t m_QueriesPerFrame = 1;
		float m_Timing = 0.f;
		bool m_bHasTiming = false;
	};

	class CommandBuffer;

	class GPUTiming
	{
	public:
		GPUTiming(const Ref<CommandBuffer>& cmd, const std::string_view name);
		~GPUTiming();

		void Start();
		void End();

	private:
		Ref<CommandBuffer> m_Cmd;
		Ref<RHIGPUTiming> m_GPUTiming;
		std::string_view m_Name;
		uint32_t m_QueryIndex = RHIGPUTiming::InvalidQuery;
		bool m_bStarted = false;
	};
}

#define EG_GPU_TIMING_SCOPED(cmd, name) GPUTiming EG_CONCAT(debug_timing, __LINE__)(cmd, name)

#else

#define EG_GPU_TIMING_SCOPED(...)

#endif
