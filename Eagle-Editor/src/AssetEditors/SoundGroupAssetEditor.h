#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetSoundGroup;

	class SoundGroupAssetEditor : public AssetEditor
	{
	public:
		SoundGroupAssetEditor(const Ref<AssetSoundGroup>& asset) : m_Asset(asset) {}

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetSoundGroup> m_Asset;
	};
}
