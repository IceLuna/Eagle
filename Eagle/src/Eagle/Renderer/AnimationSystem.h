#pragma once

namespace Eagle
{
	struct SkeletalMeshAnimation;
	class SkeletalMesh;
	class SkeletalMeshComponent;

	class AnimationSystem
	{
	public:
		// @currentTime - current time of animation to calculate
		static void Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms);
		static void UpdateBlend(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation1, const SkeletalMeshAnimation* animation2, float currentTime1, float currentTime2, float blendAlpha, std::vector<glm::mat4>* outTransforms);
		static void UpdateOnlySpecified(const std::vector<std::string>& requestedNames, const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms);
		static void Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts);
		static void UpdateDifferencePos(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* refAnim, const SkeletalMeshAnimation* sourceAnim, const SkeletalMeshAnimation* targetAnim,
			float currentTime, float currentTimeRef, float currentTimeSrc, float blendAlpha, std::vector<glm::mat4>* outTransforms);

		// Returns new currentTime
		static float StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts);

		// TODO: Copying
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> GetTransforms_RT();
		static const std::unordered_map<uint32_t, std::vector<glm::mat4>>& GetTransforms() { return m_Transforms; }

	private:
		// uint32_t = EntityID
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> m_Transforms;
		static std::unordered_map<uint32_t, std::vector<glm::mat4>> m_Transforms_RT;
	};
}
