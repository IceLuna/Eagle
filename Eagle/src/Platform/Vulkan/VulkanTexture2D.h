#pragma once

#include "Eagle/Renderer/Texture.h"
#include "VulkanImage.h"

namespace Eagle
{
	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(const Path&filepath, const Texture2DSpecifications& specs);
		VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data = nullptr, const Texture2DSpecifications& specs = {});
		~VulkanTexture2D() { m_ImageData.Release(); }

		void SetSRGB(bool bSRGB) override;
		bool IsLoaded() const override { return m_bIsLoaded; }

	private:
		void CreateImageFromData();

	private:
		bool Load(const Path& path);

	private:
		DataBuffer m_ImageData;
		bool m_bIsLoaded = true;
	};
}
