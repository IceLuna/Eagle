#include "egpch.h"
#include "AnimationGraphEditor.h"
#include "Eagle/UI/Nodes/AnimationNodes.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Graphs/BaseAnimationGraph.h"

namespace Eagle
{
	AnimationGraphEditor::AnimationGraphEditor(const Ref<AssetAnimationGraph>& graph, const std::string& name)
        : GraphEditor(name), m_Graph(graph)
	{
        Ref<BaseAnimationGraph> animGraph = MakeRef<BaseAnimationGraph>(*this, name);
        AddGraph_Internal(animGraph);

        // TODO: fix this approach
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

        if (data.Graphs.size() > 0)
            m_Graphs[0]->Deserialize(data, data.Graphs[0]);
    }

    void AnimationGraphEditor::Parse(const Ref<UIGraph>& graph, Node* node, bool bCloneVars, VariablesMap& outVariables)
    {
        for (auto& input : node->Inputs)
        {
            if (!input)
                continue;

            Node* connectedNode = graph->FindNode(input);
            auto& graphNode = connectedNode->GraphNode;
            if (!graphNode)
                continue;

            graphNode->Reset();
            const size_t inputsCount = connectedNode->Inputs.size();
            for (size_t i = 0; i < inputsCount; ++i)
            {
                const auto& input = connectedNode->Inputs[i];
                Node* inputNode = graph->FindNode(input);
                if (inputNode)
                {
                    if (inputNode->GraphNode)
                        graphNode->SetInput(inputNode->GraphNode, i);
                    else if (inputNode->Type == NodeType::Variable)
                    {
                        const auto& var = GetVariable(inputNode->Name);
                        Ref<GraphVariable> resultVar;
                        if (bCloneVars)
                        {
                            if (auto it = outVariables.find(inputNode->Name); it != outVariables.end())
                                resultVar = it->second;
                            else
                                resultVar = CopyVarByType(var);
                        }
                        else
                            resultVar = var;

                        if (resultVar)
                            outVariables[inputNode->Name] = resultVar;
                        graphNode->SetInput(resultVar, i);
                    }
                }
                else
                {
                    const auto& inputPin = connectedNode->InputPins[i];
                    if (inputPin.DefaultValue)
                        graphNode->SetInput(inputPin.DefaultValue, i);
                }
            }

            Parse(graph, connectedNode, bCloneVars, outVariables);
        }
    }

    void AnimationGraphEditor::Compile()
    {
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        auto& graphToCompile = m_Graphs[0];

        ed::SetCurrentEditor(graphToCompile->GetGraphData().Editor);

        auto& graph = m_Graph->GetGraph();

        Node* result = graphToCompile->GetOutputNode();
        graph->Reset();

        if (result->Inputs[0])
        {
            VariablesMap usedVars;
            Parse(graphToCompile, result, true, usedVars);

            graph->SetVariables(std::move(usedVars));

            // We clone it so that changing node in the editor doesn't affect the final component without compilation
            const auto& resultNode = graphToCompile->FindNode(result->Inputs[0])->GraphNode;
            graph->SetOutput(resultNode ? resultNode->Clone() : nullptr);
        }
        else
        {
            graph->SetOutput(nullptr);
        }

        ed::SetCurrentEditor(editorBefore);

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
