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
            scaleFactor = midWayLength / glm::max(framesDiff, 0.000001f);
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
    
        static void CalculateAdditivePose_Internal(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose)
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
                CalculateAdditivePose_Internal(refPose, sourcePose, child, resultPose);
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
                Transform lerpedAdditive = Transform::Blend(Transform{}, additiveAnimTr, blendAlpha);
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

                    resultTr = Transform::Blend(bone1, bone2, blendAlpha);
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

    ThreadPool AnimationSystem::s_ThreadPool("AnimationSystem", std::thread::hardware_concurrency(), false);

    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::s_Transforms;

    void AnimationSystem::Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);
        
        const auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
        SkeletalPose pose;
        if (animation)
            AnimationClip(animation, skeletalInfo.RootBone, currentTime, &pose);

        glm::mat4 rootTransform = glm::mat4(1.f);
        FinalizePose(pose, skeletalInfo.RootBone, rootTransform, skeletalInfo, *outTransforms);
    }

    void AnimationSystem::UpdateOnlySpecified(const std::vector<std::string>& requestedName, const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);

        glm::mat4 rootTransform = glm::mat4(1.f);
        const auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
        Utils::CalculateBoneTransform(requestedName, animation, skeletalInfo.RootBone, rootTransform, skeletalInfo, currentTime, *outTransforms);
    }
    
    float AnimationSystem::StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts, bool bLoop)
    {
        currentTime += animation->TicksPerSecond * ts;
        if (currentTime > animation->Duration)
            currentTime = bLoop ? (currentTime / glm::floor(currentTime / animation->Duration)) - animation->Duration : animation->Duration;
        else if (currentTime < 0.f) // Animation is playing in reverse. When looping: Current = Duration - Current - Floor(Current/Duration) * Duration, but since `CurrentTime` is negative, signs are adjusted
            currentTime = bLoop ? animation->Duration + currentTime + (glm::floor(-currentTime / animation->Duration) * animation->Duration) : 0.f;

        return currentTime;
    }

    bool AnimationSystem::IsValidTime(const SkeletalMeshAnimation* animation, float currentTime)
    {
        return currentTime <= animation->Duration;
    }
    
    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        s_ThreadPool->wait_for_tasks();

        s_Transforms.clear();
        // Reserve memory
        for (auto& mesh : meshes)
        {
            const auto& asset = mesh->GetMeshAsset();
            if (!asset)
                continue;

            s_Transforms.emplace(mesh->Parent.GetID(), std::vector<glm::mat4>{});
        }

        for (auto& mesh : meshes)
        {
            const auto& asset = mesh->GetMeshAsset();
            if (!asset)
                continue;

            s_ThreadPool->push_task([&asset, mesh, ts]()
            {
                const auto& skeletalMesh = asset->GetMesh();
                auto& transforms = s_Transforms[mesh->Parent.GetID()];

                if (mesh->AnimType == SkeletalMeshComponent::AnimationType::Clip)
                {
                    const auto& animAsset = mesh->GetAnimationAsset();
                    const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
                    Update(skeletalMesh, animation, mesh->CurrentClipPlayTime, &transforms);
                    if (animation)
                    {
                        if (animation->HasRootMotion())
                        {
                            const float speed = mesh->ClipPlaybackSpeed;
                            const float prevSpeed = mesh->PrevClipPlaybackSpeed;
                            float currentTime = mesh->CurrentClipPlayTime;
                            float prevTime = mesh->PrevClipPlayTime;
                            if (prevSpeed < 0 && speed > 0 || speed < 0 && prevSpeed > 0) // If speed changed signs
                                std::swap(currentTime, prevTime);
                            if (speed == 0.f && prevSpeed != speed) // If speed stoped
                                prevTime = currentTime;

                            Transform totalRootMotion;
                            ApplyRootMotion(mesh, totalRootMotion, CalculateRootMotion(animation, currentTime, prevTime, speed, ts, &totalRootMotion));
                        }
                        mesh->PrevClipPlayTime = mesh->CurrentClipPlayTime;
                        mesh->CurrentClipPlayTime = StepForwardAnimTime(animation, mesh->CurrentClipPlayTime, ts * mesh->ClipPlaybackSpeed, mesh->bClipLooping);
                        mesh->PrevClipPlaybackSpeed = mesh->ClipPlaybackSpeed;
                    }
                }
                else
                {
                    if (const auto& graph = mesh->GetAnimationGraph())
                    {
                        graph->Update(ts, &transforms);
                        const auto& pose = graph->GetPose();
                        if (pose.HasRootMotion())
                            ApplyRootMotion(mesh, pose.TotalRootMotion, pose.GetRootMotion());
                    }
                    else
                    {
                        const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();
                        glm::mat4 rootTransform = glm::mat4(1.f);
                        FinalizePose({}, skeletalInfo.RootBone, rootTransform, skeletalInfo, transforms);
                    }
                }
            });
        }

        s_ThreadPool->wait_for_tasks();

        return s_Transforms;
    }

    void AnimationSystem::UpdateJustTick(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update. Just Tick");

        s_ThreadPool->wait_for_tasks();

        for (auto& mesh : meshes)
        {
            s_ThreadPool->push_task([mesh, ts]()
            {
                if (mesh->AnimType == SkeletalMeshComponent::AnimationType::Clip)
                {
                    if (const auto& animAsset = mesh->GetAnimationAsset())
                    {
                        const SkeletalMeshAnimation* animation = animAsset->GetAnimation().get();
                        if (animation->HasRootMotion())
                        {
                            Transform totalRootMotion;
                            ApplyRootMotion(mesh, totalRootMotion, CalculateRootMotion(animation, mesh->CurrentClipPlayTime, mesh->PrevClipPlayTime, mesh->ClipPlaybackSpeed, ts, &totalRootMotion));
                        }
                        mesh->PrevClipPlayTime = mesh->CurrentClipPlayTime;
                        mesh->CurrentClipPlayTime = AnimationSystem::StepForwardAnimTime(animation, mesh->CurrentClipPlayTime, mesh->ClipPlaybackSpeed * ts, mesh->bClipLooping);
                    }
                }
                else if (auto& graph = mesh->GetAnimationGraph())
                {
                    graph->Update(ts, nullptr);
                    const auto& pose = graph->GetPose();
                    if (pose.HasRootMotion())
                        ApplyRootMotion(mesh, pose.TotalRootMotion, pose.GetRootMotion());
                }
            });
        }

        s_ThreadPool->wait_for_tasks();
    }

    std::unordered_map<uint32_t, std::vector<glm::mat4>> AnimationSystem::UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        s_ThreadPool->wait_for_tasks();
        s_Transforms.clear();

        // Reserve memory
        for (auto& mesh : meshes)
        {
            const auto& asset = mesh->GetMeshAsset();
            if (!asset)
                continue;

            s_Transforms.emplace(mesh->Parent.GetID(), std::vector<glm::mat4>{});
        }

        for (auto& mesh : meshes)
        {
            const auto& asset = mesh->GetMeshAsset();
            if (!asset)
                continue;

            s_ThreadPool->push_task([mesh, ts, &asset]()
            {
                const auto& skeletalMesh = asset->GetMesh();
                auto& transforms = s_Transforms[mesh->Parent.GetID()];
                const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();

                glm::mat4 rootTransform = glm::mat4(1.f);
                FinalizePose({}, skeletalInfo.RootBone, rootTransform, skeletalInfo, transforms);
            });
        }
        s_ThreadPool->wait_for_tasks();

        return s_Transforms;
    }
    
    Transform AnimationSystem::CalculateRootMotion(const SkeletalMeshAnimation* animation, float currentTime, float prevTime, float playbackSpeed, Timestep ts, Transform* outTotalRootMotion)
    {
        if (!animation || !animation->HasRootMotion())
            return {};

        const bool bPlayingForward = playbackSpeed > 0.f;

        Transform rootMotionPrev{};
        (*outTotalRootMotion).Location = Utils::InterpolatePositionRaw(animation->RootMotion, currentTime);
        (*outTotalRootMotion).Rotation = Utils::InterpolateRotationRaw(animation->RootMotion, currentTime);
        (*outTotalRootMotion).Scale3D = Utils::InterpolateScalingRaw(animation->RootMotion, currentTime);

        rootMotionPrev.Location = Utils::InterpolatePositionRaw(animation->RootMotion, prevTime);
        rootMotionPrev.Rotation = Utils::InterpolateRotationRaw(animation->RootMotion, prevTime);
        rootMotionPrev.Scale3D = Utils::InterpolateScalingRaw(animation->RootMotion, prevTime);

        Transform result{};

        if (bPlayingForward)
        {
            if (currentTime < prevTime) // Playing forward, we looped back
            {
                const auto& locations = animation->RootMotion.Locations;
                const auto& rotations = animation->RootMotion.Rotations;
                const auto& scales = animation->RootMotion.Scales;
                const Transform firstTr{ locations.front().Location, rotations.front().Rotation, scales.front().Scale };
                const Transform lastTr{ locations.back().Location, rotations.back().Rotation, scales.back().Scale };

                result = (lastTr - rootMotionPrev) + (*outTotalRootMotion - firstTr);
            }
            else
                result = *outTotalRootMotion - rootMotionPrev;
        }
        else
        {
            if (currentTime > prevTime) // Playing backward, we looped back
            {
                const auto& locations = animation->RootMotion.Locations;
                const auto& rotations = animation->RootMotion.Rotations;
                const auto& scales = animation->RootMotion.Scales;
                const Transform firstTr{ locations.front().Location, rotations.front().Rotation, scales.front().Scale };
                const Transform lastTr{ locations.back().Location, rotations.back().Rotation, scales.back().Scale };

                result = (firstTr - rootMotionPrev) + (*outTotalRootMotion - lastTr);
            }
            else
                result = *outTotalRootMotion - rootMotionPrev;
        }

        return result;
    }

    void AnimationSystem::ApplyRootMotion(SkeletalMeshComponent* mesh, const Transform& totalRootMotion, Transform rootMotion)
    {
        rootMotion.Location = glm::rotate((totalRootMotion.Rotation.Conjugate() * mesh->GetWorldTransform().Rotation).GetQuat(), rootMotion.Location);
        const auto& worldTransform = mesh->Parent.GetWorldTransform();
        mesh->Parent.SetWorldTransform(worldTransform + rootMotion);
    }

    void AnimationSystem::UpdateDifferencePos(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* refAnim, const SkeletalMeshAnimation* sourceAnim, const SkeletalMeshAnimation* targetAnim,
        float currentTime, float currentTimeRef, float currentTimeSrc, float blendAlpha, std::vector<glm::mat4>* outTransforms)
    {
        outTransforms->clear();
        outTransforms->reserve(100);

        const auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
        SkeletalPose pose1;
        SkeletalPose pose2;
        SkeletalPose pose3;

        // Calculate poses
        AnimationClip(refAnim, skeletalInfo.RootBone, currentTimeRef, &pose1);
        AnimationClip(sourceAnim, skeletalInfo.RootBone, currentTimeSrc, &pose2);
        AnimationClip(targetAnim, skeletalInfo.RootBone, currentTime, &pose3);

        // Get additive pose
        SkeletalPose additivePose;
        CalculateAdditivePose(pose1, pose2, skeletalInfo.RootBone, &additivePose);

        // Apply additive
        SkeletalPose resultPose;
        ApplyAdditive(pose3, additivePose, skeletalInfo.RootBone, blendAlpha, &resultPose);

        // Calculate final matrices
        glm::mat4 rootTransform = glm::mat4(1.f);
        FinalizePose(resultPose, skeletalInfo.RootBone, rootTransform, skeletalInfo, *outTransforms);
    }

    void AnimationSystem::CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose)
    {
        Utils::CalculateAdditivePose_Internal(refPose, sourcePose, node, resultPose);
    }

    void AnimationSystem::ApplyAdditive(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const BoneNode& node, float blendAlpha, SkeletalPose* resultPose)
    {
        if (blendAlpha == 0.f)
        {
            *resultPose = targetPose;
            return;
        }

        Utils::ApplyAdditive_Internal(targetPose, additivePose, node, blendAlpha, resultPose);
        if (targetPose.HasRootMotion())
        {
            resultPose->SetRootMotion(targetPose.GetRootMotion());
            resultPose->TotalRootMotion = targetPose.TotalRootMotion;
        }
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

        if (pose1.HasRootMotion() || pose2.HasRootMotion())
        {
            const auto& pose1RootMotion = pose1.GetRootMotion();
            const auto& pose2RootMotion = pose2.GetRootMotion();
            Transform rootMotion = Transform::Blend(pose1RootMotion, pose2RootMotion, blendAlpha);

            outPose->SetRootMotion(rootMotion);
            outPose->TotalRootMotion = Transform::Blend(pose1.TotalRootMotion, pose2.TotalRootMotion, blendAlpha);
        }
        else if (pose1.HasRootMotion())
        {
            outPose->SetRootMotion(pose1.GetRootMotion());
            outPose->TotalRootMotion = pose1.TotalRootMotion;
        }
        else if (pose2.HasRootMotion())
        {
            outPose->SetRootMotion(pose2.GetRootMotion());
            outPose->TotalRootMotion = pose2.TotalRootMotion;
        }
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
