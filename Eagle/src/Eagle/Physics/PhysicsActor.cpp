#include "egpch.h"
#include "PhysicsActor.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Math/Math.h"
#include "PhysXInternal.h"
#include "PhysicsEngine.h"
#include "Eagle/Script/ScriptEngine.h"

namespace Eagle
{
	PhysicsActor::PhysicsActor(Entity& entity, const PhysicsSettings& settings)
	: m_Settings(settings)
	, m_Entity(entity)
	, m_RigidBodyComponent(m_Entity.GetComponent<RigidBodyComponent>())
	{
		m_BodyType = m_RigidBodyComponent.BodyType;
		m_FilterData.word0 = 1; // word0 = own ID
		m_FilterData.word1 = 1; // word1 = ID mask to filter pairs that trigger a contact callback;
		m_FilterData.word2 = (uint32_t)m_RigidBodyComponent.CollisionDetection;
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
	}
	
	void PhysicsActor::SetRotation(const Rotator& rotation, bool autowake)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.q = PhysXUtils::ToPhysXQuat(rotation);
		m_RigidActor->setGlobalPose(transform, autowake);
	}
	
	void PhysicsActor::Rotate(const Rotator& rotation, bool autowake)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.q *= physx::PxQuat(rotation.GetQuat().x, {1.f, 0.f, 0.f})
					 * physx::PxQuat(rotation.GetQuat().y, {0.f, 1.f, 0.f})
					 * physx::PxQuat(rotation.GetQuat().z, {0.f, 0.f, 1.f});
		
		m_RigidActor->setGlobalPose(transform, autowake);
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
		return !IsDynamic() ? m_RigidBodyComponent.GetMass() : m_RigidActor->is<physx::PxRigidDynamic>()->getMass();
	}
	
	void PhysicsActor::SetMass(float mass)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set mass of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, mass);
	}
	
	void PhysicsActor::AddForce(const glm::vec3& force, ForceMode forceMode)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot add force to non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
			return;
		}
		else if (m_RigidBodyComponent.IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot add force to Kinamatic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot add torque to non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
			return;
		}
		else if (m_RigidBodyComponent.IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot add torque to Kinamatic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot get linear velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot set linear velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot get angular velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot set angular velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot get max linear velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot set max linear velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot get max angular velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot set max angular velocity of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot set linear damping of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot set angular damping of non-dynamic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
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
			EG_CORE_WARN("[PhysicsEngine] Cannot get kinematic target location of non-kinematic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
			return glm::vec3(0.f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");
		
		physx::PxTransform transform;
		actor->getKinematicTarget(transform);
		return PhysXUtils::FromPhysXVector(transform.p);
	}

	Rotator PhysicsActor::GetKinematicTargetRotation() const
	{
		if (!IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot get kinematic target rotation of non-kinematic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
			return glm::vec3(0.f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		physx::PxTransform transform;
		actor->getKinematicTarget(transform);
		return PhysXUtils::FromPhysXQuat(transform.q);
	}

	void PhysicsActor::SetKinematicTarget(const glm::vec3& location, const Rotator& rotation)
	{
		if (!IsKinematic())
		{
			EG_CORE_WARN("[PhysicsEngine] Cannot set kinematic target of non-kinematic PhysicsActor. Entity: '{0}'", m_Entity.GetSceneName());
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		EG_CORE_ASSERT(actor, "No actor");

		actor->setKinematicTarget(PhysXUtils::ToPhysXTranform(location, rotation));
	}

	void PhysicsActor::SetSimulationData()
	{
		for (auto& collider : m_Colliders)
			collider->SetFilterData(m_FilterData);
	}
	
	bool PhysicsActor::SetKinematic(bool bKinematic)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Static PhysicsActor can't be kinematic. Entity: '{0}'", m_Entity.GetSceneName());
			return false;
		}

		m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, bKinematic);
		return true;
	}

	void PhysicsActor::SetGravityEnabled(bool bEnabled)
	{
		m_RigidActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !bEnabled);
	}
	
	bool PhysicsActor::SetLockFlag(ActorLockFlag flag, bool value)
	{
		if (!IsDynamic())
		{
			EG_CORE_WARN("[PhysicsEngine] Can't lock Static Physics Actor. Entity: '{0}'", m_Entity.GetSceneName());
			return false;
		}

		if (value)
			m_LockFlags |= (uint32_t)flag;
		else
			m_LockFlags &= ~(uint32_t)flag;

		m_RigidActor->is<physx::PxRigidDynamic>()->setRigidDynamicLockFlags((physx::PxRigidDynamicLockFlags)m_LockFlags);

		return true;
	}

	void PhysicsActor::OnFixedUpdate(Timestep fixedDeltaTime)
	{
		if (!ScriptEngine::IsEntityModuleValid(m_Entity))
			return;

		ScriptEngine::OnPhysicsUpdateEntity(m_Entity, fixedDeltaTime);
	}
	
	Ref<BoxColliderShape> PhysicsActor::AddCollider(BoxColliderComponent& collider)
	{
		auto shape = MakeRef<BoxColliderShape>(collider, *this);
		m_Colliders.insert(shape);
		return shape;
	}

	Ref<SphereColliderShape> PhysicsActor::AddCollider(SphereColliderComponent& collider)
	{
		auto shape = MakeRef<SphereColliderShape>(collider, *this);
		m_Colliders.insert(shape);
		return shape;
	}

	Ref<CapsuleColliderShape> PhysicsActor::AddCollider(CapsuleColliderComponent& collider)
	{
		auto shape = MakeRef<CapsuleColliderShape>(collider, *this);
		m_Colliders.insert(shape);
		return shape;
	}

	Ref<MeshShape> PhysicsActor::AddCollider(MeshColliderComponent& collider)
	{
		static Ref<MeshShape> s_InvalidCollider;
		const auto& collisionMesh = collider.GetCollisionMesh();
		if (!collisionMesh)
		{
			EG_CORE_ERROR("[Physics Engine] Set collision mesh inside MeshCollider Component. Entity: '{0}'", collider.Parent.GetSceneName());
			return s_InvalidCollider;
		}

		if (collider.IsConvex())
		{
			auto shape = MakeRef<ConvexMeshShape>(collider, *this);
			if (shape->IsValid())
			{
				m_Colliders.insert(shape);
				return shape;
			}
			else
				return s_InvalidCollider;
		}
		else
		{
			if (IsDynamic() && !IsKinematic())
			{
				EG_CORE_ERROR("[Physics Engine] Can't have a non-convex MeshColliderComponent for a non-kinematic dynamic RigidBody Component. Entity: '{0}'", m_Entity.GetSceneName());
				return s_InvalidCollider;
			}
			auto shape = MakeRef<TriangleMeshShape>(collider, *this);
			if (shape->IsValid())
			{
				m_Colliders.insert(shape);
				return shape;
			}
			else
				return s_InvalidCollider;
		}
	}

	bool PhysicsActor::RemoveCollider(const Ref<ColliderShape>& shape)
	{
		auto it = m_Colliders.find(shape);
		if (it != m_Colliders.end())
		{
			ColliderShape* collider = it->get();
			m_RigidActor->detachShape(*collider->GetShape());
			collider->Release();
			m_Colliders.erase(it);
			return true;
		}
		return false;
	}

	void PhysicsActor::RemoveAllColliders()
	{
		for (auto& collider : m_Colliders)
		{
			m_RigidActor->detachShape(*collider->GetShape());
			collider->Release();
		}
		m_Colliders.clear();
	}

	void PhysicsActor::CreateRigidActor()
	{
		auto& physics = PhysXInternal::GetPhysics();
		const Transform& transform = m_Entity.GetWorldTransform();

		if (m_BodyType == RigidBodyComponent::Type::Static)
		{
			m_RigidActor = physics.createRigidStatic(PhysXUtils::ToPhysXTranform(transform));
			m_RigidActor->userData = this;
		}
		else
		{
			m_RigidActor = physics.createRigidDynamic(PhysXUtils::ToPhysXTranform(transform));

			SetLinearDamping(m_RigidBodyComponent.GetLinearDamping());
			SetAngularDamping(m_RigidBodyComponent.GetAngularDamping());

			SetKinematic(m_RigidBodyComponent.IsKinematic());

			SetLockFlag(ActorLockFlag::PositionX, m_RigidBodyComponent.IsPositionXLocked());
			SetLockFlag(ActorLockFlag::PositionY, m_RigidBodyComponent.IsPositionYLocked());
			SetLockFlag(ActorLockFlag::PositionZ, m_RigidBodyComponent.IsPositionZLocked());

			SetLockFlag(ActorLockFlag::RotationX, m_RigidBodyComponent.IsRotationXLocked());
			SetLockFlag(ActorLockFlag::RotationY, m_RigidBodyComponent.IsRotationYLocked());
			SetLockFlag(ActorLockFlag::RotationZ, m_RigidBodyComponent.IsRotationZLocked());

			SetGravityEnabled(m_RigidBodyComponent.IsGravityEnabled());

			m_RigidActor->is<physx::PxRigidDynamic>()->setSolverIterationCounts(m_Settings.SolverIterations, m_Settings.SolverVelocityIterations);
			m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, m_RigidBodyComponent.CollisionDetection == RigidBodyComponent::CollisionDetectionType::Continuous);
			m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, m_RigidBodyComponent.CollisionDetection == RigidBodyComponent::CollisionDetectionType::ContinuousSpeculative);

			SetMass(m_RigidBodyComponent.GetMass());

			m_RigidActor->userData = this;
		}
		#ifdef EG_DEBUG
			auto& name = m_Entity.GetComponent<EntitySceneNameComponent>().Name;
			m_RigidActor->setName(name.c_str());
		#endif
	}
	
	void PhysicsActor::SynchronizeTransform()
	{
		Transform transform;
		physx::PxTransform actorPose = m_RigidActor->getGlobalPose();
		transform.Location = PhysXUtils::FromPhysXVector(actorPose.p);
		transform.Rotation = PhysXUtils::FromPhysXQuat(actorPose.q);

		m_Entity.SetWorldLocation(transform.Location, false);
		m_Entity.SetWorldRotation(transform.Rotation, false);
	}
}
