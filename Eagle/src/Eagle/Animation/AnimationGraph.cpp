#include "egpch.h"
#include "AnimationGraph.h"
#include "AnimationSystem.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"

namespace Eagle
{
	bool GetVarName(const Ref<GraphVariable>& var, const VariablesMap& map, std::string& outName)
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

	void PopulateVariablesMap(const Ref<GraphNode>& node, const VariablesMap& refVarsMap, VariablesMap& resultMap)
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
					node->SetInput(newVar, i);
					resultMap[name] = newVar;
				}
				else
					node->SetInput(it->second, i);
			}
		}

		const auto& inputNodes = node->GetInputNodes();
		for (size_t i = 0; i < inputNodes.size(); ++i)
		{
			if (!inputNodes[i])
				continue;

			PopulateVariablesMap(inputNodes[i], refVarsMap, resultMap);
		}
	}

	void SetVariablesMap(const Ref<GraphNode>& node, const VariablesMap& refVarsMap, const VariablesMap& varsToUse)
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

		if (auto entry = Cast<AnimationGraphStateMachineEntry>(node))
			entry->SetVariablesToUse(varsToUse);

		const auto& inputNodes = node->GetInputNodes();
		for (size_t i = 0; i < inputNodes.size(); ++i)
		{
			if (!inputNodes[i])
				continue;

			SetVariablesMap(inputNodes[i], refVarsMap, varsToUse);
		}
	}

	AnimationGraph::AnimationGraph(const Ref<const AnimationGraph>& other)
		: m_Skeletal(other->m_Skeletal), m_Pose(other->m_Pose)
	{
		if (other->m_ResultNode)
		{
			m_ResultNode = other->m_ResultNode->Clone();
			EG_CORE_ASSERT(m_ResultNode);
		}
		
		// We can't just copy `other->m_Variables` to `m_Variables` because copied nodes will still refer to old variables.
		// So we need to go through the copied nodes and also updates the variables they use
		PopulateVariablesMap(m_ResultNode, other->m_Variables, m_Variables);
	}

	AnimationGraph::AnimationGraph(const Ref<const AnimationGraph>& other, const VariablesMap& variablesToUse)
		: m_Skeletal(other->m_Skeletal), m_Pose(other->m_Pose), m_Variables(variablesToUse)
	{
		if (other->m_ResultNode)
		{
			m_ResultNode = other->m_ResultNode->Clone();
			EG_CORE_ASSERT(m_ResultNode);
		}

		SetVariablesMap(m_ResultNode, other->m_Variables, m_Variables);
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
	}

	const Ref<SkeletalMesh>& AnimationGraph::GetSkeletal() const
	{
		static Ref<SkeletalMesh> s_Null;
		return m_Skeletal ? m_Skeletal->GetMesh() : s_Null;
	}
}
