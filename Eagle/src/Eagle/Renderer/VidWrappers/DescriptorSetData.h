#pragma once

#include "Image.h"
#include "Sampler.h"
#include "Buffer.h"

#include <ankerl/unordered_dense.h>

namespace Eagle
{
	class Texture2D;

	class DescriptorSetData
	{
	public:
		DescriptorSetData() = default;

		// Additional Structs
		struct ImageBinding
		{
			void* ImageHandle = nullptr;
			void* ImageViewHandle = nullptr;
			void* SamplerHandle = nullptr;
			bool bDepth = false;

			// Revision is changed on each resource recreation to update descriptors when a resource is recreated.
			// It should work without it because internal handles are updated, but for some reason RenderDoc is not happy without it.
			// And it thinks textures are not bound to the pipeline. So, this exists purely to make RenderDoc happy and correctly display pass inputs
			// RenderDoc version: 1.42
			// Additional thought: maybe it's not RenderDoc issues. Maybe when a resource is recreated (old handle is released, a new one is created immediately),
			// VK returns the same handle ID for the new resource? And because of that descriptor cache invalidation system doesn't detect changes and tries to reuse invalid resource view or smth?
			uint8_t Revision = 0;

			ImageBinding() = default;
			ImageBinding(const Ref<Eagle::Image>& image) : ImageHandle(image->GetHandle()), ImageViewHandle(image->GetImageViewHandle()), Revision(image->GetRevision()) {}
			ImageBinding(const Ref<Eagle::Image>& image, const ImageView& view) : ImageHandle(image->GetHandle()), ImageViewHandle(image->GetImageViewHandle(view)), Revision(image->GetRevision()) {}
			ImageBinding(const Ref<Eagle::Image>& image, const ImageView& view, const Ref<Eagle::Sampler>& sampler)
				: ImageHandle(image->GetHandle())
				, ImageViewHandle(image->GetImageViewHandle(view))
				, SamplerHandle(sampler ? sampler->GetHandle() : nullptr)
				, Revision(image->GetRevision()){}
			ImageBinding(const Ref<Eagle::Image>& image, const Ref<Eagle::Sampler>& sampler)
				: ImageHandle(image->GetHandle())
				, ImageViewHandle(image->GetImageViewHandle())
				, SamplerHandle(sampler ? sampler->GetHandle() : nullptr)
				, Revision(image->GetRevision()) {}

			ImageBinding(const Ref<Sampler>& sampler)
				: SamplerHandle(sampler->GetHandle()) {}

			bool operator!=(const ImageBinding& other) const
			{
				return ImageHandle != other.ImageHandle || ImageViewHandle != other.ImageViewHandle || SamplerHandle != other.SamplerHandle || bDepth != other.bDepth || Revision != other.Revision;
			}

			friend bool operator!=(const std::vector<ImageBinding>& left, const std::vector<ImageBinding>& right)
			{
				if (left.size() != right.size())
					return true;

				for (std::uint32_t i = 0; i < left.size(); i++)
				{
					if (left[i] != right[i])
						return true;
				}
				return false;
			}
		};

		struct BufferBinding
		{
			void* BufferHandle = nullptr;
			void* BufferViewHandle = nullptr;
			size_t Offset = 0;
			size_t Range = size_t(-1);

			// Revision is changed on each resource recreation to update descriptors when a resource is recreated.
			// It should work without it because internal handles are updated, but for some reason RenderDoc is not happy without it.
			// And it thinks textures are not bound to the pipeline. So, this exists purely to make RenderDoc happy and correctly display pass inputs
			// RenderDoc version: 1.42
			// Additional thought: maybe it's not RenderDoc issues. Maybe when a resource is recreated (old handle is released, a new one is created immediately),
			// VK returns the same handle ID for the new resource? And because of that descriptor cache invalidation system doesn't detect changes and tries to reuse invalid resource view or smth?
			uint8_t Revision = 0;

			BufferBinding() = default;
			BufferBinding(const Ref<Eagle::Buffer>& buffer)
				: BufferHandle(buffer->GetHandle()), BufferViewHandle(buffer->GetViewHandle()), Revision(buffer->GetRevision()) {}
			BufferBinding(const Ref<Eagle::Buffer>& buffer, size_t offset, size_t range)
				: BufferHandle(buffer->GetHandle()), BufferViewHandle(buffer->GetViewHandle()), Offset(offset), Range(range), Revision(buffer->GetRevision()) {}

			bool operator != (const BufferBinding& other) const
			{
				return BufferHandle != other.BufferHandle || Offset != other.Offset || Range != other.Range || Revision != other.Revision;
			}

			friend bool operator!=(const std::vector<BufferBinding>& left, const std::vector<BufferBinding>& right)
			{
				if (left.size() != right.size())
					return true;

				for (std::uint32_t i = 0; i < left.size(); i++)
				{
					if (left[i] != right[i])
						return true;
				}
				return false;
			}
		};

		struct Binding
		{
			std::vector<ImageBinding> ImageBindings = { {} };
			std::vector<BufferBinding> BufferBindings = { {} };
		};

	public:
		const ankerl::unordered_dense::map<uint32_t, Binding>& GetBindings() const { return m_Bindings; }
		ankerl::unordered_dense::map<uint32_t, Binding>& GetBindings() { return m_Bindings; }
		bool IsDirty() const { return m_bDirty; }
		void MakeDirty() { m_bDirty = true; }
		void OnFlushed() { m_bDirty = false; }

		void SetArg(uint32_t idx, const Ref<Buffer>& buffer);
		void SetArg(uint32_t idx, const Ref<Buffer>& buffer, std::size_t offset, std::size_t size);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Buffer>>& buffers);

		void SetArg(uint32_t idx, const Ref<Image>& image);
		void SetArg(uint32_t idx, const Ref<Image>& image, const ImageView& imageView);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Image>>& images);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Image>>& images, const std::span<const ImageView>& imageViews);
		void SetArgArray(uint32_t idx, const Ref<Image>& image, const std::span<const ImageView>& imageViews);

		void SetArg(uint32_t idx, const Ref<Sampler>& sampler);

		void SetArg(uint32_t idx, const Ref<Image>& image, const Ref<Sampler>& sampler, bool bDepth = false);
		void SetArg(uint32_t idx, const Ref<Image>& image, const ImageView& imageView, const Ref<Sampler>& sampler);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Image>>& images, const Ref<Sampler>& sampler);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Image>>& images, const std::span<const Ref<Sampler>>& samplers);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Image>>& images, const std::span<const ImageView>& imageViews, const std::span<const Ref<Sampler>>& samplers);
		void SetArgArray(uint32_t idx, const std::span<const Ref<Texture2D>>& textures);

	private:
		ankerl::unordered_dense::map<uint32_t, Binding> m_Bindings; // Binding -> Data
		bool m_bDirty = true;
	};
}
