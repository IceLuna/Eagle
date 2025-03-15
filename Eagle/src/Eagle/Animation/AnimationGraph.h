#pragma once

#include "Animation.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	class SkeletalMesh;
	class AssetSkeletalMesh;
	class GraphNode;
	class AnimationStateMachineGraph;

	class AnimationGraph : virtual public std::enable_shared_from_this<AnimationGraph>
	{
	public:
		AnimationGraph(const Ref<AssetSkeletalMesh>& skeletal) : m_Skeletal(skeletal) {}

		virtual ~AnimationGraph() = default;

		// Results will be written to transforms
		void Update(Timestep ts, std::vector<glm::mat4>* outTransforms);

		// Can be used by other graphs in cases when they need to calculate subgraphs
		const SkeletalPose& Update(Timestep ts);

		// Sets node that will be used to evaluate the whole graph.
		// Animation graph can have only one output
		void SetResult(const Ref<GraphNode>& node) { m_ResultNode = node; }
		const Ref<GraphNode>& GetResult() const { return m_ResultNode; }

		void Reset()
		{
			m_ResultNode.reset();
			m_Variables.clear();
			m_StateMachines.clear();
		}

		void SetVariables(const VariablesMap& vars)
		{
			m_Variables = vars;
		}

		void SetVariables(VariablesMap&& vars)
		{
			m_Variables = std::move(vars);
		}

		uint32_t AddStateMachine(const Ref<AnimationStateMachineGraph>& stateMachine)
		{
			const uint32_t index = (uint32_t)m_StateMachines.size();
			m_StateMachines.push_back(stateMachine);
			return index;
		}

		const Ref<AnimationStateMachineGraph>& GetStateMachine(uint32_t index)
		{
			EG_CORE_ASSERT(index < m_StateMachines.size());
			return m_StateMachines[index];
		}

		virtual void SetVariablesToUse(const VariablesMap& vars);

		const VariablesMap& GetVariables() const { return m_Variables; }
		VariablesMap& GetVariables() { return m_Variables; }

		const Ref<GraphVariable>& GetVariable(const std::string& name) const
		{
			static Ref<GraphVariable> s_Null;
			auto it = m_Variables.find(name);
			if (it != m_Variables.end())
				return it->second;

			return s_Null;
		}

		const Ref<AssetSkeletalMesh>& GetSkeletalAsset() const { return m_Skeletal; }

		const Ref<SkeletalMesh>& GetSkeletal() const;

		const SkeletalPose& GetPose() const { return m_Pose; }

		static Ref<AnimationGraph> Create(const Ref<const AnimationGraph>& other); // This constructor creates its own copy of variables, which is not what we want when it's a subgraph
		static Ref<AnimationGraph> Create(const Ref<const AnimationGraph>& other, const VariablesMap& variablesToUse); // But this constructor uses @variablesToUse, instead of creating its own copy of variables

	protected:
		AnimationGraph() = default;
		void Init(const Ref<const AnimationGraph>& other); // This creates its own copy of variables, which is not what we want when it's a subgraph
		void Init(const Ref<const AnimationGraph>& other, const VariablesMap& variablesToUse); // But this uses @variablesToUse, instead of creating its own copy of variables

	protected:
		Ref<AssetSkeletalMesh> m_Skeletal;
		Ref<GraphNode> m_ResultNode;
		SkeletalPose m_Pose; // Pose that was calculated by the node during the latest update
		std::vector<Ref<AnimationStateMachineGraph>> m_StateMachines;

		// Name - variable
		VariablesMap m_Variables;
	};
}
