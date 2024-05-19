#pragma once

#include "AssetEditor.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class AssetStaticMesh;

	class StaticMeshAssetEditor : public AssetEditor
	{
	public:
		StaticMeshAssetEditor(const Ref<AssetStaticMesh>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetStaticMesh> m_Asset;
	};
}
