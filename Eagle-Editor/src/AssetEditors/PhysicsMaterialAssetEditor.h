#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetPhysicsMaterial;

	class PhysicsMaterialAssetEditor : public AssetEditor
	{
	public:
		PhysicsMaterialAssetEditor(const Ref<AssetPhysicsMaterial>& asset) : m_Asset(asset) {}

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetPhysicsMaterial> m_Asset;
	};
}
