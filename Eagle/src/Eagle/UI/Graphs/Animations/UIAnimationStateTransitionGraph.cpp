#include "egpch.h"
#include "UIAnimationStateTransitionGraph.h"

namespace Eagle
{
	UIAnimationStateTransitionGraph::UIAnimationStateTransitionGraph(GraphEditor& editor, const std::string_view name)
		: UIGraph(editor, name)
	{
		SetupInitialNodes();
		SetupNodeFactory();
	}
	

	void UIAnimationStateTransitionGraph::SetupInitialNodes()
	{
		m_OutputNodeId = GraphNodeFactory::SpawnOutputTransitionNode(*this).ID;
	}
}
