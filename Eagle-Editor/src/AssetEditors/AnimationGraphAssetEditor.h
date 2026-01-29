#pragma once

#include "AssetEditor.h"

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
		Entity m_Entity;
	};
}
