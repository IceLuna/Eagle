#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

#include "Eagle/Renderer/Material.h"

namespace Eagle
{
	class AssetMaterial;
	class AssetStaticMesh;

	class MaterialAssetEditor : public AssetEditor
	{
	public:
		MaterialAssetEditor(const Ref<AssetMaterial>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		static std::string DescribeState(const AssetMaterial::Settings& before, const AssetMaterial::Settings& after);

	private:
		Ref<AssetMaterial> m_Asset;
		Ref<AssetStaticMesh> m_Sphere;
		AssetEditTracker<AssetMaterial, AssetMaterial::Settings> m_EditTracker;
	};
}
