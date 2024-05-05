#include "egpch.h"
#include "AnimationStateGraph.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"

namespace Eagle
{
	Ref<AnimationStateGraph> AnimationStateGraph::CheckTransitions(Timestep ts, float* outTransitionTime, bool* outUseSmoothTransition)
	{
		for (auto& connection : m_Connections)
		{
			if (auto transitionNode = Cast<AnimationGraphNodeTransitionOutput>(connection.Transition->GetResult()))
			{
				transitionNode->Update(ts);
				if (transitionNode->ShouldTransition())
				{
					*outTransitionTime = transitionNode->GetTransitionTime();
					*outUseSmoothTransition = transitionNode->ShouldUseSmoothTransition();
					return connection.ConnectedTo;
				}
			}
		}

		return {};
	}
}
