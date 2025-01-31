#pragma once

#include "AssetEditor.h"

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
		Ref<AssetTexture2D> m_Asset;
		std::vector<std::string> m_MipNames;
		int m_SelectedMip = 0;
		int m_GenerateMipsCount = 0;
		bool bDetailsDocked = false;
		bool bDetailsVisible = false;
	};
}
