#pragma once

#include "Eagle/Core/Entity.h"
#include "Eagle/Components/Components.h"
#include "PhysicsEngine.h"
#include "PhysicsUtils.h"
#include "PhysicsShapes.h"
#include "PhysicsActorBase.h"
#include <PhysX/PxPhysicsAPI.h>

namespace Eagle
{
	class PhysicsActor : public PhysicsActorBase
	{
	public:
		PhysicsActor(const Entity& entity);
		~PhysicsActor();

		void SceneRequestToSyncTransforms() override { SynchronizeTransform(); }

		glm::vec3 GetLocation() const { return PhysXUtils::FromPhysXVector(m_RigidActor->getGlobalPose().p); }
		void SetLocation(const glm::vec3& location, bool autowake = true);

		Rotator GetRotation() const { return PhysXUtils::FromPhysXQuat(m_RigidActor->getGlobalPose().q); }
		void SetRotation(const Rotator& rotation, bool autowake = true);
		void Rotate(const Rotator& rotation, bool autowake = true);

		Transform GetTransform() const { return PhysXUtils::FromPhysXTransform(m_RigidActor->getGlobalPose()); }
		void SetTransform(const Transform& transform, bool bAutowake = true);

		void WakeUp();
		void PutToSleep();

		void SetCollisionDetectionType(CollisionDetectionType type);
		CollisionDetectionType GetCollisionDetectionType() const { return m_CollisionDetection; }

		void SetPositionSolverIterations(uint32_t iterations);
		uint32_t GetPositionSolverIterations() const;

		void SetVelocitySolverIterations(uint32_t iterations);
		uint32_t GetVelocitySolverIterations() const;

		float GetMass() const;
		void SetMass(float mass);

		void AddForce(const glm::vec3& force, ForceMode forceMode);
		void AddForceAtLocation(const glm::vec3& location, const glm::vec3& force, ForceMode forceMode);
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

		bool IsDynamic() const { return m_BodyType == PhysicsBodyType::Dynamic; }

		bool IsKinematic() const;
		bool SetKinematic(bool bKinematic);

		bool IsGravityEnabled() const { return !m_RigidActor->getActorFlags().isSet(physx::PxActorFlag::eDISABLE_GRAVITY); }
		void SetGravityEnabled(bool bEnabled);

		void SetLockFlag(ActorLockFlag flag);
		ActorLockFlag GetLockFlags() const { return m_LockFlags; }
		PhysicsBodyType GetBodyType() const { return m_BodyType; }

		void OnFixedUpdate(Timestep fixedDeltaTime);

		Ref<BoxColliderShape> AddCollider(BoxColliderComponent& collider);
		Ref<SphereColliderShape> AddCollider(SphereColliderComponent& collider);
		Ref<CapsuleColliderShape> AddCollider(CapsuleColliderComponent& collider);
		std::array<Ref<MeshShape>, 2> AddCollider(MeshColliderComponent& collider);

		bool RemoveCollider(const Ref<ColliderShape>& shape);
		void RemoveAllColliders();
		void Release();

		void SynchronizeTransform();

	private:
		template <typename T>
		void AddColliderIfCan()
		{
			static_assert(std::is_base_of<BaseColliderComponent, T>::value);
			if (m_Entity.HasComponent<T>())
				AddCollider(m_Entity.GetComponent<T>());
		}

		void CreateRigidActor();

	private:
		PhysicsBodyType m_BodyType;
		ActorLockFlag m_LockFlags = ActorLockFlag::None;
		CollisionDetectionType m_CollisionDetection = CollisionDetectionType::Discrete;
		std::set<Ref<ColliderShape>> m_Colliders;
	};
}
