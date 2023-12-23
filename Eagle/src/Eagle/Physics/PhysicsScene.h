#pragma once

#include "Eagle/Core/GUID.h"
#include "PhysicsActor.h"
#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>

#define OVERLAP_MAX_COLLIDERS 10

namespace Eagle
{
	struct RaycastHit
	{
		GUID HitEntity;
		glm::vec3 Position;
		float Distance;
		glm::vec3 Normal;
	};

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

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, RaycastHit* outHit) const;
		bool OverlapBox(const glm::vec3& origin, const glm::vec3& halfSize, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;
		bool OverlapCapsule(const glm::vec3& origin, float radius, float halfHeight, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;
		bool OverlapSphere(const glm::vec3& origin, float radius, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;

		bool IsValid() const { return m_Scene != nullptr; }

		void Clear();
		void Reset();

		const physx::PxRenderBuffer& GetRenderBuffer() const { return m_Scene->getRenderBuffer(); }
		const PhysicsSettings& GetSettings() const { return m_Settings; }

	private:
		void CreateRegions();

		void SubstepStrategy(Timestep ts);
		void UpdateActors();
		void SyncTransforms();

		void Destroy();

		bool OverlapGeometry(const glm::vec3& origin, const physx::PxGeometry& geometry, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count) const;

	private:
		PhysicsSettings m_Settings;
		physx::PxScene* m_Scene = nullptr;
		std::unordered_map<GUID, Ref<PhysicsActor>> m_Actors;

		float m_SubstepSize;
		float m_Accumulator = 0.f;
		uint32_t m_NumSubsteps = 0;
		const uint32_t s_MaxSubsteps = 16;
	};
}
