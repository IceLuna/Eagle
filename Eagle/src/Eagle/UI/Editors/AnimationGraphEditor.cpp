#include "egpch.h"
#include "AnimationGraphEditor.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Graphs/UIAnimationGraph.h"

namespace Eagle
{
	AnimationGraphEditor::AnimationGraphEditor(const Ref<AssetAnimationGraph>& graph, const std::string& name)
        : GraphEditor(name), m_Graph(graph)
	{
        Ref<UIAnimationGraph> animGraph = MakeRef<UIAnimationGraph>(*this, name);
        AddGraph_Internal(animGraph);

        OnAddGraphPre();
        Deserialize();
        OnAddGraphPost();
    }

    void AnimationGraphEditor::Deserialize()
    {
        const auto& data = m_Graph->GetSerializationData();

        // Create variables
        for (const auto& var : data.Variables)
        {
            // Create var if doesn't exist
            if (!GetVariable(var.Name))
                CreateNewVarFromType(var.Value->GetType(), var.Value, var.Name);
        }

        m_Graphs[0]->Deserialize(data, data.Graph);
    }

    void AnimationGraphEditor::Compile()
    {
        auto& graph = m_Graph->GetGraph();
        graph->Reset();

        VariablesMap usedVars;
        auto& graphToCompile = m_Graphs[0];
        auto result = graphToCompile->Compile(usedVars);
        graph->SetVariables(std::move(usedVars));

        // We clone it so that changing node in the editor doesn't affect the final component without compilation
        if (result)
        {
            result = result->Clone();
            EG_CORE_ASSERT(result);
        }

        graph->SetResult(result);

        // Required to update graphs
        if (auto& scene = Scene::GetCurrentScene())
            scene->UpdateAnimGraphAsset(m_Graph);
    }

    GraphEditorSerializationData AnimationGraphEditor::Save()
    {
        GraphEditorSerializationData result = GraphEditor::Save();

        m_Graph->SetSerializationData(result);
        Asset::Save(m_Graph);
        return result;
    }
    
    void AnimationGraphEditor::OnGraphChanged()
    {
        if (IgnoreChangedEvent() == false)
            m_Graph->SetDirty(true);
    }

    void AnimationGraphEditor::OnAddGraphPre()
    {
        m_GraphIsDirty = m_Graph->IsDirty();
    }

    void AnimationGraphEditor::OnAddGraphPost()
    {
        if (!m_GraphIsDirty) // Creation of nodes makes it dirty, so reset it to false if it wasn't dirty
            m_Graph->SetDirty(false);
    }
}
