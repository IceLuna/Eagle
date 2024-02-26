#include "egpch.h"
#include "TextureSystem.h"
#include "RenderManager.h"

#include "VidWrappers/Texture.h"

namespace Eagle
{
	std::vector<Ref<Image>> TextureSystem::s_Images;
	std::vector<Ref<Sampler>> TextureSystem::s_Samplers;
	std::unordered_map<GUID, uint32_t> TextureSystem::s_UsedTexturesMap; // uint32_t = index to vector<Ref<Image>>
	std::vector<size_t> TextureSystem::s_FreeIndices; // Free slots inside `s_Images` and `s_Samplers`
	uint64_t TextureSystem::s_LastUpdatedAtFrame = 0;
	static uint32_t s_DummyIndex = 0u;

	static std::mutex s_Mutex;
	
	void TextureSystem::Init()
	{
		s_DummyIndex = AddTexture(Texture2D::DummyTexture);
	}

	void TextureSystem::Reset()
	{
		{
			std::scoped_lock lock(s_Mutex);

			s_Images.clear();
			s_Samplers.clear();
			s_UsedTexturesMap.clear();
			s_FreeIndices.clear();
			s_LastUpdatedAtFrame = 0;
		}
		s_DummyIndex = AddTexture(Texture2D::DummyTexture);
	}

	uint32_t TextureSystem::AddTexture(const Ref<Texture>& texture)
	{
		if (!texture)
			return 0;

		std::scoped_lock lock(s_Mutex);
		if (s_Images.size() >= RendererConfig::MaxTextures)
		{
			EG_CORE_CRITICAL("Not enough samplers to store all textures! Max supported textures: {}", RendererConfig::MaxTextures);
			return 0;
		}

		const auto& textureGUID = texture->GetGUID();
		auto it = s_UsedTexturesMap.find(textureGUID);
		if (it == s_UsedTexturesMap.end())
		{
			uint32_t index = 0;
			if (s_FreeIndices.size())
			{
				index = (uint32_t)s_FreeIndices.back();
				s_Images[index] = texture->GetImage();
				s_Samplers[index] = texture->GetSampler();
				s_FreeIndices.pop_back();
			}
			else
			{
				index = (uint32_t)s_Images.size();
				s_Images.push_back(texture->GetImage());
				s_Samplers.push_back(texture->GetSampler());
			}

			s_UsedTexturesMap[textureGUID] = index;
			s_LastUpdatedAtFrame = RenderManager::GetFrameNumber();
			return index;
		}

		return it->second;
	}

	void TextureSystem::RemoveTexture(const GUID& textureGUID)
	{
		std::scoped_lock lock(s_Mutex);
		auto it = s_UsedTexturesMap.find(textureGUID);
		if (it != s_UsedTexturesMap.end())
		{
			const uint32_t index = it->second;
			if (index == s_DummyIndex)
				return;

			s_Images[index] = s_Images[s_DummyIndex];
			s_Samplers[index] = s_Samplers[s_DummyIndex];
			s_FreeIndices.push_back(index);
			s_UsedTexturesMap.erase(it);
			s_LastUpdatedAtFrame = RenderManager::GetFrameNumber();
		}
	}
	
	uint32_t TextureSystem::GetTextureIndex(const Ref<Texture>& texture)
	{
		std::scoped_lock lock(s_Mutex);
		auto it = s_UsedTexturesMap.find(texture->GetGUID());
		if (it == s_UsedTexturesMap.end())
			return 0;
		return it->second;
	}
	
	void TextureSystem::OnTextureChanged(const Ref<Texture>& texture)
	{
		if (!texture)
			return;

		std::scoped_lock lock(s_Mutex);
		auto it = s_UsedTexturesMap.find(texture->GetGUID());
		if (it != s_UsedTexturesMap.end())
		{
			const uint32_t index = it->second;
			s_Images[index] = texture->GetImage();
			s_Samplers[index] = texture->GetSampler();
			s_LastUpdatedAtFrame = RenderManager::GetFrameNumber();
		}
	}
}
