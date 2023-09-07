#include "egpch.h"
#include "Component.h"

#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsShapes.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Components.h"

namespace Eagle
{
	void RigidBodyComponent::SetMass(float mass)
	{
		Mass = std::max(0.f, mass);
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetMass(Mass);
	}

	void RigidBodyComponent::SetLinearDamping(float linearDamping)
	{
		LinearDamping = std::max(0.f, linearDamping);
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLinearDamping(LinearDamping);
	}

	void RigidBodyComponent::SetAngularDamping(float angularDamping)
	{
		AngularDamping = std::max(0.f, angularDamping);
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetAngularDamping(AngularDamping);
	}

	void RigidBodyComponent::SetEnableGravity(bool bEnable)
	{
		bEnableGravity = bEnable;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetGravityEnabled(bEnable);
	}

	void RigidBodyComponent::SetIsKinematic(bool bKinematic)
	{
		this->bKinematic = bKinematic;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetKinematic(bKinematic);
	}

	void RigidBodyComponent::SetLockPosition(bool bLockX, bool bLockY, bool bLockZ)
	{
		bLockPositionX = bLockX;
		bLockPositionY = bLockY;
		bLockPositionZ = bLockZ;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
			{
				actor->SetLockFlag(ActorLockFlag::PositionX, bLockX);
				actor->SetLockFlag(ActorLockFlag::PositionY, bLockY);
				actor->SetLockFlag(ActorLockFlag::PositionZ, bLockZ);
			}
	}

	void RigidBodyComponent::SetLockPositionX(bool bLock)
	{
		bLockPositionX = bLock;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLockFlag(ActorLockFlag::PositionX, bLock);
	}

	void RigidBodyComponent::SetLockPositionY(bool bLock)
	{
		bLockPositionY = bLock;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLockFlag(ActorLockFlag::PositionY, bLock);
	}

	void RigidBodyComponent::SetLockPositionZ(bool bLock)
	{
		bLockPositionZ = bLock;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLockFlag(ActorLockFlag::PositionZ, bLock);
	}

	void RigidBodyComponent::SetLockRotation(bool bLockX, bool bLockY, bool bLockZ)
	{
		bLockRotationX = bLockX;
		bLockRotationY = bLockY;
		bLockRotationZ = bLockZ;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
			{
				actor->SetLockFlag(ActorLockFlag::RotationX, bLockX);
				actor->SetLockFlag(ActorLockFlag::RotationY, bLockY);
				actor->SetLockFlag(ActorLockFlag::RotationZ, bLockZ);
			}
	}

	void RigidBodyComponent::SetLockRotationX(bool bLock)
	{
		bLockRotationX = bLock;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLockFlag(ActorLockFlag::RotationX, bLock);
	}

	void RigidBodyComponent::SetLockRotationY(bool bLock)
	{
		bLockRotationY = bLock;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLockFlag(ActorLockFlag::RotationY, bLock);
	}

	void RigidBodyComponent::SetLockRotationZ(bool bLock)
	{
		bLockRotationZ = bLock;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLockFlag(ActorLockFlag::RotationZ, bLock);
	}

	void BaseColliderComponent::SetWorldTransform(const Transform& worldTransform)
	{
		SceneComponent::SetWorldTransform(worldTransform);
		UpdatePhysicsTransform();
	}

	void BaseColliderComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		SceneComponent::SetRelativeTransform(relativeTransform);
		UpdatePhysicsTransform();
	}
	
	void BoxColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void BoxColliderComponent::SetPhysicsMaterial(const Ref<PhysicsMaterial>& material)
	{
		Material = material;
		m_Shape->SetPhysicsMaterial(material);
	}

	void BoxColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void BoxColliderComponent::OnInit(Entity& entity)
	{
		BaseColliderComponent::OnInit(entity);
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
		m_Shape->SetFilterData(actor->GetFilterData());
	}

	void BoxColliderComponent::OnRemoved(Entity& entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}
	
	void BoxColliderComponent::SetSize(const glm::vec3& size)
	{
		m_Size = glm::max(size, glm::vec3(0.f));
		m_Shape->SetSize(m_Size);
	}

	void BoxColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			const glm::vec3 newSize = WorldTransform.Scale3D * m_Size;
			if (newSize != oldSize)
				m_Shape->SetSize(m_Size);
		}
	}

	void SphereColliderComponent::SetRadius(float radius)
	{
		Radius = glm::max(radius, 0.f);
		m_Shape->SetRadius(Radius);
	}
	
	void SphereColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void SphereColliderComponent::SetPhysicsMaterial(const Ref<PhysicsMaterial>& material)
	{
		Material = material;
		m_Shape->SetPhysicsMaterial(material);
	}

	void SphereColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void SphereColliderComponent::OnInit(Entity& entity)
	{
		BaseColliderComponent::OnInit(entity);
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
		m_Shape->SetFilterData(actor->GetFilterData());
	}

	void SphereColliderComponent::OnRemoved(Entity& entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}

	void SphereColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const glm::vec3& newSize = WorldTransform.Scale3D;
			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			if (newSize != oldSize)
				m_Shape->SetRadius(Radius);
		}
	}

	void CapsuleColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void CapsuleColliderComponent::SetPhysicsMaterial(const Ref<PhysicsMaterial>& material)
	{
		Material = material;
		m_Shape->SetPhysicsMaterial(material);
	}

	void CapsuleColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void CapsuleColliderComponent::SetHeightAndRadius(float height, float radius)
	{
		Height = glm::max(height, 0.f);
		Radius = glm::max(radius, 0.f);
		m_Shape->SetHeightAndRadius(Height, Radius);
	}
	
	void CapsuleColliderComponent::OnInit(Entity& entity)
	{
		BaseColliderComponent::OnInit(entity);
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
		m_Shape->SetFilterData(actor->GetFilterData());
	}

	void CapsuleColliderComponent::OnRemoved(Entity& entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}

	void CapsuleColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const glm::vec3& newSize = WorldTransform.Scale3D;
			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			if (newSize != oldSize)
				m_Shape->SetHeightAndRadius(Height, Radius);
		}
	}

	void MeshColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		if (m_Shape)
			m_Shape->SetIsTrigger(bTrigger);
	}
	
	void MeshColliderComponent::SetPhysicsMaterial(const Ref<PhysicsMaterial>& material)
	{
		Material = material;
		if (m_Shape)
			m_Shape->SetPhysicsMaterial(material);
	}

	void MeshColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		if (m_Shape)
			m_Shape->SetShowCollision(bShowCollision);
	}
	
	void MeshColliderComponent::SetCollisionMesh(const Ref<StaticMesh>& mesh)
	{
		CollisionMesh = mesh;

		auto actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
			if (CollisionMesh)
				m_Shape = actor->AddCollider(*this);
		}
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			if (CollisionMesh)
				m_Shape = actor->AddCollider(*this);
			else
				m_Shape = nullptr;
		}
		if (m_Shape)
			m_Shape->SetFilterData(actor->GetFilterData());
	}
	
	void MeshColliderComponent::OnInit(Entity& entity)
	{
		BaseColliderComponent::OnInit(entity);
		if (Parent && Parent.HasComponent<StaticMeshComponent>())
		{
			auto& comp = Parent.GetComponent<StaticMeshComponent>();
			CollisionMesh = comp.GetStaticMesh();
			SetRelativeTransform(comp.GetRelativeTransform());
		}
		
		SetCollisionMesh(CollisionMesh);
	}

	void MeshColliderComponent::OnRemoved(Entity& entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			if (m_Shape)
			{
				actor->RemoveCollider(m_Shape);
				m_Shape.reset();
			}	
		}
	}
	
	void MeshColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const glm::vec3& newSize = WorldTransform.Scale3D;
			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			if (newSize != oldSize)
				m_Shape->SetScale(newSize);
		}
	}
}
