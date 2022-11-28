#include "egpch.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Texture.h"

namespace Eagle
{
	void Pipeline::SetBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, buffer);
	}

	void Pipeline::SetBuffer(const Ref<Buffer>& buffer, size_t offset, size_t size, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, buffer, offset, size);
	}

	void Pipeline::SetBufferArray(const std::vector<Ref<Buffer>>& buffers, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArgArray(binding, buffers);
	}

	void Pipeline::SetImage(const Ref<Image>& image, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, image);
	}

	void Pipeline::SetImage(const Ref<Image>& image, const ImageView& imageView, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, image, imageView);
	}

	void Pipeline::SetImageArray(const std::vector<Ref<Image>>& images, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArgArray(binding, images);
	}

	void Pipeline::SetImageArray(const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArgArray(binding, images, imageViews);
	}

	void Pipeline::SetImageSampler(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, image, sampler);
	}

	void Pipeline::SetTexture(const Ref<Texture2D>& texture, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, texture->GetImage(), texture->GetSampler());
	}

	void Pipeline::SetTexture(const Ref<Texture2D>& texture, const ImageView& imageView, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, texture->GetImage(), imageView, texture->GetSampler());
	}

	void Pipeline::SetImageSampler(const Ref<Image>& image, const ImageView& imageView, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArg(binding, image, imageView, sampler);
	}

	void Pipeline::SetImageSamplerArray(const std::vector<Ref<Image>>& images, const std::vector<Ref<Sampler>>& samplers, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArgArray(binding, images, samplers);
	}

	void Pipeline::SetImageSamplerArray(const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews, const std::vector<Ref<Sampler>>& samplers, uint32_t set, uint32_t binding)
	{
		m_DescriptorSetData[set].SetArgArray(binding, images, imageViews, samplers);
	}

	Ref<DescriptorSet>& Pipeline::AllocateDescriptorSet(uint32_t set)
	{
		assert(m_DescriptorSets.find(set) == m_DescriptorSets.end()); // Should not be present
		Ref<DescriptorSet>& nonInitializedSet = m_DescriptorSets[set];
		nonInitializedSet = Renderer::GetDescriptorSetManager()->AllocateDescriptorSet(shared_from_this(), set);
		return nonInitializedSet;
	}
}
