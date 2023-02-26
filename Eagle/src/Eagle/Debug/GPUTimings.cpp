#include "egpch.h"

#ifdef EG_GPU_TIMINGS

#include "GPUTimings.h"
#include "Eagle/Renderer/RenderCommandManager.h"
#include "Eagle/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanGPUTimings.h"

namespace Eagle
{
	Ref<RHIGPUTiming> RHIGPUTiming::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanGPUTiming>();
		}
		EG_ASSERT(false);
		return Ref<RHIGPUTiming>();
	}

	GPUTiming::GPUTiming(Ref<CommandBuffer>& cmd, const std::string_view name, bool bScoped)
		: m_Cmd(cmd), m_Name(name), m_bScoped(bScoped)
	{
		Start();
	}

	GPUTiming::~GPUTiming()
	{
		if (m_bScoped)
			End();
	}

	void GPUTiming::Start()
	{
		EG_ASSERT(!m_bStarted);
		EG_ASSERT(m_Cmd.operator bool() && m_Name.data());

		m_bStarted = true;
		auto& gpuTimings = Renderer::GetRHITimings();
		auto it = gpuTimings.find(m_Name);
		if (it != gpuTimings.end())
		{
			m_GPUTiming = it->second;
		}
		else
		{
			m_GPUTiming = RHIGPUTiming::Create();
			Renderer::RegisterGPUTiming(m_GPUTiming, m_Name);
		}
		m_GPUTiming->bIsUsed = true;

		m_FrameIndex = Renderer::GetCurrentFrameIndex();
		m_Cmd->StartTiming(m_GPUTiming, m_FrameIndex);
#if EG_GPU_MARKERS
		m_Cmd->BeginMarker(m_Name);
#endif
	}

	void GPUTiming::End()
	{
		EG_ASSERT(m_bStarted);
		EG_ASSERT(m_Cmd.operator bool() && m_Name.data());

		m_Cmd->EndTiming(m_GPUTiming, m_FrameIndex);
#if EG_GPU_MARKERS
		m_Cmd->EndMarker();
#endif
	}
}

#endif