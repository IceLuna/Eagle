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
		static void Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts);

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
