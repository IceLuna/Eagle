#pragma once

#include "Animation.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	class SkeletalMesh;
	class AssetSkeletalMesh;
	class GraphNode;

	using VariablesMap = std::map<std::string, Ref<GraphVariable>>;
	class AnimationGraph
	{
	public:
		AnimationGraph(const Ref<AssetSkeletalMesh>& skeletal) : m_Skeletal(skeletal) {}
		AnimationGraph(const Ref<AnimationGraph>& other);

		// Results will be written to transforms
		void Update(Timestep ts, std::vector<glm::mat4>* outTransforms);

		// Sets node that will be used to evaluate the whole graph.
		// Animation graph can have only one output
		void SetOutput(const Ref<GraphNode>& node);

		void Reset()
		{
			m_ResultNode.reset();
			m_Variables.clear();
		}

		void SetVariables(const VariablesMap& vars)
		{
			m_Variables = vars;
		}

		void SetVariables(VariablesMap&& vars)
		{
			m_Variables = std::move(vars);
		}

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

	private:
		Ref<AssetSkeletalMesh> m_Skeletal;
		Ref<GraphNode> m_ResultNode;

		// Name - variable
		VariablesMap m_Variables;
	};
}
