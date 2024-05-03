#include "egpch.h"
#include "AnimationNodes.h"

#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Animation/AnimationStateMachineGraph.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	// TODO: Remove code duplication
	const SkeletalPose& AnimationGraphNodeOutput::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();

		if (m_Inputs[0])
			m_Pose = m_Inputs[0]->Update(ts);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeStateOutput::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();
		if (m_Inputs[0])
			m_Pose = m_Inputs[0]->Update(ts);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeTransitionOutput::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[0]))
			{
				casted->Update(ts);
				m_bTransition = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Bool)
				m_bTransition = Cast<GraphVariableBool>(var)->Value;
		}
		else
			m_bTransition = false;

		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				m_TransitionTime = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				m_TransitionTime = Cast<GraphVariableFloat>(var)->Value;
		}
		else
			m_TransitionTime = 0.f;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphStateMachineEntry::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();

		if (m_StateMachine)
			m_Pose = m_StateMachine->Update(ts);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	void AnimationGraphStateMachineEntry::SetVariablesToUse(const VariablesMap& vars)
	{
		m_StateMachine->SetVariablesToUse(vars);
	}

	Ref<GraphNode> AnimationGraphStateMachineEntry::Clone() const
	{
		auto clone = AnimationGraphNode::CloneNode<AnimationGraphStateMachineEntry>(m_Graph);
		clone->m_StateMachine = MakeRef<AnimationStateMachineGraph>(m_StateMachine, m_Graph->GetVariables());
		return clone;
	}

	const SkeletalPose& AnimationGraphNodeClip::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		// Used to detect if the animation clip was unused. If so, CurrentTime is reset to 0
		if (currentFrame - m_CalculatedOnFrame > 1)
			CurrentTime = 0.f;

		m_Pose.Reset();

		const auto& skeletal = m_Graph->GetSkeletal();
		const SkeletalMeshAnimation* animation = nullptr;
		float speed = 1.f;
		bool bLoop = true;

		// Anim
		if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Animation)
			{
				auto castedVar = Cast<GraphVariableAnimation>(var);
				if (castedVar->Value)
					animation = castedVar->Value->GetAnimation().get();
			}
		}
		// Speed
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				speed = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				speed = Cast<GraphVariableFloat>(var)->Value;
		}

		// Loop
		if (const auto& input = m_Inputs[2])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(input))
			{
				casted->Update(ts);
				bLoop = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[2])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bLoop = Cast<GraphVariableBool>(var)->Value;
		}

		if (m_LastAnim != animation)
		{
			CurrentTime = 0.f;
			m_LastAnim = animation;
		}

		if (animation)
		{
			if (!AnimationSystem::IsValidTime(animation, CurrentTime))
				CurrentTime = 0.f;

			AnimationSystem::AnimationClip(animation, skeletal->GetSkeletal().RootBone, CurrentTime, &m_Pose);
			CurrentTime = AnimationSystem::StepForwardAnimTime(animation, CurrentTime, ts * speed, bLoop);
		}
		else
			CurrentTime = 0.f;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeBlend::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();
		if (m_Inputs[0] && m_Inputs[1])
		{
			const auto& pose0 = m_Inputs[0];
			const auto& pose1 = m_Inputs[1];
			if (pose0 && pose1)
			{
				const auto& skeletal = m_Graph->GetSkeletal();
				float weight = 0.f;
				if (m_Inputs[2])
				{
					if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[2]))
					{
						casted->Update(ts);
						weight = casted->Result;
					}
				}
				else if (const auto& var = m_Variables[2])
				{
					if (var->GetType() == GraphVariableType::Float)
						weight = glm::clamp(Cast<GraphVariableFloat>(var)->Value, 0.f, 1.f);
				}
				pose0->Update(ts);
				pose1->Update(ts);

				AnimationSystem::BlendPoses(pose0->GetPose(), pose1->GetPose(), skeletal->GetSkeletal().RootBone, weight, &m_Pose);
			}
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeAdditiveBlend::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();
		if (m_Inputs[0] && m_Inputs[1])
		{
			const auto& pose0 = m_Inputs[0];
			const auto& pose1 = m_Inputs[1];
			if (pose0 && pose1)
			{
				const auto& skeletal = m_Graph->GetSkeletal();
				float weight = 0.f;
				if (m_Inputs[2])
				{
					if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[2]))
					{
						casted->Update(ts);
						weight = casted->Result;
					}
				}
				else if (const auto& var = m_Variables[2])
				{
					if (var->GetType() == GraphVariableType::Float)
						weight = glm::clamp(Cast<GraphVariableFloat>(var)->Value, 0.f, 1.f);
				}
				pose0->Update(ts);
				pose1->Update(ts);
				AnimationSystem::ApplyAdditive(pose0->GetPose(), pose1->GetPose(), skeletal->GetSkeletal().RootBone, weight, &m_Pose);
			}
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeCalculateAdditive::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();
		if (m_Inputs[0] && m_Inputs[1])
		{
			const auto& pose0 = m_Inputs[0];
			const auto& pose1 = m_Inputs[1];
			if (pose0 && pose1)
			{
				const auto& skeletal = m_Graph->GetSkeletal();
				pose0->Update(ts);
				pose1->Update(ts);
				AnimationSystem::CalculateAdditivePose(pose0->GetPose(), pose1->GetPose(), skeletal->GetSkeletal().RootBone, &m_Pose);
			}
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeSelectPoseByBool::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();

		bool bValue = false;
		if (m_Inputs[2])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[2]))
			{
				casted->Update(ts);
				bValue = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[2])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue = Cast<GraphVariableBool>(var)->Value;
		}

		if (bValue == false)
		{
			if (m_Inputs[0])
				m_Pose = m_Inputs[0]->Update(ts);
		}
		else
		{
			if (m_Inputs[1])
				m_Pose = m_Inputs[1]->Update(ts);
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeAnd::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		bool bValue1 = false;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[0]))
			{
				casted->Update(ts);
				bValue1 = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue1 = Cast<GraphVariableBool>(var)->Value;
		}

		bool bValue2 = false;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[1]))
			{
				casted->Update(ts);
				bValue2 = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue2 = Cast<GraphVariableBool>(var)->Value;
		}
		bResult = bValue1 && bValue2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeOr::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		bool bValue1 = false;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[0]))
			{
				casted->Update(ts);
				bValue1 = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue1 = Cast<GraphVariableBool>(var)->Value;
		}

		bool bValue2 = false;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[1]))
			{
				casted->Update(ts);
				bValue2 = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue2 = Cast<GraphVariableBool>(var)->Value;
		}
		bResult = bValue1 || bValue2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeXor::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		bool bValue1 = false;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[0]))
			{
				casted->Update(ts);
				bValue1 = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue1 = Cast<GraphVariableBool>(var)->Value;
		}

		bool bValue2 = false;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[1]))
			{
				casted->Update(ts);
				bValue2 = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue2 = Cast<GraphVariableBool>(var)->Value;
		}
		bResult = bValue1 ^ bValue2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeNot::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		bool bValue = false;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeBool>(m_Inputs[0]))
			{
				casted->Update(ts);
				bValue = casted->bResult;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Bool)
				bValue = Cast<GraphVariableBool>(var)->Value;
		}
		bResult = !bValue;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeLess::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		bResult = value1 < value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeLessEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		bResult = value1 <= value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeGreater::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		bResult = value1 > value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeGreaterEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		bResult = value1 >= value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		bResult = value1 == value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeNotEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		bResult = value1 != value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeAdd::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		Result = value1 + value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeSub::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		Result = value1 - value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeMul::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		Result = value1 * value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeDiv::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value1 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value1 = Cast<GraphVariableFloat>(var)->Value;
		}

		float value2 = 0.f;
		if (m_Inputs[1])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[1]))
			{
				casted->Update(ts);
				value2 = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				value2 = Cast<GraphVariableFloat>(var)->Value;
		}
		Result = value1 / value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeSqrt::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value = Cast<GraphVariableFloat>(var)->Value;
		}

		Result = glm::sqrt(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeSin::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value = Cast<GraphVariableFloat>(var)->Value;
		}

		Result = glm::sin(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeCos::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value = Cast<GraphVariableFloat>(var)->Value;
		}

		Result = glm::cos(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeToRad::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value = Cast<GraphVariableFloat>(var)->Value;
		}

		Result = glm::radians(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeToDeg::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		if (m_Inputs[0])
		{
			if (auto casted = Cast<AnimationGraphNodeFloat>(m_Inputs[0]))
			{
				casted->Update(ts);
				value = casted->Result;
			}
		}
		else if (const auto& var = m_Variables[0])
		{
			if (var->GetType() == GraphVariableType::Float)
				value = Cast<GraphVariableFloat>(var)->Value;
		}

		Result = glm::degrees(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
}
