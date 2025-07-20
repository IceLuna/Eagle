#pragma once

#include "Eagle/Core/ThreadPool.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	struct SkeletalMeshAnimation;
	class SkeletalMesh;
	class SkeletalMeshComponent;
	class ParticleSystemComponent;
	struct SkeletalPose;
	struct BoneNode;
	struct SkeletalMeshInfo;
	struct AnimationEvent;
	class AssetAnimationBlendSpace;
	struct BlendSpaceVertex;

	namespace Delaunay
	{
		struct Triangle;
	}
	
	class AnimationSystem
	{
	public:
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts, bool bApplyRootMotion);
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts);
		static std::unordered_map<GUID, std::unordered_map<GUID, std::vector<glm::mat4>>> Update(const std::vector<ParticleSystemComponent*>& systems, float ts);

		// @currentTime - current time of animation to calculate
		static void Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms, SkeletalPose* outPose);

		[[nodiscard]] static Transform CalculateRootMotion(const SkeletalMeshAnimation* animation, float currentTime, float prevTime, float playbackSpeed, Timestep ts, Transform* outTotalRootMotion);
		static void ApplyRootMotion(SkeletalMeshComponent* mesh, const Transform& totalRootMotion, Transform rootMotion);

		// Returns highest weighted vertex
		static const BlendSpaceVertex* CalculateBlendSpacePose(const Ref<AssetAnimationBlendSpace>& blendSpace, float x, float y, double prevTimeSeconds, double currentTimeSeconds, SkeletalPose* resultPose);
		static const BlendSpaceVertex* CalculateBlendSpacePose(const Ref<AssetAnimationBlendSpace>& blendSpace, const Delaunay::Triangle& tr, const glm::dvec3& buv, double prevTimeSeconds, double currentTimeSeconds, SkeletalPose* resultPose);

		// Clamps X/Y input to axes extremums
		static void ClampBlendSpaceInputs(const Ref<AssetAnimationBlendSpace>& blendSpace, float& x, float& y);

		// Finds triangle that should be used for interpolation.
		// Returns true on success.
		static bool FindBlendSpaceSampleTriangle(const Ref<AssetAnimationBlendSpace>& blendSpace, float x, float y, Delaunay::Triangle* outTriangle, glm::dvec3* outBUV);

		static void CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose);
		static void ApplyAdditive(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const BoneNode& node, float blendAlpha, SkeletalPose* resultPose);
		static void BlendPoses(const SkeletalPose& pose1, const SkeletalPose& pose2, const BoneNode& node, float blendAlpha, SkeletalPose* outPose);
		static void AnimationClip(const SkeletalMeshInfo& skeletal, const SkeletalMeshAnimation* animation, const BoneNode& node, float currentTime, SkeletalPose* outPose);
		static void FilterPose(const SkeletalPose& pose, BoneNode& node, const std::string& boneName, bool bIgnoreParentLocation, bool bIgnoreParentRotation, bool bIgnoreParentScale, SkeletalPose* outPose);
		static void FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms);
		static void FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal);
		static void FinalizePoseRagdoll(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms);

		// Returns new currentTime
		static float StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts, bool bLoop);
		static double WrapAnimationTime(double duration, double currentTime, bool bLoop);
		static float WrapAnimationTime(float duration, float currentTime, bool bLoop);

		// Returns true if `currentTime` is valid value for the animation
		static bool IsValidTime(const SkeletalMeshAnimation* animation, float currentTime);

		static void GetEventsToTrigger(const SkeletalMeshAnimation* animation, float prevTime, float curTime, float prevSpeed, float curSpeed, std::vector<AnimationEvent>* outEvents);

	private:
		static ThreadPool s_ThreadPool;

		// uint32_t = EntityID
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> s_Transforms;
		static std::unordered_map<GUID, std::unordered_map<GUID, std::vector<glm::mat4>>> s_EmittersTransforms; // SystemID -> Emitter ID -> Its transforms
	};
}
