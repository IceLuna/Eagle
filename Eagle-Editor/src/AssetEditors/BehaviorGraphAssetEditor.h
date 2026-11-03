#pragma once

#include "AssetEditor.h"
#include "../Undo/GraphUndo.h"

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
		std::string m_WindowName;
		Scope<BehaviorGraphEditor> m_Graph;
		GraphEditTracker m_EditTracker; // After `m_Graph`, it refers to it
		GraphAssetSync m_AssetSync; // After `m_Graph`, it writes into the asset through it when it's destroyed
	};
}
