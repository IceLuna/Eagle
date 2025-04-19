#include "egpch.h"
#include "AnimationStateGraph.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"

namespace Eagle
{
	Ref<AnimationStateGraph> AnimationStateGraph::CheckTransitions(Timestep ts, float* outTransitionTime, bool* outUseSmoothTransition)
	{
		for (auto& connection : m_Connections)
		{
			auto transitionNode = Cast<AnimationGraphNodeTransitionOutput>(connection.Transition->GetResult());
			if (!transitionNode)
				continue;

			transitionNode->Update(ts);
			if (transitionNode->ShouldTransition())
			{
				*outTransitionTime = transitionNode->GetTransitionTime();
				*outUseSmoothTransition = transitionNode->ShouldUseSmoothTransition();
				return connection.ConnectedTo;
			}
			else if (transitionNode->ShouldAutoTransition())
			{
				const float timeTillLoop = GetResult()->GetTimeTillAnimationLoops();
				const float transitionTime = transitionNode->GetTransitionTime();
				const bool bTransition = transitionTime >= timeTillLoop; // Start transitioning before animation is about to finish
				if (bTransition)
				{
					*outTransitionTime = timeTillLoop;
					*outUseSmoothTransition = transitionNode->ShouldUseSmoothTransition();
					return connection.ConnectedTo;
				}
			}
		}

		return {};
	}
	
	Ref<AnimationStateGraph> AnimationStateGraph::CreateSubgraph(const Ref<AnimationGraph>& root, const Ref<const AnimationStateGraph>& other, const VariablesMap& variablesToUse)
	{
		class LocalAnimationStateGraph : public AnimationStateGraph
		{
		public:
			LocalAnimationStateGraph() = default;

			friend class AnimationStateGraph;
		};

		auto result = MakeRef<LocalAnimationStateGraph>();
		result->SetRootGraph(root);
		result->Init(other, variablesToUse);

		return result;
	}
}
