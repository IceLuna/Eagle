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

    void BaseAnimationGraph::Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data)
    {
        UIGraph::Deserialize(editorData, data);
    }

    std::vector<GraphSerializationData> BaseAnimationGraph::Serialize()
    {
        return UIGraph::Serialize();
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
