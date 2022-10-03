#include "egpch.h"
#include "StagingManager.h"

namespace Eagle
{
	//------------------
	// Staging Manager
	//------------------

	static std::vector<Ref<StagingBuffer>> s_StagingBuffers;

	Ref<StagingBuffer>& StagingManager::AcquireBuffer(size_t size, bool bIsCPURead)
	{
		Ref<StagingBuffer>* stagingBuffer = nullptr;

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
			// Since StagingBuffer's constructor is protected, Ref can't call it's constructor. So we make this class to get around it
			class PublicStagingBuffer : public StagingBuffer { public: PublicStagingBuffer(size_t size, bool bIsCPURead) : StagingBuffer(size, bIsCPURead) {} };
			s_StagingBuffers.push_back(MakeRef<PublicStagingBuffer>(size, bIsCPURead));
			stagingBuffer = &s_StagingBuffers.back();
		}
		(*stagingBuffer)->SetFence(nullptr);
		(*stagingBuffer)->SetState(StagingBufferState::Pending);

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
