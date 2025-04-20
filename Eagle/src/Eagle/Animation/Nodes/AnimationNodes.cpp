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
		static bool GetValueFromInput(const Ref<GraphNode>& input, Timestep ts, T* outValue)
		{
			using GraphType =
				std::conditional_t<std::is_same<bool, T>::value, AnimationGraphNodeBool,
				std::conditional_t<std::is_same<float, T>::value, AnimationGraphNodeFloat,
				std::conditional_t<std::is_same<glm::vec4, T>::value, AnimationGraphNodeVec4,
				void>>>;

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
		static bool GetValueFromVariable(const Ref<GraphVariable>& variable, T* outValue)
		{
			using VariableType =
				std::conditional_t<std::is_same<bool, T>::value, GraphVariableBool,
				std::conditional_t<std::is_same<int, T>::value, GraphVariableInt,
				std::conditional_t<std::is_same<float, T>::value, GraphVariableFloat,
				std::conditional_t<std::is_same<Ref<AssetAnimation>, T>::value, GraphVariableAnimation,
				std::conditional_t<std::is_same<std::string, T>::value, GraphVariableString,
				std::conditional_t<std::is_same<glm::vec4, T>::value, GraphVariableVec4,
				void>>>>>>;

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
			if (GetValueFromInput(input, ts, outValue))
				return true;
			
			if (GetValueFromVariable(variable, outValue))
				return true;

			return false;
		}
	}

	AnimationGraphNode::AnimationGraphNode(const Weak<AnimationGraph>& graph, size_t numInputs)
		: GraphNode(graph, numInputs)
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
		if (!Utils::GetValue(m_Inputs[3], m_Variables[3], ts, &m_bAutoTransition))
			m_bAutoTransition = false;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphStateMachineEntry::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();

		if (const auto& ref = GetStateMachine())
			m_Pose = ref->Update(ts);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const Ref<AnimationStateMachineGraph>& AnimationGraphStateMachineEntry::GetStateMachine() const
	{
		// TODO: I don't like creating a ref each time. Improve it. Here two refs are created: first from `m_Graph.lock()`; second from `GetRootGraph()`
		return m_Graph.lock()->GetStateMachine(m_StateMachineIndex);
	}

	Ref<GraphNode> AnimationGraphStateMachineEntry::Clone(const Weak<AnimationGraph>& newGraph) const
	{
		auto clone = AnimationGraphNode::CloneNode<AnimationGraphStateMachineEntry>(newGraph);
		clone->m_StateMachineIndex = m_StateMachineIndex;
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
		if (Utils::GetValueFromVariable(m_Variables[0], &animationAsset) && animationAsset)
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
			AnimationSystem::AnimationClip(skeletalInfo, animation, skeletalInfo.RootBone, CurrentTime, &m_Pose);
			if (!animation->bInPlace && animation->HasRootMotion())
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

			constexpr float speedDelta = 0.00001f;
			if (speed > speedDelta)
			{
				// Playing forward
				float durationLeft = animation->Duration - CurrentTime;
				m_Pose.TimeTillAnimationLoops = durationLeft / (speed * animation->TicksPerSecond);
			}
			else if (speed < -speedDelta)
			{
				// Playing backwards
				float durationLeft = CurrentTime; // Simplified version of: `Duration - (Duration - CurrentTime)`
				m_Pose.TimeTillAnimationLoops = durationLeft / (speed * animation->TicksPerSecond);
			}
			else
			{
				// Speed is 0
				m_Pose.TimeTillAnimationLoops = FLT_MAX; // Animation is not playing, so we'll never loop
			}
		}
		else
		{
			m_PrevTime = CurrentTime = 0.f;
			m_Pose.TimeTillAnimationLoops = FLT_MAX;
		}

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

		const SkeletalPose* pose0 = nullptr;
		const SkeletalPose* pose1 = nullptr;

		if (m_Inputs[0])
			pose0 = &(m_Inputs[0]->Update(ts));

		if (m_Inputs[1])
			pose1 = &(m_Inputs[1]->Update(ts));

		float weight = 0.f;
		if (Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &weight))
			weight = glm::clamp(weight, 0.f, 1.f);

		const auto& skeletal = GetSkeletal();
		AnimationSystem::BlendPoses(pose0 ? *pose0 : SkeletalPose{}, pose1 ? *pose1 : SkeletalPose{}, skeletal->GetSkeletalMeshInfo().RootBone, weight, &m_Pose);

		if (pose0)
			m_Pose.EventsToTrigger = pose0->GetEventsToTrigger();
		if (pose1)
			m_Pose.EventsToTrigger.insert(m_Pose.EventsToTrigger.end(), pose1->GetEventsToTrigger().begin(), pose1->GetEventsToTrigger().end());

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
			Utils::GetValueFromVariable(m_Variables[1], &boneName);

			bool bIgnoreParentLocation = false;
			Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &bIgnoreParentLocation);
			bool bIgnoreParentRotation = true;
			Utils::GetValue(m_Inputs[3], m_Variables[3], ts, &bIgnoreParentRotation);
			bool bIgnoreParentScale = true;
			Utils::GetValue(m_Inputs[4], m_Variables[4], ts, &bIgnoreParentScale);

			const auto& pose = input->Update(ts);
			AnimationSystem::FilterPose(pose, skeletal->GetSkeletalMeshInfo().RootBone, boneName, bIgnoreParentLocation, bIgnoreParentRotation, bIgnoreParentScale, &m_Pose);
			m_Pose.EventsToTrigger_Pointer = &(pose.GetEventsToTrigger());
		}

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeTransformBone::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();
		if (const auto& input = m_Inputs[0])
		{
			const auto& skeletal = GetSkeletal();
			std::string boneName;
			Utils::GetValueFromVariable(m_Variables[1], &boneName);
			glm::vec4 rotation;
			Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &rotation);

			m_Pose = input->Update(ts);
			auto it = m_Pose.Bones.find(boneName);
			if (it != m_Pose.Bones.end())
			{
				glm::quat q;
				q.x = rotation.x;
				q.y = rotation.y;
				q.z = rotation.z;
				q.w = rotation.w;
				q = glm::normalize(q);

				Transform offset{};
				offset.Rotation = q;

				it->second += offset;
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

	const SkeletalPose& AnimationGraphNodeBlendPoseByBool::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();

		bool bValue = false;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &bValue);

		if (bValue != bPrevValue)
		{
			bTransitioning = true;
			m_CurrentTransitionTime = 0.f;
		}

		float transitionTime = 0.f;
		if (bTransitioning)
		{
			if (bValue == false)
				Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &transitionTime); // False pose blend time
			else
				Utils::GetValue(m_Inputs[4], m_Variables[4], ts, &transitionTime); // True pose blend time

			transitionTime = glm::max(transitionTime, 0.f);
			if (transitionTime < 0.001f)
				bTransitioning = false;
		}

		const SkeletalPose* falsePose = nullptr;
		const SkeletalPose* truePose = nullptr;

		if ((bTransitioning || !bValue) && m_Inputs[1])
			falsePose = &m_Inputs[1]->Update(ts);

		if ((bTransitioning || bValue) && m_Inputs[3])
			truePose = &m_Inputs[3]->Update(ts);

		if (bTransitioning)
		{
			const float weight = glm::clamp(m_CurrentTransitionTime / transitionTime, 0.f, 1.f);
			m_CurrentTransitionTime += ts;

			AnimationSystem::BlendPoses(falsePose ? *falsePose : SkeletalPose{}, truePose ? *truePose : SkeletalPose{}, m_Skeletal->GetSkeletalMeshInfo().RootBone, weight, &m_Pose);
			if (m_CurrentTransitionTime >= transitionTime)
			{
				// Finished transitioning
				bTransitioning = false;
			}
		}
		else
		{
			if (falsePose)
				m_Pose = *falsePose;
			else if (truePose)
				m_Pose = *truePose;
		}

		m_CalculatedOnFrame = currentFrame;
		bPrevValue = bValue;

		return m_Pose;
	}

	const SkeletalPose& AnimationGraphNodeBlendPoseByInt::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		m_Pose.Reset();

		int value = 0;
		if (Utils::GetValueFromVariable(m_Variables[0], &value))
			value = glm::max(value, 0);

		if (value != m_PrevValue)
		{
			bTransitioning = true;
			m_CurrentTransitionTime = 0.f;
			m_ValueBeforeTransition = m_PrevValue;
		}

		// Order is defined by `SpawnBlendPoseByIntNode()`.
		// First one is always the value, and then poses are enumerated: value; pose 0; pose 0 transition time; pose 1; pose 1 transition time; etc...
		// That's why we apply `* 2 + 1/2` to move to correct index
		const uint32_t prevPoseValue = m_ValueBeforeTransition;
		const uint32_t prevPoseIndex = prevPoseValue * 2 + 1;
		const uint32_t prevPoseTransitionTimeIndex = prevPoseValue * 2 + 2;

		const uint32_t currentPoseIndex = value * 2 + 1;
		const uint32_t currentPoseTransitionTimeIndex = value * 2 + 2;

		const bool bValidIndex = currentPoseTransitionTimeIndex < uint32_t(m_Inputs.size());

		float transitionTime = 0.f;
		if (bTransitioning && bValidIndex)
		{
			const uint32_t& idx = currentPoseTransitionTimeIndex;
			Utils::GetValue(m_Inputs[idx], m_Variables[idx], ts, &transitionTime);
			transitionTime = glm::max(transitionTime, 0.f);
			if (transitionTime < 0.001f)
				bTransitioning = false;
		}

		const SkeletalPose* prevPose = nullptr;
		const SkeletalPose* currentPose = nullptr;

		const bool bValidPrevIndex = prevPoseTransitionTimeIndex < uint32_t(m_Inputs.size());
		if (bTransitioning && bValidPrevIndex && m_Inputs[prevPoseIndex])
		{
			prevPose = &m_Inputs[prevPoseIndex]->Update(ts);
		}

		if (bValidIndex && m_Inputs[currentPoseIndex])
		{
			currentPose = &m_Inputs[currentPoseIndex]->Update(ts);
		}

		if (bTransitioning)
		{
			const float weight = glm::clamp(m_CurrentTransitionTime / transitionTime, 0.f, 1.f);
			m_CurrentTransitionTime += ts;

			AnimationSystem::BlendPoses(prevPose ? *prevPose : SkeletalPose{}, currentPose ? *currentPose : SkeletalPose{}, m_Skeletal->GetSkeletalMeshInfo().RootBone, weight, &m_Pose);
			if (m_CurrentTransitionTime >= transitionTime)
			{
				// Finished transitioning
				bTransitioning = false;
			}
		}
		else if (currentPose)
		{
			m_Pose = *currentPose;
		}

		m_CalculatedOnFrame = currentFrame;
		m_PrevValue = value;

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

	const SkeletalPose& AnimationGraphNodeAbs::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

		Result = glm::abs(value);

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
	
	const SkeletalPose& AnimationGraphNodeMapRange::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float value = 0.f;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &value);

		float minA = 0.f;
		float maxA = 0.f;
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &minA);
		Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &maxA);

		float minB = 0.f;
		float maxB = 0.f;
		Utils::GetValue(m_Inputs[3], m_Variables[3], ts, &minB);
		Utils::GetValue(m_Inputs[4], m_Variables[4], ts, &maxB);

		float alpha = glm::clamp((value - minA) / (maxA - minA), 0.f, 1.f);
		Result = alpha * (maxB - minB) + minB;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
	
	const SkeletalPose& AnimationGraphNodeEulerToQuat::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		glm::vec3 euler;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &euler[0]);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &euler[1]);
		Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &euler[2]);

		const glm::quat q = Rotator::FromEulerAngles(euler).GetQuat();
		Result.x = q.x;
		Result.y = q.y;
		Result.z = q.z;
		Result.w = q.w;

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
	
	const SkeletalPose& AnimationGraphNodeCachePose::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		if (m_Inputs[0])
			m_Pose = m_Inputs[0]->Update(ts);
		else
			m_Pose.Reset();

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
}
