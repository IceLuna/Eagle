#pragma once

#include "PhysicsEngine.h"
#include "PhysicsUtils.h"
#include "PhysicsShapes.h"
#include <PhysX/PxPhysicsAPI.h>
#include "Eagle/Components/Components.h"

namespace Eagle
{
	class Entity;
	class RigidBodyComponent;

	class PhysicsActor
	{
	public:
		PhysicsActor(Entity& entity, const PhysicsSettings& settings);
		~PhysicsActor();

		glm::vec3 GetLocation() const { return PhysXUtils::FromPhysXVector(m_RigidActor->getGlobalPose().p); }
		void SetLocation(const glm::vec3& location, bool autowake = true);

		Rotator GetRotation() const { return PhysXUtils::FromPhysXQuat(m_RigidActor->getGlobalPose().q); }
		void SetRotation(const Rotator& rotation, bool autowake = true);
		void Rotate(const Rotator& rotation, bool autowake = true);

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

		void SetLinearDamping(float damping);
		void SetAngularDamping(float damping);

		float GetLinearDamping() const;
		float GetAngularDamping() const;

		Transform GetKinematicTarget() const;
		glm::vec3 GetKinematicTargetLocation() const;
		Rotator GetKinematicTargetRotation() const;
		void SetKinematicTarget(const glm::vec3& location, const Rotator& rotation);
		void SetKinematicTargetLocation(const glm::vec3& location);
		void SetKinematicTargetRotation(const Rotator& rotation);

		bool IsDynamic() const { return m_BodyType == RigidBodyComponent::Type::Dynamic; }

		bool IsKinematic() const { return IsDynamic() && m_RigidBodyComponent.IsKinematic(); };
		bool SetKinematic(bool bKinematic);

		bool IsGravityEnabled() const { return !m_RigidActor->getActorFlags().isSet(physx::PxActorFlag::eDISABLE_GRAVITY); }
		void SetGravityEnabled(bool bEnabled);

		void SetLockFlag(ActorLockFlag flag);
		ActorLockFlag GetLockFlags() const { return m_LockFlags; }
		RigidBodyComponent::Type GetBodyType() const { return m_BodyType; }

		void OnFixedUpdate(Timestep fixedDeltaTime);

		const Entity& GetEntity() const { return m_Entity; }
		Entity& GetEntity() { return m_Entity; }
		const physx::PxRigidActor* GetPhysXActor() const { return m_RigidActor; }
		physx::PxRigidActor* GetPhysXActor() { return m_RigidActor; }

		Ref<BoxColliderShape> AddCollider(BoxColliderComponent& collider);
		Ref<SphereColliderShape> AddCollider(SphereColliderComponent& collider);
		Ref<CapsuleColliderShape> AddCollider(CapsuleColliderComponent& collider);
		std::array<Ref<MeshShape>, 2> AddCollider(MeshColliderComponent& collider);
		const physx::PxFilterData& GetFilterData() const { return m_FilterData; }

		bool RemoveCollider(const Ref<ColliderShape>& shape);
		void RemoveAllColliders();

		float GetSimulationTimeStep() const { return m_Settings.FixedTimeStep; }

	private:
		template <typename T>
		void AddColliderIfCan()
		{
			static_assert(std::is_base_of<BaseColliderComponent, T>::value);
			if (m_Entity.HasComponent<T>())
				AddCollider(m_Entity.GetComponent<T>());
		}

		void CreateRigidActor();
		void SynchronizeTransform();
		void SetSimulationData();

	private:
		const PhysicsSettings& m_Settings;
		physx::PxFilterData m_FilterData;
		RigidBodyComponent::Type m_BodyType;
		physx::PxRigidActor* m_RigidActor = nullptr;
		Entity m_Entity;
		RigidBodyComponent& m_RigidBodyComponent;
		ActorLockFlag m_LockFlags = ActorLockFlag::None;
		std::set<Ref<ColliderShape>> m_Colliders;

		friend class PhysicsScene;
	};
}
