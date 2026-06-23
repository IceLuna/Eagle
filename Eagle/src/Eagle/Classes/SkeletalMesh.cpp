#include "egpch.h"
#include "SkeletalMesh.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Asset/Asset.h"

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
        static SkeletalRagdollBone MergeBones(float minBoneSize, const BonesMap& boneMap, const BoneNode& node, const glm::mat4& baseTransform = glm::mat4(1.f))
        {
            SkeletalRagdollBone data;
            if (node.bVirtualBone)
                return data;

#if 0 // We dont need to account for the curret pose here
            if (auto it = currentPose.FindBone(node.Name); it != currentPose.Bones.end())
            {
                const auto& bone = it->second;
                const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
                data.LocalTransform = baseTransform * boneTransform;
            }
            else
#endif
            {
                data.LocalTransform = baseTransform * node.Transformation;
            }

            const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(data.LocalTransform).Location;
            data.AABB.Grow(parentLocation);
            data.Name = node.GetName();
            data.Children.reserve(node.Children.size());
            const bool bCanMergeToCurrent = boneMap.find(node.GetName()) != boneMap.end();
            for (const auto& child : node.Children)
            {
                if (child.bVirtualBone || IsIKBone(child.GetName()))
                    continue;

                SkeletalRagdollBone childData = MergeBones(minBoneSize, boneMap, child, data.LocalTransform);
                const glm::vec3 childPos = Math::DecomposeTransformMatrix(childData.LocalTransform).Location;
                data.AABB.Grow(childPos);

                if (bCanMergeToCurrent && childData.AABB.Length() < minBoneSize)
                {
                    // Merge
                    data.Children.insert(data.Children.end(), childData.Children.begin(), childData.Children.end());
                    data.AABB.Grow(childData.AABB);
                }
                else
                {
                    data.Children.emplace_back(std::move(childData)); // It's big enough
                }
            }

            return data;
        }

        static void PrepareAABB(SkeletalRagdollBone& bone)
        {
            // Transform AABB back to local space so that it can be transformed to any `SkeletalPose`
            bone.AABB.Transform(glm::inverse(bone.LocalTransform));
            for (auto& child : bone.Children)
                PrepareAABB(child);
        }

        static void SetUserSettings(SkeletalRagdollBone& node, const std::unordered_map<std::string, SkeletalRagdollBone::UserSettings>& ragdollPerBoneSettings)
        {
            auto it = ragdollPerBoneSettings.find(node.Name);
            if (it != ragdollPerBoneSettings.end())
                node.Settings = it->second;

            for (auto& child : node.Children)
                SetUserSettings(child, ragdollPerBoneSettings);
        }

        static void GetUserSettings(const SkeletalRagdollBone& node, std::unordered_map<std::string, SkeletalRagdollBone::UserSettings>& ragdollPerBoneSettings)
        {
            ragdollPerBoneSettings[node.Name] = node.Settings;

            for (const auto& child : node.Children)
                GetUserSettings(child, ragdollPerBoneSettings);
        }

        static void ResetUserSettings(SkeletalRagdollBone& node)
        {
            node.Settings = {};
            for (auto& child : node.Children)
                ResetUserSettings(child);
        }
	}

    SkeletalMesh::SkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
        const std::unordered_map<std::string, SkeletalRagdollBone::UserSettings>& ragdollPerBoneSettings, float minRagdollBoneSize, float maxRagdollTwist, float maxRagdollSwing,
        CollisionDetectionType collisionDetection, CollisionGroup collisionGroup, CollisionGroup interactingCollisionGroup)
        : m_Vertices(vertices)
        , m_IndicesPerMaterial(indicesPerMaterial)
        , m_Skeletal(skeletal)
        , m_AABB(aabb)
        , m_MaterialSlots((uint32_t)m_IndicesPerMaterial.size())
        , m_Materials(m_MaterialSlots)
        , m_MaterialsCallbacks(m_MaterialSlots)
        , m_MinRagdollBoneSize(minRagdollBoneSize)
        , m_MaxRagdollTwist(maxRagdollTwist)
        , m_MaxRagdollSwing(maxRagdollSwing)
        , m_CollisionDetection(collisionDetection)
        , m_CollisionGroup(collisionGroup)
        , m_InteractingCollisionGroup(interactingCollisionGroup)
    {
        // Normalize bone weights
        for (auto& vertex : m_Vertices)
        {
            float totalWeight = 0.f;
            float weigthsF32[EG_MAX_BONES_PER_VERTEX];
            for (uint32_t i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
            {
                weigthsF32[i] = Utils::ToFloat32(vertex.Weights[i]);
                totalWeight += weigthsF32[i];
            }
            for (uint32_t i = 0; i < EG_MAX_BONES_PER_VERTEX; ++i)
            {
                vertex.Weights[i] = Utils::ToFloat16(weigthsF32[i] / totalWeight);
            }
        }
        m_RagdollRoot = Utils::MergeBones(m_MinRagdollBoneSize, m_Skeletal.GetBoneInfoMap(), m_Skeletal.RootBone);
        Utils::PrepareAABB(m_RagdollRoot);
        Utils::SetUserSettings(m_RagdollRoot, ragdollPerBoneSettings);
    }

    SkeletalMesh::SkeletalMesh(const SkeletalMesh& other)
        : m_Vertices(other.m_Vertices)
        , m_IndicesPerMaterial(other.m_IndicesPerMaterial)
        , m_Skeletal(other.m_Skeletal)
        , m_AABB(other.m_AABB)
        , m_MaterialSlots(other.m_MaterialSlots)
        , m_Materials(m_MaterialSlots)
        , m_MaterialsCallbacks(m_MaterialSlots)
        , m_RagdollRoot(other.m_RagdollRoot)
        , m_MinRagdollBoneSize(other.m_MinRagdollBoneSize)
        , m_MaxRagdollTwist(other.m_MaxRagdollTwist)
        , m_MaxRagdollSwing(other.m_MaxRagdollSwing)
    {
        for (uint32_t i = 0; i < m_MaterialSlots; ++i)
        {
            SetMaterialAsset(i, other.m_Materials[i]);
        }
    }

    SkeletalMesh::~SkeletalMesh()
    {
        for (uint32_t i = 0; i < m_MaterialSlots; ++i)
        {
            if (m_Materials[i])
            {
                m_Materials[i]->RemoveOnAssetModifiedCallback(m_MaterialsCallbacks[i]);
            }
        }
    }

    void SkeletalMesh::SetMaterialAsset(uint32_t index, const Ref<AssetMaterial>& material)
    {
        if (m_Materials[index])
        {
            m_Materials[index]->RemoveOnAssetModifiedCallback(m_MaterialsCallbacks[index]);
        }
        m_Materials[index] = material;
        if (material)
        {
            material->AddOnAssetModifiedCallback(m_MaterialsCallbacks[index], [this]()
            {
                OnMaterialPropertyModified();
            });
        }
    }

    void SkeletalMesh::AddOnMaterialPropertyModifiedCallback(const GUID& id, const std::function<void()>& func)
    {
        std::scoped_lock lock(m_Mutex);
        m_Callbacks[id] = func;
    }

    void SkeletalMesh::RemoveOnMaterialPropertyModifiedCallback(const GUID& id)
    {
        std::scoped_lock lock(m_Mutex);
        m_Callbacks.erase(id);
    }

    void SkeletalMesh::OnMaterialPropertyModified()
    {
        for (auto& [_, func] : m_Callbacks)
            func();
    }

    void SkeletalMesh::RegenerateRagdollData(float minBoneSize)
    {
        m_MinRagdollBoneSize = minBoneSize;

        std::unordered_map<std::string, SkeletalRagdollBone::UserSettings> ragdollPerBoneSettings;
        Utils::GetUserSettings(m_RagdollRoot, ragdollPerBoneSettings);

        m_RagdollRoot = Utils::MergeBones(m_MinRagdollBoneSize, m_Skeletal.GetBoneInfoMap(), m_Skeletal.RootBone);
        Utils::PrepareAABB(m_RagdollRoot);
        Utils::SetUserSettings(m_RagdollRoot, ragdollPerBoneSettings);
    }

    void SkeletalMesh::ResetUserRagdollSettings()
    {
        Utils::ResetUserSettings(m_RagdollRoot);
    }

    Ref<SkeletalMesh> SkeletalMesh::Create(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
        const std::unordered_map<std::string, SkeletalRagdollBone::UserSettings>& ragdollPerBoneSettings, float minRagdollBoneSize, float maxRagdollTwist, float maxRagdollSwing,
        CollisionDetectionType collisionDetection, CollisionGroup collisionGroup, CollisionGroup interactingCollisionGroup)
	{
		class LocalSkeletalMesh : public SkeletalMesh
		{
		public:
			LocalSkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const SkeletalMeshInfo& skeletal, const AABB& aabb,
                const std::unordered_map<std::string, SkeletalRagdollBone::UserSettings>& ragdollSettings, float minRagdollBoneSize, float maxRagdollTwist, float maxRagdollSwing,
                CollisionDetectionType collisionDetection, CollisionGroup collisionGroup, CollisionGroup interactingCollisionGroup)
				: SkeletalMesh(vertices, indicesPerMaterial, skeletal, aabb, ragdollSettings, minRagdollBoneSize, maxRagdollTwist, maxRagdollSwing, collisionDetection, collisionGroup, interactingCollisionGroup) {}
		};

		return MakeRef<LocalSkeletalMesh>(vertices, indicesPerMaterial, skeletal, aabb, ragdollPerBoneSettings, minRagdollBoneSize, maxRagdollTwist, maxRagdollSwing, collisionDetection, collisionGroup, interactingCollisionGroup);
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

    static bool FindNode_Internal(BoneNode& node, uint64_t nameHash, BoneNode** outNode)
    {
        if (node.GetNameHash() == nameHash)
        {
            *outNode = &node;
            return true;
        }

        for (auto& child : node.Children)
        {
            const bool bFound = FindNode_Internal(child, nameHash, outNode);
            if (bFound)
                return true;
        }

        return false;
    }

    bool BoneNode::FindNode(uint64_t nameHash, BoneNode** outNode)
    {
        return FindNode_Internal(*this, nameHash, outNode);
    }
    
    bool BoneNode::FindNode(const std::string& name, BoneNode** outNode)
    {
        const uint64_t nameHash = Utils::CalculateBoneNameHash(name);
        return FindNode(nameHash, outNode);
    }
}
