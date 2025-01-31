#include "egpch.h"
#include "ThumbnailCache.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetManager.h"

namespace Eagle
{
	GUID ThumbnailCache::s_AssetModifiedCallbackID;
	Scope<AssetThumbnailRenderer> ThumbnailCache::s_AssetThumbnailRenderer;
	std::unordered_map<Ref<Asset>, Ref<Image>> ThumbnailCache::s_ThumbnailCache;
	bool ThumbnailCache::s_RenderingThumbnail = false;

	void ThumbnailCache::Init()
	{
		s_RenderingThumbnail = false;
		s_AssetThumbnailRenderer = MakeScope<AssetThumbnailRenderer>();

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
		s_RenderingThumbnail = false;
	}

	void ThumbnailCache::NextFrame()
	{
		s_RenderingThumbnail = false;
	}

	bool ThumbnailCache::Render(const Ref<Asset>& asset, glm::uvec2 size)
	{
		if (!asset)
			return false;

		if (s_RenderingThumbnail) // One thumbnail per frame
			return false;

		switch (asset->GetAssetType())
		{
		case Eagle::AssetType::StaticMesh:
			s_AssetThumbnailRenderer->Render(Cast<AssetStaticMesh>(asset), size);
			break;
		case Eagle::AssetType::SkeletalMesh:
			s_AssetThumbnailRenderer->Render(Cast<AssetSkeletalMesh>(asset), size);
			break;
		case Eagle::AssetType::Material:
			s_AssetThumbnailRenderer->Render(Cast<AssetMaterial>(asset), size);
			break;
		case Eagle::AssetType::Entity:
			s_AssetThumbnailRenderer->Render(Cast<AssetEntity>(asset), size);
			break;
		case Eagle::AssetType::Animation:
			s_AssetThumbnailRenderer->Render(Cast<AssetAnimation>(asset), size);
			break;
		case Eagle::AssetType::ParticleSystem:
			s_AssetThumbnailRenderer->Render(Cast<AssetParticleSystem>(asset), size);
			break;
		default:
			EG_CORE_ASSERT(!"Unknown asset type");
			return false;
		}

		s_RenderingThumbnail = true;
		s_ThumbnailCache.emplace(asset, s_AssetThumbnailRenderer->GetImage());

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
