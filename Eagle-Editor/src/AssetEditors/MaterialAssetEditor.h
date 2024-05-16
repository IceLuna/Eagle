#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetMaterial;

	class MaterialAssetEditor : public AssetEditor
	{
	public:
		MaterialAssetEditor(const Ref<AssetMaterial>& asset) : m_Asset(asset) {}

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetMaterial> m_Asset;
	};
}
