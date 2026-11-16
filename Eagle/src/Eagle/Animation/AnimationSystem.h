#pragma once

#include "Eagle/Core/ThreadPool.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	struct SkeletalMeshAnimation;
	class SkeletalMesh;
	class SkeletalMeshComponent;
	class Scene;
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
		// Ragdolls aren't animated here, but by `UpdateRagdolls` which is run after the scripts update.
		// Ragdolls are relative to mesh global transform, and if a script moves it, the ragdoll should use the latest position. That's why ragdolls update is postponed to avoid this one-frame desync.
		// There's no issues for regular skeletal animation because they're stored in relative transforms, so they don't care about the current mesh world transform. Their transforms from the previous frame are used, and they're updated by `UpdateRagdolls`.
		// @deltaTimes. The time step of each mesh (must be the same size as `meshes`)
		// @outRagdolls. Entity IDs of the ragdolls, for `UpdateRagdolls`
		static ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> Update(const std::vector<SkeletalMeshComponent*>& meshes, const std::vector<float>& deltaTimes, bool bApplyRootMotion,
			std::vector<struct AnimationEventData>* outEventsToTrigger = nullptr, std::vector<uint32_t>* outRagdolls = nullptr);
		static ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts, std::vector<uint32_t>* outRagdolls = nullptr);

		// Computes the transforms of ragdolls after the physics and scripts, so that they're drawn where their bodies are in this frame. Called right before rendering.
		// Ragdolls that scripts removed or disabled since `Update` keep their transforms from the previous frame.
		// @ragdolls. Entity IDs from `Update`
		static void UpdateRagdolls(Scene& scene, const std::vector<uint32_t>& ragdolls, ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>>& transformsMap);

		// @deltaTimes. The time step of each system (must be the same size as `systems`)
		static ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>> Update(const std::vector<ParticleSystemComponent*>& systems, const std::vector<float>& deltaTimes, std::vector<struct AnimationEventData>* outEventsToTrigger = nullptr);

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

		static void CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const SkeletalMeshInfo& skeletal, SkeletalPose* resultPose);
		static void ApplyAdditive(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const SkeletalMeshInfo& skeletal, float blendAlpha, SkeletalPose* resultPose);
		static void BlendPoses(const SkeletalPose& pose1, const SkeletalPose& pose2, const SkeletalMeshInfo& skeletal, float blendAlpha, SkeletalPose* outPose);
		static void AnimationClip(const SkeletalMeshInfo& skeletal, const SkeletalMeshAnimation* animation, const BoneNode& node, float currentTime, SkeletalPose* outPose);
		static void FilterPose(const SkeletalPose& pose, const BoneNode& rootNode, const std::string& boneName, bool bIgnoreParentLocation, bool bIgnoreParentRotation, bool bIgnoreParentScale, SkeletalPose* outPose);
		static void FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms);
		static void FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal);
		static void FinalizePoseRagdoll(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms);

		// Returns new currentTime
		static float StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts, bool bLoop);
		static double WrapAnimationTime(double duration, double currentTime, bool bLoop);
		static float WrapAnimationTime(float duration, float currentTime, bool bLoop);

		// Returns true if `currentTime` is valid value for the animation
		static bool IsValidTime(const SkeletalMeshAnimation* animation, float currentTime);

		static void GetEventsToTrigger(const SkeletalMeshAnimation* animation, float prevTime, float curTime, float speed, bool bLoop, std::vector<AnimationEvent>* outEvents);

		// Returns time in seconds until the animation reaches its end (or its start when playing backwards). Always >= 0.
		static float CalculateTimeTillAnimationLoops(const SkeletalMeshAnimation* animation, float currentTime, float playbackSpeed);

	private:
		static ThreadPool s_ThreadPool;

		// uint32_t = EntityID
		static ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> s_Transforms;
		static ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>> s_EmittersTransforms; // SystemID -> Emitter ID -> Its transforms
	};
}
