#pragma once

#include "AssetEditor.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class AssetFont;
	class Text2DComponent;

	class FontAssetEditor : public AssetEditor
	{
	public:
		FontAssetEditor(const Ref<AssetFont>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetFont> m_Asset;
		Text2DComponent* m_Component = nullptr; // Not owning
		std::string m_Text = "Hello, World!";
	};
}
