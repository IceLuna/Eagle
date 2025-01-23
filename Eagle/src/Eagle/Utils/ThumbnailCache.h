#pragma once

#include "Eagle/Asset/Asset.h"
#include "AssetThumbnailRenderer.h"

namespace Eagle
{
	class Image;

	class ThumbnailCache
	{
	public:
		static void Init();
		static void Release();
		static void NextFrame();

		static bool Render(const Ref<Asset>& asset, AssetType type, glm::uvec2 size);
		static Ref<Image> Get(const Ref<Asset>& asset);

		static constexpr bool IsRenderableAssetType(AssetType type) { return AssetThumbnailRenderer::IsRenderableAssetType(type); }

	private:
		static GUID s_AssetModifiedCallbackID;
		static Scope<AssetThumbnailRenderer> s_AssetThumbnailRenderer;
		static std::unordered_map<Ref<Asset>, Ref<Image>> s_ThumbnailCache;
		static bool s_RenderingThumbnail;
	};
}
