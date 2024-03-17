#include "egpch.h"
#include "AnimationSystem.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Core/Transform.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Animation/AnimationGraph.h"
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
                if (animationTime <= positions[index + 1].TimeStamp)
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
                if (animationTime <= rotations[index + 1].TimeStamp)
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
                if (animationTime <= scales[index + 1].TimeStamp)
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
        
        /* Figures out which position keys to interpolate b/w and performs the interpolation
        and returns the translation matrix */
        static glm::vec3 InterpolatePositionRaw(const BoneAnimation& bone, float animationTime)
        {
            const auto& locations = bone.Locations;
            if (locations.size() == 1)
                return locations[0].Location;

            size_t p0Index = GetPositionIndex(locations, animationTime);
            size_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(locations[p0Index].TimeStamp, locations[p1Index].TimeStamp, animationTime);
            glm::vec3 finalPosition = glm::mix(locations[p0Index].Location, locations[p1Index].Location, scaleFactor);
            return finalPosition;
        }

        /* Figures out which rotations keys to interpolate b/w and performs the interpolation
        and returns the rotation matrix */
        static glm::quat InterpolateRotationRaw(const BoneAnimation& bone, float animationTime)
        {
            const auto& rotations = bone.Rotations;
            if (rotations.size() == 1)
            {
                auto rotation = glm::normalize(rotations[0].Rotation);
                return rotation;
            }

            size_t p0Index = GetRotationIndex(rotations, animationTime);
            size_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(rotations[p0Index].TimeStamp, rotations[p1Index].TimeStamp, animationTime);
            glm::quat finalRotation = glm::slerp(rotations[p0Index].Rotation, rotations[p1Index].Rotation, scaleFactor);
            finalRotation = glm::normalize(finalRotation);
            return finalRotation;
        }

        /* Figures out which scaling keys to interpolate b/w and performs the interpolation
        and returns the scale matrix */
        static glm::vec3 InterpolateScalingRaw(const BoneAnimation& bone, float animationTime)
        {
            const auto& scales = bone.Scales;
            if (scales.size() == 1)
                return scales[0].Scale;

            size_t p0Index = GetScaleIndex(scales, animationTime);
            size_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(scales[p0Index].TimeStamp, scales[p1Index].TimeStamp, animationTime);
            glm::vec3 finalScale = glm::mix(scales[p0Index].Scale, scales[p1Index].Scale, scaleFactor);
            return finalScale;
        }

        static void CalculateBoneTransform(const std::vector<std::string>& requestedName, const SkeletalMeshAnimation* animation, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal,
            float currentTime, std::vector<glm::mat4>& outTransforms, bool bProcess = false)
        {
            const std::string& nodeName = node.Name;
            if (!bProcess)
                bProcess = std::find(requestedName.begin(), requestedName.end(), nodeName) != requestedName.end();

            glm::mat4 globalTransformation = (bProcess && animation) ? parentTransform : parentTransform * node.Transformation;

            if (bProcess && animation)
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
                CalculateBoneTransform(requestedName, animation, child, globalTransformation, skeletal, currentTime, outTransforms, bProcess);
        }
    
        static void ApplyAdditive_Internal(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const BoneNode& node, float blendAlpha, SkeletalPose* resultPose)
        {
            const std::string& nodeName = node.Name;

            auto itTarget = targetPose.Bones.find(nodeName);
            auto bItTargetValid = itTarget != targetPose.Bones.end();
            auto itAdditive = additivePose.Bones.find(nodeName);
            auto bItAdditiveValid = itAdditive != additivePose.Bones.end();

            Transform targetAnimTr;
            Transform additiveAnimTr;
            if (bItTargetValid)
            {
                const auto& bone = itTarget->second;
                targetAnimTr = bone;
            }
            if (bItAdditiveValid)
            {
                const auto& bone = itAdditive->second;
                additiveAnimTr = bone;
            }

            if (bItTargetValid || bItAdditiveValid)
            {
                Transform lerpedAdditive; // It's unit by default
                lerpedAdditive.Location = glm::mix(lerpedAdditive.Location, additiveAnimTr.Location, blendAlpha);
                lerpedAdditive.Rotation = glm::slerp(lerpedAdditive.Rotation.GetQuat(), additiveAnimTr.Rotation.GetQuat(), blendAlpha);
                lerpedAdditive.Scale3D = glm::mix(lerpedAdditive.Scale3D, additiveAnimTr.Scale3D, blendAlpha);

                Transform& diffTr = resultPose->Bones[nodeName];
                diffTr = lerpedAdditive + targetAnimTr;
            }

            for (auto& child : node.Children)
                ApplyAdditive_Internal(targetPose, additivePose, child, blendAlpha, resultPose);
        }
        
        static void BlendPoses_Internal(const SkeletalPose& pose1, const SkeletalPose& pose2, const BoneNode& node, float blendAlpha, SkeletalPose* outPose)
        {
            const std::string& nodeName = node.Name;

            auto it1 = pose1.Bones.find(nodeName);
            auto it2 = pose2.Bones.find(nodeName);
            const bool bValid1 = it1 != pose1.Bones.end();
            const bool bValid2 = it2 != pose2.Bones.end();

            if (bValid1 || bValid2)
            {
                auto& resultTr = outPose->Bones[nodeName];
                if (bValid1 && bValid2)
                {
                    const auto& bone1 = it1->second;
                    const auto& bone2 = it2->second;

                    resultTr.Location = glm::mix(bone1.Location, bone2.Location, blendAlpha);
                    resultTr.Rotation = glm::slerp(bone1.Rotation.GetQuat(), bone2.Rotation.GetQuat(), blendAlpha);
                    resultTr.Scale3D = glm::mix(bone1.Scale3D, bone2.Scale3D, blendAlpha);
                }
                else if (bValid1)
                {
                    const auto& bone = it1->second;
                    resultTr = bone;
                }
                else if (bValid2)
                {
                    const auto& bone = it2->second;
                    resultTr = bone;
                }
            }

            for (auto& child : node.Children)
                BlendPoses_Internal(pose1, pose2, child, blendAlpha, outPose);
        }
    }

    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::m_Transforms;
    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::m_Transforms_RT;

    static std::mutex s_Mutex;

    void AnimationSystem::Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);
        
        const auto& skeletal = mesh->GetSkeletal();
        SkeletalPose pose;
        if (animation)
            AnimationClip(animation, skeletal.RootBone, currentTime, &pose);

        glm::mat4 rootTransform = glm::mat4(1.f);
        FinalizePose(pose, skeletal.RootBone, rootTransform, skeletal, *outTransforms);
    }

    void AnimationSystem::UpdateBlend(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation1, const SkeletalMeshAnimation* animation2, float currentTime1, float currentTime2, float blendAlpha, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);

        const auto& skeletal = mesh->GetSkeletal();
        SkeletalPose pose1;
        SkeletalPose pose2;

        // Calculate poses
        AnimationClip(animation1, skeletal.RootBone, currentTime1, &pose1);
        AnimationClip(animation2, skeletal.RootBone, currentTime2, &pose2);

        // Blend poses
        SkeletalPose blendedPose;
        BlendPoses(pose1, pose2, skeletal.RootBone, blendAlpha, &blendedPose);

        // Calculate final matrices
        glm::mat4 rootTransform = glm::mat4(1.f);
        FinalizePose(blendedPose, skeletal.RootBone, rootTransform, skeletal, *outTransforms);
    }

    void AnimationSystem::UpdateOnlySpecified(const std::vector<std::string>& requestedName, const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);

        glm::mat4 rootTransform = glm::mat4(1.f);
        const auto& skeletal = mesh->GetSkeletal();
        Utils::CalculateBoneTransform(requestedName, animation, skeletal.RootBone, rootTransform, skeletal, currentTime, *outTransforms);
    }
    
    float AnimationSystem::StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts, bool bLoop)
    {
        currentTime += animation->TicksPerSecond * ts;
        if (currentTime > animation->Duration)
            currentTime = bLoop ? 0.f : animation->Duration;
        else if (currentTime < 0.f)
            currentTime = bLoop ? animation->Duration : 0.f; // Animation is playing in reverse

        return currentTime;
    }

    bool AnimationSystem::IsValidTime(const SkeletalMeshAnimation* animation, float currentTime)
    {
        return currentTime <= animation->Duration;
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
            auto& transforms = m_Transforms[mesh->Parent.GetID()];

            if (mesh->AnimType == SkeletalMeshComponent::AnimationType::Clip)
            {
                const auto& animAsset = mesh->GetAnimationAsset();
                const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
                Update(skeletalMesh, animation, mesh->CurrentClipPlayTime, &transforms);

                if (animation)
                    mesh->CurrentClipPlayTime = StepForwardAnimTime(animation, mesh->CurrentClipPlayTime, ts * mesh->ClipPlaybackSpeed, mesh->bClipLooping);
            }
            else
            {
                if (const auto& graph = mesh->GetAnimationGraph())
                    graph->Update(ts, &transforms);
                else
                {
                    const auto& skeletal = skeletalMesh->GetSkeletal();
                    glm::mat4 rootTransform = glm::mat4(1.f);
                    FinalizePose({}, skeletal.RootBone, rootTransform, skeletal, transforms);
                }
            }


        }

        {
            std::scoped_lock lock(s_Mutex);
            m_Transforms_RT = m_Transforms;
        }
    }

    void AnimationSystem::UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        m_Transforms.clear();

        for (auto& mesh : meshes)
        {
            const auto& asset = mesh->GetMeshAsset();
            if (!asset)
                continue;

            const auto& skeletalMesh = asset->GetMesh();
            auto& transforms = m_Transforms[mesh->Parent.GetID()];
            const auto& skeletal = skeletalMesh->GetSkeletal();

            glm::mat4 rootTransform = glm::mat4(1.f);
            FinalizePose({}, skeletal.RootBone, rootTransform, skeletal, transforms);
        }

        {
            std::scoped_lock lock(s_Mutex);
            m_Transforms_RT = m_Transforms;
        }
    }
    
    void AnimationSystem::UpdateDifferencePos(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* refAnim, const SkeletalMeshAnimation* sourceAnim, const SkeletalMeshAnimation* targetAnim,
        float currentTime, float currentTimeRef, float currentTimeSrc, float blendAlpha, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);

        const auto& skeletal = mesh->GetSkeletal();
        SkeletalPose pose1;
        SkeletalPose pose2;
        SkeletalPose pose3;

        // Calculate poses
        AnimationClip(refAnim, skeletal.RootBone, currentTimeRef, &pose1);
        AnimationClip(sourceAnim, skeletal.RootBone, currentTimeSrc, &pose2);
        AnimationClip(targetAnim, skeletal.RootBone, currentTime, &pose3);

        // Get additive pose
        SkeletalPose additivePose;
        CalculateAdditivePose(pose1, pose2, skeletal.RootBone, &additivePose);

        // Apply additive
        SkeletalPose resultPose;
        ApplyAdditive(pose3, additivePose, skeletal.RootBone, blendAlpha, &resultPose);

        // Calculate final matrices
        glm::mat4 rootTransform = glm::mat4(1.f);
        FinalizePose(resultPose, skeletal.RootBone, rootTransform, skeletal, *outTransforms);
    }

    void AnimationSystem::CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose)
    {
        const std::string& nodeName = node.Name;

        auto itRef = refPose.Bones.find(nodeName);
        auto bItRefValid = itRef != refPose.Bones.end();
        auto itSrc = sourcePose.Bones.find(nodeName);
        auto bItSrcValid = itSrc != sourcePose.Bones.end();

        Transform refAnimTr;
        Transform srcAnimTr;
        if (bItRefValid)
        {
            const auto& bone = itRef->second;
            refAnimTr = bone;
        }
        if (bItSrcValid)
        {
            const auto& bone = itSrc->second;
            srcAnimTr = bone;
        }

        if (bItRefValid || bItSrcValid)
        {
            Transform& diffTr = resultPose->Bones[nodeName];
            diffTr = srcAnimTr - refAnimTr;
        }

        for (auto& child : node.Children)
            CalculateAdditivePose(refPose, sourcePose, child, resultPose);
    }

    void AnimationSystem::ApplyAdditive(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const BoneNode& node, float blendAlpha, SkeletalPose* resultPose)
    {
        if (blendAlpha == 0.f)
        {
            *resultPose = targetPose;
            return;
        }

        Utils::ApplyAdditive_Internal(targetPose, additivePose, node, blendAlpha, resultPose);
    }

    void AnimationSystem::BlendPoses(const SkeletalPose& pose1, const SkeletalPose& pose2, const BoneNode& node, float blendAlpha, SkeletalPose* outPose)
    {
        if (blendAlpha == 0.f)
        {
            *outPose = pose1;
            return;
        }
        else if (blendAlpha == 1.f)
        {
            *outPose = pose2;
            return;
        }

        Utils::BlendPoses_Internal(pose1, pose2, node, blendAlpha, outPose);
    }

    void AnimationSystem::AnimationClip(const SkeletalMeshAnimation* animation, const BoneNode& node, float currentTime, SkeletalPose* outPose)
    {
        const std::string& nodeName = node.Name;

        if (auto it = animation->Bones.find(nodeName); it != animation->Bones.end())
        {
            const auto& bone = it->second;
            auto& tr = outPose->Bones[nodeName];
            tr.Location = Utils::InterpolatePositionRaw(bone, currentTime);
            tr.Rotation = Utils::InterpolateRotationRaw(bone, currentTime);
            tr.Scale3D = Utils::InterpolateScalingRaw(bone, currentTime);
        }

        for (auto& child : node.Children)
            AnimationClip(animation, child, currentTime, outPose);
    }

    void AnimationSystem::FinalizePose(const SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms)
    {
        const std::string& nodeName = node.Name;
        glm::mat4 globalTransformation;
        if (auto it = pose.Bones.find(nodeName); it != pose.Bones.end())
        {
            const auto& bone = it->second;
            globalTransformation = parentTransform * Math::ToTransformMatrix(bone);
        }
        else
            globalTransformation = parentTransform * node.Transformation;

        if (auto it = skeletal.BoneInfoMap.find(nodeName); it != skeletal.BoneInfoMap.end())
        {
            const uint32_t index = it->second.BoneID;
            const glm::mat4& offset = it->second.Offset;
            if (index >= outTransforms.size())
                outTransforms.resize(index + 1);

            outTransforms[index] = skeletal.InverseTransform * globalTransformation * offset;
        }

        for (auto& child : node.Children)
            FinalizePose(pose, child, globalTransformation, skeletal, outTransforms);
    }
}
