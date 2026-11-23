#include "egpch.h"

#ifdef EG_GPU_TIMINGS

#include "GPUTimings.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/RenderManager.h"

#include "Platform/Vulkan/VulkanGPUTimings.h"

namespace Eagle
{
	extern std::mutex g_TimingsMutex;

	// How much each frame moves the average towards its own value
	static constexpr float s_GPUTimingAveraging = 0.05f;

	RHIGPUTiming::RHIGPUTiming()
		: m_UsedQueries(RendererConfig::FramesInFlight, 0u)
		, m_LastUsedTime(std::chrono::steady_clock::now())
	{}

	void RHIGPUTiming::Update(uint32_t frameIndex, std::chrono::steady_clock::time_point now)
	{
		const uint32_t used = m_UsedQueries[frameIndex];
		m_UsedQueries[frameIndex] = 0;
		if (used == 0)
			return;

		m_LastUsedTime = now;
		if (used > m_QueriesPerFrame)
		{
			// Some of its uses weren't measured, so the total is incomplete. The other frames in flight wrote to the old pool, so they're skipped too
			m_QueriesPerFrame = std::max(used, m_QueriesPerFrame * 2);
			RecreatePool();
			std::fill(m_UsedQueries.begin(), m_UsedQueries.end(), 0u);
			return;
		}

		float timing = 0.f;
		if (!ReadTimings(frameIndex, used, timing))
			return;

		if (m_bHasTiming)
		{
			m_Timing += (timing - m_Timing) * s_GPUTimingAveraging;
		}
		else
		{
			m_Timing = timing;
			m_bHasTiming = true;
		}
	}

	Ref<RHIGPUTiming> RHIGPUTiming::Create(const std::string_view name)
	{
		Ref <RHIGPUTiming> result;
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan: result = MakeRef<VulkanGPUTiming>(); break;
			default: EG_ASSERT(false);
		}

		result->m_Name = name;
		return result;
	}

	static std::vector<RHIGPUTiming*> s_TimingsStack;

	void RHIGPUTiming::SetParent(RHIGPUTiming* parent)
	{
		if (m_Parent == parent)
			return;

		if (m_Parent)
			m_Parent->RemoveChild(this);
		
		m_Parent = parent;
		if (m_Parent)
			m_Parent->AddChild(this);
	}

	GPUTiming::GPUTiming(const Ref<CommandBuffer>& cmd, const std::string_view name)
		: m_Cmd(cmd), m_Name(name)
	{
		Start();
	}

	GPUTiming::~GPUTiming()
	{
		End();
	}

	void GPUTiming::Start()
	{
		EG_ASSERT(!m_bStarted);
		EG_ASSERT(m_Cmd.operator bool() && m_Name.data());

		m_bStarted = true;
		const auto& gpuTimings = RenderManager::GetRHITimings();
		auto it = gpuTimings.find(m_Name);
		if (it != gpuTimings.end())
		{
			m_GPUTiming = it->second;
		}
		else
		{
			m_GPUTiming = RHIGPUTiming::Create(m_Name);
			RenderManager::RegisterGPUTiming(m_GPUTiming, m_Name);
		}

		m_QueryIndex = m_GPUTiming->AllocateQuery(RenderManager::GetCurrentFrameIndex());
		if (m_QueryIndex != RHIGPUTiming::InvalidQuery)
			m_Cmd->StartTiming(m_GPUTiming, m_QueryIndex);
#ifdef EG_GPU_MARKERS
		m_Cmd->BeginMarker(m_Name);
#endif

		{
			RHIGPUTiming* parent = nullptr;
			if (!s_TimingsStack.empty())
			{
				parent = s_TimingsStack.back();
			}
			std::scoped_lock lock(g_TimingsMutex);
			m_GPUTiming->SetParent(parent);
		}
		s_TimingsStack.push_back(m_GPUTiming.get());
		RenderManager::RegisterGPUTimingParentless(m_GPUTiming, m_Name);
	}

	void GPUTiming::End()
	{
		EG_ASSERT(m_bStarted);
		EG_ASSERT(m_Cmd.operator bool() && m_Name.data());

		if (m_QueryIndex != RHIGPUTiming::InvalidQuery)
			m_Cmd->EndTiming(m_GPUTiming, m_QueryIndex);
#ifdef EG_GPU_MARKERS
		m_Cmd->EndMarker();
#endif

		s_TimingsStack.pop_back();
	}
}

#endif
