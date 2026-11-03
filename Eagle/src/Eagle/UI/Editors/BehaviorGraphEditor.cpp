#include "egpch.h"
#include "BehaviorGraphEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/Graphs/AIBehavior/UIBehaviorGraph.h"

namespace Eagle
{
    BehaviorGraphEditor::BehaviorGraphEditor(const Ref<AssetBehaviorGraph>& asset, const std::string& name)
        : GraphEditor(name), m_Asset(asset)
    {
        m_Graphs.emplace_back(CreateRootGraph());

        OnAddGraphPre();
        Deserialize();
        OnAddGraphPost();
    }

    void BehaviorGraphEditor::Deserialize()
    {
        const auto& data = m_Asset->GetSerializationData();
        m_Graphs[0]->Deserialize(data, data.Graph);
    }

    void BehaviorGraphEditor::Save()
    {
        WriteToAsset();
        Asset::Save(m_Asset);
    }

    void BehaviorGraphEditor::WriteToAsset()
    {
        m_Asset->SetSerializationData(GraphEditor::CaptureState());
    }

    Ref<UIGraph> BehaviorGraphEditor::CreateRootGraph()
    {
        m_Graph = MakeRef<UIBehaviorGraph>(*this, m_Name);
        return m_Graph;
    }

    void BehaviorGraphEditor::OnAddGraphPre()
    {
        m_GraphIsDirty = m_Asset->IsDirty();
        m_Graph->SetBuildsDisabled(true);
    }

    void BehaviorGraphEditor::OnAddGraphPost()
    {
        m_Graph->SetBuildsDisabled(false);
        m_Graph->ValidateNodes();
        m_Graph->RebuildBehaviorTree();
        if (!m_GraphIsDirty) // Creation of nodes makes it dirty, so reset it to false if it wasn't dirty
            m_Asset->SetDirty(false);
    }
}
