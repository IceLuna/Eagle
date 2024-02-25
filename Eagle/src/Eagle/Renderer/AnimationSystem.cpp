#include "egpch.h"
#include "AnimationSystem.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Core/Transform.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Classes/Animation.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
    namespace Utils
    {
        /* Gets normalized value for Lerp & Slerp */
        static float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
        {
            float scaleFactor = 0.0f;
            float midWayLength = animationTime - lastTimeStamp;
            float framesDiff = nextTimeStamp - lastTimeStamp;
            scaleFactor = midWayLength / framesDiff;
            return scaleFactor;
        }

        /* Gets the current index on KeyPositions to interpolate to based on
            the current animation time */
        static size_t GetPositionIndex(const std::vector<KeyPosition>& positions, float animationTime)
        {
            for (size_t index = 0; index < positions.size() - 1; ++index)
            {
                if (animationTime < positions[index + 1].TimeStamp)
                    return index;
            }
            EG_CORE_ASSERT(false);
            return 0;
        }

        /* Gets the current index on KeyRotations to interpolate to based on the
        current animation time */
        static size_t GetRotationIndex(const std::vector<KeyRotation>& rotations, float animationTime)
        {
            for (size_t index = 0; index < rotations.size() - 1; ++index)
            {
                if (animationTime < rotations[index + 1].TimeStamp)
                    return index;
            }
            EG_CORE_ASSERT(false);
            return 0;
        }

        /* Gets the current index on KeyScalings to interpolate to based on the
        current animation time */
        static size_t GetScaleIndex(const std::vector<KeyScale>& scales, float animationTime)
        {
            for (size_t index = 0; index < scales.size() - 1; ++index)
            {
                if (animationTime < scales[index + 1].TimeStamp)
                    return index;
            }
            EG_CORE_ASSERT(false);
            return 0;
        }

        /* Figures out which position keys to interpolate b/w and performs the interpolation
        and returns the translation matrix */
        static glm::mat4 InterpolatePosition(const BoneAnimation& bone, float animationTime)
        {
            const auto& locations = bone.Locations;
            if (locations.size() == 1)
                return glm::translate(glm::mat4(1.0f), locations[0].Location);

            size_t p0Index = GetPositionIndex(locations, animationTime);
            size_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(locations[p0Index].TimeStamp, locations[p1Index].TimeStamp, animationTime);
            glm::vec3 finalPosition = glm::mix(locations[p0Index].Location, locations[p1Index].Location, scaleFactor);
            return glm::translate(glm::mat4(1.0f), finalPosition);
        }

        /* Figures out which rotations keys to interpolate b/w and performs the interpolation
        and returns the rotation matrix */
        static glm::mat4 InterpolateRotation(const BoneAnimation& bone, float animationTime)
        {
            const auto& rotations = bone.Rotations;
            if (rotations.size() == 1)
            {
                auto rotation = glm::normalize(rotations[0].Rotation);
                return glm::toMat4(rotation);
            }

            size_t p0Index = GetRotationIndex(rotations, animationTime);
            size_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(rotations[p0Index].TimeStamp, rotations[p1Index].TimeStamp, animationTime);
            glm::quat finalRotation = glm::slerp(rotations[p0Index].Rotation, rotations[p1Index].Rotation, scaleFactor);
            finalRotation = glm::normalize(finalRotation);
            return glm::toMat4(finalRotation);
        }

        /* Figures out which scaling keys to interpolate b/w and performs the interpolation
        and returns the scale matrix */
        static glm::mat4 InterpolateScaling(const BoneAnimation& bone, float animationTime)
        {
            const auto& scales = bone.Scales;
            if (scales.size() == 1)
                return glm::scale(glm::mat4(1.0f), scales[0].Scale);

            size_t p0Index = GetScaleIndex(scales, animationTime);
            size_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(scales[p0Index].TimeStamp, scales[p1Index].TimeStamp, animationTime);
            glm::vec3 finalScale = glm::mix(scales[p0Index].Scale, scales[p1Index].Scale, scaleFactor);
            return glm::scale(glm::mat4(1.0f), finalScale);
        }

        static void CalculateBoneTransform(const SkeletalMeshAnimation* animation, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, float currentTime, std::vector<glm::mat4>& outTransforms)
        {
            const std::string& nodeName = node.Name;
            glm::mat4 globalTransformation = animation ? parentTransform : parentTransform * node.Transformation;

            if (animation)
            {
                if (auto it = animation->Bones.find(nodeName); it != animation->Bones.end())
                {
                    const auto& bone = it->second;
                    glm::mat4 translation = InterpolatePosition(bone, currentTime);
                    glm::mat4 rotation = InterpolateRotation(bone, currentTime);
                    glm::mat4 scale = InterpolateScaling(bone, currentTime);

                    const glm::mat4 nodeTransform = translation * rotation * scale;
                    globalTransformation = parentTransform * nodeTransform;
                }
            }

            if (auto it = skeletal.BoneInfoMap.find(nodeName); it != skeletal.BoneInfoMap.end())
            {
                const uint32_t index = it->second.BoneID;
                const glm::mat4& offset = it->second.Offset;
                if (index >= outTransforms.size())
                    outTransforms.resize(index + 1);

                outTransforms[index] = skeletal.InverseTransform * globalTransformation * offset;
            }

            for (auto& child : node.Children)
                CalculateBoneTransform(animation, child, globalTransformation, skeletal, currentTime, outTransforms);
        }
    }

    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::m_Transforms;
    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::m_Transforms_RT;

    static std::mutex s_Mutex;

    void AnimationSystem::Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);

        glm::mat4 rootTransform = glm::mat4(1.f);
        const auto& skeletal = mesh->GetSkeletal();
        Utils::CalculateBoneTransform(animation, skeletal.RootBone, rootTransform, skeletal, currentTime, *outTransforms);
    }
    
    float AnimationSystem::StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts)
    {
        currentTime += animation->TicksPerSecond * ts;
        if (currentTime > animation->Duration)
            currentTime = 0.f;

        return currentTime;
    }

    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::GetTransforms_RT()
    {
        static std::mutex s_Mutex;
        return m_Transforms_RT;
    }
    
    void AnimationSystem::Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        m_Transforms.clear();

        for (auto& mesh : meshes)
        {
            const auto& asset = mesh->GetMeshAsset();
            if (!asset)
                continue;

            const auto& skeletalMesh = asset->GetMesh();
            const auto& animAsset = mesh->GetAnimationAsset();
            const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
            auto& transforms = m_Transforms[mesh->Parent.GetID()];

            Update(skeletalMesh, animation, mesh->CurrentPlayTime, &transforms);
            if (animation)
                mesh->CurrentPlayTime = StepForwardAnimTime(animation, mesh->CurrentPlayTime, ts);
        }

        {
            std::scoped_lock lock(s_Mutex);
            m_Transforms_RT = m_Transforms;
        }
    }
}
