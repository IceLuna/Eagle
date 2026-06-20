#include "egpch.h"
#include "AnimationSystem.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsRagdollActor.h"
#include "Eagle/Utils/DelaunayTriangulation.h"

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
            // Note: This is the logic we replicate using `std::lower_bound`
            // 
            // for (size_t index = 0; index < positions.size() - 1; ++index)
            // {
            //     if (animationTime <= positions[index + 1].TimeStamp)
            //         return index;
            // }
            
            auto it = std::lower_bound(positions.begin() + 1, positions.end(), animationTime, [](const KeyPosition& key, float animationTime)
            {
                return key.TimeStamp < animationTime;
            });
            return it - positions.begin() - 1;
        }

        /* Gets the current index on KeyRotations to interpolate to based on the
        current animation time */
        static size_t GetRotationIndex(const std::vector<KeyRotation>& rotations, float animationTime)
        {
            auto it = std::lower_bound(rotations.begin() + 1, rotations.end(), animationTime, [](const KeyRotation& key, float animationTime)
            {
                return key.TimeStamp < animationTime;
            });
            return it - rotations.begin() - 1;
        }

        /* Gets the current index on KeyScalings to interpolate to based on the
        current animation time */
        static size_t GetScaleIndex(const std::vector<KeyScale>& scales, float animationTime)
        {
            auto it = std::lower_bound(scales.begin() + 1, scales.end(), animationTime, [](const KeyScale& key, float animationTime)
            {
                return key.TimeStamp < animationTime;
            });
            return it - scales.begin() - 1;
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

        static void CalculateAdditivePose_Internal(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose)
        {
            const uint64_t nodeHash = node.GetNameHash();

            auto itRef = refPose.FindBone(nodeHash);
            auto bItRefValid = itRef != refPose.Bones.end();
            auto itSrc = sourcePose.FindBone(nodeHash);
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
                Transform& diffTr = resultPose->Bones[nodeHash];
                diffTr = srcAnimTr - refAnimTr;
            }

            for (auto& child : node.Children)
                CalculateAdditivePose_Internal(refPose, sourcePose, child, resultPose);
        }

        static void ApplyAdditive_Internal(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const BoneNode& node, float blendAlpha, SkeletalPose* resultPose)
        {
            const uint64_t nodeHash = node.GetNameHash();

            auto itTarget = targetPose.FindBone(nodeHash);
            auto bItTargetValid = itTarget != targetPose.Bones.end();
            auto itAdditive = additivePose.FindBone(nodeHash);
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
                Transform& diffTr = resultPose->Bones[nodeHash];
                diffTr = lerpedAdditive + targetAnimTr;
            }

            for (auto& child : node.Children)
                ApplyAdditive_Internal(targetPose, additivePose, child, blendAlpha, resultPose);
        }
        
        static void BlendPoses_Internal(const SkeletalPose& pose1, const SkeletalPose& pose2, const BoneNode& node, float blendAlpha, SkeletalPose* outPose)
        {
            const uint64_t nodeHash = node.GetNameHash();

            auto it1 = pose1.FindBone(nodeHash);
            auto it2 = pose2.FindBone(nodeHash);
            const bool bValid1 = it1 != pose1.Bones.end();
            const bool bValid2 = it2 != pose2.Bones.end();

            if (bValid1 || bValid2)
            {
                auto& resultTr = outPose->Bones[nodeHash];
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
        
        static void BlendPoses_Internal(const SkeletalPose& pose1, const SkeletalPose& pose2, const SkeletalPose& pose3, const BoneNode& node, const glm::vec3& buv, SkeletalPose* outPose)
        {
            const uint64_t nodeHash = node.GetNameHash();

            auto it1 = pose1.FindBone(nodeHash);
            auto it2 = pose2.FindBone(nodeHash);
            auto it3 = pose3.FindBone(nodeHash);
            const bool bValid1 = it1 != pose1.Bones.end();
            const bool bValid2 = it2 != pose2.Bones.end();
            const bool bValid3 = it3 != pose3.Bones.end();

            const Transform defaultTr = {};
            const Transform& bone1Tr = bValid1 ? it1->second : defaultTr;
            const Transform& bone2Tr = bValid2 ? it2->second : defaultTr;
            const Transform& bone3Tr = bValid3 ? it3->second : defaultTr;

            if (bValid1 || bValid2 || bValid3)
                outPose->Bones[nodeHash] = Transform::Blend(bone1Tr, bone2Tr, bone3Tr, buv);

            for (auto& child : node.Children)
                BlendPoses_Internal(pose1, pose2, pose3, child, buv, outPose);
        }
    
        static void FilterBone_Internal(const SkeletalPose& pose, BoneNode& node, const std::string& boneName, bool bIgnoreParentLocation, bool bIgnoreParentRotation, bool bIgnoreParentScale, SkeletalPose* outPose, bool bProcess = false)
        {
            if (!bProcess)
            {
                const bool bTargetBone = boneName == node.GetName();
                bProcess = bTargetBone;

                // Only change target bone node
                if (bTargetBone)
                {
                    node.bIgnoreParentLocation = bIgnoreParentLocation;
                    node.bIgnoreParentRotation = bIgnoreParentRotation;
                    node.bIgnoreParentScale = bIgnoreParentScale;
                }
            }

            if (bProcess)
            {
                if (auto it = pose.FindBone(node.GetNameHash()); it != pose.Bones.end())
                    outPose->Bones.emplace(node.GetNameHash(), it->second); // Copy bone transform
            }

            for (auto& child : node.Children)
                FilterBone_Internal(pose, child, boneName, bIgnoreParentLocation, bIgnoreParentRotation, bIgnoreParentScale, outPose, bProcess);
        }
    
        static bool CheckEvent_Forward(float eventTime, float prevTime, float curTime)
        {
            return eventTime >= prevTime && eventTime <= curTime;
        }

        static bool CheckEvent_ForwardLoopedOver(float eventTime, float prevTime, float curTime)
        {
            return eventTime >= prevTime || eventTime <= curTime;
        }

        static bool CheckEvent_Backward(float eventTime, float prevTime, float curTime)
        {
            return eventTime <= prevTime && eventTime >= curTime;
        }

        static bool CheckEvent_BackwardLoopedOver(float eventTime, float prevTime, float curTime)
        {
            return eventTime <= prevTime || eventTime >= curTime;
        }

        // @parentNode. We need to use parent nodes base transformation that's not affected by any other animation.
        static glm::mat4 FilterTransform(const SkeletalMeshInfo& skeletal, const glm::mat4& parentTr, const BoneNode& node, Transform& nodeBoneTr, const BoneNode* parentNode)
        {
            if (!node.bIgnoreParentLocation && !node.bIgnoreParentRotation && !node.bIgnoreParentScale)
                return Math::ToTransformMatrix(nodeBoneTr); // Not ignoring anything

            Transform parent = Math::DecomposeTransformMatrix(parentTr);

            // We revert parent node's transformation, so that calculating "parentNodeTr * nodeTr" gives us back "nodeTr".
            // Basically, embedding `inverse(parentNodeTr)` into `nodeTr`
            if (!node.bIgnoreParentLocation)
                parent.Location = glm::vec3(0);
            if (!node.bIgnoreParentRotation)
                parent.Rotation = Rotator{};
            if (!node.bIgnoreParentScale)
                parent.Scale3D = glm::vec3(1);

            // But we don't need to ignore it completely, we just take parent node's base transformation
            if (parentNode)
            {
                Transform parentBaseTransform;
                if (parentNode != &skeletal.RootBone) // RootBone can be affected by `CoordCorrection`, but since we ignore parent here, we need to apply it again
                    parentBaseTransform = Math::DecomposeTransformMatrix(skeletal.CoordCorrection * parentNode->Transformation);
                else
                    parentBaseTransform = Math::DecomposeTransformMatrix(parentNode->Transformation);

                Transform baseTransform;
                if (node.bIgnoreParentLocation)
                    baseTransform.Location = parentBaseTransform.Location;
                if (node.bIgnoreParentRotation)
                    baseTransform.Rotation = parentBaseTransform.Rotation;
                if (node.bIgnoreParentScale)
                    baseTransform.Scale3D = parentBaseTransform.Scale3D;

                nodeBoneTr = baseTransform + nodeBoneTr;
            }

            const glm::mat4 newBoneTr = glm::inverse(Math::ToTransformMatrix(parent)) * Math::ToTransformMatrix(nodeBoneTr);
            nodeBoneTr = Math::DecomposeTransformMatrix(newBoneTr);
            return newBoneTr;
        }

        static void FinalizePose_Internal(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms, const BoneNode* parentNode = nullptr)
        {
            const uint64_t nodeHash = node.GetNameHash();
            glm::mat4 globalTransformation;
            if (auto it = pose.FindBone(nodeHash); it != pose.Bones.end())
            {
                auto& boneTr = it->second;
                globalTransformation = parentTransform * Utils::FilterTransform(skeletal, parentTransform, node, boneTr, parentNode);
            }
            else
            {
                pose.Bones[nodeHash] = Math::DecomposeTransformMatrix(node.Transformation);
                globalTransformation = parentTransform * node.Transformation;
            }

            if (auto it = skeletal.FindBoneInfo(nodeHash); skeletal.IsValid(it))
            {
                const uint32_t index = it->second.BoneID;
                const glm::mat4& offset = it->second.Offset;
                if (index >= outTransforms.size())
                    outTransforms.resize(index + 1);

                outTransforms[index] = skeletal.InverseTransform * globalTransformation * offset;
            }

            for (auto& child : node.Children)
                FinalizePose_Internal(pose, child, globalTransformation, skeletal, outTransforms, &node);
        }

        static void FinalizePose_Internal(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, const BoneNode* parentNode = nullptr)
        {
            const uint64_t nodeHash = node.GetNameHash();
            glm::mat4 globalTransformation;
            if (auto it = pose.FindBone(nodeHash); it != pose.Bones.end())
            {
                auto& bone = it->second;
                globalTransformation = parentTransform * Utils::FilterTransform(skeletal, parentTransform, node, bone, parentNode);
            }
            else
            {
                pose.Bones[nodeHash] = Math::DecomposeTransformMatrix(node.Transformation);
                globalTransformation = parentTransform * node.Transformation;
            }

            for (auto& child : node.Children)
                FinalizePose_Internal(pose, child, globalTransformation, skeletal, &node);
        }

        static void CalculateBlendSpaceVertexAnimation(const Delaunay::Vertex& v, const SkeletalMeshInfo& skeletal, double prevTimeSeconds, double currentTimeSeconds, bool bGatherAnimEvents, const glm::mat4& parentTransform, SkeletalPose* outPose)
        {
            BlendSpaceVertex* bsVertex = (BlendSpaceVertex*)v.UserData;
            if (bsVertex && (bsVertex->Animation))
            {
                const SkeletalMeshAnimation* meshAnim = bsVertex->Animation->GetAnimation().get();
                const float ticksPerSecond = bsVertex->AnimSpeed * meshAnim->TicksPerSecond;
                const double currentTime = AnimationSystem::WrapAnimationTime(double(meshAnim->Duration), currentTimeSeconds * ticksPerSecond, true);
                AnimationSystem::AnimationClip(skeletal, meshAnim, skeletal.RootBone, float(currentTime), outPose);
                if (bGatherAnimEvents)
                {
                    const double prevTime = AnimationSystem::WrapAnimationTime(double(meshAnim->Duration), prevTimeSeconds * ticksPerSecond, true);
                    AnimationSystem::GetEventsToTrigger(meshAnim, float(prevTime), float(currentTime), bsVertex->AnimSpeed, bsVertex->AnimSpeed, &outPose->EventsToTrigger);
                }
            }
            else
            {
                AnimationSystem::FinalizePose(*outPose, skeletal.RootBone, parentTransform, skeletal);
            }
        }

        // Same as above, but the time isn't provided as seconds, but in ticks as alpha (in [0; 1] range of the duration)
        static void CalculateBlendSpaceVertexAnimation_TimeInTicksAlpha(const Delaunay::Vertex& v, const SkeletalMeshInfo& skeletal, double prevTime, double currentTime, bool bGatherAnimEvents, const glm::mat4& parentTransform, SkeletalPose* outPose)
        {
            BlendSpaceVertex* bsVertex = (BlendSpaceVertex*)v.UserData;
            if (bsVertex && (bsVertex->Animation))
            {
                const SkeletalMeshAnimation* meshAnim = bsVertex->Animation->GetAnimation().get();
                currentTime *= meshAnim->Duration;
                AnimationSystem::AnimationClip(skeletal, meshAnim, skeletal.RootBone, float(currentTime), outPose);
                if (bGatherAnimEvents)
                {
                    prevTime *= meshAnim->Duration;
                    AnimationSystem::GetEventsToTrigger(meshAnim, float(prevTime), float(currentTime), bsVertex->AnimSpeed, bsVertex->AnimSpeed, &outPose->EventsToTrigger);
                }
            }
            else
            {
                AnimationSystem::FinalizePose(*outPose, skeletal.RootBone, parentTransform, skeletal);
            }
        }
    }

    ThreadPool AnimationSystem::s_ThreadPool("AnimationSystem", std::thread::hardware_concurrency() - 1u, false);

    ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> AnimationSystem::s_Transforms;
    ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>> AnimationSystem::s_EmittersTransforms;

    static_assert(std::is_same<decltype(AnimationEventData::EntityID), EntityIDType>::value);

    void AnimationSystem::Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms, SkeletalPose* outPose)
    {
        outTransforms->clear();
        outTransforms->reserve(100);
        
        const auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
        outPose->Reset();
        if (animation)
        {
            outPose->Bones.reserve(animation->GetNumBones());
            AnimationClip(skeletalInfo, animation, skeletalInfo.RootBone, currentTime, outPose);
        }

        glm::mat4 rootTransform = glm::mat4(1.f);
        FinalizePose(*outPose, skeletalInfo.RootBone, rootTransform, skeletalInfo, *outTransforms);
    }
    
    float AnimationSystem::StepForwardAnimTime(const SkeletalMeshAnimation* animation, float currentTime, float ts, bool bLoop)
    {
        currentTime += animation->TicksPerSecond * ts;
        currentTime = WrapAnimationTime(animation->Duration, currentTime, bLoop);
        return currentTime;
    }

    bool AnimationSystem::IsValidTime(const SkeletalMeshAnimation* animation, float currentTime)
    {
        return currentTime <= animation->Duration;
    }

    void AnimationSystem::GetEventsToTrigger(const SkeletalMeshAnimation* animation, float prevTime, float curTime, float prevSpeed, float curSpeed, std::vector<AnimationEvent>* outEvents)
    {
        if (prevSpeed < 0 && curSpeed > 0 || curSpeed < 0 && prevSpeed > 0) // If speed changed signs
            std::swap(curTime, prevTime);

        if (curSpeed == 0.f)
            return;

        const bool bForward = curSpeed > 0.f;
        const bool bLoopedOver = bForward ? curTime < prevTime : curTime > prevTime;

        const auto comparisonFunc = bForward ?
            bLoopedOver ? &Utils::CheckEvent_ForwardLoopedOver : &Utils::CheckEvent_Forward :
            bLoopedOver ? &Utils::CheckEvent_BackwardLoopedOver : &Utils::CheckEvent_Backward;

        for (const auto& event : animation->Events)
        {
            if (comparisonFunc(event.Time, prevTime, curTime))
                outEvents->push_back(event);
        }
    }
    
    ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> AnimationSystem::Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts, bool bApplyRootMotion, std::vector<AnimationEventData>* outEventsToTrigger)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        s_ThreadPool->wait();

        s_Transforms.clear();
        s_Transforms.reserve(meshes.size());
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

            s_ThreadPool->detach_task([&asset, mesh, ts]()
            {
                const auto& skeletalMesh = asset->GetMesh();
                auto& transforms = s_Transforms[mesh->Parent.GetID()];
                if (mesh->IsRagdollEnabled())
                {
                    // When it's in a ragdoll state, we don't update animations,
                    // but rather read `LastPose` which already contains data from ragdoll simulation
                    auto& ragdollActor = mesh->GetRagdollActor();
                    if (ragdollActor->DoesNeedSync())
                    {
                        ragdollActor->SynchronizeTransform(); // Update `LastPose`
                    }
                    const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();
                    FinalizePoseRagdoll(mesh->LastPose, skeletalInfo.RootBone, glm::mat4(1.f), skeletalInfo, transforms);
                    return;
                }

                if (mesh->AnimType == AnimationType::Clip)
                {
                    const auto& animAsset = mesh->GetAnimationAsset();
                    const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
                    Update(skeletalMesh, animation, mesh->CurrentClipPlayTime, &transforms, &mesh->LastPose);
                    if (animation)
                    {
                        if (!animation->bInPlace && animation->HasRootMotion())
                        {
                            const float speed = mesh->ClipPlaybackSpeed;
                            const float prevSpeed = mesh->PrevClipPlaybackSpeed;
                            float currentTime = mesh->CurrentClipPlayTime;
                            float prevTime = mesh->PrevClipPlayTime;
                            if (prevSpeed < 0 && speed > 0 || speed < 0 && prevSpeed > 0) // If speed changed signs
                                std::swap(currentTime, prevTime);
                            if (speed == 0.f && prevSpeed != speed) // If speed stoped
                                prevTime = currentTime;

                            mesh->LastPose.SetRootMotion(CalculateRootMotion(animation, currentTime, prevTime, speed, ts, &(mesh->LastPose.TotalRootMotion)));
                        }
                        AnimationSystem::GetEventsToTrigger(animation, mesh->PrevClipPlayTime, mesh->CurrentClipPlayTime, mesh->PrevClipPlaybackSpeed, mesh->ClipPlaybackSpeed, &(mesh->LastPose.EventsToTrigger));

                        mesh->PrevClipPlayTime = mesh->CurrentClipPlayTime;
                        mesh->CurrentClipPlayTime = StepForwardAnimTime(animation, mesh->CurrentClipPlayTime, ts * mesh->ClipPlaybackSpeed, mesh->bClipLooping);
                        mesh->PrevClipPlaybackSpeed = mesh->ClipPlaybackSpeed;
                    }
                }
                else
                {
                    mesh->LastPose.Reset();
                    if (const auto& graph = mesh->GetAnimationGraph())
                    {
                        graph->Update(ts, &transforms);
                        mesh->LastPose = graph->GetPose();
                    }
                    else
                    {
                        const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();
                        FinalizePose(mesh->LastPose, skeletalInfo.RootBone, glm::mat4(1.f), skeletalInfo, transforms);
                    }
                }
            });
        }

        s_ThreadPool->wait();

        for (auto& mesh : meshes)
        {
            if (bApplyRootMotion && mesh->LastPose.HasRootMotion())
                ApplyRootMotion(mesh, mesh->LastPose.TotalRootMotion, mesh->LastPose.GetRootMotion());

            if (outEventsToTrigger)
            {
                auto& events = mesh->LastPose.GetEventsToTrigger();
                if (!events.empty())
                {
                    auto& data = outEventsToTrigger->emplace_back();
                    data.EntityID = mesh->Parent.GetID();
                    data.Events = std::move(events);
                }
            }
        }

        return s_Transforms;
    }

    ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> AnimationSystem::UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        s_ThreadPool->wait();
        s_Transforms.clear();
        s_Transforms.reserve(meshes.size());

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

            s_ThreadPool->detach_task([mesh, ts, &asset]()
            {
                const auto& skeletalMesh = asset->GetMesh();
                auto& transforms = s_Transforms[mesh->Parent.GetID()];
                const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();
                if (mesh->IsRagdollEnabled())
                {
                    // When it's in a ragdoll state, we don't update animations,
                    // but rather read `LastPose` which already contains data from ragdoll simulation
                    auto& ragdollActor = mesh->GetRagdollActor();
                    if (ragdollActor->DoesNeedSync())
                    {
                        ragdollActor->SynchronizeTransform(); // Update `LastPose`
                    }
                    FinalizePoseRagdoll(mesh->LastPose, skeletalInfo.RootBone, glm::mat4(1.f), skeletalInfo, transforms);
                    return;
                }

                mesh->LastPose.Reset();

                glm::mat4 rootTransform = glm::mat4(1.f);
                FinalizePose(mesh->LastPose, skeletalInfo.RootBone, rootTransform, skeletalInfo, transforms);
            });
        }
        s_ThreadPool->wait();

        return s_Transforms;
    }
    
    ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>> AnimationSystem::Update(const std::vector<ParticleSystemComponent*>& systems, float ts, std::vector<AnimationEventData>* outEventsToTrigger)
    {
        if (systems.empty())
            return {};

        EG_CPU_TIMING_SCOPED("Animation System. Update Particle System animations");

        s_ThreadPool->wait();

        s_EmittersTransforms.clear();
        s_EmittersTransforms.reserve(systems.size());

        for (auto& system : systems)
        {
            const auto& asset = system->GetAsset();
            if (!asset)
                continue;

            auto& perEmitterTransforms = s_EmittersTransforms[system->GetSystemID()];
            const auto& emitters = asset->GetEmitters();
            const size_t emittersCount = emitters.size();

            // Allocate enough memory for all emitters to avoid reallocations during multi-threaded calculations
            for (size_t i = 0; i < emittersCount; ++i)
            {
                const auto& emitter = emitters[i];
                if (emitter.IsSkeletalMeshUsed())
                    perEmitterTransforms.emplace(emitter.ID, std::vector<glm::mat4>{});
            }
        }

        for (auto& system : systems)
        {
            const auto& asset = system->GetAsset();
            if (!asset)
                continue;

            const auto& emitters = asset->GetEmitters();
            const size_t emittersCount = emitters.size();
            auto& perEmitterTransforms = s_EmittersTransforms[system->GetSystemID()];

            for (size_t i = 0; i < emittersCount; ++i)
            {
                const auto& emitter = emitters[i];
                if (!emitter.IsSkeletalMeshUsed())
                    continue;

                auto& transforms = perEmitterTransforms.at(emitter.ID);

                s_ThreadPool->detach_task([system, &emitter, &transforms, i, ts]()
                {
                    const auto& skeletalMesh = Cast<AssetSkeletalMesh>(emitter.MeshAsset)->GetMesh();
                    auto& animData = system->PerEmitterAnimData[i];
                    if (animData.SrcOfLastPose && animData.SrcOfLastPose.HasComponent<SkeletalMeshComponent>())
                    {
                        const auto& skComp = animData.SrcOfLastPose.GetComponent<SkeletalMeshComponent>();
                        animData.LastPose = skComp.LastPose;

                        const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();
                        FinalizePose(animData.LastPose, skeletalInfo.RootBone, glm::mat4(1.f), skeletalInfo, transforms);

                        animData.SrcOfLastPose = Entity::Null;
                    }
                    else
                    {

                        const auto& animAsset = emitter.MeshAnimationAsset;
                        const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
                        Update(skeletalMesh, animation, animData.CurrentClipPlayTime, &transforms, &animData.LastPose);
                        if (animation)
                        {
                            if (emitter.bTriggerAnimationEvents)
                                AnimationSystem::GetEventsToTrigger(animation, animData.PrevClipPlayTime, animData.CurrentClipPlayTime, animData.PrevClipPlaybackSpeed, emitter.ClipPlaybackSpeed, &(animData.LastPose.EventsToTrigger));

                            animData.PrevClipPlayTime = animData.CurrentClipPlayTime;
                            animData.CurrentClipPlayTime = StepForwardAnimTime(animation, animData.CurrentClipPlayTime, ts * emitter.ClipPlaybackSpeed, emitter.bClipLooping);
                            animData.PrevClipPlaybackSpeed = emitter.ClipPlaybackSpeed;
                        }
                    }
                });
            }
        }

        s_ThreadPool->wait();

        if (outEventsToTrigger)
        {
            for (auto& system : systems)
            {
                const auto& asset = system->GetAsset();
                if (!asset)
                    continue;

                const auto& emitters = asset->GetEmitters();
                const size_t emittersCount = emitters.size();
                for (size_t i = 0; i < emittersCount; ++i)
                {
                    const auto& emitter = emitters[i];
                    if (!emitter.bTriggerAnimationEvents)
                        continue;

                    auto& pose = system->PerEmitterAnimData[i].LastPose;
                    auto& events = pose.GetEventsToTrigger();
                    if (!events.empty())
                    {
                        auto& data = outEventsToTrigger->emplace_back();
                        data.EntityID = system->Parent.GetID();
                        data.Events = std::move(events);
                    }
                }
            }
        }

        return s_EmittersTransforms;
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
        const glm::vec3 locationMask = glm::vec3(
            mesh->IsRootMotionLockFlagSet(RootMotionLockFlag::PositionX) ? 0.f : 1.f,
            mesh->IsRootMotionLockFlagSet(RootMotionLockFlag::PositionY) ? 0.f : 1.f,
            mesh->IsRootMotionLockFlagSet(RootMotionLockFlag::PositionZ) ? 0.f : 1.f);

        rootMotion.Location *= locationMask;
        rootMotion.Location = glm::rotate((totalRootMotion.Rotation.Conjugate() * mesh->GetWorldTransform().Rotation).GetQuat(), rootMotion.Location);
        const auto& worldTransform = mesh->Parent.GetWorldTransform();
        mesh->Parent.SetWorldTransform(worldTransform + rootMotion);
    }

    const BlendSpaceVertex* AnimationSystem::CalculateBlendSpacePose(const Ref<AssetAnimationBlendSpace>& blendSpace, float x, float y, double prevTimeSeconds, double currentTimeSeconds, SkeletalPose* resultPose)
    {
        const auto& skeletalInfo = blendSpace->GetSkeletalMesh()->GetMesh()->GetSkeletalMeshInfo();

        Delaunay::Triangle tr;
        glm::dvec3 buv;
        if (!FindBlendSpaceSampleTriangle(blendSpace, x, y, &tr, &buv))
        {
            AnimationSystem::FinalizePose(*resultPose, skeletalInfo.RootBone, glm::mat4(1), skeletalInfo);
            return nullptr;
        }

        return CalculateBlendSpacePose(blendSpace, tr, buv, prevTimeSeconds, currentTimeSeconds, resultPose);
    }

    const BlendSpaceVertex* AnimationSystem::CalculateBlendSpacePose(const Ref<AssetAnimationBlendSpace>& blendSpace, const Delaunay::Triangle& tr, const glm::dvec3& buv, double prevTimeSeconds, double currentTimeSeconds, SkeletalPose* resultPose)
    {
        const auto& skeletalInfo = blendSpace->GetSkeletalMesh()->GetMesh()->GetSkeletalMeshInfo();
        const bool bSync = blendSpace->IsSyncEnabled();
        const BlendSpaceVertex* highestWeightedVertex = nullptr;

        const glm::mat4 rootTransform = glm::mat4(1.f);

        const uint32_t highestWeightedAnimIdx =
            buv[0] > buv[1] && buv[0] > buv[2] ? 0 :
            buv[1] > buv[0] && buv[1] > buv[2] ? 1 : 2;
        highestWeightedVertex = (BlendSpaceVertex*)tr.V[highestWeightedAnimIdx].UserData;

        const BlendSpaceEventsTriggerMode animMode = blendSpace->GetEventsTriggerMode();
        bool bGatherEvents[3] = { false, false, false };
        if (animMode != BlendSpaceEventsTriggerMode::None)
        {
            switch (animMode)
            {
            case BlendSpaceEventsTriggerMode::HighestWeightedAnimation:
                bGatherEvents[highestWeightedAnimIdx] = true;
                break;
            case BlendSpaceEventsTriggerMode::AllAnimations:
                bGatherEvents[0] = bGatherEvents[1] = bGatherEvents[2] = true;
                break;
            }
        }

        SkeletalPose poses[3];
        if (bSync)
        {
            // In [0; 1] range
            double highestWeightedCurrentTimeAlpha = 0.0;
            double highestWeightedPrevTimeAlpha = 0.0;
            {
                if (highestWeightedVertex && (highestWeightedVertex->Animation))
                {
                    const SkeletalMeshAnimation* meshAnim = highestWeightedVertex->Animation->GetAnimation().get();
                    const float ticksPerSecond = meshAnim->TicksPerSecond; // Don't apply animation speed here. It's managed in `AnimationGraphNodeBlendSpace`

                    const double currentTime = WrapAnimationTime(double(meshAnim->Duration), currentTimeSeconds * ticksPerSecond, true);
                    const double prevTime = WrapAnimationTime(double(meshAnim->Duration), prevTimeSeconds * ticksPerSecond, true);

                    highestWeightedCurrentTimeAlpha = currentTime / meshAnim->Duration;
                    highestWeightedPrevTimeAlpha = prevTime / meshAnim->Duration;
                }
            }

            for (uint32_t i = 0; i < 3; ++i)
            {
                Utils::CalculateBlendSpaceVertexAnimation_TimeInTicksAlpha(tr.V[i], skeletalInfo, highestWeightedPrevTimeAlpha, highestWeightedCurrentTimeAlpha, bGatherEvents[i], rootTransform, &poses[i]);
            }
        }
        else
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                Utils::CalculateBlendSpaceVertexAnimation(tr.V[i], skeletalInfo, prevTimeSeconds, currentTimeSeconds, bGatherEvents[i], rootTransform, &poses[i]);
            }
        }

        const glm::vec3 buvf = glm::vec3(buv);
        Utils::BlendPoses_Internal(poses[0], poses[1], poses[2], skeletalInfo.RootBone, buvf, resultPose);

        if (poses[0].HasRootMotion() || poses[1].HasRootMotion() || poses[2].HasRootMotion())
        {
            const auto& pose1RM = poses[0].GetRootMotion();
            const auto& pose2RM = poses[1].GetRootMotion();
            const auto& pose3RM = poses[2].GetRootMotion();

            Transform rootMotion = Transform::Blend(pose1RM, pose2RM, pose3RM, buvf);
            resultPose->TotalRootMotion = Transform::Blend(poses[0].TotalRootMotion, poses[1].TotalRootMotion, poses[2].TotalRootMotion, buvf);
            resultPose->TimeTillAnimationLoops = glm::min(poses[0].TimeTillAnimationLoops, glm::min(poses[1].TimeTillAnimationLoops, poses[2].TimeTillAnimationLoops));
        }

        resultPose->EventsToTrigger = poses[0].GetEventsToTrigger();
        resultPose->EventsToTrigger.insert(resultPose->EventsToTrigger.end(), poses[1].GetEventsToTrigger().begin(), poses[1].GetEventsToTrigger().end());
        resultPose->EventsToTrigger.insert(resultPose->EventsToTrigger.end(), poses[2].GetEventsToTrigger().begin(), poses[2].GetEventsToTrigger().end());

        return highestWeightedVertex;
    }

    void AnimationSystem::ClampBlendSpaceInputs(const Ref<AssetAnimationBlendSpace>& blendSpace, float& x, float& y)
    {
        const auto& horAxis = blendSpace->GetHorizontalAxis();
        const auto& verAxis = blendSpace->GetVerticalAxis();

        // Clamp and also move a bit for the edge to avoid anim flicking
        // Since triangle test might fail in some cases if it's on an edge
        const double offset = 0.0001f;
        x = glm::clamp(x, float(horAxis.Min + offset), float(horAxis.Max - offset));
        y = glm::clamp(y, float(verAxis.Min + offset), float(verAxis.Max - offset));
    }

    bool AnimationSystem::FindBlendSpaceSampleTriangle(const Ref<AssetAnimationBlendSpace>& blendSpace, float x, float y, Delaunay::Triangle* outTriangle, glm::dvec3* outBUV)
    {
        const auto& triangulation = blendSpace->GetTriangulation();
        if (triangulation.empty())
        {
            return false;
        }

        ClampBlendSpaceInputs(blendSpace, x, y);

        const Delaunay::Vertex sampleV{ double(x), double(y) };
        for (const auto& tr : triangulation)
        {
            const glm::dvec3 buv = tr.CalculateBarycentric(sampleV);
            if (tr.InTriangle(buv))
            {
                *outTriangle = tr;
                *outBUV = buv;
                return true;
            }
        }

        return false;
    }

    void AnimationSystem::CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const BoneNode& node, SkeletalPose* resultPose)
    {
        Utils::CalculateAdditivePose_Internal(refPose, sourcePose, node, resultPose);
        resultPose->TimeTillAnimationLoops = sourcePose.TimeTillAnimationLoops; // We're probably interested in the source pose, not reference

        resultPose->EventsToTrigger = refPose.GetEventsToTrigger();
        resultPose->EventsToTrigger.insert(resultPose->EventsToTrigger.end(), sourcePose.GetEventsToTrigger().begin(), sourcePose.GetEventsToTrigger().end());
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
        resultPose->TimeTillAnimationLoops = glm::min(targetPose.TimeTillAnimationLoops, additivePose.TimeTillAnimationLoops);

        resultPose->EventsToTrigger = targetPose.GetEventsToTrigger();
        resultPose->EventsToTrigger.insert(resultPose->EventsToTrigger.end(), additivePose.GetEventsToTrigger().begin(), additivePose.GetEventsToTrigger().end());
    }

    void AnimationSystem::BlendPoses(const SkeletalPose& pose1, const SkeletalPose& pose2, const BoneNode& node, float blendAlpha, SkeletalPose* outPose)
    {
        // We check if the animation was filtered.
        // In that case, we avoid this optimization
        // Because the user probably wants to combine animations.
        const bool bWasFiltered = pose1.bWasFiltered || pose2.bWasFiltered;
        if (!bWasFiltered)
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
        outPose->TimeTillAnimationLoops = glm::min(pose1.TimeTillAnimationLoops, pose2.TimeTillAnimationLoops);

        outPose->EventsToTrigger = pose1.GetEventsToTrigger();
        outPose->EventsToTrigger.insert(outPose->EventsToTrigger.end(), pose2.GetEventsToTrigger().begin(), pose2.GetEventsToTrigger().end());
    }

    void AnimationSystem::AnimationClip(const SkeletalMeshInfo& skeletal, const SkeletalMeshAnimation* animation, const BoneNode& node, float currentTime, SkeletalPose* outPose)
    {
        const uint64_t nodeHash = node.GetNameHash();

        if (auto it = animation->FindBone(nodeHash); animation->IsValid(it))
        {
            const bool bRoot = &node == &skeletal.RootBone;

            const auto& bone = it->second;
            auto& tr = outPose->Bones[nodeHash];
            tr.Location = (animation->bInPlace && bRoot) ? bone.Locations[0].Location : Utils::InterpolatePositionRaw(bone, currentTime);
            tr.Rotation = Utils::InterpolateRotationRaw(bone, currentTime);
            tr.Scale3D = Utils::InterpolateScalingRaw(bone, currentTime);
        }

        for (auto& child : node.Children)
            AnimationClip(skeletal, animation, child, currentTime, outPose);
    }

    void AnimationSystem::FilterPose(const SkeletalPose& pose, BoneNode& node, const std::string& boneName, bool bIgnoreParentLocation, bool bIgnoreParentRotation, bool bIgnoreParentScale, SkeletalPose* outPose)
    {
        const size_t nameHash = Utils::CalculateBoneNameHash(boneName);
        if (boneName.empty() || (pose.FindBone(nameHash) == pose.Bones.end()))
        {
            *outPose = pose;
            return;
        }

        Utils::FilterBone_Internal(pose, node, boneName, bIgnoreParentLocation, bIgnoreParentRotation, bIgnoreParentScale, outPose);
        outPose->bWasFiltered = true;
        if (pose.HasRootMotion())
        {
            outPose->SetRootMotion(pose.GetRootMotion());
            outPose->TotalRootMotion = pose.TotalRootMotion;
        }

        outPose->TimeTillAnimationLoops = pose.TimeTillAnimationLoops;
    }

    void AnimationSystem::FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms)
    {
        Utils::FinalizePose_Internal(pose, node, parentTransform, skeletal, outTransforms);
    }

    void AnimationSystem::FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal)
    {
        Utils::FinalizePose_Internal(pose, node, parentTransform, skeletal);
    }
    
    void AnimationSystem::FinalizePoseRagdoll(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms)
    {
        const uint64_t nodeHash = node.GetNameHash();
        glm::mat4 globalTransformation;
        if (auto it = pose.FindBone(nodeHash); it != pose.Bones.end())
        {
            const auto& bone = it->second;
            globalTransformation = Math::ToTransformMatrix(bone); // It's already a global transform
        }
        else
        {
            globalTransformation = parentTransform * node.Transformation;
            pose.Bones[nodeHash] = Math::DecomposeTransformMatrix(globalTransformation);
        }

        if (auto it = skeletal.FindBoneInfo(nodeHash); skeletal.IsValid(it))
        {
            const uint32_t index = it->second.BoneID;
            const glm::mat4& offset = it->second.Offset;
            if (index >= outTransforms.size())
                outTransforms.resize(index + 1);

            outTransforms[index] = skeletal.InverseTransform * globalTransformation * offset;
        }

        for (auto& child : node.Children)
            FinalizePoseRagdoll(pose, child, globalTransformation, skeletal, outTransforms);
    }

    double AnimationSystem::WrapAnimationTime(double duration, double currentTime, bool bLoop)
    {
        if (currentTime > duration)
            currentTime = bLoop ? currentTime - (glm::floor(currentTime / duration) * duration) : duration;
        else if (currentTime < 0.f) // Animation is playing in reverse
            currentTime = bLoop ? glm::abs(currentTime - (glm::floor(currentTime / duration) * duration)) : 0.f;

        return currentTime;
    }

    float AnimationSystem::WrapAnimationTime(float duration, float currentTime, bool bLoop)
    {
        return (float)WrapAnimationTime(double(duration), double(currentTime), bLoop);
    }
}
