#include "egpch.h"
#include "Animation.h"
#include "Eagle/Classes/SkeletalMesh.h"

namespace Eagle
{
    namespace Utils
    {
		static bool FindRootBoneName(const BonesAnimMap& bones, const SkeletalMeshInfo& skeletalInfo, const BoneNode& node, std::string* outBoneName)
		{
			const auto& meshBoneInfoMap = skeletalInfo.BoneInfoMap;
			bool bFoundRootBone = false;

			for (const auto& [boneName, _] : bones)
			{
				const auto it = meshBoneInfoMap.find(boneName);
				if (it == meshBoneInfoMap.end())
					continue;

				const bool bRootNode = boneName == node.Name;
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

    bool SkeletalMeshAnimation::ExtractRootMotion(const SkeletalMeshInfo& skeletalInfo)
    {
		if (HasRootMotion())
		{
			EG_CORE_ERROR("Failed to extract root motion data. It's already extracted!");
            return false;
		}

		std::string rootBoneName;
		const bool bFoundRootBone = Utils::FindRootBoneName(Bones, skeletalInfo, skeletalInfo.RootBone, &rootBoneName);
		if (!bFoundRootBone)
		{
			EG_CORE_ERROR("Failed to extract root motion data. Failed to find the root bone!");
			return false;
		}

		auto it = Bones.find(rootBoneName);
		EG_CORE_ASSERT(it != Bones.end());

		auto& bone = it->second;
		RootMotion.Locations.reserve(bone.Locations.size());
		RootMotion.Rotations.reserve(bone.Rotations.size());
		RootMotion.Scales.reserve(bone.Scales.size());
		RootMotion.BoneID = bone.BoneID;

		// Here we need to add (0) transformation because if an animation has some initial transformation,
		// During lerp, it will jump from (0) to (initial transformation) immediately.
		// If we don't add (0) transformation, initial transformation of an animation will be incorrectly applied.
		// For example, if animation is a sideways walk.
		RootMotion.Locations.emplace_back();
		RootMotion.Rotations.emplace_back();
		RootMotion.Scales.emplace_back();

		glm::vec3 firstLocation = bone.Locations.front().Location;
		for (auto& locationKey : bone.Locations)
		{
			locationKey.Location -= firstLocation; // Offset everything for the root motion data
			RootMotion.Locations.emplace_back(locationKey);
			locationKey.Location = firstLocation;
		}

		for (auto& rotationKey : bone.Rotations)
		{
			const float angleY = Utils::AngleAroundYAxis(rotationKey.Rotation);

			auto& rootKey = RootMotion.Rotations.emplace_back();
			rootKey.Rotation = glm::quat{ glm::cos(angleY * 0.5f), glm::vec3{0.0f, 1.0f, 0.0f} * glm::sin(angleY * 0.5f) };
			rootKey.TimeStamp = rotationKey.TimeStamp;

			rotationKey.Rotation = glm::conjugate(glm::quat(glm::cos(angleY * 0.5f), glm::vec3{ 0.0f, 1.0f, 0.0f } * glm::sin(angleY * 0.5f))) * rotationKey.Rotation;
		}

		for (auto& scaleKey : bone.Scales)
		{
			RootMotion.Scales.emplace_back(scaleKey);
			scaleKey.Scale = glm::vec3(1.f);
		}

		// And here we cancel-out the effect of adding (0)-transformation so that the animation can loop
		RootMotion.Rotations.emplace_back(); // Last rotation is also a unit quat
		RootMotion.Rotations.back().TimeStamp = Duration;

		RootMotion.Scales.emplace_back(); // Last scale is also a unit scale
		RootMotion.Scales.back().TimeStamp = Duration;

		return true;
    }

    bool SkeletalMeshAnimation::RemoveRootMotion(const SkeletalMeshInfo& skeletalInfo)
    {
        if (HasRootMotion() == false)
            return false;

		std::string rootBoneName;
		const bool bFoundRootBone = Utils::FindRootBoneName(Bones, skeletalInfo, skeletalInfo.RootBone, &rootBoneName);
		if (!bFoundRootBone)
		{
			EG_CORE_ERROR("Failed to remove root motion data. Failed to find the root bone!");
			return false;
		}

		auto it = Bones.find(rootBoneName);
		EG_CORE_ASSERT(it != Bones.end());

		auto& bone = it->second;

		for (size_t i = 0; i < bone.Locations.size(); ++i)
		{
			auto& locationKey = bone.Locations[i];
			locationKey.Location += RootMotion.Locations[i + 1].Location; // `+ 1` because root motion always has a unit transformation at index 0
		}

		for (size_t i = 0; i < bone.Rotations.size(); ++i)
		{
			const auto& rootRotation = RootMotion.Rotations[i + 1]; // `+ 1` because root motion always has a unit transformation at index 0
			const float angleY = glm::acos(rootRotation.Rotation.w) * 2.f;

			const glm::quat rotationAppliedToBoneInv = (glm::quat(glm::cos(angleY * 0.5f), glm::vec3{ 0.0f, 1.0f, 0.0f } * glm::sin(angleY * 0.5f)));

			auto& rotationKey = bone.Rotations[i];
			rotationKey.Rotation = rotationAppliedToBoneInv * rotationKey.Rotation;
		}

		for (size_t i = 0; i < bone.Scales.size(); ++i)
		{
			auto& scaleKey = bone.Scales[i];
			scaleKey.Scale = RootMotion.Scales[i + 1].Scale; // `+ 1` because root motion always has a unit transformation at index 0
		}

		RootMotion = {};
		return true;
	}
}
