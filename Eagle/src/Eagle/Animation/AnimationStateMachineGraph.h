#pragma once

#include "Animation.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	class GraphNode;
	class AnimationStateGraph;

	class AnimationStateMachineGraph
	{
	public:
		AnimationStateMachineGraph() = default;
		AnimationStateMachineGraph(const Ref<AnimationStateMachineGraph>& other, const VariablesMap& variablesToUse);

		// Can be used by other graphs in cases when they need to calculate subgraphs
		const SkeletalPose& Update(Timestep ts);

		void AddState(const Ref<AnimationStateGraph>& state) { m_States.push_back(state); }

		void SetVariablesToUse(const VariablesMap& vars);

	private:
		std::vector<Ref<AnimationStateGraph>> m_States;
		Ref<AnimationStateGraph> m_CurrentState;
		Ref<AnimationStateGraph> m_TransitioningToState;

		float m_CurrentTransitionTime = 0.f; // How much time has passed since we started transitioning
		float m_TransitionTime = 0.f; // Time it should take to transition from "m_CurrentState" to "m_TransitioningToState"
		bool bUseSmoothTransition = true; // If false, Frozen transition is used

		SkeletalPose m_Pose; // Pose that was calculated by the node during the latest update
	};
}
