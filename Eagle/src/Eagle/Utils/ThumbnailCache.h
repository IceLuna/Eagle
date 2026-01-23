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

		static bool Render(const Ref<Asset>& asset);
		static Ref<Image> Get(const Ref<Asset>& asset);

		static constexpr glm::vec2 GetThumbnailSize() { return glm::vec2(96.f, 96.f); }

		static constexpr bool IsRenderableAssetType(AssetType type) { return AssetThumbnailRenderer::IsRenderableAssetType(type); }
	};
}
