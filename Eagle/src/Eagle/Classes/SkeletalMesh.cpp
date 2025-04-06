#include "egpch.h"
#include "SkeletalMesh.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Animation/AnimationSystem.h"

namespace Eagle
{
	namespace Utils
	{
        static bool IsIKBone(std::string name)
        {
            constexpr std::array IKs = { "_ik", "ik_", "_target", "target_", "_pole", "pole_" };
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return std::tolower(c); });

            for (const auto& ik : IKs)
            {
                if (name.find(ik) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        }

        // Merges bone-colliders based on `minBoneSize`
        static SkeletalRagdollBones MergeBones(float minBoneSize, const BonesMap& boneMap, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& baseTransform = glm::mat4(1.f))
        {
            SkeletalRagdollBones data;
            if (node.bVirtualBone)
                return data;

            if (auto it = currentPose.Bones.find(node.Name); it != currentPose.Bones.end())
            {
                const auto& bone = it->second;
                const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
                data.LocalTransform = baseTransform * boneTransform;
            }
            else
                data.LocalTransform = baseTransform * node.Transformation;

            const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(data.LocalTransform).Location;
            data.AABB.Grow(parentLocation);
            data.Name = node.Name;
            data.Children.reserve(node.Children.size());
            const bool bCanMergeToCurrent = boneMap.find(node.Name) != boneMap.end();
            for (const auto& child : node.Children)
            {
                if (IsIKBone(child.Name))
                    continue;

                SkeletalRagdollBones childData = MergeBones(minBoneSize, boneMap, child, currentPose, data.LocalTransform);
                const glm::vec3 childPos = Math::DecomposeTransformMatrix(childData.LocalTransform).Location;
                data.AABB.Grow(childPos);

                if (bCanMergeToCurrent && childData.AABB.Length() < minBoneSize)
                {
                    // Merge
                    data.Children.insert(data.Children.end(), childData.Children.begin(), childData.Children.end());
                    data.AABB.Grow(childData.AABB);
                }
                else
                    data.Children.emplace_back(childData); // It's big enough
            }

            return data;
        }

        static void SetUserSettings(SkeletalRagdollBones& node, const std::unordered_map<std::string, SkeletalRagdollBones::UserSettings>& ragdollPerBoneSettings)
        {
            auto it = ragdollPerBoneSettings.find(node.Name);
            if (it != ragdollPerBoneSettings.end())
                node.Settings = it->second;

            for (auto& child : node.Children)
                SetUserSettings(child, ragdollPerBoneSettings);
        }
	}

    SkeletalMesh::SkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
        const std::unordered_map<std::string, SkeletalRagdollBones::UserSettings>& ragdollPerBoneSettings, float minRagdollBoneSize, float maxRagdollTwist, float maxRagdollSwing)
        : m_Vertices(vertices)
        , m_IndicesPerMaterial(indicesPerMaterial)
        , m_Skeletal(skeletal)
        , m_AABB(aabb)
        , m_MaterialSlots((uint32_t)m_IndicesPerMaterial.size())
        , m_Materials(m_MaterialSlots)
        , m_MinRagdollBoneSize(minRagdollBoneSize)
        , m_MaxRagdollTwist(maxRagdollTwist)
        , m_MaxRagdollSwing(maxRagdollSwing)
    {
        RegenerateRagdollData(m_MinRagdollBoneSize);
        Utils::SetUserSettings(m_RagdollRoot, ragdollPerBoneSettings);
    }

    SkeletalMesh::SkeletalMesh(const SkeletalMesh& other)
        : m_Vertices(other.m_Vertices)
        , m_IndicesPerMaterial(other.m_IndicesPerMaterial)
        , m_Skeletal(other.m_Skeletal)
        , m_AABB(other.m_AABB)
        , m_MaterialSlots(other.m_MaterialSlots)
        , m_Materials(other.m_Materials)
        , m_RagdollRoot(other.m_RagdollRoot)
        , m_MinRagdollBoneSize(other.m_MinRagdollBoneSize)
        , m_MaxRagdollTwist(other.m_MaxRagdollTwist)
        , m_MaxRagdollSwing(other.m_MaxRagdollSwing)
    {}

    void SkeletalMesh::RegenerateRagdollData(float minBoneSize)
    {
        m_MinRagdollBoneSize = minBoneSize;

        // Fill it with base pose data
        const glm::mat4 rootTransform = glm::mat4(1.f);
        SkeletalPose basePose;
        AnimationSystem::FinalizePose(basePose, m_Skeletal.RootBone, rootTransform);
        m_RagdollRoot = Utils::MergeBones(m_MinRagdollBoneSize, m_Skeletal.BoneInfoMap, m_Skeletal.RootBone, basePose);
    }

    Ref<SkeletalMesh> SkeletalMesh::Create(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
        const std::unordered_map<std::string, SkeletalRagdollBones::UserSettings>& ragdollPerBoneSettings, float minRagdollBoneSize, float maxRagdollTwist, float maxRagdollSwing)
	{
		class LocalSkeletalMesh : public SkeletalMesh
		{
		public:
			LocalSkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
                const std::unordered_map<std::string, SkeletalRagdollBones::UserSettings>& ragdollSettings, float minRagdollBoneSize, float maxRagdollTwist, float maxRagdollSwing)
				: SkeletalMesh(vertices, indicesPerMaterial, skeletal, aabb, ragdollSettings, minRagdollBoneSize, maxRagdollTwist, maxRagdollSwing) {}
		};

		return MakeRef<LocalSkeletalMesh>(vertices, indicesPerMaterial, skeletal, aabb, ragdollPerBoneSettings, minRagdollBoneSize, maxRagdollTwist, maxRagdollSwing);
	}

	Ref<SkeletalMesh> SkeletalMesh::Create(const Ref<SkeletalMesh>& other)
	{
		class LocalSkeletalMesh : public SkeletalMesh
		{
		public:
			LocalSkeletalMesh(const SkeletalMesh& other)
				: SkeletalMesh(other) {}
		};

		return MakeRef<LocalSkeletalMesh>(*other.get());
	}
}
