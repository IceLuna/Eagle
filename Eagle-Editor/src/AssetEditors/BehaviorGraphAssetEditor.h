#pragma once

#include "AssetEditor.h"

namespace Eagle
{
	class AssetBehaviorGraph;
	class BehaviorGraphEditor;

	class BehaviorGraphAssetEditor : public AssetEditor
	{
	public:
		BehaviorGraphAssetEditor(const Ref<AssetBehaviorGraph>& asset);

		void OnImGuiRender(bool* pOpen) override;

		void SetInFocus() override;

		void OnEvent(Event& e) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetBehaviorGraph> m_Asset;
		Scope<BehaviorGraphEditor> m_Graph;
	};
}
