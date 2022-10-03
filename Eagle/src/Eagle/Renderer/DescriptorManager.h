#pragma once

#include "DescriptorSetData.h"

namespace Eagle
{
	class DescriptorSet;
	class Pipeline;

	struct DescriptorWriteData
	{
		const DescriptorSet* DescriptorSet = nullptr;
		DescriptorSetData* DescriptorSetData = nullptr;
	};

	class DescriptorManager
	{
	protected:
		DescriptorManager() = default;

	public:
		virtual ~DescriptorManager() = default;

		virtual Ref<DescriptorSet> AllocateDescriptorSet(const Ref<Pipeline>& pipeline, uint32_t set) = 0;
		static void WriteDescriptors(const Ref<Pipeline>& pipeline, const std::vector<DescriptorWriteData>& writeDatas);

		static Ref<DescriptorManager> Create(uint32_t numDescriptors, uint32_t maxSets);

		static constexpr uint32_t MaxSets = 40960u;
		static constexpr uint32_t MaxNumDescriptors = 81920u;
	};

	class DescriptorSet
	{
	protected:
		DescriptorSet(uint32_t setIndex)
			: m_SetIndex(setIndex) {}

	public:
		virtual ~DescriptorSet() = default;

		uint32_t GetSetIndex() const { return m_SetIndex; }
		virtual void* GetHandle() const = 0;

	protected:
		uint32_t m_SetIndex = 0;
	};
}
