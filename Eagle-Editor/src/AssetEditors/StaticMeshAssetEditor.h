#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class AssetStaticMesh;
	class StaticMeshComponent;

	class StaticMeshAssetEditor : public AssetEditor
	{
	public:
		StaticMeshAssetEditor(const Ref<AssetStaticMesh>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		static std::string DescribeState(const AssetStaticMesh::Settings& before, const AssetStaticMesh::Settings& after);

	private:
		Ref<AssetStaticMesh> m_Asset;
		StaticMeshComponent* m_Component = nullptr; // Not owning
		AssetEditTracker<AssetStaticMesh, AssetStaticMesh::Settings> m_EditTracker;
	};
}
