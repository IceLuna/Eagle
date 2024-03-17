#include "egpch.h"
#include "AnimationGraph.h"
#include "AnimationGraphNodes.h"
#include "AnimationSystem.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	bool GetVarName(const Ref<AnimationGraphVariable>& var, const VariablesMap& map, std::string& outName)
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

	static void PopulateVariablesMap(const Ref<AnimationGraphNode>& node, const Ref<AnimationGraphNode>& refNode, const VariablesMap& refVarsMap, VariablesMap& resultMap)
	{
		if (!node)
			return;

		const auto& refInputVars = refNode->GetInputVariables();
		const auto& inputVars = node->GetInputVariables();
		for (size_t i = 0; i < refInputVars.size(); ++i)
		{
			const auto& var = refInputVars[i];
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

		const auto& refInputNodes = refNode->GetInputNodes();
		const auto& inputNodes = node->GetInputNodes();
		for (size_t i = 0; i < inputNodes.size(); ++i)
			PopulateVariablesMap(inputNodes[i], refInputNodes[i], refVarsMap, resultMap);
	}

	AnimationGraph::AnimationGraph(const Ref<AnimationGraph>& other)
		: m_Skeletal(other->m_Skeletal)
	{
		m_ResultNode = other->m_ResultNode ? other->m_ResultNode->Clone() : nullptr;
		
		PopulateVariablesMap(m_ResultNode, other->m_ResultNode, other->m_Variables, m_Variables);
	}

	void AnimationGraph::Update(Timestep ts, std::vector<glm::mat4>* outTransforms)
	{
		const auto& skeletal = m_Skeletal->GetMesh()->GetSkeletal();
		glm::mat4 rootTransform = glm::mat4(1.f);
		const SkeletalPose* result = nullptr;
		if (m_ResultNode)
			result = &m_ResultNode->Update(ts);

		if (outTransforms)
			AnimationSystem::FinalizePose(result ? *result : SkeletalPose{}, skeletal.RootBone, rootTransform, skeletal, *outTransforms);
	}

	void AnimationGraph::SetOutput(const Ref<AnimationGraphNode>& node)
	{
		m_ResultNode = node;
	}
	
	const Ref<SkeletalMesh>& AnimationGraph::GetSkeletal() const
	{
		static Ref<SkeletalMesh> s_Null;
		return m_Skeletal ? m_Skeletal->GetMesh() : s_Null;
	}
}
