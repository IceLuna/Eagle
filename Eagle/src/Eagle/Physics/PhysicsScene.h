#pragma once

#include "Eagle/AI/NavigationUtils.h"
#include "Eagle/Core/GUID.h"
#include "PhysicsActor.h"

#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>

namespace Eagle
{
	struct RaycastHit
	{
		Entity HitEntity;
		glm::vec3 Position;
		float Distance;
		glm::vec3 Normal;
	};

	class PhysicsRagdollActor;
	struct PhysicsSettings;

	class PhysicsScene
	{
	public:
		PhysicsScene(const PhysicsSettings& settings);
		~PhysicsScene() { Destroy(); }

		void ConstructFromScene(Scene* scene);

		void Simulate(Timestep ts, bool bCallScripts);

		Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity);
		const Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity) const;

		//Adds RigidBodyComponent to the entity if none provided.
		Ref<PhysicsActor> CreatePhysicsActor(Entity& entity);
		void RemovePhysicsActor(const Ref<PhysicsActor>& physicsActor);

		glm::vec3 GetGravity() const { return PhysXUtils::FromPhysXVector(m_Scene->getGravity()); }
		void SetGravity(const glm::vec3& gravity) { m_Scene->setGravity(PhysXUtils::ToPhysXVector(gravity)); }
		void SetUpdateRate(uint32_t updateRate);
		void SetDebugOnPlay(bool bEnable) { m_Settings.bDebugOnPlay = bEnable; }
		void SetDebugType(DebugType type) { m_Settings.DebugType = type; }

		float GetSimulationTimeStep() const { return m_SubstepSize; }

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, PhysicsQueryType query, CollisionGroup collisionGroup, RaycastHit* outHit, const std::set<GUID>* entitiesToIgnore = nullptr) const;

		// These don't return the same entity multiple times per each collider
		UniqueQueryHits OverlapBox(const Transform& transform, const glm::vec3& boxHalfSize, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;
		UniqueQueryHits OverlapCapsule(const Transform& transform, float radius, float halfHeight, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;
		UniqueQueryHits OverlapSphere(const Transform& transform, float radius, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;
		UniqueQueryHits SweepBox(const Transform& transform, const glm::vec3& boxHalfSize, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;
		UniqueQueryHits SweepCapsule(const Transform& transform, float radius, float halfHeight, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;
		UniqueQueryHits SweepSphere(const Transform& transform, float radius, const glm::vec3& direction, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;

		bool IsValid() const { return m_Scene != nullptr; }

		void Clear();
		void Reset();

		Ref<PhysicsRagdollActor> CreateRagdoll(const SkeletalMeshComponent& skeletalComp);
		void ReleaseRagdoll(const SkeletalMeshComponent& skeletalComp);

		const physx::PxRenderBuffer& GetRenderBuffer() const { return m_Scene->getRenderBuffer(); }
		const PhysicsSettings& GetSettings() const { return m_Settings; }

		QueryHits CollectCollidersWithinVolume(const AABB& volume);
		OverlapGeometryData AppendColliderGeometry(const AABB& aabb, const QueryHits& overlapHits);

		void StartDebugging();
		void StopDebugging();

	private:
		void CreateRegions();

		void SubstepStrategy(Timestep ts);
		void UpdateActors();
		void SyncTransforms();

		void Destroy();

		QueryHits OverlapScene(const physx::PxGeometry& geometry, const physx::PxTransform& pose, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;
		// Result doesn't contain the same entity per each collider
		UniqueQueryHits OverlapScene_Unique(const physx::PxGeometry& geometry, const physx::PxTransform& pose, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;

		// Result doesn't contain the same entity per each collider
		UniqueQueryHits SweepScene_Unique(const physx::PxGeometry& geometry, const Transform& transform, const glm::vec3& dir, float distance, PhysicsQueryType queryType, CollisionGroup collisionGroup, const std::set<GUID>* entitiesToIgnore = nullptr) const;

	private:
		PhysicsSettings m_Settings;
		physx::PxScene* m_Scene = nullptr;
		ankerl::unordered_dense::map<GUID, Ref<PhysicsActor>> m_Actors;
		ankerl::unordered_dense::map<GUID, Ref<PhysicsRagdollActor>> m_RagdollActors;
		mutable QueryHits m_QueryHits; // Exists just to avoid allocations on every query
		mutable UniqueQueryHits m_UniqueQueryHits; // Exists just to avoid allocations on every query
		std::vector<uint32_t> m_BroadPhaseRegionHandles;

		float m_SubstepSize = 1.f;
		float m_Accumulator = 0.f;
		uint32_t m_NumSubsteps = 0;
		const uint32_t s_MaxSubsteps = 16;

		bool bStartedDebugSession = false;
	};
}
