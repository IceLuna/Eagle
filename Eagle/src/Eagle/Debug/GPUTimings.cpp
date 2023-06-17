#include "egpch.h"

#ifdef EG_GPU_TIMINGS

#include "GPUTimings.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/RenderManager.h"

#include "Platform/Vulkan/VulkanGPUTimings.h"

namespace Eagle
{
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

	RHIGPUTiming::~RHIGPUTiming()
	{
		if (m_Parent)
			m_Parent->RemoveChild(this);

		for (auto& child : m_Children)
			child->SetParent(nullptr);
	}

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
		m_GPUTiming->bIsUsed = true;

		m_FrameIndex = RenderManager::GetCurrentFrameIndex();
		m_Cmd->StartTiming(m_GPUTiming, m_FrameIndex);
#if EG_GPU_MARKERS
		m_Cmd->BeginMarker(m_Name);
#endif

		if (s_TimingsStack.empty())
		{
			m_GPUTiming->SetParent(nullptr);
		}
		else
		{
			RHIGPUTiming* parent = s_TimingsStack.back();
			m_GPUTiming->SetParent(parent);
		}
		s_TimingsStack.push_back(m_GPUTiming.get());
		RenderManager::RegisterGPUTimingParentless(m_GPUTiming, m_Name);
	}

	void GPUTiming::End()
	{
		EG_ASSERT(m_bStarted);
		EG_ASSERT(m_Cmd.operator bool() && m_Name.data());

		m_Cmd->EndTiming(m_GPUTiming, m_FrameIndex);
#if EG_GPU_MARKERS
		m_Cmd->EndMarker();
#endif

		s_TimingsStack.pop_back();
	}
}

#endif
