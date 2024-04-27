#include "egpch.h"
#include "AnimationStateTransitionGraph.h"

namespace Eagle
{
	AnimationStateTransitionGraph::AnimationStateTransitionGraph(GraphEditor& editor, const std::string_view name)
		: UIGraph(editor, name)
	{
		SetupInitialNodes();
		SetupNodeFactory();
	}
	

	void AnimationStateTransitionGraph::SetupInitialNodes()
	{
		m_OutputNodeId = GraphNodeFactory::SpawnOutputTransitionNode(*this).ID;
	}
}
