#include "egpch.h"
#include "AnimationNodes.h"

#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Animation/AnimationStateMachineGraph.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Asset/Asset.h"

#include <type_traits>

namespace Eagle
{
	namespace Utils
	{
		template <typename T>
		static bool GetValue(const Ref<GraphNode>& input, Timestep ts, T* outValue)
		{
			using GraphType =
				std::conditional_t<std::is_same<bool, T>::value, AnimationGraphNodeBool,
				std::conditional_t<std::is_same<float, T>::value, AnimationGraphNodeFloat,
				void>>;

			if (input)
			{
				if (auto casted = Cast<GraphType>(input))
				{
					casted->Update(ts);
					*outValue = casted->Result;
					return true;
				}
			}

			return false;
		}

		// Return true if success
		template <typename T>
		static bool GetValue(const Ref<GraphVariable>& variable, T* outValue)
		{
			using VariableType =
				std::conditional_t<std::is_same<bool, T>::value, GraphVariableBool,
				std::conditional_t<std::is_same<float, T>::value, GraphVariableFloat,
				std::conditional_t<std::is_same<Ref<AssetAnimation>, T>::value, GraphVariableAnimation,
				std::conditional_t<std::is_same<std::string, T>::value, GraphVariableString,
				void>>>>;

			if (variable)
			{
				if (auto casted = Cast<VariableType>(variable))
				{
					*outValue = casted->Value;
					return true;
				}
			}

			return false;
		}

		// Return true if success
		template <typename T>
		static bool GetValue(const Ref<GraphNode>& input, const Ref<GraphVariable>& variable, Timestep ts, T* outValue)
		{
			if (GetValue(input, ts, outValue))
				return true;
			
			if (GetValue(variable, outValue))
				return true;

			return false;
		}
	}

	AnimationGraphNode::AnimationGraphNode(const Weak<AnimationGraph>&graph, size_t numInputs)
		: GraphNode(numInputs)
		, m_Graph(graph)
		, m_Skeletal(graph.lock()->GetSkeletal())
	{
	}

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

		if (!Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &m_bTransition))
			m_bTransition = false;
		if (!Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &m_TransitionTime))
			m_TransitionTime = 0.f;
		if (!Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &m_bUseSmoothTransition))
			m_bUseSmoothTransition = true;

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
		clone->m_StateMachine = MakeRef<AnimationStateMachineGraph>(m_StateMachine, m_Graph.lock()->GetVariables());
		return clone;
	}

	const SkeletalPose& AnimationGraphNodeClip::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		// Used to detect if the animation clip was unused. If so, CurrentTime is reset to 0
		if (currentFrame - m_CalculatedOnFrame > 1)
			m_PrevTime = CurrentTime = 0.f;

		m_Pose.Reset();

		const auto& skeletal = GetSkeletal();
		const SkeletalMeshAnimation* animation = nullptr;
		float speed = 1.f;
		bool bLoop = true;

		// Anim
		Ref<AssetAnimation> animationAsset;
		if (Utils::GetValue(m_Variables[0], &animationAsset) && animationAsset)
			animation = animationAsset->GetAnimation().get();
		
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &speed);
		Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &bLoop);

		if (m_LastAnim != animation)
		{
			m_PrevTime = CurrentTime = 0.f;
			m_LastAnim = animation;
		}

		if (animation)
		{
			if (!AnimationSystem::IsValidTime(animation, CurrentTime))
				m_PrevTime = CurrentTime = 0.f;

			const auto& skeletalInfo = skeletal->GetSkeletalMeshInfo();
			AnimationSystem::AnimationClip(animation, skeletalInfo.RootBone, CurrentTime, &m_Pose);
			if (animation->HasRootMotion())
			{
				if (m_PrevSpeed < 0 && speed > 0 || speed < 0 && m_PrevSpeed > 0) // If speed changed signs
					std::swap(CurrentTime, m_PrevTime);
				if (speed == 0.f && m_PrevSpeed != speed) // If speed stoped
					m_PrevTime = CurrentTime;

				m_Pose.SetRootMotion(AnimationSystem::CalculateRootMotion(animation, CurrentTime, m_PrevTime, speed, ts, &m_Pose.TotalRootMotion));
			}
			m_PrevTime = CurrentTime;
			CurrentTime = AnimationSystem::StepForwardAnimTime(animation, CurrentTime, ts * speed, bLoop);
			AnimationSystem::GetEventsToTrigger(animation, m_PrevTime, CurrentTime, m_PrevSpeed, speed, &m_Pose.EventsToTrigger);
		}
		else
			m_PrevTime = CurrentTime = 0.f;

		m_CalculatedOnFrame = currentFrame;
		m_PrevSpeed = speed;

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
			const auto& input0 = m_Inputs[0];
			const auto& input1 = m_Inputs[1];
			if (input0 && input1)
			{
				const auto& skeletal = GetSkeletal();
				float weight = 0.f;
				if (Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &weight))
					weight = glm::clamp(weight, 0.f, 1.f);

				weight = glm::clamp(weight, 0.f, 1.f);
				const auto& pose0 = input0->Update(ts);
				const auto& pose1 = input1->Update(ts);

				AnimationSystem::BlendPoses(pose0, pose1, skeletal->GetSkeletalMeshInfo().RootBone, weight, &m_Pose);

				m_Pose.EventsToTrigger = pose0.GetEventsToTrigger();
				m_Pose.EventsToTrigger.insert(m_Pose.EventsToTrigger.end(), pose1.GetEventsToTrigger().begin(), pose1.GetEventsToTrigger().end());
			}
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeFilterBones::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();
		if (const auto& input = m_Inputs[0])
		{
			const auto& skeletal = GetSkeletal();
			std::string boneName;
			Utils::GetValue(m_Variables[1], &boneName);

			const auto& pose = input->Update(ts);
			AnimationSystem::FilterPose(pose, skeletal->GetSkeletalMeshInfo().RootBone, boneName, &m_Pose);
			m_Pose.EventsToTrigger_Pointer = &(pose.GetEventsToTrigger());
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
			const auto& input0 = m_Inputs[0];
			const auto& input1 = m_Inputs[1];
			if (input0 && input1)
			{
				const auto& skeletal = GetSkeletal();
				float weight = 0.f;
				if (Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &weight))
					weight = glm::clamp(weight, 0.f, 1.f);

				const auto& pose0 = input0->Update(ts);
				const auto& pose1 = input1->Update(ts);
				AnimationSystem::ApplyAdditive(pose0, pose1, skeletal->GetSkeletalMeshInfo().RootBone, weight, &m_Pose);

				m_Pose.EventsToTrigger = pose0.GetEventsToTrigger();
				m_Pose.EventsToTrigger.insert(m_Pose.EventsToTrigger.end(), pose1.GetEventsToTrigger().begin(), pose1.GetEventsToTrigger().end());
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
			const auto& input0 = m_Inputs[0];
			const auto& input1 = m_Inputs[1];
			if (input0 && input1)
			{
				const auto& skeletal = GetSkeletal();
				const auto& pose0 = input0->Update(ts);
				const auto& pose1 = input1->Update(ts);
				AnimationSystem::CalculateAdditivePose(pose0, pose1, skeletal->GetSkeletalMeshInfo().RootBone, &m_Pose);

				m_Pose.EventsToTrigger = pose0.GetEventsToTrigger();
				m_Pose.EventsToTrigger.insert(m_Pose.EventsToTrigger.end(), pose1.GetEventsToTrigger().begin(), pose1.GetEventsToTrigger().end());
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
		Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &bValue);

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

		Result = false;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &Result);

		if (Result)
		{
			bool bValue2 = false;
			Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &bValue2);

			Result = bValue2;
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeOr::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		bool bValue1 = false;
		bool bValue2 = false;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &bValue1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &bValue2);

		Result = bValue1 || bValue2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeXor::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		bool bValue1 = false;
		bool bValue2 = false;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &bValue1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &bValue2);

		Result = bValue1 ^ bValue2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeNot::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		Result = true; // Set to true, so that if 'GetValue' fails, to make `Result` be `false` after the inversion
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &Result);

		Result = !Result;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeLess::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

		Result = value1 < value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeLessEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

		Result = value1 <= value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeGreater::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

		Result = value1 > value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeGreaterEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

		Result = value1 >= value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

		Result = value1 == value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeNotEqual::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

		Result = value1 != value2;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeAdd::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value1 = 0.f;
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

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
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

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
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

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
		float value2 = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value1);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &value2);

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
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

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
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

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
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

		Result = glm::cos(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeASin::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

		Result = glm::asin(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeACos::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

		Result = glm::acos(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeToRad::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

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
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

		Result = glm::degrees(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
}
