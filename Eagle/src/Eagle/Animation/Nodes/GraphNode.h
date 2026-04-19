#pragma once

#include "Eagle/Animation/Animation.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/UI/Graphs/GraphVariables.h"

namespace Eagle
{
	class AnimationGraph;
	struct SkeletalPose;

	// TODO: currently, it's still tied to Animation Graph. Find a way to fix it (for example, currently Update() returns SkeletalPose)
	class GraphNode
	{
	public:
		GraphNode(const Weak<AnimationGraph>& graph, size_t numInputs)
			: m_Graph(graph)
		{
			m_Inputs.resize(numInputs);
			m_Variables.resize(numInputs);
		}

		virtual ~GraphNode() = default;

		virtual SkeletalPose& Update(Timestep ts) = 0;

		// @createdNodes. Map of nodes that were created during cloning. It's used to prevent the same node being cloned multiple times.
		// For example, let's consider a situation: we have node `A` with 2 inputs, and node `B` is connected to both `A` inputs.
		// We want to create node `B` just once and set it to both inputs of `A`, and that's what `createdNodes` allows us to do.
		virtual Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph, std::map<const GraphNode*, Ref<GraphNode>>& createdNodes) const = 0;
		Ref<GraphNode> Clone(const Weak<AnimationGraph>& newGraph) const
		{
			std::map<const GraphNode*, Ref<GraphNode>> createdNodes;
			return Clone(newGraph, createdNodes);
		}

		void SetInput(const Ref<GraphNode>& node, size_t index)
		{
			m_Inputs[index] = node;
			m_Variables[index].reset();
		}

		void SetInput(const Ref<GraphVariable>& var, size_t index)
		{
			m_Variables[index] = var;
			m_Inputs[index].reset();
		}

		void ClearInput(size_t index)
		{
			m_Inputs[index].reset();
			m_Variables[index].reset();
		}

		void AddInput()
		{
			m_Inputs.emplace_back();
			m_Variables.emplace_back();
		}

		void PopInput()
		{
			m_Inputs.pop_back();
			m_Variables.pop_back();
		}

		const Weak<AnimationGraph>& GetGraph() const { return m_Graph; }
		const std::vector<Ref<GraphNode>>& GetInputNodes() const { return m_Inputs; }
		const std::vector<Ref<GraphVariable>>& GetInputVariables() const { return m_Variables; }
		float GetTimeTillAnimationLoops() const { return m_Pose.TimeTillAnimationLoops; }

		void ResetInputs()
		{
			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				m_Inputs[i].reset();
				m_Variables[i].reset();
			}
		}

		const SkeletalPose& GetPose() const { return m_Pose; }

	protected:
		template<typename T, class... Args>
		Ref<T> CloneNode(std::map<const GraphNode*, Ref<GraphNode>>& createdNodes, Args&&... args) const
		{
			Ref<T> clone = MakeRef<T>(std::forward<Args>(args)...);
			clone->m_CalculatedOnFrame = m_CalculatedOnFrame;
			clone->m_Pose = m_Pose;

			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				clone->m_Inputs[i] = m_Inputs[i] ? m_Inputs[i]->Clone(clone->m_Graph, createdNodes) : nullptr;
				// Vars are copied as is because otherwise each node would have its own copy of a variable
				// Making it impossible/hard to make a variable-change affect every node
				clone->m_Variables[i] = m_Variables[i];
			}

			return clone;
		}

	protected:
		Weak<AnimationGraph> m_Graph;
		std::vector<Ref<GraphNode>> m_Inputs;
		std::vector<Ref<GraphVariable>> m_Variables;

		SkeletalPose m_Pose; // Pose that was calculated by the node during the latest update

		size_t m_CalculatedOnFrame = 0;
	};
}
