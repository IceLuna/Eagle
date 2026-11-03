#pragma once

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"

namespace Eagle
{
	class AssetTexture2D;

	class Texture2DAssetEditor : public AssetEditor
	{
	public:
		Texture2DAssetEditor(const Ref<AssetTexture2D>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		static std::string DescribeState(const AssetTexture2D::Settings& before, const AssetTexture2D::Settings& after);

	private:
		Ref<AssetTexture2D> m_Asset;
		AssetEditTracker<AssetTexture2D, AssetTexture2D::Settings> m_EditTracker;
		std::vector<std::string> m_MipNames;
		int m_SelectedMip = 0;
		int m_GenerateMipsCount = 0;
	};
}
