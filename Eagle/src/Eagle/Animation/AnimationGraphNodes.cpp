#include "egpch.h"
#include "AnimationGraphNodes.h"
#include "AnimationGraph.h"
#include "AnimationSystem.h"

#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	const SkeletalPose& AnimationGraphNodeClip::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

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
				auto castedVar = Cast<AnimationGraphVariableAnimation>(var);
				if (castedVar->Value)
					animation = castedVar->Value->GetAnimation().get();
			}
		}
		// Speed
		if (const auto& var = m_Variables[1])
		{
			if (var->GetType() == GraphVariableType::Float)
				speed = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				bLoop = Cast<AnimationGraphVariableBool>(var)->Value;
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
			const auto& skeletal = m_Graph->GetSkeletal();
			float weight = 0.f;
			if (const auto& var = m_Variables[2])
			{
				if (var->GetType() == GraphVariableType::Float)
					weight = glm::clamp(Cast<AnimationGraphVariableFloat>(var)->Value, 0.f, 1.f);
			}
			m_Inputs[0]->Update(ts);
			m_Inputs[1]->Update(ts);
			AnimationSystem::BlendPoses(m_Inputs[0]->GetPose(), m_Inputs[1]->GetPose(), skeletal->GetSkeletal().RootBone, weight, &m_Pose);
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
			const auto& skeletal = m_Graph->GetSkeletal();
			float weight = 0.f;
			if (const auto& var = m_Variables[2])
			{
				if (var->GetType() == GraphVariableType::Float)
					weight = glm::clamp(Cast<AnimationGraphVariableFloat>(var)->Value, 0.f, 1.f);
			}
			m_Inputs[0]->Update(ts);
			m_Inputs[1]->Update(ts);
			AnimationSystem::ApplyAdditive(m_Inputs[0]->GetPose(), m_Inputs[1]->GetPose(), skeletal->GetSkeletal().RootBone, weight, &m_Pose);
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
			const auto& skeletal = m_Graph->GetSkeletal();
			m_Inputs[0]->Update(ts);
			m_Inputs[1]->Update(ts);
			AnimationSystem::CalculateAdditivePose(m_Inputs[0]->GetPose(), m_Inputs[1]->GetPose(), skeletal->GetSkeletal().RootBone, &m_Pose);
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
				bValue = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue1 = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue2 = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue1 = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue2 = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue1 = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue2 = Cast<AnimationGraphVariableBool>(var)->Value;
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
				bValue = Cast<AnimationGraphVariableBool>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value1 = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value2 = Cast<AnimationGraphVariableFloat>(var)->Value;
		}
		Result = value1 / value2;

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
				value = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value = Cast<AnimationGraphVariableFloat>(var)->Value;
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
				value = Cast<AnimationGraphVariableFloat>(var)->Value;
		}

		Result = glm::degrees(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
}
