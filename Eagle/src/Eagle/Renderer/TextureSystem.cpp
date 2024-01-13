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
	
	void TextureSystem::Init()
	{
		AddTexture(Texture2D::DummyTexture);
	}

	void TextureSystem::Reset()
	{
		s_Images.clear();
		s_Samplers.clear();
		s_UsedTexturesMap.clear();
		s_LastUpdatedAtFrame = 0;
		AddTexture(Texture2D::DummyTexture);
	}

	uint32_t TextureSystem::AddTexture(const Ref<Texture>& texture)
	{
		if (!texture)
			return 0;

		if (s_Images.size() >= RendererConfig::MaxTextures)
		{
			EG_CORE_CRITICAL("Not enough samplers to store all textures! Max supported textures: {}", RendererConfig::MaxTextures);
			return 0;
		}

		auto it = s_UsedTexturesMap.find(texture);
		if (it == s_UsedTexturesMap.end())
		{
			const uint32_t index = (uint32_t)s_Images.size();
			s_Images.push_back(texture->GetImage());
			s_Samplers.push_back(texture->GetSampler());

			s_UsedTexturesMap[texture] = index;
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
