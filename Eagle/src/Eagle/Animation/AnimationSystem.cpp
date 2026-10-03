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
        // Gets normalized value for Lerp & Slerp. Clamped so that times outside of the key range don't extrapolate
        static float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
        {
            const float midWayLength = animationTime - lastTimeStamp;
            const float framesDiff = nextTimeStamp - lastTimeStamp;
            return glm::clamp(midWayLength / glm::max(framesDiff, 0.000001f), 0.f, 1.f);
        }

        // Returns index `i` of the key to interpolate from (`i + 1` is the key to interpolate to). `keys.size()` must be >= 2
        template<typename Key>
        static size_t FindKeyIndex(const std::vector<Key>& keys, float animationTime)
        {
            auto it = std::lower_bound(keys.begin() + 1, keys.end() - 1, animationTime, [](const Key& key, float time)
            {
                return key.TimeStamp < time;
            });
            return size_t(it - keys.begin()) - 1;
        }

        static glm::vec3 InterpolatePositionRaw(const BoneAnimation& bone, float animationTime)
        {
            const auto& locations = bone.Locations;
            if (locations.empty())
                return glm::vec3(0.f);
            if (locations.size() == 1)
                return locations[0].Location;

            const size_t p0Index = FindKeyIndex(locations, animationTime);
            const size_t p1Index = p0Index + 1;
            const float scaleFactor = GetScaleFactor(locations[p0Index].TimeStamp, locations[p1Index].TimeStamp, animationTime);
            return glm::mix(locations[p0Index].Location, locations[p1Index].Location, scaleFactor);
        }

        static glm::quat InterpolateRotationRaw(const BoneAnimation& bone, float animationTime)
        {
            const auto& rotations = bone.Rotations;
            if (rotations.empty())
                return glm::quat(1.f, 0.f, 0.f, 0.f);
            if (rotations.size() == 1)
                return glm::normalize(rotations[0].Rotation);

            const size_t p0Index = FindKeyIndex(rotations, animationTime);
            const size_t p1Index = p0Index + 1;
            const float scaleFactor = GetScaleFactor(rotations[p0Index].TimeStamp, rotations[p1Index].TimeStamp, animationTime);
            // Adjacent keys are close to each other, so nlerp should be visually identical to slerp here, but a lot cheaper
            return Transform::NLerp(rotations[p0Index].Rotation, rotations[p1Index].Rotation, scaleFactor);
        }

        static glm::vec3 InterpolateScalingRaw(const BoneAnimation& bone, float animationTime)
        {
            const auto& scales = bone.Scales;
            if (scales.empty())
                return glm::vec3(1.f);
            if (scales.size() == 1)
                return scales[0].Scale;

            const size_t p0Index = FindKeyIndex(scales, animationTime);
            const size_t p1Index = p0Index + 1;
            const float scaleFactor = GetScaleFactor(scales[p0Index].TimeStamp, scales[p1Index].TimeStamp, animationTime);
            return glm::mix(scales[p0Index].Scale, scales[p1Index].Scale, scaleFactor);
        }

        static Transform FirstKeyTransform(const BoneAnimation& bone)
        {
            Transform result;
            if (!bone.Locations.empty())
                result.Location = bone.Locations.front().Location;
            if (!bone.Rotations.empty())
                result.Rotation = bone.Rotations.front().Rotation;
            if (!bone.Scales.empty())
                result.Scale3D = bone.Scales.front().Scale;
            return result;
        }

        static Transform LastKeyTransform(const BoneAnimation& bone)
        {
            Transform result;
            if (!bone.Locations.empty())
                result.Location = bone.Locations.back().Location;
            if (!bone.Rotations.empty())
                result.Rotation = bone.Rotations.back().Rotation;
            if (!bone.Scales.empty())
                result.Scale3D = bone.Scales.back().Scale;
            return result;
        }

        static void CalculateAdditivePose_Internal(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const SkeletalMeshInfo& skeletal, SkeletalPose* resultPose)
        {
            for (const auto& entry : skeletal.FlattenedBones)
            {
                const uint64_t nodeHash = entry.Node->GetNameHash();

                auto itRef = refPose.FindBone(nodeHash);
                const bool bItRefValid = itRef != refPose.Bones.end();
                auto itSrc = sourcePose.FindBone(nodeHash);
                const bool bItSrcValid = itSrc != sourcePose.Bones.end();

                if (bItRefValid || bItSrcValid)
                {
                    const Transform refAnimTr = bItRefValid ? itRef->second : Transform{};
                    const Transform srcAnimTr = bItSrcValid ? itSrc->second : Transform{};
                    resultPose->Bones[nodeHash] = srcAnimTr - refAnimTr;
                }
            }
        }

        static void ApplyAdditive_Internal(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const SkeletalMeshInfo& skeletal, float blendAlpha, SkeletalPose* resultPose)
        {
            for (const auto& entry : skeletal.FlattenedBones)
            {
                const uint64_t nodeHash = entry.Node->GetNameHash();

                auto itTarget = targetPose.FindBone(nodeHash);
                const bool bItTargetValid = itTarget != targetPose.Bones.end();
                auto itAdditive = additivePose.FindBone(nodeHash);
                const bool bItAdditiveValid = itAdditive != additivePose.Bones.end();

                if (bItTargetValid || bItAdditiveValid)
                {
                    const Transform targetAnimTr = bItTargetValid ? itTarget->second : Transform{};
                    const Transform additiveAnimTr = bItAdditiveValid ? itAdditive->second : Transform{};
                    const Transform lerpedAdditive = Transform::BlendFast(Transform{}, additiveAnimTr, blendAlpha);
                    resultPose->Bones[nodeHash] = lerpedAdditive + targetAnimTr;
                }
            }
        }
        
        static void BlendPoses_Internal(const SkeletalPose& pose1, const SkeletalPose& pose2, const SkeletalMeshInfo& skeletal, float blendAlpha, SkeletalPose* outPose)
        {
            for (const auto& entry : skeletal.FlattenedBones)
            {
                const uint64_t nodeHash = entry.Node->GetNameHash();

                auto it1 = pose1.FindBone(nodeHash);
                auto it2 = pose2.FindBone(nodeHash);
                const bool bValid1 = it1 != pose1.Bones.end();
                const bool bValid2 = it2 != pose2.Bones.end();

                if (bValid1 && bValid2)
                    outPose->Bones[nodeHash] = Transform::BlendFast(it1->second, it2->second, blendAlpha);
                else if (bValid1)
                    outPose->Bones[nodeHash] = it1->second;
                else if (bValid2)
                    outPose->Bones[nodeHash] = it2->second;
            }
        }
        
        static void BlendPoses_Internal(const SkeletalPose& pose1, const SkeletalPose& pose2, const SkeletalPose& pose3, const SkeletalMeshInfo& skeletal, const glm::vec3& buv, SkeletalPose* outPose)
        {
            const Transform defaultTr = {};
            for (const auto& entry : skeletal.FlattenedBones)
            {
                const uint64_t nodeHash = entry.Node->GetNameHash();

                auto it1 = pose1.FindBone(nodeHash);
                auto it2 = pose2.FindBone(nodeHash);
                auto it3 = pose3.FindBone(nodeHash);
                const bool bValid1 = it1 != pose1.Bones.end();
                const bool bValid2 = it2 != pose2.Bones.end();
                const bool bValid3 = it3 != pose3.Bones.end();

                if (bValid1 || bValid2 || bValid3)
                {
                    const Transform& bone1Tr = bValid1 ? it1->second : defaultTr;
                    const Transform& bone2Tr = bValid2 ? it2->second : defaultTr;
                    const Transform& bone3Tr = bValid3 ? it3->second : defaultTr;
                    outPose->Bones[nodeHash] = Transform::Blend(bone1Tr, bone2Tr, bone3Tr, buv);
                }
            }
        }
    
        static void FilterBone_Internal(const SkeletalPose& pose, const BoneNode& node, uint64_t targetHash, SkeletalPose* outPose, bool bProcess = false)
        {
            const uint64_t nodeHash = node.GetNameHash();
            bProcess = bProcess || (nodeHash == targetHash);

            if (bProcess)
            {
                if (auto it = pose.FindBone(nodeHash); it != pose.Bones.end())
                    outPose->Bones.emplace(nodeHash, it->second); // Copy bone transform
            }

            for (const auto& child : node.Children)
                FilterBone_Internal(pose, child, targetHash, outPose, bProcess);
        }

        // @parentNode. We need to use parent nodes base transformation that's not affected by any other animation.
        // @parentOverride. Per-pose "ignore parent" settings for this bone. Can be null
        static glm::mat4 FilterTransform(const SkeletalMeshInfo& skeletal, const glm::mat4& parentTr, Transform& nodeBoneTr, const BoneNode* parentNode, const BoneParentOverride* parentOverride)
        {
            if (!parentOverride || (!parentOverride->bIgnoreParentLocation && !parentOverride->bIgnoreParentRotation && !parentOverride->bIgnoreParentScale))
                return Math::ToTransformMatrix(nodeBoneTr); // Not ignoring anything

            const bool bIgnoreParentLocation = parentOverride->bIgnoreParentLocation;
            const bool bIgnoreParentRotation = parentOverride->bIgnoreParentRotation;
            const bool bIgnoreParentScale = parentOverride->bIgnoreParentScale;

            Transform parent = Math::DecomposeTransformMatrix(parentTr);

            // We revert parent node's transformation, so that calculating "parentNodeTr * nodeTr" gives us back "nodeTr".
            // Basically, embedding `inverse(parentNodeTr)` into `nodeTr`
            if (!bIgnoreParentLocation)
                parent.Location = glm::vec3(0);
            if (!bIgnoreParentRotation)
                parent.Rotation = Rotator{};
            if (!bIgnoreParentScale)
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
                if (bIgnoreParentLocation)
                    baseTransform.Location = parentBaseTransform.Location;
                if (bIgnoreParentRotation)
                    baseTransform.Rotation = parentBaseTransform.Rotation;
                if (bIgnoreParentScale)
                    baseTransform.Scale3D = parentBaseTransform.Scale3D;

                nodeBoneTr = baseTransform + nodeBoneTr;
            }

            const glm::mat4 newBoneTr = glm::inverse(Math::ToTransformMatrix(parent)) * Math::ToTransformMatrix(nodeBoneTr);
            nodeBoneTr = Math::DecomposeTransformMatrix(newBoneTr);
            return newBoneTr;
        }

        static void FinalizePose_Internal(SkeletalPose& pose, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>* outTransforms)
        {
            std::vector<glm::mat4> globalTransforms;
            globalTransforms.resize(skeletal.FlattenedBones.size());

            if (outTransforms && outTransforms->size() < skeletal.GetNumBoneTransforms())
                outTransforms->resize(skeletal.GetNumBoneTransforms());

            const bool bHasOverrides = !pose.ParentOverrides.empty();
            for (size_t i = 0; i < skeletal.FlattenedBones.size(); ++i)
            {
                const auto& entry = skeletal.FlattenedBones[i];
                const BoneNode& curNode = *entry.Node;
                const uint64_t nodeHash = curNode.GetNameHash();
                const glm::mat4& parentTr = (entry.ParentIndex < 0) ? parentTransform : globalTransforms[entry.ParentIndex];

                glm::mat4 globalTransformation;
                if (auto it = pose.FindBone(nodeHash); it != pose.Bones.end())
                {
                    const BoneParentOverride* parentOverride = bHasOverrides ? pose.FindParentOverride(nodeHash) : nullptr;
                    globalTransformation = parentTr * Utils::FilterTransform(skeletal, parentTr, it->second, entry.ParentNode, parentOverride);
                }
                else
                {
                    pose.Bones[nodeHash] = Math::DecomposeTransformMatrix(curNode.Transformation);
                    globalTransformation = parentTr * curNode.Transformation;
                }
                globalTransforms[i] = globalTransformation;

                if (outTransforms && entry.bHasBoneInfo)
                {
                    if (entry.BoneID >= outTransforms->size())
                        outTransforms->resize(entry.BoneID + 1);

                    (*outTransforms)[entry.BoneID] = skeletal.InverseTransform * globalTransformation * entry.Offset;
                }
            }
        }

        static void CalculateBlendSpaceVertexRootMotion(const SkeletalMeshAnimation* meshAnim, float prevTime, float currentTime, float speed, SkeletalPose* outPose)
        {
            if (!meshAnim->bInPlace && meshAnim->HasRootMotion())
                outPose->SetRootMotion(AnimationSystem::CalculateRootMotion(meshAnim, currentTime, prevTime, speed, 0.f, &outPose->TotalRootMotion));
        }

        static void CalculateBlendSpaceVertexAnimation(const Delaunay::Vertex& v, const SkeletalMeshInfo& skeletal, double prevTimeSeconds, double currentTimeSeconds, bool bGatherAnimEvents, const glm::mat4& parentTransform, SkeletalPose* outPose)
        {
            BlendSpaceVertex* bsVertex = (BlendSpaceVertex*)v.UserData;
            if (bsVertex && (bsVertex->Animation))
            {
                const SkeletalMeshAnimation* meshAnim = bsVertex->Animation->GetAnimation().get();
                const float ticksPerSecond = bsVertex->AnimSpeed * meshAnim->TicksPerSecond;
                const float currentTime = (float)AnimationSystem::WrapAnimationTime(double(meshAnim->Duration), currentTimeSeconds * ticksPerSecond, true);
                const float prevTime = (float)AnimationSystem::WrapAnimationTime(double(meshAnim->Duration), prevTimeSeconds * ticksPerSecond, true);
                AnimationSystem::AnimationClip(skeletal, meshAnim, skeletal.RootBone, currentTime, outPose);
                CalculateBlendSpaceVertexRootMotion(meshAnim, prevTime, currentTime, bsVertex->AnimSpeed, outPose);
                outPose->TimeTillAnimationLoops = AnimationSystem::CalculateTimeTillAnimationLoops(meshAnim, currentTime, bsVertex->AnimSpeed);
                if (bGatherAnimEvents)
                    AnimationSystem::GetEventsToTrigger(meshAnim, prevTime, currentTime, bsVertex->AnimSpeed, true, &outPose->EventsToTrigger);
            }
            else
            {
                AnimationSystem::FinalizePose(*outPose, skeletal.RootBone, parentTransform, skeletal);
            }
        }

        // Same as above, but the time isn't provided as seconds, but in ticks as alpha (in [0; 1] range of the duration)
        // Note: doesn't set `TimeTillAnimationLoops`, it's handled by the caller
        static void CalculateBlendSpaceVertexAnimation_TimeInTicksAlpha(const Delaunay::Vertex& v, const SkeletalMeshInfo& skeletal, double prevTime, double currentTime, bool bGatherAnimEvents, const glm::mat4& parentTransform, SkeletalPose* outPose)
        {
            BlendSpaceVertex* bsVertex = (BlendSpaceVertex*)v.UserData;
            if (bsVertex && (bsVertex->Animation))
            {
                const SkeletalMeshAnimation* meshAnim = bsVertex->Animation->GetAnimation().get();
                const float currentTimeTicks = float(currentTime * meshAnim->Duration);
                const float prevTimeTicks = float(prevTime * meshAnim->Duration);
                AnimationSystem::AnimationClip(skeletal, meshAnim, skeletal.RootBone, currentTimeTicks, outPose);
                CalculateBlendSpaceVertexRootMotion(meshAnim, prevTimeTicks, currentTimeTicks, bsVertex->AnimSpeed, outPose);
                if (bGatherAnimEvents)
                    AnimationSystem::GetEventsToTrigger(meshAnim, prevTimeTicks, currentTimeTicks, bsVertex->AnimSpeed, true, &outPose->EventsToTrigger);
            }
            else
            {
                AnimationSystem::FinalizePose(*outPose, skeletal.RootBone, parentTransform, skeletal);
            }
        }

        static void UpdateSkeletalMeshComponent(SkeletalMeshComponent* mesh, std::vector<glm::mat4>& transforms, float ts)
        {
            const auto& skeletalMesh = mesh->GetMeshAsset()->GetMesh();
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
                AnimationSystem::FinalizePoseRagdoll(mesh->LastPose, skeletalInfo.RootBone, glm::mat4(1.f), skeletalInfo, transforms);
                return;
            }

            if (mesh->AnimType == AnimationType::Clip)
            {
                const auto& animAsset = mesh->GetAnimationAsset();
                const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
                AnimationSystem::Update(skeletalMesh, animation, mesh->CurrentClipPlayTime, &transforms, &mesh->LastPose);
                if (animation)
                {
                    const float stepSpeed = mesh->PrevClipPlaybackSpeed;
                    if (!animation->bInPlace && animation->HasRootMotion())
                    {
                        mesh->LastPose.SetRootMotion(AnimationSystem::CalculateRootMotion(animation, mesh->CurrentClipPlayTime, mesh->PrevClipPlayTime, stepSpeed, ts, &(mesh->LastPose.TotalRootMotion)));
                    }
                    AnimationSystem::GetEventsToTrigger(animation, mesh->PrevClipPlayTime, mesh->CurrentClipPlayTime, stepSpeed, mesh->bClipLooping, &(mesh->LastPose.EventsToTrigger));

                    mesh->PrevClipPlayTime = mesh->CurrentClipPlayTime;
                    mesh->CurrentClipPlayTime = AnimationSystem::StepForwardAnimTime(animation, mesh->CurrentClipPlayTime, ts * mesh->ClipPlaybackSpeed, mesh->bClipLooping);
                    mesh->PrevClipPlaybackSpeed = mesh->ClipPlaybackSpeed;
                }
            }
            else
            {
                mesh->LastPose.Reset();
                if (const auto& graph = mesh->GetAnimationGraph())
                {
                    graph->Update(ts, &transforms);
                    mesh->LastPose = std::move(graph->GetPose());
                }
                else
                {
                    const auto& skeletalInfo = skeletalMesh->GetSkeletalMeshInfo();
                    AnimationSystem::FinalizePose(mesh->LastPose, skeletalInfo.RootBone, glm::mat4(1.f), skeletalInfo, transforms);
                }
            }
        }

        static uint32_t GetAnimationWorkerThreadsCount()
        {
            const uint32_t hwThreads = std::max(1u, std::thread::hardware_concurrency());
            return hwThreads - 1u;
        }

        struct SkeletalMeshJob
        {
            SkeletalMeshComponent* Mesh = nullptr;
            std::vector<glm::mat4>* Transforms = nullptr;
        };

        static void PrepareSkeletalMeshJobs(const std::vector<SkeletalMeshComponent*>& meshes, ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>>& transformsMap, std::vector<Utils::SkeletalMeshJob>* jobs)
        {
            jobs->clear();
            jobs->reserve(meshes.size());
            for (auto& mesh : meshes)
            {
                if (!mesh->GetMeshAsset())
                    continue;

                jobs->push_back({ mesh, nullptr });
                transformsMap.emplace(mesh->Parent.GetID(), std::vector<glm::mat4>{});
            }

            for (auto& job : *jobs)
                job.Transforms = &transformsMap[job.Mesh->Parent.GetID()];
        }
    }

    ThreadPool AnimationSystem::s_ThreadPool("AnimationSystem", Utils::GetAnimationWorkerThreadsCount(), false);
    static std::vector<Utils::SkeletalMeshJob> s_Jobs;

    ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> AnimationSystem::s_Transforms;
    ankerl::unordered_dense::map<GUID, ankerl::unordered_dense::map<GUID, std::vector<glm::mat4>>> AnimationSystem::s_EmittersTransforms;

    static_assert(std::is_same<decltype(AnimationEventData::EntityID), EntityIDType>::value);

    void AnimationSystem::Update(const Ref<SkeletalMesh>& mesh, const SkeletalMeshAnimation* animation, float currentTime, std::vector<glm::mat4>* outTransforms, SkeletalPose* outPose)
    {
        outTransforms->clear();
        
        const auto& skeletalInfo = mesh->GetSkeletalMeshInfo();
        outPose->Reset();
        if (animation)
        {
            outPose->Bones.reserve(animation->GetNumBones());
            AnimationClip(skeletalInfo, animation, skeletalInfo.RootBone, currentTime, outPose);
        }

        const glm::mat4 rootTransform = glm::mat4(1.f);
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
        return currentTime >= 0.f && currentTime <= animation->Duration;
    }

    float AnimationSystem::CalculateTimeTillAnimationLoops(const SkeletalMeshAnimation* animation, float currentTime, float playbackSpeed)
    {
        constexpr float speedDelta = 0.00001f;
        const float ticksPerSecond = glm::abs(playbackSpeed) * animation->TicksPerSecond;
        if (glm::abs(playbackSpeed) <= speedDelta || ticksPerSecond <= 0.f)
            return FLT_MAX; // Animation is not playing, so we'll never loop

        // Forward - time left till the end. Backwards - time left till the start.
        const float ticksLeft = playbackSpeed > 0.f ? (animation->Duration - currentTime) : currentTime;
        return glm::max(ticksLeft, 0.f) / ticksPerSecond;
    }

    void AnimationSystem::GetEventsToTrigger(const SkeletalMeshAnimation* animation, float prevTime, float curTime, float speed, bool bLoop, std::vector<AnimationEvent>* outEvents)
    {
        if (!animation || speed == 0.f || prevTime == curTime)
            return;

        const bool bForward = speed > 0.f;
        const bool bLoopedOver = bForward ? curTime < prevTime : curTime > prevTime;
        if (bLoopedOver && !bLoop)
            return;

        // A non-looping animation gets clamped at its end. Include that end point so an event placed exactly there fires (once, since the next interval is empty)
        const bool bIncludeCurTime = !bLoop && (bForward ? curTime >= animation->Duration : curTime <= 0.f);

        for (const auto& event : animation->Events)
        {
            const float t = event.Time;
            bool bTrigger = false;
            if (bForward)
            {
                if (bLoopedOver)
                    bTrigger = t >= prevTime || t < curTime; // [prev; Duration] + [0; cur)
                else
                    bTrigger = t >= prevTime && (t < curTime || (bIncludeCurTime && t <= curTime)); // [prev; cur)
            }
            else
            {
                if (bLoopedOver)
                    bTrigger = t <= prevTime || t > curTime; // [0; prev] + (cur; Duration]
                else
                    bTrigger = t <= prevTime && (t > curTime || (bIncludeCurTime && t >= curTime)); // (cur; prev]
            }

            if (bTrigger)
                outEvents->push_back(event);
        }
    }
    
    ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> AnimationSystem::Update(const std::vector<SkeletalMeshComponent*>& meshes, float ts, bool bApplyRootMotion, std::vector<AnimationEventData>* outEventsToTrigger)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        s_ThreadPool->wait();

        s_Transforms.clear();
        s_Transforms.reserve(meshes.size());
        Utils::PrepareSkeletalMeshJobs(meshes, s_Transforms, &s_Jobs);

        s_ThreadPool->detach_loop(size_t(0), s_Jobs.size(), [ts](size_t i)
        {
            Utils::UpdateSkeletalMeshComponent(s_Jobs[i].Mesh, *s_Jobs[i].Transforms, ts);
        });
        s_ThreadPool->wait();

        for (const auto& job : s_Jobs)
        {
            SkeletalMeshComponent* mesh = job.Mesh;
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
                    events.clear();
                }
            }
        }

        return std::exchange(s_Transforms, {});
    }

    ankerl::unordered_dense::map<uint32_t, std::vector<glm::mat4>> AnimationSystem::UpdateBasePose(const std::vector<SkeletalMeshComponent*>& meshes, float ts)
    {
        EG_CPU_TIMING_SCOPED("Animation System. Update");

        s_ThreadPool->wait();
        s_Transforms.clear();
        s_Transforms.reserve(meshes.size());
        Utils::PrepareSkeletalMeshJobs(meshes, s_Transforms, &s_Jobs);

        s_ThreadPool->detach_loop(size_t(0), s_Jobs.size(), [](size_t i)
        {
            SkeletalMeshComponent* mesh = s_Jobs[i].Mesh;
            auto& transforms = *s_Jobs[i].Transforms;
            const auto& skeletalMesh = mesh->GetMeshAsset()->GetMesh();
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

            const glm::mat4 rootTransform = glm::mat4(1.f);
            FinalizePose(mesh->LastPose, skeletalInfo.RootBone, rootTransform, skeletalInfo, transforms);
        });
        s_ThreadPool->wait();

        return std::exchange(s_Transforms, {});
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
                        animData.LastPose.EventsToTrigger.clear();

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
                                AnimationSystem::GetEventsToTrigger(animation, animData.PrevClipPlayTime, animData.CurrentClipPlayTime, animData.PrevClipPlaybackSpeed, emitter.bClipLooping, &(animData.LastPose.EventsToTrigger));

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
                        events.clear();
                    }
                }
            }
        }

        return std::exchange(s_EmittersTransforms, {});
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
                const Transform firstTr = Utils::FirstKeyTransform(animation->RootMotion);
                const Transform lastTr = Utils::LastKeyTransform(animation->RootMotion);

                result = (lastTr - rootMotionPrev) + (*outTotalRootMotion - firstTr);
            }
            else
                result = *outTotalRootMotion - rootMotionPrev;
        }
        else
        {
            if (currentTime > prevTime) // Playing backward, we looped back
            {
                const Transform firstTr = Utils::FirstKeyTransform(animation->RootMotion);
                const Transform lastTr = Utils::LastKeyTransform(animation->RootMotion);

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
        float timeTillLoops = FLT_MAX;
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

                    timeTillLoops = CalculateTimeTillAnimationLoops(meshAnim, float(currentTime), highestWeightedVertex->AnimSpeed);
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
                timeTillLoops = glm::min(timeTillLoops, poses[i].TimeTillAnimationLoops);
            }
        }

        const glm::vec3 buvf = glm::vec3(buv);
        Utils::BlendPoses_Internal(poses[0], poses[1], poses[2], skeletalInfo, buvf, resultPose);

        if (poses[0].HasRootMotion() || poses[1].HasRootMotion() || poses[2].HasRootMotion())
        {
            const Transform rootMotion = Transform::Blend(poses[0].GetRootMotion(), poses[1].GetRootMotion(), poses[2].GetRootMotion(), buvf);
            resultPose->SetRootMotion(rootMotion);
            resultPose->TotalRootMotion = Transform::Blend(poses[0].TotalRootMotion, poses[1].TotalRootMotion, poses[2].TotalRootMotion, buvf);
        }
        resultPose->TimeTillAnimationLoops = timeTillLoops;

        resultPose->EventsToTrigger = poses[0].GetEventsToTrigger();
        resultPose->AppendEvents(poses[1]);
        resultPose->AppendEvents(poses[2]);

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

    void AnimationSystem::CalculateAdditivePose(const SkeletalPose& refPose, const SkeletalPose& sourcePose, const SkeletalMeshInfo& skeletal, SkeletalPose* resultPose)
    {
        Utils::CalculateAdditivePose_Internal(refPose, sourcePose, skeletal, resultPose);
        resultPose->TimeTillAnimationLoops = sourcePose.TimeTillAnimationLoops; // We're probably interested in the source pose, not reference

        resultPose->EventsToTrigger = refPose.GetEventsToTrigger();
        resultPose->AppendEvents(sourcePose);
    }

    void AnimationSystem::ApplyAdditive(const SkeletalPose& targetPose, const SkeletalPose& additivePose, const SkeletalMeshInfo& skeletal, float blendAlpha, SkeletalPose* resultPose)
    {
        if (blendAlpha == 0.f)
        {
            *resultPose = targetPose;
            resultPose->AppendEvents(additivePose);
            return;
        }

        Utils::ApplyAdditive_Internal(targetPose, additivePose, skeletal, blendAlpha, resultPose);
        if (targetPose.HasRootMotion())
        {
            resultPose->SetRootMotion(targetPose.GetRootMotion());
            resultPose->TotalRootMotion = targetPose.TotalRootMotion;
        }
        resultPose->TimeTillAnimationLoops = glm::min(targetPose.TimeTillAnimationLoops, additivePose.TimeTillAnimationLoops);
        resultPose->MergeParentOverrides(targetPose);

        resultPose->EventsToTrigger = targetPose.GetEventsToTrigger();
        resultPose->AppendEvents(additivePose);
    }

    void AnimationSystem::BlendPoses(const SkeletalPose& pose1, const SkeletalPose& pose2, const SkeletalMeshInfo& skeletal, float blendAlpha, SkeletalPose* outPose)
    {
        EG_CORE_ASSERT(outPose != &pose1 && outPose != &pose2, "BlendPoses: `outPose` must not alias an input pose");

        // We check if the animation was filtered.
        // In that case, we avoid this optimization
        // Because the user probably wants to combine animations.
        const bool bWasFiltered = pose1.bWasFiltered || pose2.bWasFiltered;
        if (!bWasFiltered && (blendAlpha == 0.f || blendAlpha == 1.f))
        {
            const SkeletalPose& selected = blendAlpha == 0.f ? pose1 : pose2;
            const SkeletalPose& other = blendAlpha == 0.f ? pose2 : pose1;
            *outPose = selected;

            outPose->AppendEvents(other);
            outPose->MergeParentOverrides(other);
            return;
        }

        outPose->Reset();
        Utils::BlendPoses_Internal(pose1, pose2, skeletal, blendAlpha, outPose);

        if (pose1.HasRootMotion() || pose2.HasRootMotion())
        {
            const Transform rootMotion = Transform::BlendFast(pose1.GetRootMotion(), pose2.GetRootMotion(), blendAlpha);
            outPose->SetRootMotion(rootMotion);
            outPose->TotalRootMotion = Transform::BlendFast(pose1.TotalRootMotion, pose2.TotalRootMotion, blendAlpha);
        }
        outPose->TimeTillAnimationLoops = glm::min(pose1.TimeTillAnimationLoops, pose2.TimeTillAnimationLoops);
        outPose->MergeParentOverrides(pose1);
        outPose->MergeParentOverrides(pose2);

        outPose->EventsToTrigger = pose1.GetEventsToTrigger();
        outPose->AppendEvents(pose2);
    }

    void AnimationSystem::AnimationClip(const SkeletalMeshInfo& skeletal, const SkeletalMeshAnimation* animation, const BoneNode& node, float currentTime, SkeletalPose* outPose)
    {
        EG_CORE_ASSERT(&node == &skeletal.RootBone);

        const BoneNode * inPlaceBone = nullptr;
        if (animation->bInPlace)
        {
            inPlaceBone = animation->FindRootMotionBone(skeletal);
            if (!inPlaceBone)
                inPlaceBone = &skeletal.RootBone;
        }

        for (const auto& entry : skeletal.FlattenedBones)
        {
            const BoneNode& curNode = *entry.Node;
            const uint64_t nodeHash = curNode.GetNameHash();

            if (const BoneAnimation* bone = animation->FindBone(nodeHash))
            {
                auto& tr = outPose->Bones[nodeHash];
                const bool bPinLocation = (&curNode == inPlaceBone) && !bone->Locations.empty();
                tr.Location = bPinLocation ? bone->Locations[0].Location : Utils::InterpolatePositionRaw(*bone, currentTime);
                tr.Rotation = Utils::InterpolateRotationRaw(*bone, currentTime);
                tr.Scale3D = Utils::InterpolateScalingRaw(*bone, currentTime);
            }
        }
    }

    void AnimationSystem::FilterPose(const SkeletalPose& pose, const BoneNode& rootNode, const std::string& boneName, bool bIgnoreParentLocation, bool bIgnoreParentRotation, bool bIgnoreParentScale, SkeletalPose* outPose)
    {
        const uint64_t nameHash = Utils::CalculateBoneNameHash(boneName);
        if (boneName.empty() || (pose.FindBone(nameHash) == pose.Bones.end()))
        {
            *outPose = pose;
            return;
        }

        Utils::FilterBone_Internal(pose, rootNode, nameHash, outPose);
        outPose->bWasFiltered = true;
        if (pose.HasRootMotion())
        {
            outPose->SetRootMotion(pose.GetRootMotion());
            outPose->TotalRootMotion = pose.TotalRootMotion;
        }

        outPose->TimeTillAnimationLoops = pose.TimeTillAnimationLoops;
        outPose->EventsToTrigger = pose.EventsToTrigger;

        outPose->MergeParentOverrides(pose);
        BoneParentOverride parentOverride;
        parentOverride.BoneHash = nameHash;
        parentOverride.bIgnoreParentLocation = bIgnoreParentLocation;
        parentOverride.bIgnoreParentRotation = bIgnoreParentRotation;
        parentOverride.bIgnoreParentScale = bIgnoreParentScale;
        outPose->AddParentOverride(parentOverride);
    }

    void AnimationSystem::FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms)
    {
        EG_CORE_ASSERT(&node == &skeletal.RootBone);
        Utils::FinalizePose_Internal(pose, parentTransform, skeletal, &outTransforms);
    }

    void AnimationSystem::FinalizePose(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal)
    {
        EG_CORE_ASSERT(&node == &skeletal.RootBone);
        Utils::FinalizePose_Internal(pose, parentTransform, skeletal, nullptr);
    }
    
    void AnimationSystem::FinalizePoseRagdoll(SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const SkeletalMeshInfo& skeletal, std::vector<glm::mat4>& outTransforms)
    {
        EG_CORE_ASSERT(&node == &skeletal.RootBone);
        thread_local std::vector<glm::mat4> globalTransforms;
        globalTransforms.resize(skeletal.FlattenedBones.size());

        if (outTransforms.size() < skeletal.GetNumBoneTransforms())
            outTransforms.resize(skeletal.GetNumBoneTransforms());

        for (size_t i = 0; i < skeletal.FlattenedBones.size(); ++i)
        {
            const auto& entry = skeletal.FlattenedBones[i];
            const BoneNode& curNode = *entry.Node;
            const uint64_t nodeHash = curNode.GetNameHash();
            const glm::mat4& parentTr = (entry.ParentIndex < 0) ? parentTransform : globalTransforms[entry.ParentIndex];

            glm::mat4 globalTransformation;
            if (auto it = pose.FindBone(nodeHash); it != pose.Bones.end())
            {
                const auto& bone = it->second;
                globalTransformation = Math::ToTransformMatrix(bone); // It's already a global transform
            }
            else
            {
                globalTransformation = parentTr * curNode.Transformation;
                pose.Bones[nodeHash] = Math::DecomposeTransformMatrix(globalTransformation);
            }
            globalTransforms[i] = globalTransformation;

            if (entry.bHasBoneInfo)
            {
                if (entry.BoneID >= outTransforms.size())
                    outTransforms.resize(entry.BoneID + 1);

                outTransforms[entry.BoneID] = skeletal.InverseTransform * globalTransformation * entry.Offset;
            }
        }
    }

    double AnimationSystem::WrapAnimationTime(double duration, double currentTime, bool bLoop)
    {
        if (duration <= 0.0)
            return 0.0;

        if (currentTime > duration)
            currentTime = bLoop ? currentTime - (glm::floor(currentTime / duration) * duration) : duration;
        else if (currentTime < 0.0) // Animation is playing in reverse
            currentTime = bLoop ? currentTime - (glm::floor(currentTime / duration) * duration) : 0.0;

        return currentTime;
    }

    float AnimationSystem::WrapAnimationTime(float duration, float currentTime, bool bLoop)
    {
        return (float)WrapAnimationTime(double(duration), double(currentTime), bLoop);
    }
}
