#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

namespace Eagle
{
	class AssetTextureCube;

	class TextureCubeAssetEditor : public AssetEditor
	{
	public:
		TextureCubeAssetEditor(const Ref<AssetTextureCube>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		static std::string DescribeState(const AssetTextureCube::Settings& before, const AssetTextureCube::Settings& after);

	private:
		Ref<AssetTextureCube> m_Asset;
		AssetEditTracker<AssetTextureCube, AssetTextureCube::Settings> m_EditTracker;

		int m_LayersSize = 0;
		int m_PrefilterSize = 0;
	};
}
