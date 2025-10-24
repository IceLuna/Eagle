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
		// TODO: I don't like creating a ref each time. Improve it. Here two refs are created: first from `m_Graph.lock()`; second possibly from `GetStateMachine()`
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

		float transitionTime = 0.f;
		{
			if (bValue == false)
				Utils::GetValue(m_Inputs[2], m_Variables[2], ts, &transitionTime); // False pose blend time
			else
				Utils::GetValue(m_Inputs[4], m_Variables[4], ts, &transitionTime); // True pose blend time
		}

		if (bValue != bPrevValue)
		{
			if (!bTransitioning)
				m_CurrentTransitionTime = bValue ? 0.f : transitionTime;
			bTransitioning = true;
		}

		transitionTime = glm::max(transitionTime, 0.f);
		if (transitionTime < 0.001f)
			bTransitioning = false;

		const SkeletalPose* falsePose = nullptr;
		const SkeletalPose* truePose = nullptr;

		if ((bTransitioning || !bValue) && m_Inputs[1])
			falsePose = &m_Inputs[1]->Update(ts);

		if ((bTransitioning || bValue) && m_Inputs[3])
			truePose = &m_Inputs[3]->Update(ts);

		if (bTransitioning)
		{
			const float weight = glm::clamp(m_CurrentTransitionTime / transitionTime, 0.f, 1.f);
			m_CurrentTransitionTime = bValue ? m_CurrentTransitionTime + ts : m_CurrentTransitionTime - ts;

			AnimationSystem::BlendPoses(falsePose ? *falsePose : SkeletalPose{}, truePose ? *truePose : SkeletalPose{}, m_Skeletal->GetSkeletalMeshInfo().RootBone, weight, &m_Pose);
			if (m_CurrentTransitionTime >= transitionTime || m_CurrentTransitionTime <= 0.f)
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

	const SkeletalPose& AnimationGraphNodeAnimVarIsValid::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		Ref<AssetAnimation> value;
		Utils::GetValueFromVariable(m_Variables[0], &value);

		Result = value.operator bool();

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
	
	const SkeletalPose& AnimationGraphNodeBlendSpace::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		float x = 0, y = 0;
		Utils::GetValue(m_Inputs[0], m_Variables[0], ts, &x);
		Utils::GetValue(m_Inputs[1], m_Variables[1], ts, &y);

		AnimationSystem::ClampBlendSpaceInputs(m_BlendSpace, x, y);

		if (m_PrevInputX == std::numeric_limits<float>::infinity())
			m_PrevInputX = x;
		if (m_PrevInputY == std::numeric_limits<float>::infinity())
			m_PrevInputY = y;

		const float blendTime = m_BlendSpace->GetBlendTime();
		if (blendTime > 0.f && (m_PrevInputX != x || m_PrevInputY != y))
		{
			if (bBlending)
			{
				// If we're already blending, reset to prev frame's blend values
				m_XBeforeTransition = m_PrevX;
				m_YBeforeTransition = m_PrevY;
				CalculateDistanceToBlend(x, y, m_PrevX, m_PrevY);

				// Advance animation a bitr. We can't reset it to 0, because it'll basically result in using prev frames state.
				// Which means, animation won't advance while inputs are changing.
				m_CurrentTransitionTime = ts;
			}
			else
			{
				m_XBeforeTransition = m_PrevInputX;
				m_YBeforeTransition = m_PrevInputY;
				CalculateDistanceToBlend(x, y, m_PrevInputX, m_PrevInputY);
				bBlending = true;
				m_CurrentTransitionTime = 0.f;
			}
		}

		m_PrevInputX = x;
		m_PrevInputY = y;

		if (bBlending)
		{
			const auto& hor = m_BlendSpace->GetHorizontalAxis();
			const auto& ver = m_BlendSpace->GetVerticalAxis();

			const float alpha = m_CurrentTransitionTime / blendTime;
			x = m_XBeforeTransition + glm::mix(0.f, m_XDistanceToBlend, alpha);
			y = m_YBeforeTransition + glm::mix(0.f, m_YDistanceToBlend, alpha);

			// Flip to the other side, if it rolled over
			if (x < hor.Min)
				x = float(hor.Max + (x - hor.Min));
			else if (x > hor.Max)
				x = float(hor.Min + (x - hor.Max));
			
			if (y < ver.Min)
				y = float(ver.Max + (y - ver.Min));
			else if (y > ver.Max)
				y = float(ver.Min + (y - ver.Max));
		}

		m_Pose.Reset();
		const bool bSyncEnabled = m_BlendSpace->IsSyncEnabled();
		if (bSyncEnabled)
		{
			Delaunay::Triangle tr;
			glm::dvec3 buv;
			if (AnimationSystem::FindBlendSpaceSampleTriangle(m_BlendSpace, x, y, &tr, &buv))
			{
				const uint32_t highestWeightedAnimIdx =
					buv[0] > buv[1] && buv[0] > buv[2] ? 0 :
					buv[1] > buv[0] && buv[1] > buv[2] ? 1 : 2;
				const BlendSpaceVertex* highestWeighted = (BlendSpaceVertex*)tr.V[highestWeightedAnimIdx].UserData;

				// If sync is enabled, we can't just calculate the final pose.
				// First, we need to check if highest weighted animation has changed.
				// If so, we need to recalculate `CurrentTime` and `PrevTime` according to new heighest weighed animation duration (otherwise it would flicker).
				// For example, if `CurrentTime` is 1.5s (50% of 3s animation) and we notice that the animation has changed,
				// We need to recalculate it to be 50% of the new animation (if it's 1.5s, then `CurrentTime` must be 0.75s). That's what we do here.
				if (m_PrevHighestWeighted != highestWeighted && m_PrevHighestWeighted)
				{
					if (highestWeighted->Animation && m_PrevHighestWeighted->Animation)
					{
						const SkeletalMeshAnimation* curMeshAnim = highestWeighted->Animation->GetAnimation().get();
						const SkeletalMeshAnimation* prevMeshAnim = m_PrevHighestWeighted->Animation->GetAnimation().get();

						// Convert: to Ticks and then to [0; 1] range
						CurrentTime = AnimationSystem::WrapAnimationTime(double(prevMeshAnim->Duration), CurrentTime * prevMeshAnim->TicksPerSecond, true);
						CurrentTime = CurrentTime / prevMeshAnim->Duration;

						// Convert: to Ticks and then to [0; 1] range
						PrevTime = AnimationSystem::WrapAnimationTime(double(prevMeshAnim->Duration), PrevTime * prevMeshAnim->TicksPerSecond, true);
						PrevTime = PrevTime / prevMeshAnim->Duration;

						const double durationInSeconds = double(curMeshAnim->Duration) / curMeshAnim->TicksPerSecond;
						CurrentTime *= durationInSeconds;
						PrevTime *= durationInSeconds;
					}
				}
				AnimationSystem::CalculateBlendSpacePose(m_BlendSpace, tr, buv, PrevTime, CurrentTime, &m_Pose);
				m_PrevHighestWeighted = highestWeighted;
				PrevTime = CurrentTime;
				CurrentTime += ts * highestWeighted->AnimSpeed;
			}
		}
		else
		{
			AnimationSystem::CalculateBlendSpacePose(m_BlendSpace, x, y, PrevTime, CurrentTime, &m_Pose);
			m_PrevHighestWeighted = nullptr;
			PrevTime = CurrentTime;
			CurrentTime += ts;
		}

		m_CalculatedOnFrame = currentFrame;
		m_CurrentTransitionTime += ts;
		m_PrevX = x;
		m_PrevY = y;

		if (bBlending && m_CurrentTransitionTime > blendTime)
		{
			m_CurrentTransitionTime = 0.f;
			bBlending = false;
		}

		return m_Pose;
	}
	
	void AnimationGraphNodeBlendSpace::CalculateDistanceToBlend(float x, float y, float prevX, float prevY)
	{
		const auto& hor = m_BlendSpace->GetHorizontalAxis();
		const auto& ver = m_BlendSpace->GetVerticalAxis();
		const bool bShortestBlend = m_BlendSpace->IsShortestBlendPathEnabled();

		const float distanceX = x - prevX;
		const float distanceY = y - prevY;
		if (bShortestBlend)
		{
			const float minX = glm::min(prevX, x);
			const float maxX = glm::max(prevX, x);
			const float altDistanceX = (x < prevX ? 1.f : -1.f) * ((float(hor.Max) - maxX) + (minX - float(hor.Min)));

			const float minY = glm::min(prevY, y);
			const float maxY = glm::max(prevY, y);
			const float altDistanceY = (y < prevY ? 1.f : -1.f) * ((float(ver.Max) - maxY) + (minY - float(ver.Min)));

			// Choose the shortest path
			m_XDistanceToBlend = glm::abs(distanceX) > glm::abs(altDistanceX) ? altDistanceX : distanceX;
			m_YDistanceToBlend = glm::abs(distanceY) > glm::abs(altDistanceY) ? altDistanceY : distanceY;
		}
		else
		{
			m_XDistanceToBlend = distanceX;
			m_YDistanceToBlend = distanceY;
		}
	}
	
	const SkeletalPose& AnimationGraphNodeIntToFloat::Update(Timestep ts)
	{
		const size_t currentFrame = RenderManager::GetFrameNumber_CPU();
		if (currentFrame <= m_CalculatedOnFrame)
			return m_Pose;

		int value = 0;
		Utils::GetValueFromVariable(m_Variables[0], &value);

		Result = float(value);

		m_CalculatedOnFrame = currentFrame;

		return m_Pose;
	}
}
