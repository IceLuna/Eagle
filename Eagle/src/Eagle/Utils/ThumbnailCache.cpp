#include "egpch.h"
#include "ThumbnailCache.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetManager.h"

namespace Eagle
{
	static GUID s_AssetModifiedCallbackID;
	static Scope<AssetThumbnailRenderer> s_AssetThumbnailRenderer;
	static std::unordered_map<Ref<Asset>, Ref<Image>> s_ThumbnailCache;
	static bool s_RenderingThumbnail = false;

	// Contains the frame number it was rendered on. We need to wait `FramesInFlight` frames for it to be ready
	static std::unordered_map<Ref<Asset>, std::pair<Ref<Image>, uint64_t>> s_PendingThumbnails;

	void ThumbnailCache::Init()
	{
		s_RenderingThumbnail = false;
		s_AssetThumbnailRenderer = MakeScope<AssetThumbnailRenderer>(GetThumbnailSize());

		AssetManager::AddOnAssetModifiedCallback(s_AssetModifiedCallbackID, [](const Ref<Asset>& asset)
		{
			auto it = s_ThumbnailCache.find(asset);
			if (it != s_ThumbnailCache.end())
			{
				s_ThumbnailCache.erase(it);
			}
		});
	}

	void ThumbnailCache::Release()
	{
		AssetManager::RemoveOnAssetModifiedCallback(s_AssetModifiedCallbackID);
		s_AssetThumbnailRenderer.reset();
		s_ThumbnailCache.clear();
		s_PendingThumbnails.clear();
		s_RenderingThumbnail = false;
	}

	void ThumbnailCache::NextFrame()
	{
		s_RenderingThumbnail = false;

		for (auto it = s_PendingThumbnails.begin(); it != s_PendingThumbnails.end(); )
		{
			const auto& pair = it->second;
			const auto& frameNumber = pair.second;
			if ((RenderManager::GetFrameNumber_CPU() - frameNumber) > RendererConfig::FramesInFlight)
			{
				const auto& asset = it->first;
				const auto& image = pair.first;
				s_ThumbnailCache.emplace(asset, image);
				it = s_PendingThumbnails.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	bool ThumbnailCache::Render(const Ref<Asset>& asset)
	{
		if (!asset)
			return false;

		if (s_RenderingThumbnail) // One thumbnail per frame
			return false;

		switch (asset->GetAssetType())
		{
		case Eagle::AssetType::StaticMesh:
			s_AssetThumbnailRenderer->Render(Cast<AssetStaticMesh>(asset));
			break;
		case Eagle::AssetType::SkeletalMesh:
			s_AssetThumbnailRenderer->Render(Cast<AssetSkeletalMesh>(asset));
			break;
		case Eagle::AssetType::Material:
			s_AssetThumbnailRenderer->Render(Cast<AssetMaterial>(asset));
			break;
		case Eagle::AssetType::Entity:
			s_AssetThumbnailRenderer->Render(Cast<AssetEntity>(asset));
			break;
		case Eagle::AssetType::Animation:
			s_AssetThumbnailRenderer->Render(Cast<AssetAnimation>(asset));
			break;
		case Eagle::AssetType::ParticleSystem:
			s_AssetThumbnailRenderer->Render(Cast<AssetParticleSystem>(asset));
			break;
		default:
			EG_CORE_ASSERT(!"Unsupported asset type");
			return false;
		}

		s_RenderingThumbnail = true;
		s_PendingThumbnails.emplace(asset, std::pair{ s_AssetThumbnailRenderer->GetImage(), RenderManager::GetFrameNumber_CPU() });

		return true;
	}

	Ref<Image> ThumbnailCache::Get(const Ref<Asset>& asset)
	{
		if (!asset)
			return nullptr;

		auto it = s_ThumbnailCache.find(asset);
		return it != s_ThumbnailCache.end() ? it->second : nullptr;
	}
}
