#include "egpch.h"
#include "Animation.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "AnimationSystem.h"

namespace Eagle
{
    namespace Utils
    {
		static bool FindRootBoneName(const BonesAnimMap& bones, const SkeletalMeshInfo& skeletalInfo, const BoneNode& node, std::string* outBoneName)
		{
			const bool bAnimated = bones.find(node.GetName()) != bones.end();
			const bool bMeshBone = skeletalInfo.IsValid(skeletalInfo.FindBoneInfo(node.GetNameHash()));
			if (bAnimated && bMeshBone)
			{
				*outBoneName = node.GetName();
				return true;
			}

			if (node.Children.size() == 1)
				return FindRootBoneName(bones, skeletalInfo, node.Children[0], outBoneName);

			return false;
		}

		static float AngleAroundYAxis(const glm::quat& quat)
		{
			const glm::vec3 rotatedX = quat * glm::vec3(1.f, 0.f, 0.f);
			return std::atan2(-rotatedX.z, rotatedX.x);
		}
    }

	const BoneNode* SkeletalMeshAnimation::FindRootMotionBone(const SkeletalMeshInfo& skeletalInfo) const
	{
		const BoneNode* node = &skeletalInfo.RootBone;
		while (node)
		{
			const bool bAnimated = FindBone(node->GetNameHash()) != nullptr;
			const bool bMeshBone = skeletalInfo.IsValid(skeletalInfo.FindBoneInfo(node->GetNameHash()));
			if (bAnimated && bMeshBone)
				return node;

			node = node->Children.size() == 1 ? &node->Children[0] : nullptr;
		}
		return nullptr;
	}

    bool SkeletalMeshAnimation::ExtractRootMotion(const SkeletalMeshInfo& skeletalInfo, RootMotionMode mode)
    {
		if (HasRootMotion())
		{
			RemoveRootMotion(skeletalInfo);
		}
		if (mode == RootMotionMode::Disabled)
		{
			return false;
		}

		std::string rootBoneName;
		const bool bFoundRootBone = Utils::FindRootBoneName(m_AnimBones, skeletalInfo, skeletalInfo.RootBone, &rootBoneName);
		if (!bFoundRootBone)
		{
			EG_CORE_ERROR("Failed to extract root motion data. Failed to find the root bone!");
			return false;
		}

		auto it = m_AnimBones.find(rootBoneName);
		EG_CORE_ASSERT(it != m_AnimBones.end());

		auto& bone = it->second;
		if (bone.Locations.empty())
		{
			EG_CORE_ERROR("Failed to extract root motion data. The root bone '{}' doesn't have location keys!", rootBoneName);
			return false;
		}

		RootMotion = {};
		PreRootMotionLocations.clear();
		PreRootMotionLocations.reserve(bone.Locations.size());
		RootMotion.Locations.reserve(bone.Locations.size());
		RootMotion.Rotations.reserve(bone.Rotations.size());
		RootMotion.Scales.reserve(bone.Scales.size());
		RootMotion.BoneID = bone.BoneID;

		bool bFromBasePose = mode == RootMotionMode::BasePose;
		Transform baseRootTr;
		if (bFromBasePose)
		{
			SkeletalPose pose{};
			AnimationSystem::FinalizePose(pose, skeletalInfo.RootBone, glm::mat4(1), skeletalInfo);
			auto itRootTr = pose.FindBone(rootBoneName);
			if (itRootTr != pose.Bones.end())
			{
				baseRootTr = itRootTr->second;
			}
			else
			{
				EG_CORE_ERROR("Failed to extract root motion from the base pose. Couldn't find root bone in the base pose. Falling back to the first animation frame!");
				mode = RootMotionMode::AnimFirstFrame;
				bFromBasePose = false;
			}
		}

		// Animation should preserve its first frame data, and all other frames will be the same as the first one.
		// Root motion will contain the delta between the first and the current animation frame and apply it manually.
		const glm::vec3 firstLocation = bFromBasePose ? baseRootTr.Location : bone.Locations.front().Location;
		const glm::vec3 firstAnimLocation = bone.Locations.front().Location;
		for (auto& locationKey : bone.Locations)
		{
			PreRootMotionLocations.push_back(locationKey.Location);
			locationKey.Location -= firstLocation; // Offset everything for the root motion data
			RootMotion.Locations.emplace_back(locationKey);
			locationKey.Location = firstAnimLocation;
		}

		const float basePoseAngleY = bFromBasePose ? Utils::AngleAroundYAxis(baseRootTr.Rotation.GetQuat()) : 0.0f;
		for (auto& rotationKey : bone.Rotations)
		{
			const float angleY = bFromBasePose ? basePoseAngleY : Utils::AngleAroundYAxis(rotationKey.Rotation);

			const glm::quat offset = glm::angleAxis(angleY, glm::vec3{ 0.0f, 1.0f, 0.0f });
			auto& rootKey = RootMotion.Rotations.emplace_back();
			rootKey.Rotation = offset;
			rootKey.TimeStamp = rotationKey.TimeStamp;

			rotationKey.Rotation = glm::conjugate(offset) * rotationKey.Rotation;
		}

		for (auto& scaleKey : bone.Scales)
		{
			RootMotion.Scales.emplace_back(scaleKey);
			scaleKey.Scale = glm::vec3(1.f);
		}

		RootMotionType = mode;

		return true;
    }

    bool SkeletalMeshAnimation::RemoveRootMotion(const SkeletalMeshInfo& skeletalInfo)
    {
        if (HasRootMotion() == false)
            return false;

		std::string rootBoneName;
		const bool bFoundRootBone = Utils::FindRootBoneName(m_AnimBones, skeletalInfo, skeletalInfo.RootBone, &rootBoneName);
		if (!bFoundRootBone)
		{
			EG_CORE_ERROR("Failed to remove root motion data. Failed to find the root bone!");
			return false;
		}

		auto it = m_AnimBones.find(rootBoneName);
		EG_CORE_ASSERT(it != m_AnimBones.end());

		auto& bone = it->second;

		const size_t numLocations = glm::min(bone.Locations.size(), PreRootMotionLocations.size());
		for (size_t i = 0; i < numLocations; ++i)
		{
			bone.Locations[i].Location = PreRootMotionLocations[i];
		}
		PreRootMotionLocations.clear();

		const size_t numRotations = glm::min(bone.Rotations.size(), RootMotion.Rotations.size());
		for (size_t i = 0; i < numRotations; ++i)
		{
			auto& rotationKey = bone.Rotations[i];
			rotationKey.Rotation = RootMotion.Rotations[i].Rotation * rotationKey.Rotation;
		}

		const size_t numScales = glm::min(bone.Scales.size(), RootMotion.Scales.size());
		for (size_t i = 0; i < numScales; ++i)
		{
			bone.Scales[i].Scale = RootMotion.Scales[i].Scale;
		}

		RootMotion = {};
		RootMotionType = RootMotionMode::Disabled;
		return true;
	}
}
