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
	
	Ref<AnimationStateGraph> AnimationStateGraph::Create(const Ref<const AnimationStateGraph>& other, const VariablesMap& variablesToUse)
	{
		class LocalAnimationStateGraph : public AnimationStateGraph
		{
		public:
			LocalAnimationStateGraph() = default;
		};

		auto result = MakeRef<LocalAnimationStateGraph>();
		result->Init(other, variablesToUse);

		return result;
	}
}
