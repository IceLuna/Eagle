#include "egpch.h"
#include "AnimationGraph.h"
#include "AnimationGraphNodes.h"
#include "AnimationSystem.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	AnimationGraph::AnimationGraph(const Ref<AnimationGraph>& other)
		: m_Skeletal(other->m_Skeletal)
	{
		m_ResultNode = other->m_ResultNode ? other->m_ResultNode->Clone() : nullptr;
		for (const auto& [name, var] : other->m_Variables)
			m_Variables[name] = CopyVarByType(var);
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
