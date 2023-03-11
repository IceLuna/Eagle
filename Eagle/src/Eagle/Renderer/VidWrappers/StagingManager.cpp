#include "egpch.h"
#include "StagingManager.h"
#include "Eagle/Renderer/RenderManager.h"

namespace Eagle
{
	//------------------
	// Staging Manager
	//------------------

	static std::vector<Ref<StagingBuffer>> s_StagingBuffers;

	Ref<StagingBuffer>& StagingManager::AcquireBuffer(size_t size, bool bIsCPURead)
	{
		Ref<StagingBuffer>* stagingBuffer = nullptr;
		size = glm::max(size, 1024ull); // making sure to not allocate too small buffers

		for (auto& staging : s_StagingBuffers)
		{
			if (staging->IsCPURead() == bIsCPURead && size <= staging->GetSize())
			{
				if (staging->GetState() == StagingBufferState::Free)
				{
					stagingBuffer = &staging;
					break;
				}
				else if (staging->GetState() == StagingBufferState::InFlight)
				{
					const auto& fence = staging->GetFence();
					EG_CORE_ASSERT(fence);
					if (fence->IsSignaled())
					{
						stagingBuffer = &staging;
						break;
					}
				}
			}
		}

		if (!stagingBuffer)
		{
			// Since StagingBuffer's constructor is protected, Ref can't call its constructor. So we make this class to get around it
			class PublicStagingBuffer : public StagingBuffer { public: PublicStagingBuffer(size_t size, bool bIsCPURead) : StagingBuffer(size, bIsCPURead) {} };
			s_StagingBuffers.push_back(MakeRef<PublicStagingBuffer>(size, bIsCPURead));
			stagingBuffer = &s_StagingBuffers.back();
		}
		(*stagingBuffer)->SetFence(nullptr);
		(*stagingBuffer)->SetState(StagingBufferState::Pending);
		(*stagingBuffer)->m_FrameNumberUsed = RenderManager::GetFrameNumber();

		return *stagingBuffer;
	}

	void StagingManager::ReleaseBuffers()
	{
		auto it = s_StagingBuffers.begin();
		while (it != s_StagingBuffers.end())
		{
			StagingBufferState state = (*it)->GetState();
			if (state == StagingBufferState::Free)
				it = s_StagingBuffers.erase(it);
			else if (state == StagingBufferState::InFlight && (*it)->GetFence()->IsSignaled())
				it = s_StagingBuffers.erase(it);
			else
				++it;
		}
	}

	void StagingManager::NextFrame()
	{
		const uint64_t currentFrameNumber = RenderManager::GetFrameNumber();
		auto it = s_StagingBuffers.begin();
		while (it != s_StagingBuffers.end())
		{
			StagingBufferState state = (*it)->GetState();
			if (state == StagingBufferState::InFlight && (*it)->GetFence()->IsSignaled())
			{
				state = StagingBufferState::Free;
				(*it)->SetState(state);
			}
			if (state == StagingBufferState::Free && (currentFrameNumber - (*it)->m_FrameNumberUsed) > s_ReleaseAfterNFrames)
				it = s_StagingBuffers.erase(it);
			else
				++it;
		}
	}

	//------------------
	// Staging Buffer
	//------------------
	StagingBuffer::StagingBuffer(size_t size, bool bIsCPURead)
		: m_bIsCPURead(bIsCPURead)
	{
		BufferSpecifications specs;
		specs.Size = size;
		specs.MemoryType = bIsCPURead ? MemoryType::GpuToCpu : MemoryType::CpuToGpu;
		specs.Usage = bIsCPURead ? BufferUsage::TransferDst : BufferUsage::TransferSrc;

		m_Buffer = Buffer::Create(specs);
	}
}
