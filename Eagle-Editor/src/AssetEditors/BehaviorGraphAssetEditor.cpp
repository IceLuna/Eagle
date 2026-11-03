#include "BehaviorGraphAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/Editors/BehaviorGraphEditor.h"

namespace Eagle
{
	BehaviorGraphAssetEditor::BehaviorGraphAssetEditor(const Ref<AssetBehaviorGraph>& asset)
		: m_Asset(asset)
		, m_WindowName(AssetEditor::GetAssetWindowName(m_Asset))
		, m_Graph(MakeScope<BehaviorGraphEditor>(m_Asset, m_WindowName))
		, m_EditTracker(*m_Graph, m_Asset)
		, m_AssetSync(*m_Graph, m_UndoHistory)
	{
		SetTabWindowName(m_WindowName);

		m_Graph->SetBeginWindowCallback([this]() { m_EditTracker.BeginFrame(&m_UndoHistory); });

		m_Graph->SetRenderAssetSettingsCallback([this]()
		{
			UI::BeginPropertyGrid("BehaviorGraph_AssetSettings");
			AssetEditor::DrawAlwaysIncludeInGameBuilds(m_Asset);
			UI::EndPropertyGrid();
		});
	}

	void BehaviorGraphAssetEditor::OnImGuiRender(bool* pOpen)
	{
		PrepareTabWindow();
		m_Graph->OnImGuiRender(pOpen);
		m_EditTracker.EndFrame();
		m_AssetSync.Update();
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
