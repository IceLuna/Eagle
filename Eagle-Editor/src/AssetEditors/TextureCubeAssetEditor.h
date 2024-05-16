#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetTextureCube;

	class TextureCubeAssetEditor : public AssetEditor
	{
	public:
		TextureCubeAssetEditor(const Ref<AssetTextureCube>& asset) : m_Asset(asset) {}

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetTextureCube> m_Asset;
	};
}
