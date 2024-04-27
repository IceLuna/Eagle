#include "egpch.h"
#include "AnimationStateGraph.h"

namespace Eagle
{
	AnimationStateGraph::AnimationStateGraph(GraphEditor& editor, const std::string_view name)
		: UIGraph(editor, name)
	{
        SetupInitialNodes();
        SetupNodeFactory();
	}

    void AnimationStateGraph::SetupInitialNodes()
    {
        m_OutputNodeId = GraphNodeFactory::SpawnStateOutputPoseNode(*this).ID;
    }

    void AnimationStateGraph::SetupNodeFactory()
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
