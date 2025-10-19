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
        GraphNodeFactory::FillAnimationNodes(m_NodeFactory);
        UIGraph::SetupNodeFactory();
    }
}
