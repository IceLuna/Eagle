#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetAnimation;

	class AnimationAssetEditor : public AssetEditor
	{
	public:
		AnimationAssetEditor(const Ref<AssetAnimation>& asset) : m_Asset(asset) {}

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetAnimation> m_Asset;
	};
}
