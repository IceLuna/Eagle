#pragma once

#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class Image;

	class Framebuffer
	{
	protected:
		Framebuffer(const std::vector<Ref<Image>>& images, glm::uvec2 size, const void* renderPassHandle)
			: m_Images(images)
			, m_Size(size)
			, m_RenderPassHandle(renderPassHandle) {}

		Framebuffer(const Ref<Image>& image, const ImageView& imageView, glm::uvec2 size, const void* renderPassHandle)
			: m_Images({ image })
			, m_ImageView(imageView)
			, m_Size(size)
			, m_RenderPassHandle(renderPassHandle) {}

	public:
		virtual ~Framebuffer() = default;
		
		const std::vector<Ref<Image>>& GetImages() const { return m_Images; }
		const void* GetRenderPassHandle() const { return m_RenderPassHandle; }
		virtual void* GetHandle() const = 0;
		glm::uvec2 GetSize() const { return m_Size; }

		static Ref<Framebuffer> Create(const std::vector<Ref<Image>>& images, glm::uvec2 size, const void* renderPassHandle);
		static Ref<Framebuffer> Create(const Ref<Image>& image, const ImageView& imageView, glm::uvec2 size, const void* renderPassHandle);

	protected:
		std::vector<Ref<Image>> m_Images;
		ImageView m_ImageView;
		glm::uvec2 m_Size;
		const void* m_RenderPassHandle = nullptr;
	};
}