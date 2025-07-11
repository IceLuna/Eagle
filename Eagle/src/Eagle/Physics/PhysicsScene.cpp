#include "egpch.h"
#include "PhysicsScene.h"
#include "PhysicsEngine.h"
#include "PhysicsSettings.h"
#include "PhysXDebugger.h"
#include "PhysXInternal.h"
#include "ContactListener.h"
#include "PhysicsRagdollActor.h"

#include "Eagle/AINavigation/AINavigationUtils.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Debug/CPUTimings.h"

namespace Eagle
{
    static ContactListener s_ContactListener;
    static Ref<PhysicsActor> s_InvalidPhysicsActor = nullptr;

    PhysicsScene::PhysicsScene(const PhysicsSettings& settings)
    : m_Settings(settings)
    {
        physx::PxSceneDesc sceneDesc(PhysXInternal::GetPhysics().getTolerancesScale());
        sceneDesc.dynamicTreeRebuildRateHint *= 10;
        sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD | physx::PxSceneFlag::eENABLE_PCM;
        sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
        sceneDesc.kineKineFilteringMode = physx::PxPairFilteringMode::eKEEP;
        sceneDesc.staticKineFilteringMode = physx::PxPairFilteringMode::eKEEP;
        sceneDesc.gravity = PhysXUtils::ToPhysXVector(settings.Gravity);
        sceneDesc.broadPhaseType = PhysXUtils::ToPhysXBroadphaseType(settings.BroadphaseAlgorithm);
        sceneDesc.cpuDispatcher = PhysXInternal::GetCPUDispatcher();
        sceneDesc.filterShader = m_Settings.bEditorScene ? (physx::PxSimulationFilterShader)PhysXInternal::EditorFilterShader :(physx::PxSimulationFilterShader)PhysXInternal::FilterShader;
        sceneDesc.simulationEventCallback = &s_ContactListener;
        sceneDesc.frictionType = PhysXUtils::ToPhysXFrictionType(settings.FrictionModel);

        EG_CORE_ASSERT(sceneDesc.isValid(), "Invalid scene desc");

        m_Scene = PhysXInternal::GetPhysics().createScene(sceneDesc);
        EG_CORE_ASSERT(m_Scene, "Invalid scene");
        m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.f);
        m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.f);
        m_Scene->userData = this;
        //m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 0.01f);
        //m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LIMITS, 1.0f);

        CreateRegions();
        SetUpdateRate(m_Settings.UpdateRate);
    }
    
    void PhysicsScene::ConstructFromScene(Scene* scene)
    {
        scene->OnEach([this](Entity entity)
        {
            CreatePhysicsActor(entity);
        });
    }

    void PhysicsScene::UpdateActors()
    {
        for (auto& [guid, actor] : m_Actors)
            actor->OnFixedUpdate(m_SubstepSize);
    }

    void PhysicsScene::SyncTransforms()
    {
        uint32_t nActiveActors;
        physx::PxActor** activeActors = m_Scene->getActiveActors(nActiveActors);

        for (uint32_t i = 0; i < nActiveActors; ++i)
        {
            if (activeActors[i]->userData)
            {
                PhysicsActorBase* actor = (PhysicsActorBase*)activeActors[i]->userData;
                actor->SceneRequestToSyncTransforms();
            }
        }
    }

    void PhysicsScene::Simulate(Timestep ts, bool bCallScripts)
    {
        EG_CPU_TIMING_SCOPED("PhysicsScene. Simulate + Sync + Update");

        SubstepStrategy(ts);
        for (uint32_t i = 0; i < m_NumSubsteps; ++i)
        {
            m_Scene->simulate(m_SubstepSize);
            m_Scene->fetchResults(true);

            SyncTransforms();
            if (bCallScripts)
                UpdateActors();
        }
    }

    void PhysicsScene::SubstepStrategy(Timestep ts)
    {
        if (m_Accumulator > m_SubstepSize)
            m_Accumulator = 0.f;

        m_Accumulator += ts;
        if (m_Accumulator < m_SubstepSize)
        {
            m_NumSubsteps = 0;
            return;
        }

        m_NumSubsteps = glm::min((uint32_t)(m_Accumulator / m_SubstepSize), s_MaxSubsteps);
        m_Accumulator -= m_NumSubsteps * m_SubstepSize;
    }
    
    Ref<PhysicsActor>& PhysicsScene::GetPhysicsActor(const Entity& entity)
    {
        auto it = m_Actors.find(entity.GetGUID());
        return it != m_Actors.end() ? it->second : s_InvalidPhysicsActor;
    }

    const Ref<PhysicsActor>& PhysicsScene::GetPhysicsActor(const Entity& entity) const
    {
        auto it = m_Actors.find(entity.GetGUID());
        return it != m_Actors.end() ? it->second : s_InvalidPhysicsActor;
    }
    
    Ref<PhysicsActor> PhysicsScene::CreatePhysicsActor(Entity& entity)
    {
        static const Ref<PhysicsActor> s_InvalidActor;
        const bool bHasRigidBody = entity.HasComponent<RigidBodyComponent>();

        if (!bHasRigidBody)
            entity.AddComponent<RigidBodyComponent>();

        Ref<PhysicsActor> actor = MakeRef<PhysicsActor>(entity);
        m_Actors[entity.GetGUID()] = actor;
        m_Scene->addActor(*actor->GetPhysXActor());

        actor->SetSimulationData();
        
        return actor;
    }
    
    void PhysicsScene::RemovePhysicsActor(const Ref<PhysicsActor>& physicsActor)
    {
        if (!physicsActor)
            return;

        m_Scene->removeActor(*physicsActor->GetPhysXActor());
        m_Actors.erase(physicsActor->GetEntity().GetGUID());
    }
    
    void PhysicsScene::SetUpdateRate(uint32_t updateRate)
    {
        m_Settings.UpdateRate = glm::clamp(updateRate, PhysicsSettings::s_MinUpdateRate, PhysicsSettings::s_MaxUpdateRate);
        m_SubstepSize = 1.f / m_Settings.UpdateRate;
    }

    bool PhysicsScene::Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, RaycastHit* outHit) const
    {
        // TODO v0.7: Ask a user how many hits they need
        physx::PxRaycastBuffer hitInfo;
        bool bResult = m_Scene->raycast(PhysXUtils::ToPhysXVector(origin), PhysXUtils::ToPhysXVector(dir), maxDistance, hitInfo);

        if (bResult)
        {
            if (hitInfo.block.actor->userData)
            {
                const PhysicsActorBase* actor = (PhysicsActorBase*)hitInfo.block.actor->userData;
                outHit->HitEntity = actor->GetEntity();
            }
            outHit->Position = PhysXUtils::FromPhysXVector(hitInfo.block.position);
            outHit->Normal = PhysXUtils::FromPhysXVector(hitInfo.block.normal);
            outHit->Distance = hitInfo.block.distance;
        }

        return bResult;
    }
    
    bool PhysicsScene::OverlapBox(const glm::vec3& origin, const glm::vec3& halfSize, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const
    {
        return OverlapGeometry(origin, physx::PxBoxGeometry(halfSize.x, halfSize.y, halfSize.z), buffer, count);
    }
    
    bool PhysicsScene::OverlapCapsule(const glm::vec3& origin, float radius, float halfHeight, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const
    {
        return OverlapGeometry(origin, physx::PxCapsuleGeometry(radius, halfHeight), buffer, count);
    }
    
    bool PhysicsScene::OverlapSphere(const glm::vec3& origin, float radius, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const
    {
        return OverlapGeometry(origin, physx::PxSphereGeometry(radius), buffer, count);
    }

    OverlapGeometryData PhysicsScene::CollectGeometry(const AABB& aabb)
    {
        QueryHits results = CollectCollidersWithinVolume(aabb);
        return AppendColliderGeometry(aabb, results);
    }

    void PhysicsScene::CreateRegions()
    {
        const PhysicsSettings& settings = m_Settings;

        for (uint32_t handle : m_BroadPhaseRegionHandles)
        {
            m_Scene->removeBroadPhaseRegion(handle);
        }
        m_BroadPhaseRegionHandles.clear();

        if (settings.BroadphaseAlgorithm != BroadphaseType::MultiBoxPrune)
            return;
        
        std::vector<physx::PxBounds3> regionBounds((uint64_t)settings.WorldBoundsSubdivisions * settings.WorldBoundsSubdivisions);
        physx::PxBounds3 globalBounds(PhysXUtils::ToPhysXVector(settings.WorldAABB.Min), PhysXUtils::ToPhysXVector(settings.WorldAABB.Max));
        uint32_t regionCount = physx::PxBroadPhaseExt::createRegionsFromWorldBounds(regionBounds.data(), globalBounds, settings.WorldBoundsSubdivisions);
        regionCount = glm::min(uint32_t(regionBounds.size()), regionCount);
        m_BroadPhaseRegionHandles.resize(regionCount);

        for (uint32_t i = 0; i < regionCount; ++i)
        {
            physx::PxBroadPhaseRegion region;
            region.bounds = regionBounds[i];
            m_BroadPhaseRegionHandles[i] = m_Scene->addBroadPhaseRegion(region, true);
        }
    }
    
    void PhysicsScene::Clear()
    {
        if (m_Scene)
        {
            while (m_Actors.size())
                RemovePhysicsActor(m_Actors.begin()->second);

            m_Actors.clear(); //Just in case
            m_RagdollActors.clear();
        }
    }

    void PhysicsScene::Reset()
    {
        Clear();
        m_Accumulator = 0.f;
        m_NumSubsteps = 0;
    }

    Ref<PhysicsRagdollActor> PhysicsScene::CreateRagdoll(const SkeletalMeshComponent& skeletalComp)
    {
        const auto& asset = skeletalComp.GetMeshAsset();
        if (!asset)
        {
            EG_CORE_ERROR("Failed to create a ragdoll actor. Skeletal mesh asset is not present (Entity: {})", skeletalComp.Parent.GetName());
            return {};
        }

        Ref<PhysicsRagdollActor> result = MakeRef<PhysicsRagdollActor>(skeletalComp.Parent, m_Scene);
        m_RagdollActors[skeletalComp.Parent.GetGUID()] = result;
        return result;
    }

    void PhysicsScene::ReleaseRagdoll(const SkeletalMeshComponent& skeletalComp)
    {
        m_RagdollActors.erase(skeletalComp.Parent.GetGUID());
    }

    void PhysicsScene::Destroy()
    {
        if (m_Scene)
        {
            StopDebugging();
            
            for (uint32_t handle : m_BroadPhaseRegionHandles)
            {
                m_Scene->removeBroadPhaseRegion(handle);
            }
            m_BroadPhaseRegionHandles.clear();

            while(m_Actors.size())
                RemovePhysicsActor(m_Actors.begin()->second);

            m_Actors.clear(); //Just in case
            m_RagdollActors.clear(); //Just in case
            m_Scene->release();
            m_Scene = nullptr;
        }
    }
    
    bool PhysicsScene::OverlapGeometry(const glm::vec3& origin, const physx::PxGeometry& geometry, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const
    {
        physx::PxOverlapBuffer buf(buffer.data(), EG_OVERLAP_MAX_COLLIDERS);
        physx::PxTransform pose = PhysXUtils::ToPhysXTranform(origin);

        bool bResult = m_Scene->overlap(geometry, pose, buf);

        if (bResult)
        {
            memcpy(buffer.data(), buf.touches, buf.nbTouches * sizeof(physx::PxOverlapHit));
            count = buf.nbTouches;
        }

        return bResult;
    }
    
    QueryHits PhysicsScene::CollectCollidersWithinVolume(const AABB& volume)
    {
        QueryHits hits;

        UnboundedOverlapHitCallback unboundedOverlapHitCallback =
            [&hits](std::optional<SceneQueryHit>&& hit)
            {
                if (hit && hit->IsValid())
                {
                    const SceneQueryHit& sceneQueryHit = *hit;
                    hits.push_back(sceneQueryHit);
                }

                return true;
            };

        BoxOverlapRequest request;
        request.Dimension = volume.Extents();
        request.Pose = Transform(volume.Center());
        request.Type = QueryType::Static;
        request.OverlapHitCallback = unboundedOverlapHitCallback;

        // results are in outHits
        QueryScene(request);
        return hits;
    }

    OverlapGeometryData PhysicsScene::AppendColliderGeometry(const AABB& aabb, const QueryHits& overlapHits)
    {
        OverlapGeometryData geometry;
        geometry.ScanBounds = aabb;

        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(100);
        indices.reserve(100);
        geometry.Vertices.reserve(100);
        geometry.Indices.reserve(100);

        std::size_t indicesCount = 0;

        for (const auto& overlapHit : overlapHits)
        {
            if (!overlapHit.Body)
                continue;

            // Create an AABB for the Recast tile in local space and pass it in to GetGeometry so that large geometry sets
            // (like heightfields) can just return the subset of geometry that overlaps the AABB.
            Transform pose = PhysXUtils::FromPhysXTransform(overlapHit.Shape->GetShape()->getLocalPose());
            const glm::vec3 offset = -pose.Location;
            AABB localScanBounds = AABB(geometry.ScanBounds.Min + offset, geometry.ScanBounds.Max + offset);
            overlapHit.Shape->GetGeometry(vertices, indices, &localScanBounds);

            // Note: returned geometry data is also in local space
            Transform tBody = PhysXUtils::FromPhysXTransform(overlapHit.Body->getGlobalPose());
            glm::mat4 t = Math::ToTransformMatrix(tBody + pose);

            if (vertices.empty())
                continue;

            for (const glm::vec3& vertex : vertices)
            {
                const glm::vec3 translated = t * glm::vec4(vertex, 1.f);
                geometry.Vertices.push_back(translated);
            }

            for (size_t i = 0; i < indices.size(); i += 3)
            {
                geometry.Indices.push_back(uint32_t(indicesCount + indices[i]));
                geometry.Indices.push_back(uint32_t(indicesCount + indices[i + 1]));
                geometry.Indices.push_back(uint32_t(indicesCount + indices[i + 2]));
            }

            indicesCount += vertices.size();
            vertices.clear();
            indices.clear();
        }

        return geometry;
    }
    
    void PhysicsScene::QueryScene(const BoxOverlapRequest& request)
    {
        QueryHits hits;
        m_OverlapBuffer.resize(64);

        // Prepare overlap data
        const glm::vec3 halfExtent = request.Pose.Scale3D * request.Dimension * 0.5f;
        physx::PxBoxGeometry box = physx::PxBoxGeometry(PhysXUtils::ToPhysXVector(halfExtent));
        const physx::PxTransform pose = PhysXUtils::ToPhysXTranform(request.Pose);

        UnboundedOverlapCallback callback(request.OverlapHitCallback, m_OverlapBuffer, hits);
        PhysXQueryFilterCallback filterCallback(physx::PxQueryHitType::eTOUCH);
        const physx::PxQueryFilterData queryData(PhysXUtils::GetPxQueryFlags(request.Type));

        m_Scene->overlap(box, pose, callback, queryData, &filterCallback);
    }

    void PhysicsScene::StartDebugging()
    {
#ifndef EG_RELEASE
        if (m_Settings.bDebugOnPlay && !PhysXDebugger::IsDebugging())
        {
            bStartedDebugSession = true;
            PhysXDebugger::StartDebugging(Project::GetPhysicsDebugInfoPath(), m_Settings.DebugType == DebugType::Live);
        }
#endif
    }

    void PhysicsScene::StopDebugging()
    {
#ifndef EG_RELEASE
        if (bStartedDebugSession && PhysXDebugger::IsDebugging())
        {
            PhysXDebugger::StopDebugging();
            bStartedDebugSession = false;
        }
#endif
    }
}
