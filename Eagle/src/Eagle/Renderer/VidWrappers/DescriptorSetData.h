#pragma once

#include "Image.h"
#include "Sampler.h"
#include "Buffer.h"

namespace Eagle
{
	class Texture2D;

	class DescriptorSetData
	{
		// Additional Structs
	public:
		struct ImageBinding
		{
			void* ImageHandle = nullptr;
			void* ImageViewHandle = nullptr;
			void* SamplerHandle = nullptr;

			ImageBinding() = default;
			ImageBinding(const Ref<Eagle::Image>& image) : ImageHandle(image->GetHandle()), ImageViewHandle(image->GetImageViewHandle()) {}
			ImageBinding(const Ref<Eagle::Image>& image, const ImageView& view) : ImageHandle(image->GetHandle()), ImageViewHandle(image->GetImageViewHandle(view)) {}
			ImageBinding(const Ref<Eagle::Image>& image, const ImageView& view, const Ref<Eagle::Sampler>& sampler)
				: ImageHandle(image->GetHandle())
				, ImageViewHandle(image->GetImageViewHandle(view))
				, SamplerHandle(sampler ? sampler->GetHandle() : nullptr) {}
			ImageBinding(const Ref<Eagle::Image>& image, const Ref<Eagle::Sampler>& sampler)
				: ImageHandle(image->GetHandle())
				, ImageViewHandle(image->GetImageViewHandle())
				, SamplerHandle(sampler ? sampler->GetHandle() : nullptr) {}

			bool operator!=(const ImageBinding& other) const
			{
				return ImageHandle != other.ImageHandle || ImageViewHandle != other.ImageViewHandle || SamplerHandle != other.SamplerHandle;
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

			BufferBinding() = default;
			BufferBinding(const Ref<Eagle::Buffer>& buffer)
				: BufferHandle(buffer->GetHandle()), BufferViewHandle(buffer->GetViewHandle()) {}
			BufferBinding(const Ref<Eagle::Buffer>& buffer, size_t offset, size_t range)
				: BufferHandle(buffer->GetHandle()), BufferViewHandle(buffer->GetViewHandle()), Offset(offset), Range(range) {}

			bool operator != (const BufferBinding& other) const
			{
				return BufferHandle != other.BufferHandle || Offset != other.Offset || Range != other.Range;
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
		const std::unordered_map<uint32_t, Binding>& GetBindings() const { return m_Bindings; }
		std::unordered_map<uint32_t, Binding>& GetBindings() { return m_Bindings; }
		bool IsDirty() const { return m_bDirty; }
		void MakeDirty() { m_bDirty = true; }
		void OnFlushed() { m_bDirty = false; }

		void SetArg(uint32_t idx, const Ref<Buffer>& buffer);
		void SetArg(uint32_t idx, const Ref<Buffer>& buffer, std::size_t offset, std::size_t size);
		void SetArgArray(uint32_t idx, const std::vector<Ref<Buffer>>& buffers);

		void SetArg(uint32_t idx, const Ref<Image>& image);
		void SetArg(uint32_t idx, const Ref<Image>& image, const ImageView& imageView);
		void SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images);
		void SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews);
		void SetArgArray(uint32_t idx, const Ref<Image>& image, const std::vector<ImageView>& imageViews);

		void SetArg(uint32_t idx, const Ref<Image>& image, const Ref<Sampler>& sampler);
		void SetArg(uint32_t idx, const Ref<Image>& image, const ImageView& imageView, const Ref<Sampler>& sampler);
		void SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images, const std::vector<Ref<Sampler>>& samplers);
		void SetArgArray(uint32_t idx, const std::vector<Ref<Image>>& images, const std::vector<ImageView>& imageViews, const std::vector<Ref<Sampler>>& samplers);
		void SetArgArray(uint32_t idx, const std::vector<Ref<Texture2D>>& textures);

	private:
		std::unordered_map<uint32_t, Binding> m_Bindings; // Binding -> Data
		bool m_bDirty = true;
	};
}
