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
            std::string name = node.GetName();
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

                    if (graphNode->GetName() == name)
                    {
                        name = node.GetName() + std::to_string(i++);
                        bContinue = true;
                        break;
                    }
                }
            }
            node.SetName(name);
            node.Graph->SetName(name);
        }

        UIGraph::OnNodeAdded(node);
    }

    void UIAnimationGraph::SetupInitialNodes()
    {
        m_OutputNodeId = GraphNodeFactory::SpawnOutputPoseNode(*this).ID;
    }

    void UIAnimationGraph::SetupNodeFactory()
    {
        GraphNodeFactory::FillAnimationNodes(m_NodeFactory);
        UIGraph::SetupNodeFactory();

        // Other
        {
            auto& otherCategory = m_NodeFactory["Other"];
            otherCategory["New State Machine"] = &GraphNodeFactory::SpawnAnimationStateMachine;
        }
    }
}
