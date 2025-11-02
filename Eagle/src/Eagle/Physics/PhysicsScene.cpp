#include "egpch.h"
#include "PhysicsScene.h"
#include "PhysicsEngine.h"
#include "PhysicsSettings.h"
#include "PhysXDebugger.h"
#include "PhysXInternal.h"
#include "ContactListener.h"
#include "PhysicsRagdollActor.h"

#include "Eagle/AI/NavigationUtils.h"
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
        const bool bHasRigidBody = entity.HasComponent<RigidBodyComponent>();

        if (!bHasRigidBody)
            entity.AddComponent<RigidBodyComponent>();

        Ref<PhysicsActor> actor = MakeRef<PhysicsActor>(entity);
        m_Actors[entity.GetGUID()] = actor;
        m_Scene->addActor(*actor->GetPhysXActor());

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

    bool PhysicsScene::Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, PhysicsQueryType query, CollisionGroup collisionGroup, RaycastHit* outHit, const std::set<GUID>* entitiesToIgnore) const
    {
        using namespace physx;
        const PxHitFlags hitFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL;
        PxRaycastBuffer hitInfo;
        
        PhysXQueryFilterCallback filterCallback(PxQueryHitType::eBLOCK, collisionGroup, entitiesToIgnore);
        bool bResult = m_Scene->raycast(PhysXUtils::ToPhysXVector(origin), PhysXUtils::ToPhysXVector(dir), maxDistance, hitInfo, hitFlags,
            PhysXUtils::GetPxQueryFilterData(query), &filterCallback);

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
    
    UniqueQueryHits PhysicsScene::OverlapBox(const Transform& transform, const glm::vec3& boxHalfSize, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        physx::PxBoxGeometry geometry(boxHalfSize.x, boxHalfSize.y, boxHalfSize.z);
        return OverlapScene_Unique(geometry, PhysXUtils::ToPhysXTranform(transform), queryType, collisionGroup, entitiesToIgnore);
    }

    UniqueQueryHits PhysicsScene::OverlapCapsule(const Transform& transform, float radius, float halfHeight, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        physx::PxCapsuleGeometry geometry(radius, halfHeight);
        return OverlapScene_Unique(geometry, PhysXUtils::ToPhysXTranform(transform), queryType, collisionGroup, entitiesToIgnore);
    }

    UniqueQueryHits PhysicsScene::OverlapSphere(const Transform& transform, float radius, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        physx::PxSphereGeometry geometry(radius);
        return OverlapScene_Unique(geometry, PhysXUtils::ToPhysXTranform(transform), queryType, collisionGroup, entitiesToIgnore);
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
    
    QueryHits PhysicsScene::CollectCollidersWithinVolume(const AABB& volume)
    {
        Transform pose = volume.Center();
        const glm::vec3 halfExtent = pose.Scale3D * volume.Extents() * 0.5f;
        physx::PxBoxGeometry box = physx::PxBoxGeometry(PhysXUtils::ToPhysXVector(halfExtent));
        return OverlapScene(box, PhysXUtils::ToPhysXTranform(pose), PhysicsQueryType::Static, s_CollisionGroupAny);
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
    
    QueryHits PhysicsScene::OverlapScene(const physx::PxGeometry& geometry, const physx::PxTransform& pose, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        m_QueryHits.clear();
        UnboundedOverlap callback(m_QueryHits);
        PhysXQueryFilterCallback filterCallback(physx::PxQueryHitType::eTOUCH, collisionGroup, entitiesToIgnore);
        const physx::PxQueryFilterData queryData = PhysXUtils::GetPxQueryFilterData(queryType);
        m_Scene->overlap(geometry, pose, callback, queryData, &filterCallback);

        return m_QueryHits;
    }

    UniqueQueryHits PhysicsScene::OverlapScene_Unique(const physx::PxGeometry& geometry, const physx::PxTransform& pose, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        m_UniqueQueryHits.clear();
        UniqueUnboundedOverlap callback(m_UniqueQueryHits);
        PhysXQueryFilterCallback filterCallback(physx::PxQueryHitType::eTOUCH, collisionGroup, entitiesToIgnore);
        const physx::PxQueryFilterData queryData = PhysXUtils::GetPxQueryFilterData(queryType);
        m_Scene->overlap(geometry, pose, callback, queryData, &filterCallback);

        return m_UniqueQueryHits;
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
