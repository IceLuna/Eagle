#pragma once

#include "AssetEditor.h"
#include "../Undo/GraphUndo.h"

namespace Eagle
{
	class AssetAnimationGraph;
	class AnimationGraphEditor;

	class AnimationGraphAssetEditor : public AssetEditor
	{
	public:
		AnimationGraphAssetEditor(const Ref<AssetAnimationGraph>& asset);

		void OnImGuiRender(bool* pOpen) override;

		void SetInFocus() override;

		void OnEvent(Event& e) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		Ref<AssetAnimationGraph> m_Asset;
		std::string m_WindowName;
		Scope<AnimationGraphEditor> m_Graph;
		GraphEditTracker m_EditTracker; // After `m_Graph`, it refers to it
		GraphAssetSync m_AssetSync; // After `m_Graph`, it writes into the asset through it when it's destroyed
		Entity m_Entity;
	};
}
