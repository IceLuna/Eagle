#pragma once

#include "Eagle/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Eagle
{
	class Scene;
	class SceneRenderer;
	class Image;

	class AssetStaticMesh;
	class AssetSkeletalMesh;
	class AssetMaterial;
	class AssetEntity;
	class AssetAnimation;
	class AssetParticleSystem;

	class AssetThumbnailRenderer
	{
	public:
		AssetThumbnailRenderer(glm::uvec2 size);
		virtual ~AssetThumbnailRenderer() {}

		// It's supposed to be called only once and the result (GetImage()) to be reused
		template <typename T>
		void Render(const Ref<T>& asset, bool bNeedSkybox = true)
		{
			Prepare(bNeedSkybox);
			SetupScene(asset);
			Render();
		}

		const Ref<Image>& GetImage() const { return m_Image; }

		static constexpr bool IsRenderableAssetType(AssetType type)
		{
			switch (type)
			{
			case Eagle::AssetType::StaticMesh:
			case Eagle::AssetType::SkeletalMesh:
			case Eagle::AssetType::Material:
			case Eagle::AssetType::Entity:
			case Eagle::AssetType::Animation:
			case Eagle::AssetType::ParticleSystem:
				return true;
			}

			return false;
		}

	protected:
		void Prepare(bool bNeedSkyboxLighting = true);
		void Render();
		void SetupScene(const Ref<AssetStaticMesh>& asset);
		void SetupScene(const Ref<AssetSkeletalMesh>& asset);
		void SetupScene(const Ref<AssetMaterial>& asset);
		void SetupScene(const Ref<AssetEntity>& asset);
		void SetupScene(const Ref<AssetAnimation>& asset);
		void SetupScene(const Ref<AssetParticleSystem>& asset);

	protected:
		Ref<Image> m_Image;
		Ref<Scene> m_Scene;
		Ref<SceneRenderer> m_Renderer;

		// Internal copy of an asset that can be used for rendering preview.
		// Currently used for particle assets to create a copy of it which will be destroyed immediately.
		Ref<Asset> m_TempAsset;
	};
}
