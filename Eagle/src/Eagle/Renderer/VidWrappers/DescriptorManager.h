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
		DescriptorManager(uint32_t maxNumDescriptors, uint32_t maxSets)
			: m_MaxNumDescriptors(maxNumDescriptors), m_MaxSets(maxSets) {}

	public:
		virtual ~DescriptorManager() = default;

		virtual Ref<DescriptorSet> CopyDescriptorSet(const Ref<DescriptorSet>& src) = 0;
		virtual Ref<DescriptorSet> AllocateDescriptorSet(const Ref<Pipeline>& pipeline, uint32_t set) = 0;
		static void WriteDescriptors(const Ref<Pipeline>& pipeline, const std::vector<DescriptorWriteData>& writeDatas);

		uint32_t GetMaxSets() const { return m_MaxSets; }
		uint32_t GetMaxNumDescriptors() const { return m_MaxNumDescriptors; }

		static Ref<DescriptorManager> Create(uint32_t numDescriptors, uint32_t maxSets);

		static constexpr uint32_t MaxSets = 40960u;
		static constexpr uint32_t MaxNumDescriptors = 81920u;

	private:
		uint32_t m_MaxNumDescriptors = 0;
		uint32_t m_MaxSets = 0;
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
