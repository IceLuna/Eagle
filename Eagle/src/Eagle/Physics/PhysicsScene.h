#pragma once

#include "Eagle/AINavigation/AINavigationUtils.h"
#include "Eagle/Core/GUID.h"
#include "PhysicsActor.h"

#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>

#define EG_OVERLAP_MAX_COLLIDERS 10 // TODO v0.7: I think there's no need to it

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

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, RaycastHit* outHit) const;
		bool OverlapBox(const glm::vec3& origin, const glm::vec3& halfSize, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;
		bool OverlapCapsule(const glm::vec3& origin, float radius, float halfHeight, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;
		bool OverlapSphere(const glm::vec3& origin, float radius, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;

		bool IsValid() const { return m_Scene != nullptr; }

		void Clear();
		void Reset();

		Ref<PhysicsRagdollActor> CreateRagdoll(const SkeletalMeshComponent& skeletalComp);
		void ReleaseRagdoll(const SkeletalMeshComponent& skeletalComp);

		const physx::PxRenderBuffer& GetRenderBuffer() const { return m_Scene->getRenderBuffer(); }
		const PhysicsSettings& GetSettings() const { return m_Settings; }

		OverlapGeometryData CollectGeometry(const AABB& aabb);
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

		bool OverlapGeometry(const glm::vec3& origin, const physx::PxGeometry& geometry, std::array<physx::PxOverlapHit, EG_OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;
		void QueryScene(const BoxOverlapRequest& request);

	private:
		PhysicsSettings m_Settings;
		physx::PxScene* m_Scene = nullptr;
		std::unordered_map<GUID, Ref<PhysicsActor>> m_Actors;
		std::unordered_map<GUID, Ref<PhysicsRagdollActor>> m_RagdollActors;
		std::vector<physx::PxOverlapHit> m_OverlapBuffer;
		std::vector<uint32_t> m_BroadPhaseRegionHandles;

		float m_SubstepSize = 1.f;
		float m_Accumulator = 0.f;
		uint32_t m_NumSubsteps = 0;
		const uint32_t s_MaxSubsteps = 16;

		bool bStartedDebugSession = false;
	};
}
