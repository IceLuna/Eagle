#pragma once

#include "Eagle/Core/Transform.h"
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
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

    // string - bone name
    using BonesAnimMap = std::unordered_map<std::string, BoneAnimation>;
    struct SkeletalMeshAnimation
    {
        BonesAnimMap Bones;
        BoneAnimation RootMotion;

        float Duration = 0.f;
        float TicksPerSecond = 0.f;

        bool HasRootMotion() const { return RootMotion.Locations.size() > 0; }
    };

    struct SkeletalPose
    {
        std::unordered_map<std::string, Transform> Bones;
        Transform TotalRootMotion;
        bool bWasFiltered = false;

        void Reset()
        {
            Bones.clear();
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

    private:
        Transform m_RootMotion;
        bool bHasRootMotion = false;
    };
}
