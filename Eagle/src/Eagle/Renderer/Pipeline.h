#pragma once

#include "DescriptorSetData.h"
#include "DescriptorManager.h"

namespace Eagle
{
	class Texture2D;

	class Pipeline : virtual public std::enable_shared_from_this<Pipeline>
	{
	public:
		virtual ~Pipeline()
		{
			m_DescriptorSetData.clear();
			m_DescriptorSets.clear();
		}

		virtual void Recreate() = 0;

		void SetBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding);
		void SetBuffer(const Ref<Buffer>& buffer, size_t offset, size_t size, uint32_t set, uint32_t binding);
		void SetBufferArray(const std::vector<Ref<Buffer>>& buffers, uint32_t set, uint32_t binding);

		void SetImage(const Ref<Image>& image, uint32_t set, uint32_t binding);
		void SetImage(const Ref<Image>& image, const ImageView& imageView, uint32_t set, uint32_t binding);
		void SetImageArray(const std::vector<Ref<Image>>& images, uint32_t set, uint32_t binding);
		void SetImageArray(const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews, uint32_t set, uint32_t binding);

		void SetImageSampler(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);
		void SetImageSampler(const Ref<Texture2D>& texture, uint32_t set, uint32_t binding);
		void SetImageSampler(const Ref<Texture2D>& texture, const ImageView& imageView, uint32_t set, uint32_t binding);
		void SetImageSampler(const Ref<Image>& image, const ImageView& imageView, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);
		void SetImageSamplerArray(const std::vector<Ref<Image>>& images, const std::vector<Ref<Sampler>>& samplers, uint32_t set, uint32_t binding);
		void SetImageSamplerArray(const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews, const std::vector<Ref<Sampler>>& samplers, uint32_t set, uint32_t binding);

		virtual void* GetPipelineHandle() const = 0;
		virtual void* GetPipelineLayoutHandle() const = 0;

		const std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() const { return m_DescriptorSetData; }
		std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() { return m_DescriptorSetData; }
		const std::unordered_map<uint32_t, Ref<DescriptorSet>>& GetDescriptorSets() const { return m_DescriptorSets; }

		Ref<DescriptorSet>& AllocateDescriptorSet(uint32_t set);

	protected:
		std::unordered_map<uint32_t, DescriptorSetData> m_DescriptorSetData; // Set -> Data
		std::unordered_map<uint32_t, Ref<DescriptorSet>> m_DescriptorSets; // Set -> DescriptorSet
	};
}
