#include "egpch.h"
#include "BaseAnimationGraph.h"
#include "Eagle/UI/Editors/GraphEditor.h"
#include "Eagle/UI/Nodes/GraphNodeFactory.h"

namespace Eagle
{
    BaseAnimationGraph::BaseAnimationGraph(GraphEditor& editor, const std::string_view name)
        : UIGraph(editor, name)
    {
        SetupInitialNodes();
        SetupNodeFactory();
    }

    void BaseAnimationGraph::Deserialize(const GraphSerializationData& data)
    {
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        m_GraphData.Editor->SetViewScroll(ImVec2(data.ScrollOffset.x, data.ScrollOffset.y));
        m_GraphData.Editor->SetViewZoom(data.Zoom);

        // Create variables
        for (const auto& var : data.Variables)
        {
            // Create var if doesn't exist
            if (!m_Editor.GetVariable(var.Name))
                m_Editor.CreateNewVarFromType(var.Value->GetType(), var.Value, var.Name);
        }

        // Create nodes
        int maxNodeID = m_GraphData.NextId;
        for (const auto& nodeData : data.Nodes)
        {
            if (m_OutputNodeId.Get() == nodeData.NodeID) // Special case for the base node
            {
                ed::SetNodePosition(m_OutputNodeId, ImVec2(nodeData.Position.x, nodeData.Position.y));
                continue;
            }

            m_GraphData.NextId = int(nodeData.NodeID); // So that the node is created with the required ID

            if (nodeData.bVariable)
            {
                if (const auto& var = m_Editor.GetVariable(nodeData.Name))
                {
                    Node& createdNode = GraphNodeFactory::SpawnVarNode(*this, nodeData.Name, GetPinType(var->GetType()));
                    ed::SetNodePosition(createdNode.ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                }
            }
            else
            {
                for (const auto& [unused, factory] : m_NodeFactory)
                {
                    auto it = factory.find(nodeData.Name);
                    if (it != factory.end())
                    {
                        auto func = it->second;
                        Node& createdNode = (*func)(*this, nodeData.Name);
                        createdNode.Size = ImVec2(nodeData.Size.x, nodeData.Size.y);
                        ed::SetNodePosition(createdNode.ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                        ed::SetGroupSize(createdNode.ID, createdNode.Size);
                        createdNode.UserData = nodeData.UserData;

                        // Set default values
                        const size_t inputPinsCount = createdNode.InputPins.size();
                        if (inputPinsCount == nodeData.DefaultValues.size()) // Should always match, but this check is here just in case
                        {
                            for (size_t i = 0; i < inputPinsCount; ++i)
                                createdNode.InputPins[i].DefaultValue = nodeData.DefaultValues[i];
                        }
                        break;
                    }
                }
            }

            if (m_GraphData.NextId > maxNodeID)
                maxNodeID = m_GraphData.NextId; // Save the max node ID so that we can set `m_NextId` to it after all nodes are created
        }
        m_GraphData.NextId = maxNodeID;

        // Link nodes
        for (const auto& nodeData : data.Nodes)
        {
            for (const auto& connection : nodeData.OutputConnections)
            {
                if (Node* connectToNode = FindNode(connection.NodeID))
                {
                    Node* currentNode = FindNode(nodeData.NodeID);
                    const Pin* startPin = &currentNode->OutputPins[0];
                    const Pin* endPin = &connectToNode->InputPins[connection.PinIndex];

                    m_GraphData.Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
                    m_GraphData.Links.back().Color = GetIconColor(startPin->Type);
                    OnLinkCreated(m_GraphData.Links.back());
                }
            }
        }
    
        ed::SetCurrentEditor(editorBefore);
    }

    void BaseAnimationGraph::SetupInitialNodes()
    {
        m_OutputNodeId = GraphNodeFactory::SpawnOutputPoseNode(*this).ID;
    }

    void BaseAnimationGraph::SetupNodeFactory()
    {
        // Animations category
        {
            auto& animationsCategory = m_NodeFactory["Animations"];
            animationsCategory["Animation Clip"] = &GraphNodeFactory::SpawnAnimClipNode;
            animationsCategory["Blend Poses"] = &GraphNodeFactory::SpawnAnimBlendNode;
            animationsCategory["Additive Blend"] = &GraphNodeFactory::SpawnAnimAdditiveBlendNode;
            animationsCategory["Calculate Additive"] = &GraphNodeFactory::SpawnAnimCalculateAdditiveNode;
            animationsCategory["Select Pose by Bool"] = &GraphNodeFactory::SpawnSelectPoseByBoolNode;
        }

        UIGraph::SetupNodeFactory();

        // Other
        {
            auto& otherCategory = m_NodeFactory["Other"];
            otherCategory["New State Machine"] = &GraphNodeFactory::SpawnStateMachine;
        }
    }
}
