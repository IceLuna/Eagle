#pragma once

#include "Eagle/Core/ThreadPool.h"

namespace Eagle
{
	struct SkeletalMeshAnimation;
	class SkeletalMesh;
	class SkeletalMeshComponent;
	struct SkeletalPose;
	struct BoneNode;
	struct SkeletalMeshInfo;
	
	class AnimationSystem
	{
	public:
		[[nodiscard]] static std::unordered_map<uint32_t, std::vector<glm::mat4>> Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts);
		[[nodiscard]] static std::unordered_map<uint32_t, std::vector<glm::mat4>> UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts);

		static void UpdateJustTick(const std::vector<SkeletalMeshComponent*>& meshes, float ts); // Same as Update, but it's better to use this function to potentially save on perf if the rendering is paused

		// @currentTime - current time of animation to calculate
		static void Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms);
		static void UpdateOnlySpecified(const std::vector<std::string>& requestedNames, const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms);
		static void UpdateDifferencePos(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* refAnim, const SkeletalMeshAnimation* sourceAnim, const SkeletalMeshAnimation* targetAnim,
			float currentTime, float currentTimeRef, float currentTimeSrc, float blendAlpha, std::vector<glm::mat4>* outTransforms);

		static void CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose);
		static void ApplyAdditive(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const BoneNode& node, float blendAlpha, SkeletalPose* resultPose);
		static void BlendPoses(const SkeletalPose& pose1, const SkeletalPose& pose2, const BoneNode& node, float blendAlpha, SkeletalPose* outPose);
		static void AnimationClip(const SkeletalMeshAnimation* animation, const BoneNode& node, float currentTime, SkeletalPose* outPose);
		static void FinalizePose(const SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms);

		// Returns new currentTime
		static float StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts, bool bLoop);

		// Returns true if `currentTime` is valid value for the animation
		static bool IsValidTime(const SkeletalMeshAnimation* animation, float currentTime);

	private:
		static ThreadPool s_ThreadPool;

		// uint32_t = EntityID
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> s_Transforms;
	};
}
