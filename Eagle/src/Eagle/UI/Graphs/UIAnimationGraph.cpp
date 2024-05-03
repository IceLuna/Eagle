#include "egpch.h"
#include "UIAnimationGraph.h"
#include "Eagle/UI/Editors/GraphEditor.h"
#include "Eagle/Animation/Nodes/GraphNodeFactory.h"

namespace Eagle
{
    UIAnimationGraph::UIAnimationGraph(GraphEditor& editor, const std::string_view name)
        : UIGraph(editor, name)
    {
        SetupInitialNodes();
        SetupNodeFactory();
    }

    void UIAnimationGraph::OnNodeAdded(Node& node)
    {
        if (node.Graph)
        {
            // Set unique name for the graph node.
            // Do this before calling parent's "OnNodeAdded"
            std::string name = node.UserData;
            uint32_t i = 0;
            bool bContinue = true;
            while (bContinue)
            {
                bContinue = false;
                for (const auto& nodeID : m_NodesWithGraph)
                {
                    Node* graphNode = FindNode(nodeID);
                    if (!graphNode)
                        continue;

                    if (graphNode->UserData == name)
                    {
                        name = node.Name + std::to_string(i++);
                        bContinue = true;
                        break;
                    }
                }
            }
            node.UserData = std::move(name);
            node.Graph->SetName(node.UserData); // TODO: don't forget to update it after renaming the node
        }

        UIGraph::OnNodeAdded(node);
    }

    void UIAnimationGraph::SetupInitialNodes()
    {
        m_OutputNodeId = GraphNodeFactory::SpawnOutputPoseNode(*this).ID;
    }

    void UIAnimationGraph::SetupNodeFactory()
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
