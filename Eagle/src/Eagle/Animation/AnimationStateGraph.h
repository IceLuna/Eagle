#pragma once

#include "AnimationGraph.h"

namespace Eagle
{
	class AnimationStateGraph;

	struct StatesConnection
	{
		Ref<AnimationStateGraph> ConnectedTo;
		Ref<AnimationGraph> Transition;
	};

	class AnimationStateGraph : public AnimationGraph
	{
	public:
		AnimationStateGraph(const Ref<AssetSkeletalMesh>& skeletal) : AnimationGraph(skeletal) {}
		AnimationStateGraph(const Ref<const AnimationStateGraph>& other, const VariablesMap& variablesToUse) : AnimationGraph(other, variablesToUse) {}

		void AddConnection(const StatesConnection& connection) { m_Connections.push_back(connection); }

		void SetConnections(const std::vector<StatesConnection>& connections) { m_Connections = connections; }
		const std::vector<StatesConnection>& GetConnections() const { return m_Connections; }

		// Returns a valid "Ref<AnimationStateGraph>" if should transition.
		// Also returns the time it should take for a full transition (outTransitionTime)
		Ref<AnimationStateGraph> CheckTransitions(Timestep ts, float* outTransitionTime, bool* outUseSmoothTransition);

	private:
		// Note: this class is not responsible for cloning and setting variables (SetVariablesToUse) of connections because of infinite recursion complications.
		// So, animation state machine is responsible for it since it has all the information it needs to prevent infinite recursion.
		std::vector<StatesConnection> m_Connections;
	};
}
