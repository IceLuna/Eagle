#pragma once

#include "Eagle/Core/Core.h"

#ifdef EG_GPU_TIMINGS

namespace Eagle
{
	class RHIGPUTiming
	{
	public:
		virtual ~RHIGPUTiming();

		float GetTiming() const { return m_Timing; }

		virtual void* GetQueryPoolHandle() = 0;
		virtual void QueryTiming(uint32_t frameInFlight) = 0;

		static Ref<RHIGPUTiming> Create(const std::string_view name);

		void SetParent(RHIGPUTiming* parent);
		const RHIGPUTiming* GetParent() const { return m_Parent; }

		const std::vector <RHIGPUTiming*>& GetChildren() const { return m_Children; }
		const std::string_view GetName() const { return m_Name; }

	public:
		bool bIsUsed = true;

	protected:
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
		float m_Timing = 0.f;
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
		uint32_t m_FrameIndex = uint32_t(-1);
		bool m_bStarted = false;
	};
}

#define EG_GPU_TIMING_SCOPED(cmd, name) GPUTiming EG_CONCAT(debug_timing, __LINE__)(cmd, name)

#else

#define EG_GPU_TIMING_SCOPED(...)

#endif
