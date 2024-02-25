#pragma once

#include "Eagle/Core/GUID.h"

namespace Eagle
{
	class Image;
	class Sampler;
	class Texture;

	class TextureSystem
	{
	public:

		static void Init();
		static void Reset();

		// Tries to add texture to the system. Returns its index in vector<Ref<Image>> Images
		static uint32_t AddTexture(const Ref<Texture>& texture);
		static void RemoveTexture(const GUID& textureGUID);

		static uint32_t GetTextureIndex(const Ref<Texture>& texture);
		static uint64_t GetUpdatedFrameNumber() { return s_LastUpdatedAtFrame; }
		static const std::vector<Ref<Image>>& GetImages() { return s_Images; }
		static const std::vector<Ref<Sampler>>& GetSamplers() { return s_Samplers; }

	private:
		static void OnTextureChanged(const Ref<Texture>& texture);

	private:
		static std::vector<Ref<Image>> s_Images;
		static std::vector<Ref<Sampler>> s_Samplers;
		static std::unordered_map<GUID, uint32_t> s_UsedTexturesMap; // uint32_t = index to vector<Ref<Image>>
		static std::vector<size_t> s_FreeIndices;
		static uint64_t s_LastUpdatedAtFrame;

		friend class VulkanTexture2D;
	};
}
