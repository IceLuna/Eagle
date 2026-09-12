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
			const auto& meshBoneInfoMap = skeletalInfo.GetBoneInfoMap();
			bool bFoundRootBone = false;

			for (const auto& [boneName, _] : bones)
			{
				const auto it = meshBoneInfoMap.find(boneName);
				if (it == meshBoneInfoMap.end())
					continue;

				const bool bRootNode = boneName == node.GetName();
				if (bRootNode)
				{
					*outBoneName = boneName;
					bFoundRootBone = true;
					break;
				}
			}

			if (bFoundRootBone)
				return true;

			if (node.Children.size() == 1)
				bFoundRootBone = FindRootBoneName(bones, skeletalInfo, node.Children[0], outBoneName);

			return bFoundRootBone;
		}

		static float AngleAroundYAxis(const glm::quat& quat)
		{
			static glm::vec3 xAxis = { 1.0f, 0.0f, 0.0f };
			static glm::vec3 yAxis = { 0.0f, 1.0f, 0.0f };
			auto rotatedOrthogonal = quat * xAxis;
			auto projected = glm::normalize(rotatedOrthogonal - (yAxis * glm::dot(rotatedOrthogonal, yAxis)));
			return acos(glm::dot(xAxis, projected));
		}
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
		glm::vec3 firstLocation = bFromBasePose ? baseRootTr.Location : bone.Locations.front().Location;
		glm::vec3 firstAnimLocation = bone.Locations.front().Location;
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

			const glm::quat offset = glm::quat{ glm::cos(angleY * 0.5f), glm::vec3{ 0.0f, 1.0f, 0.0f } * glm::sin(angleY * 0.5f) };
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

		if (auto itHash = FindBone(Utils::CalculateBoneNameHash(rootBoneName)); IsValid(itHash))
		{
			itHash->second = bone;
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

		for (size_t i = 0; i < bone.Locations.size(); ++i)
		{
			bone.Locations[i].Location = PreRootMotionLocations[i];
		}
		PreRootMotionLocations.clear();

		for (size_t i = 0; i < bone.Rotations.size(); ++i)
		{
			const auto& rootRotation = RootMotion.Rotations[i];
			const float angleY = glm::acos(rootRotation.Rotation.w) * 2.f;

			const glm::quat rotationAppliedToBoneInv = (glm::quat(glm::cos(angleY * 0.5f), glm::vec3{ 0.0f, 1.0f, 0.0f } * glm::sin(angleY * 0.5f)));

			auto& rotationKey = bone.Rotations[i];
			rotationKey.Rotation = rotationAppliedToBoneInv * rotationKey.Rotation;
		}

		for (size_t i = 0; i < bone.Scales.size(); ++i)
		{
			auto& scaleKey = bone.Scales[i];
			scaleKey.Scale = RootMotion.Scales[i].Scale;
		}

		if (auto itHash = FindBone(Utils::CalculateBoneNameHash(rootBoneName)); IsValid(itHash))
		{
			itHash->second = bone;
		}

		RootMotion = {};
		RootMotionType = RootMotionMode::Disabled;
		return true;
	}
}
