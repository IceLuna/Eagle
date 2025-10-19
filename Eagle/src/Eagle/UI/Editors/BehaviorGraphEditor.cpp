#include "egpch.h"
#include "BehaviorGraphEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/Graphs/AIBehavior/UIBehaviorGraph.h"

namespace Eagle
{
    BehaviorGraphEditor::BehaviorGraphEditor(const Ref<AssetBehaviorGraph>& asset, const std::string& name)
        : GraphEditor(name), m_Asset(asset)
    {
        m_Graph = MakeRef<UIBehaviorGraph>(*this, name);
        AddGraph_Internal(m_Graph);

        OnAddGraphPre();
        Deserialize();
        OnAddGraphPost();
    }

    void BehaviorGraphEditor::Deserialize()
    {
        const auto& data = m_Asset->GetSerializationData();
        m_Graphs[0]->Deserialize(data, data.Graph);
    }

    GraphEditorSerializationData BehaviorGraphEditor::Save()
    {
        GraphEditorSerializationData result = GraphEditor::Save();
        m_Asset->SetSerializationData(result);
        Asset::Save(m_Asset);
        return result;
    }

    void BehaviorGraphEditor::OnAddGraphPre()
    {
        m_GraphIsDirty = m_Asset->IsDirty();
        m_Graph->SetBuildsDisabled(true);
    }

    void BehaviorGraphEditor::OnAddGraphPost()
    {
        m_Graph->SetBuildsDisabled(false);
        m_Graph->CheckIfNodesAreValid();
        m_Graph->RebuildBehaviorTree();
        if (!m_GraphIsDirty) // Creation of nodes makes it dirty, so reset it to false if it wasn't dirty
            m_Asset->SetDirty(false);
    }
}
