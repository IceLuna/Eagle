#pragma once

#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	struct SkeletalPose;

	// TODO: currently, it's still tied to Animation Graph. Find a way to fix it (for example, currently Update() returns SkeletalPose)
	class GraphNode
	{
	public:
		GraphNode(size_t numInputs)
		{
			m_Inputs.resize(numInputs);
			m_Variables.resize(numInputs);
		}

		virtual ~GraphNode() = default;

		virtual const SkeletalPose& Update(Timestep ts) = 0;
		virtual Ref<GraphNode> Clone() const = 0;

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

		const std::vector<Ref<GraphNode>>& GetInputNodes() const { return m_Inputs; }
		const std::vector<Ref<GraphVariable>>& GetInputVariables() const { return m_Variables; }

		void Reset()
		{
			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				m_Inputs[i].reset();
				m_Variables[i].reset();
			}
		}

	protected:
		template<typename T, class... Args>
		Ref<T> CloneNode(Args&&... args) const
		{
			Ref<T> clone = MakeRef<T>(std::forward<Args>(args)...);
			clone->m_CalculatedOnFrame = m_CalculatedOnFrame;

			for (size_t i = 0; i < m_Inputs.size(); ++i)
			{
				// TODO: Why vars are copied as is?
				clone->m_Inputs[i] = m_Inputs[i] ? m_Inputs[i]->Clone() : nullptr;
				clone->m_Variables[i] = m_Variables[i]; // Vars are copied as is
			}

			return clone;
		}

	protected:
		std::vector<Ref<GraphNode>> m_Inputs;
		std::vector<Ref<GraphVariable>> m_Variables;
		size_t m_CalculatedOnFrame = 0;
	};
}
