#include "egpch.h"
#include "UIAnimationStateGraph.h"

namespace Eagle
{
    UIAnimationStateGraph::UIAnimationStateGraph(GraphEditor& editor, const std::string_view name)
		: UIGraph(editor, name)
	{
        SetupInitialNodes();
        SetupNodeFactory();
	}

    void UIAnimationStateGraph::SetupInitialNodes()
    {
        m_OutputNodeId = GraphNodeFactory::SpawnStateOutputPoseNode(*this).ID;
    }

    void UIAnimationStateGraph::SetupNodeFactory()
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
    }
}
