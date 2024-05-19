#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetAnimation;
	class SkeletalMeshComponent;

	class AnimationAssetEditor : public AssetEditor
	{
	public:
		AnimationAssetEditor(const Ref<AssetAnimation>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetAnimation> m_Asset;
		SkeletalMeshComponent* m_Component = nullptr;
		bool bPlayAnimation = true;
		bool bInPlace = true;
	};
}
