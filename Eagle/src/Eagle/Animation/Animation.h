#pragma once

#include "Eagle/Math/Transform.h"
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
    struct SkeletalMeshInfo;
    struct BoneNode;

    enum class AnimationType
    {
        Clip,
        Graph
    };

    enum class RootMotionMode
    {
        Disabled, BasePose, AnimFirstFrame
    };

    struct KeyPosition
    {
        glm::vec3 Location = glm::vec3(0.f);
        float TimeStamp = 0.f;
    };

    struct KeyRotation
    {
        glm::quat Rotation = glm::quat(1.f, 0.f, 0.f, 0.f); // Unit
        float TimeStamp = 0.f;
    };

    struct KeyScale
    {
        glm::vec3 Scale = glm::vec3(1.f);
        float TimeStamp = 0.f;
    };

	struct BoneAnimation
	{
        std::vector<KeyPosition> Locations;
        std::vector<KeyRotation> Rotations;
        std::vector<KeyScale> Scales;
        
        uint32_t BoneID = 0;
	};

    struct AnimationEvent
    {
        std::string Name = "Event name";
        float Time = 0.f; // A value between [0; Duration] when an event should be triggered
    };

    struct AnimationEventData
    {
        uint32_t EntityID = 0;
        std::vector<AnimationEvent> Events;
    };

    // string - bone name
    using BonesAnimMap = ankerl::unordered_dense::map<std::string, BoneAnimation>;
    struct SkeletalMeshAnimation
    {
    private:
        BonesAnimMap m_AnimBones;
        // Name hash -> index into `m_AnimBones` (its values are stored contiguously).
        ankerl::unordered_dense::map<uint64_t, uint32_t> m_AnimBoneIndexByHash;
    public:
        BoneAnimation RootMotion;
        std::vector<AnimationEvent> Events;
        std::vector<glm::vec3> PreRootMotionLocations;
        RootMotionMode RootMotionType = RootMotionMode::Disabled;

        float Duration = 0.f; // In ticks
        float TicksPerSecond = 0.f;
        bool bInPlace = false;

        bool HasRootMotion() const { return RootMotionType != RootMotionMode::Disabled && RootMotion.Locations.size() > 0; }

        bool ExtractRootMotion(const SkeletalMeshInfo& skeletalInfo, RootMotionMode mode);
        bool RemoveRootMotion(const SkeletalMeshInfo& skeletalInfo);

        // Returns the bone that root motion is extracted from and that `bInPlace` pins in place.
        // It's the first bone (walking down from the skeleton root through single-child nodes) that both is a mesh bone and has an animation track.
        const BoneNode* FindRootMotionBone(const SkeletalMeshInfo& skeletalInfo) const;

        void SetAnimationBones(BonesAnimMap&& animBones)
        {
            m_AnimBones = std::move(animBones);

            m_AnimBoneIndexByHash.clear();
            m_AnimBoneIndexByHash.reserve(m_AnimBones.size());
            uint32_t index = 0;
            for (const auto& bone : m_AnimBones)
                m_AnimBoneIndexByHash[Utils::CalculateBoneNameHash(bone.first)] = index++;
        }

        const BonesAnimMap& GetAnimationBones() const { return m_AnimBones; }

        auto FindBone(const std::string& name) const
        {
            return m_AnimBones.find(name);
        }

        auto FindBone(const std::string& name)
        {
            return m_AnimBones.find(name);
        }

        // Returns nullptr if the animation doesn't have a track for this bone
        const BoneAnimation* FindBone(uint64_t nameHash) const
        {
            auto it = m_AnimBoneIndexByHash.find(nameHash);
            return it != m_AnimBoneIndexByHash.end() ? &(m_AnimBones.begin() + it->second)->second : nullptr;
        }

        bool IsValid(const BonesAnimMap::const_iterator& it) const { return it != m_AnimBones.end(); }

        size_t GetNumBones() const { return m_AnimBones.size(); }
    };

    struct BoneParentOverride
    {
        uint64_t BoneHash = 0;
        bool bIgnoreParentLocation = false;
        bool bIgnoreParentRotation = false;
        bool bIgnoreParentScale = false;
    };

    struct SkeletalPose
    {
        // Key - name hash
        ankerl::unordered_dense::map<uint64_t, Transform> Bones;
        Transform TotalRootMotion;

        std::vector<AnimationEvent> EventsToTrigger;
        std::vector<BoneParentOverride> ParentOverrides;

        float TimeTillAnimationLoops = FLT_MAX;
        bool bWasFiltered = false;

        void Reset()
        {
            Bones.clear();
            EventsToTrigger.clear();
            ParentOverrides.clear();
            m_RootMotion = {};
            TotalRootMotion = {};
            bHasRootMotion = false;
            bWasFiltered = false;
            TimeTillAnimationLoops = FLT_MAX;
        }

        void SetRootMotion(const Transform& rootMotion)
        {
            m_RootMotion = rootMotion;
            bHasRootMotion = true;
        }

        const Transform& GetRootMotion() const { return m_RootMotion; }
        bool HasRootMotion() const { return bHasRootMotion; }

        std::vector<AnimationEvent>& GetEventsToTrigger() { return EventsToTrigger; }
        const std::vector<AnimationEvent>& GetEventsToTrigger() const { return EventsToTrigger; }

        void AppendEvents(const SkeletalPose& other)
        {
            if (&other != this)
                EventsToTrigger.insert(EventsToTrigger.end(), other.EventsToTrigger.begin(), other.EventsToTrigger.end());
        }

        const BoneParentOverride* FindParentOverride(uint64_t nameHash) const
        {
            for (const auto& entry : ParentOverrides)
                if (entry.BoneHash == nameHash)
                    return &entry;
            return nullptr;
        }

        void AddParentOverride(const BoneParentOverride& value)
        {
            for (auto& entry : ParentOverrides)
            {
                if (entry.BoneHash == value.BoneHash)
                {
                    entry = value;
                    return;
                }
            }
            ParentOverrides.push_back(value);
        }

        void MergeParentOverrides(const SkeletalPose& other)
        {
            if (&other == this)
                return;
            for (const auto& entry : other.ParentOverrides)
                AddParentOverride(entry);
        }

        auto FindBone(uint64_t nameHash)
        {
            return Bones.find(nameHash);
        }

        auto FindBone(const std::string& name)
        {
            return Bones.find(Utils::CalculateBoneNameHash(name));
        }

        auto FindBone(uint64_t nameHash) const
        {
            return Bones.find(nameHash);
        }

        auto FindBone(const std::string& name) const
        {
            return Bones.find(Utils::CalculateBoneNameHash(name));
        }

    private:
        Transform m_RootMotion;
        bool bHasRootMotion = false;
    };

    enum class RootMotionLockFlag
    {
        None = 0,
        PositionX = BIT(0), PositionY = BIT(1), PositionZ = BIT(2), Position = PositionX | PositionY | PositionZ
    };
    DECLARE_FLAGS(RootMotionLockFlag);
}
