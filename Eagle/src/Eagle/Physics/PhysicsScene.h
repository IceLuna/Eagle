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
		glm::vec3 Normal;
		float Distance;
	};

	struct PhysicsSettings;

	class PhysicsScene
	{
	public:
		PhysicsScene(const PhysicsSettings& settings);
		~PhysicsScene() { Destroy(); }

		void ConstructFromScene(Scene* scene);

		void Simulate(Timestep ts, bool bFixedUpdate = true);

		const Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity) const;
		Ref<PhysicsActor> CreatePhysicsActor(Entity& entity);
		void RemovePhysicsActor(const Ref<PhysicsActor>& physicsActor);

		glm::vec3 GetGravity() const { return PhysXUtils::FromPhysXVector(m_Scene->getGravity()); }
		void SetGravity(const glm::vec3& gravity) { m_Scene->setGravity(PhysXUtils::ToPhysXVector(gravity)); }

		bool Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, RaycastHit* outHit);
		bool OverlapBox(const glm::vec3& origin, const glm::vec3& halfSize, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count);
		bool OverlapCapsule(const glm::vec3& origin, float radius, float halfHeight, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count);
		bool OverlapSphere(const glm::vec3& origin, float radius, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count);

		bool IsValid() const { return m_Scene != nullptr; }

	private:
		template <typename T>
		void AddColliderIfCan(Ref<PhysicsActor>& actor)
		{
			static_assert(std::is_base_of<BaseColliderComponent, T>::value);
			Entity& entity = actor->GetEntity();
			if (entity.HasComponent<T>())
				actor->AddCollider(entity.GetComponent<T>(), entity);
		}

		void CreateRegions();

		bool Advance(Timestep ts);
		void SubstepStrategy(Timestep ts);

		void Destroy();

		bool OverlapGeometry(const glm::vec3& origin, const physx::PxGeometry& geometry, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count);

	private:
		PhysicsSettings m_Settings;
		physx::PxScene* m_Scene = nullptr;
		std::unordered_map<GUID, Ref<PhysicsActor>> m_Actors;

		float m_SubstepSize;
		float m_Accumulator = 0.f;
		uint32_t m_NumSubsteps = 0;
		const uint32_t c_MaxSubsteps = 8;

	};
}
