#pragma once

#include "Eagle/Core/Transform.h"
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
    struct SkeletalMeshInfo;

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

    // string - bone name
    using BonesAnimMap = std::unordered_map<std::string, BoneAnimation>;
    struct SkeletalMeshAnimation
    {
        BonesAnimMap Bones;
        BoneAnimation RootMotion;
        std::vector<AnimationEvent> Events;

        float Duration = 0.f;
        float TicksPerSecond = 0.f;

        bool HasRootMotion() const { return RootMotion.Locations.size() > 0; }

        bool ExtractRootMotion(const SkeletalMeshInfo& skeletalInfo);
        bool RemoveRootMotion(const SkeletalMeshInfo& skeletalInfo);
    };

    struct SkeletalPose
    {
        std::unordered_map<std::string, Transform> Bones;
        Transform TotalRootMotion;

        std::unordered_set<std::string> EventsToTrigger;
        // Sometimes we can avoid copying `EventsToTrigger` to save on perf.
        // In such cases, we can just get the pointer to an existing data.
        // For example, `Animation Clip` is connected to `Filter Bones`. The `EventsToTrigger` will remain the same after `Filter Bones` is executed.
        // So, `Filter Bones` pose will get a pointer to `EventsToTrigger` of `Animation Clip` node.
        const std::unordered_set<std::string>* EventsToTrigger_Pointer = nullptr;

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
        }

        void SetRootMotion(const Transform& rootMotion)
        {
            m_RootMotion = rootMotion;
            bHasRootMotion = true;
        }

        const Transform& GetRootMotion() const { return m_RootMotion; }
        bool HasRootMotion() const { return bHasRootMotion; }

        const std::unordered_set<std::string>& GetEventsToTrigger() const { return EventsToTrigger_Pointer ? *EventsToTrigger_Pointer : EventsToTrigger; }

    private:
        Transform m_RootMotion;
        bool bHasRootMotion = false;
    };
}
