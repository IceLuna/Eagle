#include "BehaviorGraphAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/Editors/BehaviorGraphEditor.h"

namespace Eagle
{
	BehaviorGraphAssetEditor::BehaviorGraphAssetEditor(const Ref<AssetBehaviorGraph>& asset)
		: m_Asset(asset), m_Graph(MakeScope<BehaviorGraphEditor>(m_Asset, m_Asset->GetPath().u8string()))
	{
	}

	void BehaviorGraphAssetEditor::OnImGuiRender(bool* pOpen)
	{
		m_Graph->OnImGuiRender(pOpen);
	}

	void BehaviorGraphAssetEditor::SetInFocus()
	{
		m_Graph->SetInFocus();
	}

	void BehaviorGraphAssetEditor::OnEvent(Event& e)
	{
		AssetEditor::OnEvent(e);
		m_Graph->OnEvent(e);
	}
}
