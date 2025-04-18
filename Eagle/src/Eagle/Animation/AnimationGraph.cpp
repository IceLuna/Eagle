#include "egpch.h"
#include "AnimationGraph.h"
#include "AnimationSystem.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"
#include "Eagle/Animation/AnimationStateMachineGraph.h"
#include "Eagle/Animation/AnimationStateGraph.h"

namespace Eagle
{
	static bool GetVarName(const Ref<GraphVariable>& var, const VariablesMap& map, std::string& outName)
	{
		for (const auto& [name, mapVar] : map)
		{
			if (var == mapVar)
			{
				outName = name;
				return true;
			}
		}
		return false;
	}

	// @bUpdateInput. We don't always want to update/replace input of the node with a copied var. For example, when parsing used vars of state machines
	static void PopulateVariablesMap_Internal(const Ref<GraphNode>& node, const VariablesMap& refVarsMap, VariablesMap& resultMap, bool bUpdateInput = true)
	{
		if (!node)
			return;

		const auto& inputVars = node->GetInputVariables();
		for (size_t i = 0; i < inputVars.size(); ++i)
		{
			const auto& var = inputVars[i];
			if (!var)
				continue;

			std::string name;
			if (GetVarName(var, refVarsMap, name))
			{
				auto it = resultMap.find(name);
				if (it == resultMap.end())
				{
					auto newVar = CopyVarByType(var);
					resultMap[name] = newVar;
					if (bUpdateInput)
						node->SetInput(newVar, i);
				}
				else if (bUpdateInput)
					node->SetInput(it->second, i);
			}
		}

		const auto& inputNodes = node->GetInputNodes();
		for (size_t i = 0; i < inputNodes.size(); ++i)
		{
			if (!inputNodes[i])
				continue;

			PopulateVariablesMap_Internal(inputNodes[i], refVarsMap, resultMap, bUpdateInput);
		}
	}

	static void PopulateVariablesMap_Internal(const Ref<AnimationStateMachineGraph>& stateMachine, const VariablesMap& refVarsMap, VariablesMap& resultMap)
	{
		// Cache is needed to avoid processing same graphs.
		std::unordered_set<Ref<AnimationGraph>> cache;

		const auto& states = stateMachine->GetStates();
		for (const auto& state : states)
		{
			if (cache.find(state) == cache.end())
			{
				PopulateVariablesMap_Internal(state->GetResult(), refVarsMap, resultMap, false);
				cache.emplace(state);
			}

			const auto& connections = state->GetConnections();
			for (const auto& connection : connections)
			{
				if (connection.ConnectedTo)
				{
					if (cache.find(connection.ConnectedTo) == cache.end())
					{
						PopulateVariablesMap_Internal(connection.ConnectedTo->GetResult(), refVarsMap, resultMap, false);
						cache.emplace(connection.ConnectedTo);
					}
				}

				if (connection.Transition)
				{
					if (cache.find(connection.Transition) == cache.end())
					{
						PopulateVariablesMap_Internal(connection.Transition->GetResult(), refVarsMap, resultMap, false);
						cache.emplace(connection.Transition);
					}
				}
			}
		}
	}

	static void PopulateVariablesMap(const Ref<GraphNode>& node, const std::vector<Ref<AnimationStateMachineGraph>>& stateMachines, const VariablesMap& refVarsMap, VariablesMap& resultMap)
	{
		PopulateVariablesMap_Internal(node, refVarsMap, resultMap);
		for (const auto& stateMachine : stateMachines)
			PopulateVariablesMap_Internal(stateMachine, refVarsMap, resultMap);
	}

	static void SetVariablesMap(const Ref<GraphNode>& node, const VariablesMap& refVarsMap, const VariablesMap& varsToUse)
	{
		if (!node)
			return;

		const auto& inputVars = node->GetInputVariables();
		for (size_t i = 0; i < inputVars.size(); ++i)
		{
			const auto& var = inputVars[i];
			if (!var)
				continue;

			std::string name;
			if (GetVarName(var, refVarsMap, name))
			{
				auto it = varsToUse.find(name);
				if (it != varsToUse.end())
				{
					node->SetInput(it->second, i);
				}
			}
		}

		const auto& inputNodes = node->GetInputNodes();
		for (size_t i = 0; i < inputNodes.size(); ++i)
		{
			if (!inputNodes[i])
				continue;

			SetVariablesMap(inputNodes[i], refVarsMap, varsToUse);
		}
	}

	void AnimationGraph::Update(Timestep ts, std::vector<glm::mat4>* outTransforms)
	{
		Update(ts); // Updates `m_Pose`

		if (outTransforms)
		{
			const auto& skeletalInfo = m_Skeletal->GetMesh()->GetSkeletalMeshInfo();
			constexpr glm::mat4 rootTransform = glm::mat4(1.f);
			AnimationSystem::FinalizePose(m_Pose, skeletalInfo.RootBone, rootTransform, skeletalInfo, *outTransforms);
		}
	}

	const SkeletalPose& AnimationGraph::Update(Timestep ts)
	{
		m_Pose.Reset();
		if (m_ResultNode)
			m_Pose = m_ResultNode->Update(ts);

		return m_Pose;
	}
	
	void AnimationGraph::SetVariablesToUse(const VariablesMap& vars)
	{
		SetVariablesMap(m_ResultNode, m_Variables, vars);
		m_Variables = vars;

		for (const auto& stateMachine : m_StateMachines)
		{
			stateMachine->SetVariablesToUse(m_Variables);
		}
	}

	const Ref<SkeletalMesh>& AnimationGraph::GetSkeletal() const
	{
		static Ref<SkeletalMesh> s_Null;
		return m_Skeletal ? m_Skeletal->GetMesh() : s_Null;
	}
	
	Ref<AnimationGraph> AnimationGraph::Create(const Ref<const AnimationGraph>& other)
	{
		class LocalAnimationGraph : public AnimationGraph
		{
		public:
			LocalAnimationGraph() = default;
		};

		auto result = MakeRef<LocalAnimationGraph>();
		result->Init(other);
		return result;
	}
	
	Ref<AnimationGraph> AnimationGraph::CreateSubgraph(const Ref<AnimationGraph>& root, const Ref<const AnimationGraph>& other, const VariablesMap& variablesToUse)
	{
		class LocalAnimationGraph : public AnimationGraph
		{
		public:
			LocalAnimationGraph() = default;

			friend class AnimationGraph;
		};

		auto result = MakeRef<LocalAnimationGraph>();
		result->SetRootGraph(root);
		result->Init(other, variablesToUse);
		return result;
	}
	
	void AnimationGraph::Init(const Ref<const AnimationGraph>& other)
	{
		m_Skeletal = other->m_Skeletal;
		m_Pose = other->m_Pose;

		Ref<AnimationGraph> sharedThis = shared_from_this();

		if (other->m_ResultNode)
		{
			m_ResultNode = other->m_ResultNode->Clone(sharedThis);
			EG_CORE_ASSERT(m_ResultNode);
		}

		// We can't just copy `other->m_Variables` to `m_Variables` because copied nodes will still refer to old variables.
		// So we need to go through the copied nodes and also updates the variables they use
		PopulateVariablesMap(m_ResultNode, other->m_StateMachines, other->m_Variables, m_Variables);

		for (const auto& stateMachine : other->m_StateMachines)
		{
			m_StateMachines.emplace_back(MakeRef<AnimationStateMachineGraph>(sharedThis, stateMachine, m_Variables));
		}
	}
	
	void AnimationGraph::Init(const Ref<const AnimationGraph>& other, const VariablesMap& variablesToUse)
	{
		m_Skeletal = other->m_Skeletal;
		m_Pose = other->m_Pose;
		m_Variables = variablesToUse;

		Ref<AnimationGraph> sharedThis = shared_from_this();

		if (other->m_ResultNode)
			m_ResultNode = other->m_ResultNode->Clone(sharedThis);

		SetVariablesMap(m_ResultNode, other->m_Variables, m_Variables);

		for (const auto& stateMachine : other->m_StateMachines)
		{
			m_StateMachines.emplace_back(MakeRef<AnimationStateMachineGraph>(sharedThis, stateMachine, m_Variables));
		}
	}
}
