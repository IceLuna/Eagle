#include "egpch.h"
#include "AnimationGraphAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"

namespace Eagle
{
	AnimationGraphAssetEditor::AnimationGraphAssetEditor(const Ref<AssetAnimationGraph>& asset)
		: m_Asset(asset), m_Graph(MakeScope<AnimationGraphEditor>(m_Asset, m_Asset->GetPath().u8string()))
	{
	}

	void AnimationGraphAssetEditor::OnImGuiRender(bool* pOpen)
	{
		m_Graph->OnImGuiRender(pOpen);
	}

	void AnimationGraphAssetEditor::SetInFocus()
	{
		m_Graph->SetInFocus();
	}

	void AnimationGraphAssetEditor::OnEvent(Event& e)
	{
		m_Graph->OnEvent(e);
	}
}
