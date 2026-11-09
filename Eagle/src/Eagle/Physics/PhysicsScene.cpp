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
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
    static ContactListener s_ContactListener;
    static Ref<PhysicsActor> s_InvalidPhysicsActor = nullptr;

    namespace DebugDraw
    {
        constexpr uint32_t s_CircleSegments = 24;

        static void AddLine(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxVec3& start, const physx::PxVec3& end)
        {
            RendererLine& line = lines.emplace_back();
            line.Start.Location = PhysXUtils::FromPhysXVector(pose.transform(start));
            line.End.Location = PhysXUtils::FromPhysXVector(pose.transform(end));
        }

        // An arc of a circle around `center`, in the plane of the `axis1` and `axis2` directions. Angles in radians, from `axis1` towards `axis2`
        static void AddArc(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxVec3& center, const physx::PxVec3& axis1, const physx::PxVec3& axis2,
            float radius, float angleStart, float angleEnd, uint32_t segments)
        {
            const float step = (angleEnd - angleStart) / float(segments);
            physx::PxVec3 previous = center + (axis1 * std::cos(angleStart) + axis2 * std::sin(angleStart)) * radius;
            for (uint32_t i = 1; i <= segments; ++i)
            {
                const float angle = angleStart + step * float(i);
                const physx::PxVec3 point = center + (axis1 * std::cos(angle) + axis2 * std::sin(angle)) * radius;
                AddLine(lines, pose, previous, point);
                previous = point;
            }
        }

        static void AddBox(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxBoxGeometry& box)
        {
            const physx::PxVec3& e = box.halfExtents;
            const physx::PxVec3 corners[8] =
            {
                { -e.x, -e.y, -e.z }, { e.x, -e.y, -e.z }, { e.x, -e.y, e.z }, { -e.x, -e.y, e.z },
                { -e.x, e.y, -e.z }, { e.x, e.y, -e.z }, { e.x, e.y, e.z }, { -e.x, e.y, e.z },
            };
            constexpr uint32_t edges[12][2] =
            {
                { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // Bottom
                { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // Top
                { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, // Sides
            };
            for (const auto& edge : edges)
                AddLine(lines, pose, corners[edge[0]], corners[edge[1]]);
        }

        static void AddSphere(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxSphereGeometry& sphere)
        {
            const physx::PxVec3 x(1.f, 0.f, 0.f), y(0.f, 1.f, 0.f), z(0.f, 0.f, 1.f), center(0.f);
            AddArc(lines, pose, center, x, y, sphere.radius, 0.f, physx::PxTwoPi, s_CircleSegments);
            AddArc(lines, pose, center, x, z, sphere.radius, 0.f, physx::PxTwoPi, s_CircleSegments);
            AddArc(lines, pose, center, y, z, sphere.radius, 0.f, physx::PxTwoPi, s_CircleSegments);
        }

        // PhysX capsules are along the X axis
        static void AddCapsule(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxCapsuleGeometry& capsule)
        {
            const physx::PxVec3 x(1.f, 0.f, 0.f), y(0.f, 1.f, 0.f), z(0.f, 0.f, 1.f);
            const float r = capsule.radius;
            const float h = capsule.halfHeight;
            const physx::PxVec3 top(h, 0.f, 0.f), bottom(-h, 0.f, 0.f);

            // Where the hemispheres meet the cylinder
            AddArc(lines, pose, top, y, z, r, 0.f, physx::PxTwoPi, s_CircleSegments);
            AddArc(lines, pose, bottom, y, z, r, 0.f, physx::PxTwoPi, s_CircleSegments);

            // Hemispheres
            const uint32_t halfSegments = s_CircleSegments / 2;
            AddArc(lines, pose, top, y, x, r, 0.f, physx::PxPi, halfSegments);
            AddArc(lines, pose, top, z, x, r, 0.f, physx::PxPi, halfSegments);
            AddArc(lines, pose, bottom, y, -x, r, 0.f, physx::PxPi, halfSegments);
            AddArc(lines, pose, bottom, z, -x, r, 0.f, physx::PxPi, halfSegments);

            // Cylinder
            for (const physx::PxVec3& offset : { y * r, -y * r, z * r, -z * r })
                AddLine(lines, pose, bottom + offset, top + offset);
        }

        // The edges of its polygons
        static void AddConvexMesh(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxConvexMeshGeometry& geometry)
        {
            const physx::PxConvexMesh* mesh = geometry.convexMesh;
            if (!mesh)
                return;

            const physx::PxVec3* vertices = mesh->getVertices();
            const physx::PxU8* indices = mesh->getIndexBuffer();
            const uint32_t polygonsCount = mesh->getNbPolygons();
            for (uint32_t i = 0; i < polygonsCount; ++i)
            {
                physx::PxHullPolygon polygon;
                if (!mesh->getPolygonData(i, polygon))
                    continue;

                for (uint32_t v = 0; v < polygon.mNbVerts; ++v)
                {
                    const physx::PxU8 start = indices[polygon.mIndexBase + v];
                    const physx::PxU8 end = indices[polygon.mIndexBase + (v + 1) % polygon.mNbVerts];
                    AddLine(lines, pose, geometry.scale.transform(vertices[start]), geometry.scale.transform(vertices[end]));
                }
            }
        }

        // The edges of its triangles
        static void AddTriangleMesh(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxTriangleMeshGeometry& geometry)
        {
            const physx::PxTriangleMesh* mesh = geometry.triangleMesh;
            if (!mesh)
                return;

            const physx::PxVec3* vertices = mesh->getVertices();
            const uint32_t trianglesCount = mesh->getNbTriangles();
            const bool b16BitIndices = mesh->getTriangleMeshFlags().isSet(physx::PxTriangleMeshFlag::e16_BIT_INDICES);
            const void* triangles = mesh->getTriangles();
            auto getIndex = [b16BitIndices, triangles](uint32_t i) -> uint32_t
            {
                return b16BitIndices ? static_cast<const physx::PxU16*>(triangles)[i] : static_cast<const physx::PxU32*>(triangles)[i];
            };

            lines.reserve(lines.size() + trianglesCount * 3);
            for (uint32_t i = 0; i < trianglesCount; ++i)
            {
                const physx::PxVec3 a = geometry.scale.transform(vertices[getIndex(i * 3)]);
                const physx::PxVec3 b = geometry.scale.transform(vertices[getIndex(i * 3 + 1)]);
                const physx::PxVec3 c = geometry.scale.transform(vertices[getIndex(i * 3 + 2)]);
                AddLine(lines, pose, a, b);
                AddLine(lines, pose, b, c);
                AddLine(lines, pose, c, a);
            }
        }

        static void AddShape(std::vector<RendererLine>& lines, const physx::PxTransform& pose, const physx::PxGeometry& geometry)
        {
            switch (geometry.getType())
            {
                case physx::PxGeometryType::eBOX: AddBox(lines, pose, static_cast<const physx::PxBoxGeometry&>(geometry)); break;
                case physx::PxGeometryType::eSPHERE: AddSphere(lines, pose, static_cast<const physx::PxSphereGeometry&>(geometry)); break;
                case physx::PxGeometryType::eCAPSULE: AddCapsule(lines, pose, static_cast<const physx::PxCapsuleGeometry&>(geometry)); break;
                case physx::PxGeometryType::eCONVEXMESH: AddConvexMesh(lines, pose, static_cast<const physx::PxConvexMeshGeometry&>(geometry)); break;
                case physx::PxGeometryType::eTRIANGLEMESH: AddTriangleMesh(lines, pose, static_cast<const physx::PxTriangleMeshGeometry&>(geometry)); break;
                default:
                    EG_CORE_ASSERT(false);
                    break;
            }
        }
    }

    static physx::PxSolverType::Enum ToPhysXSolverType(SolverType solver)
    {
        switch (solver)
        {
            case SolverType::PGS:
                return physx::PxSolverType::ePGS;
            case SolverType::TGS:
                return physx::PxSolverType::eTGS;
            default:
                EG_CORE_ASSERT(!"Unknown solver type");
                return physx::PxSolverType::eTGS;
        }
    }

    PhysicsScene::PhysicsScene(const PhysicsSettings& settings, bool bEditorScene)
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
        sceneDesc.filterShader = bEditorScene ? (physx::PxSimulationFilterShader)PhysXInternal::EditorFilterShader :(physx::PxSimulationFilterShader)PhysXInternal::FilterShader;
        sceneDesc.simulationEventCallback = &s_ContactListener;
        sceneDesc.solverType = ToPhysXSolverType(m_Settings.Solver);

        EG_CORE_ASSERT(sceneDesc.isValid(), "Invalid scene desc");

        m_Scene = PhysXInternal::GetPhysics().createScene(sceneDesc);
        EG_CORE_ASSERT(m_Scene, "Invalid scene");
        m_Scene->userData = this;

        CreateRegions();
        SetUpdateRate(m_Settings.UpdateRate);

        m_ControllerManager = PxCreateControllerManager(*m_Scene);
    }

    physx::PxController* PhysicsScene::CreateController(physx::PxBoxControllerDesc& desc)
    {
        return m_ControllerManager->createController(desc);
    }

    physx::PxController* PhysicsScene::CreateController(physx::PxCapsuleControllerDesc& desc)
    {
        return m_ControllerManager->createController(desc);
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

    UniqueQueryHits PhysicsScene::SweepBox(const Transform& transform, const glm::vec3& boxHalfSize, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        physx::PxBoxGeometry geometry(boxHalfSize.x, boxHalfSize.y, boxHalfSize.z);
        return SweepScene_Unique(geometry, transform, direction, distance, queryType, collisionGroup, entitiesToIgnore);
    }

    UniqueQueryHits PhysicsScene::SweepCapsule(const Transform& transform, float radius, float halfHeight, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        physx::PxCapsuleGeometry geometry(radius, halfHeight);
        return SweepScene_Unique(geometry, transform, direction, distance, queryType, collisionGroup, entitiesToIgnore);
    }

    UniqueQueryHits PhysicsScene::SweepSphere(const Transform& transform, float radius, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        physx::PxSphereGeometry geometry(radius);
        return SweepScene_Unique(geometry, transform, direction, distance, queryType, collisionGroup, entitiesToIgnore);
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
            region.mBounds = regionBounds[i];
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
            m_ControllerManager->release();
            m_ControllerManager = nullptr;
            m_Scene->release();
            m_Scene = nullptr;
        }
    }
    
    QueryHits PhysicsScene::CollectCollidersWithinVolume(const AABB& volume)
    {
        Transform pose = volume.Center();
        const glm::vec3 halfExtent = pose.Scale3D * volume.Extents();
        physx::PxBoxGeometry box = physx::PxBoxGeometry(PhysXUtils::ToPhysXVector(halfExtent));
        return OverlapScene(box, PhysXUtils::ToPhysXTranform(pose), PhysicsQueryType::Static, s_CollisionGroupAny);
    }

    OverlapGeometryData PhysicsScene::AppendColliderGeometry(const AABB& aabb, const QueryHits& overlapHits)
    {
        OverlapGeometryData geometry;
        geometry.ScanBounds = aabb;

        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        geometry.Vertices.reserve(1000);
        geometry.Indices.reserve(1000);

        std::size_t indicesCount = 0;

        for (const auto& overlapHit : overlapHits)
        {
            if (!overlapHit.Body)
                continue;

            // The shape's world pose. The poses are composed, so that the shape's offset is rotated by the body.
            // Create an AABB for the Recast tile in local space and pass it in to GetGeometry so that large geometry sets
            // (like heightfields) can just return the subset of geometry that overlaps the AABB.
            const physx::PxTransform shapeGlobalPose = overlapHit.Body->getGlobalPose() * overlapHit.Shape->GetShape()->getLocalPose();
            const glm::mat4 t = Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(shapeGlobalPose));
            const AABB localScanBounds = AABB::Transformed(geometry.ScanBounds, glm::inverse(t));
            overlapHit.Shape->GetGeometry(vertices, indices, &localScanBounds);

            // Note: returned geometry data is in the shape's local space

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

    UniqueQueryHits PhysicsScene::SweepScene_Unique(const physx::PxGeometry& geometry, const Transform& transform, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore) const
    {
        m_UniqueQueryHits.clear();
        UniqueUnboundedSweep callback(m_UniqueQueryHits);
        PhysXQueryFilterCallback filterCallback(physx::PxQueryHitType::eTOUCH, collisionGroup, entitiesToIgnore);
        const physx::PxQueryFilterData queryData = PhysXUtils::GetPxQueryFilterData(queryType);

        physx::PxTransform pose = PhysXUtils::ToPhysXTranform(transform);
        physx::PxVec3 pxDir = PhysXUtils::ToPhysXVector(direction);
        m_Scene->sweep(geometry, pose, pxDir, distance, callback, physx::PxHitFlag::eDEFAULT, queryData, &filterCallback);

        return m_UniqueQueryHits;
    }

    void PhysicsScene::StartDebugging()
    {
        if (m_Settings.bDebugOnPlay && PhysicsEngine::IsVisualDebuggingSupported() && !PhysXDebugger::IsDebugging())
        {
            bStartedDebugSession = true;
            PhysXDebugger::StartDebugging(Project::GetPhysicsDebugInfoPath(), m_Settings.DebugType == DebugType::Live);
        }
    }

    void PhysicsScene::StopDebugging()
    {
        if (bStartedDebugSession)
        {
            PhysXDebugger::StopDebugging();
            bStartedDebugSession = false;
        }
    }

    void PhysicsScene::GatherCollisionDebugLines(std::vector<RendererLine>& outLines)
    {
        const physx::PxActorTypeFlags actorTypes = physx::PxActorTypeFlag::eRIGID_STATIC | physx::PxActorTypeFlag::eRIGID_DYNAMIC;
        m_DebugDrawActors.resize(m_Scene->getNbActors(actorTypes));
        if (m_DebugDrawActors.empty())
            return;

        EG_CPU_TIMING_SCOPED("PhysicsScene. Gather collision debug lines");

        m_DebugDrawActors.resize(m_Scene->getActors(actorTypes, m_DebugDrawActors.data(), (uint32_t)m_DebugDrawActors.size()));
        for (physx::PxActor* actor : m_DebugDrawActors)
        {
            const physx::PxActorFlags actorFlags = actor->getActorFlags();
            if (!actorFlags.isSet(physx::PxActorFlag::eVISUALIZATION) || actorFlags.isSet(physx::PxActorFlag::eDISABLE_SIMULATION))
                continue;

            const physx::PxRigidActor* rigidActor = actor->is<physx::PxRigidActor>();
            if (!rigidActor)
                continue;

            m_DebugDrawShapes.resize(rigidActor->getNbShapes());
            if (m_DebugDrawShapes.empty())
                continue;

            m_DebugDrawShapes.resize(rigidActor->getShapes(m_DebugDrawShapes.data(), (uint32_t)m_DebugDrawShapes.size()));

            const physx::PxTransform actorPose = rigidActor->getGlobalPose();
            for (const physx::PxShape* shape : m_DebugDrawShapes)
            {
                if (!shape->getFlags().isSet(physx::PxShapeFlag::eVISUALIZATION))
                    continue;

                DebugDraw::AddShape(outLines, actorPose * shape->getLocalPose(), shape->getGeometry());
            }
        }
    }
}
