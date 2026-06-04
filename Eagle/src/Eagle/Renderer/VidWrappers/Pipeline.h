#pragma once

#include "DescriptorSetData.h"
#include "DescriptorManager.h"
#include "Eagle/Renderer/RenderManager.h"

namespace Eagle
{
	class Texture2D;

	class Pipeline : virtual public std::enable_shared_from_this<Pipeline>
	{
	public:
		virtual void Recreate() = 0;

		void SetBuffer(const Ref<Buffer>& buffer, uint32_t set, uint32_t binding);
		void SetBuffer(const Ref<Buffer>& buffer, size_t offset, size_t size, uint32_t set, uint32_t binding);
		void SetBufferArray(const std::span<const Ref<Buffer>>& buffers, uint32_t set, uint32_t binding);

		void SetImage(const Ref<Image>& image, uint32_t set, uint32_t binding);
		void SetImage(const Ref<Image>& image, const ImageView& imageView, uint32_t set, uint32_t binding);
		void SetImageArray(const std::span<const Ref<Image>>& images, uint32_t set, uint32_t binding);
		void SetImageArray(const std::span<const Ref<Image>>& images, const std::span<const ImageView>& imageViews, uint32_t set, uint32_t binding);
		void SetImageArray(const Ref<Image>& image, const std::span<const ImageView>& imageViews, uint32_t set, uint32_t binding);

		void SetSampler(const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);

		void SetImageSampler(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);
		void SetImageSampler(const Ref<Image>& image, const ImageView& imageView, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);
		void SetImageSamplerDepth(const Ref<Image>& image, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding); // Used for ImageReadAccess::DepthStencilRead if it's used as a read-only attachment & binding
		void SetImageSamplerArray(const std::span<const Ref<Image>>& images, const std::span<const Ref<Sampler>>& samplers, uint32_t set, uint32_t binding);
		void SetImageSamplerArray(const std::span<const Ref<Image>>& images, const Ref<Sampler>& sampler, uint32_t set, uint32_t binding);
		void SetImageSamplerArray(const std::span<const Ref<Image>>& images, const std::span<const ImageView>& imageViews, const std::span<const Ref<Sampler>>& samplers, uint32_t set, uint32_t binding);

		void SetTexture(const Ref<Texture2D>& texture, uint32_t set, uint32_t binding);
		void SetTexture(const Ref<Texture2D>& texture, const ImageView& imageView, uint32_t set, uint32_t binding);
		void SetTextureArray(const std::span<const Ref<Texture2D>>& textures, uint32_t set, uint32_t binding);

		virtual void* GetPipelineHandle() const = 0;
		virtual void* GetPipelineLayoutHandle() const = 0;

		const std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() const { return m_DescriptorSetData[RenderManager::GetCurrentFrameIndex()]; }
		std::unordered_map<uint32_t, DescriptorSetData>& GetDescriptorSetsData() { return m_DescriptorSetData[RenderManager::GetCurrentFrameIndex()]; }
		const std::unordered_map<uint32_t, Ref<DescriptorSet>>& GetDescriptorSets() const { return m_DescriptorSets[RenderManager::GetCurrentFrameIndex()]; }

		void ResetDescriptors();

		Ref<DescriptorSet>& AllocateDescriptorSet(uint32_t set);

	protected:
		std::array<std::unordered_map<uint32_t, DescriptorSetData>, RendererConfig::FramesInFlight> m_DescriptorSetData; // Set -> Data
		std::array<std::unordered_map<uint32_t, Ref<DescriptorSet>>, RendererConfig::FramesInFlight> m_DescriptorSets; // Set -> DescriptorSet
	};
}
