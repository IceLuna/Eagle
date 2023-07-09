#include "egpch.h"
#include "TextureSystem.h"
#include "RenderManager.h"

#include "VidWrappers/Texture.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

namespace Eagle
{
	std::vector<Ref<Image>> TextureSystem::s_Images;
	std::vector<Ref<Sampler>> TextureSystem::s_Samplers;
	std::unordered_map<Ref<Texture>, uint32_t> TextureSystem::s_UsedTexturesMap; // size_t = index to vector<Ref<Image>>
	uint64_t TextureSystem::s_LastUpdatedAtFrame = 0;
	
	static uint32_t s_CurrentTextureIndex = 1; // 0 - DummyTexture

	void TextureSystem::Init()
	{
		// Init textures & Fill with dummy textures
		s_Images.resize(EG_MAX_TEXTURES);
		s_Samplers.resize(EG_MAX_TEXTURES);
		s_UsedTexturesMap.reserve(EG_MAX_TEXTURES);
		std::fill(s_Images.begin(),   s_Images.end(), Texture2D::DummyTexture->GetImage());
		std::fill(s_Samplers.begin(), s_Samplers.end(), Texture2D::DummyTexture->GetSampler());
	}

	void TextureSystem::Shutdown()
	{
		s_Images.clear();
		s_Samplers.clear();
		s_UsedTexturesMap.clear();
		s_CurrentTextureIndex = 1; // 0 - DummyTexture
		s_LastUpdatedAtFrame = 0;
	}

	uint32_t TextureSystem::AddTexture(const Ref<Texture>& texture)
	{
		if (!texture)
			return 0;

		auto it = s_UsedTexturesMap.find(texture);
		if (it == s_UsedTexturesMap.end())
		{
			const uint32_t index = s_CurrentTextureIndex;
			s_Images[index] = texture->GetImage();
			s_Samplers[index] = texture->GetSampler();

			s_UsedTexturesMap[texture] = index;
			s_CurrentTextureIndex++;
			s_LastUpdatedAtFrame = RenderManager::GetFrameNumber();
			return index;
		}

		return it->second;
	}
	
	uint32_t TextureSystem::GetTextureIndex(const Ref<Texture>& texture)
	{
		auto it = s_UsedTexturesMap.find(texture);
		if (it == s_UsedTexturesMap.end())
			return 0;
		return it->second;
	}
	
	void TextureSystem::OnTextureChanged(const Ref<Texture>& texture)
	{
		if (!texture)
			return;

		auto it = s_UsedTexturesMap.find(texture);
		if (it != s_UsedTexturesMap.end())
		{
			const uint32_t index = it->second;
			s_Images[index] = texture->GetImage();
			s_Samplers[index] = texture->GetSampler();
			s_LastUpdatedAtFrame = RenderManager::GetFrameNumber();
		}
	}
}
