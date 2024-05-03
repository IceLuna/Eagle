#include "egpch.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationSystem.h"

#include "Eagle/Animation/Nodes/GraphNode.h"
#include "Eagle/Classes/SkeletalMesh.h"

namespace Eagle
{
	static Ref<AnimationStateGraph> GetGraphFromCache(std::unordered_map<Ref<AnimationStateGraph>, Ref<AnimationStateGraph>>& cache,
		std::vector<Ref<AnimationStateGraph>>& states, const Ref<AnimationStateGraph>& stateToCopy, const VariablesMap& variablesToUse)
	{
		auto it = cache.find(stateToCopy);
		if (it != cache.end())
			return it->second;
		
		Ref<AnimationStateGraph> copiedState = states.emplace_back(MakeRef<AnimationStateGraph>(stateToCopy, variablesToUse));
		cache.emplace(stateToCopy, copiedState);

		return copiedState;
	}

	AnimationStateMachineGraph::AnimationStateMachineGraph(const Ref<AnimationStateMachineGraph>& other, const VariablesMap& variablesToUse)
		: m_Pose(other->m_Pose)
	{
		// Key - graph that was copied
		// Value - It's copy
		// Cache is needed to avoid duplication of graphs.
		std::unordered_map<Ref<AnimationStateGraph>, Ref<AnimationStateGraph>> cache;

		m_States.reserve(other->m_States.size());
		for (const auto& stateToCopy : other->m_States)
		{
			Ref<AnimationStateGraph> copiedState = GetGraphFromCache(cache, m_States, stateToCopy, variablesToUse);

			const auto& connectionsToCopy = stateToCopy->GetConnections();
			for (const auto& connection : connectionsToCopy)
			{
				StatesConnection state;
				state.ConnectedTo = GetGraphFromCache(cache, m_States, connection.ConnectedTo, variablesToUse);
				state.Transition = MakeRef<AnimationGraph>(connection.Transition, variablesToUse);

				copiedState->AddConnection(state);
			}
		}
		if (m_States.empty() == false)
			m_CurrentState = m_States[0];
	}

	const SkeletalPose& AnimationStateMachineGraph::Update(Timestep ts)
	{
		m_Pose.Reset();

		if (!m_CurrentState)
			return m_Pose;

		m_Pose = m_CurrentState->Update(ts);

		if (m_TransitioningToState)
		{
			const auto& pose = m_TransitioningToState->Update(ts);
			const auto& skeletal = m_CurrentState->GetSkeletal();

			const float weight = m_CurrentTransitionTime / m_TransitionTime;
			m_CurrentTransitionTime += ts;

			AnimationSystem::BlendPoses(m_Pose, pose, skeletal->GetSkeletal().RootBone, weight, &m_Pose);
			if (m_CurrentTransitionTime >= m_TransitionTime)
			{
				// Finished transitioning. Update the current state
				m_CurrentState.reset();
				std::swap(m_CurrentState, m_TransitioningToState);
			}
		}
		else // If we're not in a transition, check if we should transition
		{
			m_TransitioningToState = m_CurrentState->CheckTransitions(ts, &m_TransitionTime);
			if (m_TransitioningToState)
				m_CurrentTransitionTime = 0.f;
		}

		return m_Pose;
	}
	
	void AnimationStateMachineGraph::SetVariablesToUse(const VariablesMap& vars)
	{
		// Cache is required to avoid infinite recursion.
		// For example, if `state1` and `state2` are connected, then `state1` will trigger `state2`, and `state2` trigger `state1`
		std::unordered_set<void*> cache;
		for (auto& state : m_States)
		{
			auto& connections = state->GetConnections();
			for (auto& connection : connections)
			{
				if (connection.ConnectedTo)
				{
					if (cache.find(connection.ConnectedTo.get()) == cache.end())
					{
						connection.ConnectedTo->SetVariablesToUse(vars);
						cache.emplace(connection.ConnectedTo.get());
					}
				}
				if (connection.Transition)
				{
					if (cache.find(connection.Transition.get()) == cache.end())
					{
						connection.Transition->SetVariablesToUse(vars);
						cache.emplace(connection.Transition.get());
					}
				}
			}
		}
	}
}
