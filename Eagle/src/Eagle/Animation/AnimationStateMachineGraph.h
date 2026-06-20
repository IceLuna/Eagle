#pragma once

#include "Animation.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	class GraphNode;
	class AnimationStateGraph;
	class AnimationGraph;

	class AnimationStateMachineGraph
	{
	public:
		AnimationStateMachineGraph() = default;
		AnimationStateMachineGraph(const Ref<AnimationGraph>& root, const Ref<AnimationStateMachineGraph>& other, const VariablesMap& variablesToUse);
		virtual ~AnimationStateMachineGraph();

		// Can be used by other graphs in cases when they need to calculate subgraphs
		SkeletalPose& Update(Timestep ts);

		void AddState(const Ref<AnimationStateGraph>& state)
		{
			m_States.push_back(state);
			if (m_States.size() == 1)
				m_CurrentState = m_States[0];
		}

		const std::vector<Ref<AnimationStateGraph>>& GetStates() const { return m_States; }

		void SetVariablesToUse(const VariablesMap& vars);

	private:
		Weak<AnimationGraph> m_RootGraph;
		std::vector<Ref<AnimationStateGraph>> m_States;
		Ref<AnimationStateGraph> m_CurrentState;
		Ref<AnimationStateGraph> m_TransitioningToState;

		float m_CurrentTransitionTime = 0.f; // How much time has passed since we started transitioning
		float m_TransitionTime = 0.f; // Time it should take to transition from "m_CurrentState" to "m_TransitioningToState"
		bool bUseSmoothTransition = true; // If false, Frozen transition is used

		SkeletalPose m_Pose; // Pose that was calculated by the node during the latest update
		size_t m_CalculatedOnFrame = 0;
	};
}
