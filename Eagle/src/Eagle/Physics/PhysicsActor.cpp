#include "egpch.h"
#include "PhysicsActor.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Math/Math.h"
#include "PhysXInternal.h"
#include "PhysicsEngine.h"
#include "Eagle/Script/ScriptEngine.h"

namespace Eagle
{
	PhysicsActor::PhysicsActor(Entity& entity)
	: m_Entity(entity)
	, m_RigidBodyComponent(m_Entity.GetComponent<RigidBodyComponent>())
	{
		CreateRigidActor();
	}
	
	PhysicsActor::~PhysicsActor()
	{
	}
	
	void PhysicsActor::SetLocation(const glm::vec3& location, bool autowake)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.p = PhysXUtils::ToPhysXVector(location);
		m_RigidActor->setGlobalPose(transform, autowake);

		if (m_RigidBodyComponent.BodyType == RigidBodyComponent::Type::Static)
			SynchronizeTransform();
	}
	
	void PhysicsActor::SetRotation(const glm::vec3& rotation, bool autowake)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.q = PhysXUtils::ToPhysXQuat(rotation);
		m_RigidActor->setGlobalPose(transform, autowake);

		if (m_RigidBodyComponent.BodyType == RigidBodyComponent::Type::Static)
			SynchronizeTransform();
	}
	
	void PhysicsActor::Rotate(const glm::vec3& rotation, bool autowake)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.q *= physx::PxQuat(rotation.x, {1.f, 0.f, 0.f}) 
					 * physx::PxQuat(rotation.y, {0.f, 1.f, 0.f})
					 * physx::PxQuat(rotation.z, {0.f, 0.f, 1.f});
		
		m_RigidActor->setGlobalPose(transform, autowake);

		if (m_RigidBodyComponent.BodyType == RigidBodyComponent::Type::Static)
			SynchronizeTransform();
	}
	
	void PhysicsActor::WakeUp()
	{
		if (IsDynamic())
			m_RigidActor->is<physx::PxRigidDynamic>()->wakeUp();
	}
	
	void PhysicsActor::PutToSleep()
	{
		if (IsDynamic())
			m_RigidActor->is<physx::PxRigidDynamic>()->putToSleep();
	}
	
	float PhysicsActor::GetMass() const
	{
		//TODO: check if we can always return 'm_RigidBodyComponent.Mass'
		return !IsDynamic() ? m_RigidBodyComponent.Mass : m_RigidActor->is<physx::PxRigidDynamic>()->getMass();
	}
	
	void PhysicsActor::SetMass(float mass)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set mass of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, mass);
		m_RigidBodyComponent.Mass = mass;
	}
	
	void PhysicsActor::AddForce(const glm::vec3& force, ForceMode forceMode)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot add force to non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		actor->addForce(PhysXUtils::ToPhysXVector(force), (physx::PxForceMode::Enum)forceMode);

	}
	
	void PhysicsActor::AddTorque(const glm::vec3& torque, ForceMode forceMode)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot add torque to non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		actor->addTorque(PhysXUtils::ToPhysXVector(torque), (physx::PxForceMode::Enum)forceMode);
	}
	
	glm::vec3 PhysicsActor::GetLinearVelocity() const
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get linear velocity of non-dynamic PhysicsActor");
			return glm::vec3(0.f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		return PhysXUtils::FromPhysXVector(actor->getLinearVelocity());
	}
	
	void PhysicsActor::SetLinearVelocity(const glm::vec3& velocity)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set linear velocity of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		actor->setLinearVelocity(PhysXUtils::ToPhysXVector(velocity));
	}
	
	glm::vec3 PhysicsActor::GetAngularVelocity() const
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get angular velocity of non-dynamic PhysicsActor");
			return glm::vec3(0.f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		return PhysXUtils::FromPhysXVector(actor->getAngularVelocity());
	}
	
	void PhysicsActor::SetAngularVelocity(const glm::vec3& velocity)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set angular velocity of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setAngularVelocity(PhysXUtils::ToPhysXVector(velocity));
	}
	
	float PhysicsActor::GetMaxLinearVelocity() const
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get max linear velocity of non-dynamic PhysicsActor");
			return 0.f;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		
		return actor->getMaxLinearVelocity();
	}
	
	void PhysicsActor::SetMaxLinearVelocity(float maxVelocity)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set max linear velocity of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setMaxLinearVelocity(maxVelocity);
	}
	
	float PhysicsActor::GetMaxAngularVelocity() const
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get max angular velocity of non-dynamic PhysicsActor");
			return 0.f;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		return actor->getMaxAngularVelocity();
	}
	
	void PhysicsActor::SetMaxAngularVelocity(float maxVelocity)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set max angular velocity of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setMaxAngularVelocity(maxVelocity);
	}
	
	void PhysicsActor::SetLinearDamping(float damping) const
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set linear damping of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setLinearDamping(damping);
	}
	
	void PhysicsActor::SetAngularDamping(float damping) const
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set angular damping of non-dynamic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setAngularDamping(damping);
	}

	glm::vec3 PhysicsActor::GetKinematicTargetLocation() const
	{
		if (!IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get kinematic target location of non-kinematic PhysicsActor");
			return glm::vec3(0.f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		
		physx::PxTransform transform;
		actor->getKinematicTarget(transform);
		return PhysXUtils::FromPhysXVector(transform.p);
	}

	glm::vec3 PhysicsActor::GetKinematicTargetRotation() const
	{
		if (!IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get kinematic target rotation of non-kinematic PhysicsActor");
			return glm::vec3(0.f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		physx::PxTransform transform;
		actor->getKinematicTarget(transform);
		return glm::eulerAngles(PhysXUtils::FromPhysXQuat(transform.q));
	}

	void PhysicsActor::SetKinematicTarget(const glm::vec3& location, const glm::vec3& rotation)
	{
		if (!IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set kinematic target of non-kinematic PhysicsActor");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setKinematicTarget(PhysXUtils::ToPhysXTranform(location, rotation));
	}

	void PhysicsActor::SetSimulationData()
	{
		physx::PxFilterData filterData;
		filterData.word0 = 1;
		filterData.word1 = 1;
		filterData.word2 = (uint32_t)m_RigidBodyComponent.CollisionDetection;

		for (auto& collider : m_Colliders)
			collider->SetFilterData(filterData);
	}
	
	void PhysicsActor::SetKinematic(bool bKinematic)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Static PhysicsActor can't be kinematic");
			return;
		}

		m_RigidBodyComponent.IsKinematic = bKinematic;
		m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, bKinematic);
	}

	void PhysicsActor::SetGravityEnabled(bool bEnabled)
	{
		m_RigidActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !bEnabled);
	}
	
	void PhysicsActor::SetLockFlag(ActorLockFlag flag, bool value)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Can't lock Static Physics Actor");
			return;
		}

		if (value)
			m_LockFlags |= (uint32_t)flag;
		else
			m_LockFlags &= ~(uint32_t)flag;

		m_RigidActor->is<physx::PxRigidDynamic>()->setRigidDynamicLockFlags((physx::PxRigidDynamicLockFlags)m_LockFlags);
	}

	void PhysicsActor::OnFixedUpdate(Timestep fixedDeltaTime)
	{
		if (!ScriptEngine::IsEntityModuleValid(m_Entity))
			return;

		ScriptEngine::OnPhysicsUpdateEntity(m_Entity, fixedDeltaTime);
	}
	
	void PhysicsActor::AddCollider(BoxColliderComponent& collider)
	{
		m_Colliders.push_back(MakeRef<BoxColliderShape>(collider, *this));
	}

	void PhysicsActor::AddCollider(SphereColliderComponent& collider)
	{
		m_Colliders.push_back(MakeRef<SphereColliderShape>(collider, *this));
	}

	void PhysicsActor::AddCollider(CapsuleColliderComponent& collider)
	{
		m_Colliders.push_back(MakeRef<CapsuleColliderShape>(collider, *this));
	}

	void PhysicsActor::AddCollider(MeshColliderComponent& collider)
	{
		if (!collider.CollisionMesh)
		{
			EG_CORE_ERROR("[Physics Engine] Set collision mesh inside MeshCollider Component. Entity: '{0}'", collider.Parent.GetComponent<EntitySceneNameComponent>().Name);
			return;
		}

		if (collider.IsConvex)
		{
			m_Colliders.push_back(MakeRef<ConvexMeshShape>(collider, *this));
		}
		else
		{
			if (IsDynamic() && !IsKinematic())
			{
				EG_CORE_ERROR("[Physics Engine] Can't have a non-convex MeshColliderComponent for a non-kinematic dynamic RigidBody Component");
				return;
			}
			m_Colliders.push_back(MakeRef<TriangleMeshShape>(collider, *this));
		}
	}

	void PhysicsActor::CreateRigidActor()
	{
		auto& physics = PhysXInternal::GetPhysics();
		
		//glm::mat4 transform = Math::ToTransformMatrix(m_Entity.GetWorldTransform());
		const Transform& transform = m_Entity.GetWorldTransform();

		if (m_RigidBodyComponent.BodyType == RigidBodyComponent::Type::Static)
		{
			m_RigidActor = physics.createRigidStatic(PhysXUtils::ToPhysXTranform(transform));
			m_RigidActor->userData = this;
		}
		else
		{
			const PhysicsSettings& settings = PhysicsEngine::GetSettings();

			m_RigidActor = physics.createRigidDynamic(PhysXUtils::ToPhysXTranform(transform));

			SetLinearDamping(m_RigidBodyComponent.LinearDamping);
			SetAngularDamping(m_RigidBodyComponent.AngularDamping);

			SetKinematic(m_RigidBodyComponent.IsKinematic);

			SetLockFlag(ActorLockFlag::LocationX, m_RigidBodyComponent.LockPositionX);
			SetLockFlag(ActorLockFlag::LocationY, m_RigidBodyComponent.LockPositionY);
			SetLockFlag(ActorLockFlag::LocationZ, m_RigidBodyComponent.LockPositionZ);

			SetLockFlag(ActorLockFlag::RotationX, m_RigidBodyComponent.LockRotationX);
			SetLockFlag(ActorLockFlag::RotationY, m_RigidBodyComponent.LockRotationY);
			SetLockFlag(ActorLockFlag::RotationZ, m_RigidBodyComponent.LockRotationZ);

			SetGravityEnabled(m_RigidBodyComponent.EnableGravity);

			m_RigidActor->is<physx::PxRigidDynamic>()->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
			m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, m_RigidBodyComponent.CollisionDetection == RigidBodyComponent::CollisionDetectionType::Continuous);
			m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, m_RigidBodyComponent.CollisionDetection == RigidBodyComponent::CollisionDetectionType::ContinuousSpeculative);
		
			if (m_Entity.HasComponent<BoxColliderComponent>()) AddCollider(m_Entity.GetComponent<BoxColliderComponent>());
			if (m_Entity.HasComponent<SphereColliderComponent>()) AddCollider(m_Entity.GetComponent<SphereColliderComponent>());
			if (m_Entity.HasComponent<CapsuleColliderComponent>()) AddCollider(m_Entity.GetComponent<CapsuleColliderComponent>());
			if (m_Entity.HasComponent<MeshColliderComponent>()) AddCollider(m_Entity.GetComponent<MeshColliderComponent>());
		
			SetMass(m_RigidBodyComponent.Mass);

			m_RigidActor->userData = this;

			#ifdef EG_DEBUG
				auto& name = m_Entity.GetComponent<EntitySceneNameComponent>().Name;
				m_RigidActor->setName(name.c_str());
			#endif
		}

	}
	
	void PhysicsActor::SynchronizeTransform()
	{
		Transform transform;
		physx::PxTransform actorPose = m_RigidActor->getGlobalPose();
		transform.Location = PhysXUtils::FromPhysXVector(actorPose.p);
		transform.Rotation = glm::eulerAngles(PhysXUtils::FromPhysXQuat(actorPose.q));

		m_Entity.SetWorldLocation(transform.Location);
		m_Entity.SetWorldRotation(transform.Rotation);
	}
}
