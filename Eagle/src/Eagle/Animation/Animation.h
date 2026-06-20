#pragma once

#include "Eagle/Math/Transform.h"
#include <glm/gtx/quaternion.hpp>
#include <ankerl/unordered_dense.h>

namespace Eagle
{
    struct SkeletalMeshInfo;

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
    using BonesAnimMapByHash = ankerl::unordered_dense::map<uint64_t, BoneAnimation>;
    struct SkeletalMeshAnimation
    {
    private:
        BonesAnimMap m_AnimBones;
        BonesAnimMapByHash m_AnimBonesByHash;
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

        void SetAnimationBones(BonesAnimMap&& animBones)
        {
            m_AnimBones = std::move(animBones);

            m_AnimBonesByHash.clear();
            for (const auto& bone : m_AnimBones)
            {
                const uint64_t hash = Utils::CalculateBoneNameHash(bone.first);
                m_AnimBonesByHash[hash] = bone.second;
            }
        }

        const BonesAnimMap& GetAnimationBones() const { return m_AnimBones; }

        auto FindBone(const std::string& name) const
        {
            return m_AnimBones.find(name);
        }

        auto FindBone(uint64_t nameHash) const
        {
            return m_AnimBonesByHash.find(nameHash);
        }

        bool IsValid(const BonesAnimMap::const_iterator& it) const { return it != m_AnimBones.end(); }
        bool IsValid(const BonesAnimMapByHash::const_iterator& it) const { return it != m_AnimBonesByHash.end(); }

        size_t GetNumBones() const { return m_AnimBones.size(); }
    };

    struct SkeletalPose
    {
        // Key - name hash
        ankerl::unordered_dense::map<uint64_t, Transform> Bones;
        Transform TotalRootMotion;

        std::vector<AnimationEvent> EventsToTrigger;
        // Sometimes we can avoid copying `EventsToTrigger` to save on perf.
        // In such cases, we can just get the pointer to an existing data.
        // For example, `Animation Clip` is connected to `Filter Bones`. The `EventsToTrigger` will remain the same after `Filter Bones` is executed.
        // So, `Filter Bones` pose will get a pointer to `EventsToTrigger` of `Animation Clip` node.
        std::vector<AnimationEvent>* EventsToTrigger_Pointer = nullptr;

        float TimeTillAnimationLoops = FLT_MAX;
        bool bWasFiltered = false;

        void Reset()
        {
            Bones.clear();
            EventsToTrigger.clear();
            EventsToTrigger_Pointer = nullptr;
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

        std::vector<AnimationEvent>& GetEventsToTrigger() { return EventsToTrigger_Pointer ? *EventsToTrigger_Pointer : EventsToTrigger; }
        const std::vector<AnimationEvent>& GetEventsToTrigger() const { return EventsToTrigger_Pointer ? *EventsToTrigger_Pointer : EventsToTrigger; }

        auto FindBone(uint64_t nameHash)
        {
            return Bones.find(nameHash);
        }

        auto FindBone(const std::string& name)
        {
            const uint64_t hash = Utils::CalculateBoneNameHash(name);
            return Bones.find(hash);
        }

        auto FindBone(uint64_t nameHash) const
        {
            return Bones.find(nameHash);
        }

        auto FindBone(const std::string& name) const
        {
            const uint64_t hash = ankerl::unordered_dense::hash<std::string>()(name);
            return Bones.find(hash);
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
