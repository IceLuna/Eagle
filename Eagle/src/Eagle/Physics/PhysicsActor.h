#pragma once

#include <PhysX/PxPhysicsAPI.h>
#include "PhysicsUtils.h"
#include "PhysicsShapes.h"
#include "Eagle/Core/Entity.h"

namespace Eagle
{
	class Entity;
	class RigidBodyComponent;

	class PhysicsActor
	{
	public:
		PhysicsActor(const Entity& entity);
		~PhysicsActor();

		glm::vec3 GetLocation() const { return PhysXUtils::FromPhysXVector(m_RigidActor->getGlobalPose().p); }
		void SetLocation(const glm::vec3& location, bool autowake = true);

		glm::vec3 GetRotation() const { return glm::eulerAngles(PhysXUtils::FromPhysXQuat(m_RigidActor->getGlobalPose().q)); }
		void SetRotation(const glm::vec3& rotation, bool autowake = true);
		void Rotate(const glm::vec3& rotation, bool autowake = true);

		void WakeUp();
		void PutToSleep();

		float GetMass() const;
		void SetMass(float mass);

		void AddForce(const glm::vec3& force, ForceMode forceMode);
		void AddTorque(const glm::vec3& torque, ForceMode forceMode);

		glm::vec3 GetLinearVelocity() const;
		void SetLinearVelocity(const glm::vec3& velocity);
		glm::vec3 GetAngularVelocity() const;
		void SetAngularVelocity(const glm::vec3& velocity);

		float GetMaxLinearVelocity() const;
		void SetMaxLinearVelocity(float maxVelocity);
		float GetMaxAngularVelocity() const;
		void SetMaxAngularVelocity(float maxVelocity);

		void SetLinearDamping(float damping) const;
		void SetAngularDamping(float damping) const;

		bool IsDynamic() const { return m_RigidBodyComponent.BodyType == RigidBodyComponent::Type::Dynamic; }

		bool IsKinematic() const { return IsDynamic() && m_RigidBodyComponent.IsKinematic; };
		void SetKinematic(bool bKinematic);

		bool IsGravityDisabled() const { return m_RigidActor->getActorFlags().isSet(physx::PxActorFlag::eDISABLE_GRAVITY); }
		void SetGravityDisabled(bool bDisabled);

		bool IsLockFlagSet(ActorLockFlag flag) const { return (uint32_t)flag & m_LockFlags; }
		void SetLockFlag(ActorLockFlag flag, bool value);
		ActorLockFlag GetLockFlags() const { return (ActorLockFlag)m_LockFlags; }

		void OnFixedUpdate(Timestep fixedDeltaTime);

		const Entity& GetEntity() const { return m_Entity; }
		const physx::PxRigidActor* GetPhysXActor() const { return m_RigidActor; }
		physx::PxRigidActor* GetPhysXActor() { return m_RigidActor; }

		void AddCollider(BoxColliderComponent& collider, const Entity& entity, const glm::vec3& offset = glm::vec3(0.f));
		void AddCollider(SphereColliderComponent& collider, const Entity& entity, const glm::vec3& offset = glm::vec3(0.f));
		void AddCollider(CapsuleColliderComponent& collider, const Entity& entity, const glm::vec3& offset = glm::vec3(0.f));
		void AddCollider(MeshColliderComponent& collider, const Entity& entity, const glm::vec3& offset = glm::vec3(0.f));

	private:
		void CreateRigidActor();
		void SynchronizeTransform();

	private:
		physx::PxRigidActor* m_RigidActor = nullptr;
		RigidBodyComponent& m_RigidBodyComponent;
		Entity m_Entity;
		uint32_t m_LockFlags = 0;
		std::vector<Ref<ColliderShape>> m_Colliders;
	};
}
