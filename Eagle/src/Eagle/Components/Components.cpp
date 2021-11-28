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
		Mass = mass;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetMass(mass);
	}

	void RigidBodyComponent::SetLinearDamping(float linearDamping)
	{
		LinearDamping = linearDamping;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetLinearDamping(linearDamping);
	}

	void RigidBodyComponent::SetAngularDamping(float angularDamping)
	{
		AngularDamping = angularDamping;
		if (const auto& actor = Parent.GetPhysicsActor())
			if (actor->GetBodyType() == Type::Dynamic)
				actor->SetAngularDamping(angularDamping);
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
	
	void BoxColliderComponent::SetSize(const glm::vec3& size)
	{
		m_Size = size;
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
		Radius = radius;
		m_Shape->SetRadius(radius);
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
		Height = height;
		Radius = radius;
		m_Shape->SetHeightAndRadius(height, radius);
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
		if (Parent)
			if (Parent.HasComponent<StaticMeshComponent>())
				CollisionMesh = Parent.GetComponent<StaticMeshComponent>().StaticMesh;
		
		SetCollisionMesh(CollisionMesh);
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
