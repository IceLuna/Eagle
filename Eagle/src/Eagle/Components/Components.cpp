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
			actor->SetMass(Mass);
	}

	void RigidBodyComponent::SetLinearDamping(float linearDamping)
	{
		LinearDamping = std::max(0.f, linearDamping);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLinearDamping(LinearDamping);
	}

	void RigidBodyComponent::SetAngularDamping(float angularDamping)
	{
		AngularDamping = std::max(0.f, angularDamping);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetAngularDamping(AngularDamping);
	}

	void RigidBodyComponent::SetEnableGravity(bool bEnable)
	{
		bEnableGravity = bEnable;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetGravityEnabled(bEnable);
	}

	void RigidBodyComponent::SetIsKinematic(bool bKinematic)
	{
		this->bKinematic = bKinematic;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetKinematic(bKinematic);
	}

	void RigidBodyComponent::SetMaxLinearVelocity(float velocity)
	{
		MaxLinearVelocity = glm::max(0.f, velocity);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetMaxLinearVelocity(MaxLinearVelocity);
	}

	void RigidBodyComponent::SetMaxAngularVelocity(float velocity)
	{
		MaxAngularVelocity = glm::max(0.f, velocity);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetMaxAngularVelocity(MaxAngularVelocity);
	}

	void RigidBodyComponent::SetLockFlag(ActorLockFlag flag, bool value)
	{
		if (value)
			m_LockFlags |= flag;
		else
			m_LockFlags &= ~flag;

		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLockFlag(m_LockFlags);
	}

	void RigidBodyComponent::SetLockFlag(ActorLockFlag flag)
	{
		m_LockFlags = flag;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLockFlag(m_LockFlags);
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
	
	void BoxColliderComponent::OnInit(Entity entity)
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

	void BoxColliderComponent::OnRemoved(Entity entity)
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
	
	void SphereColliderComponent::OnInit(Entity entity)
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

	void SphereColliderComponent::OnRemoved(Entity entity)
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
	
	void CapsuleColliderComponent::OnInit(Entity entity)
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

	void CapsuleColliderComponent::OnRemoved(Entity entity)
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
		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetIsTrigger(bTrigger);
	}
	
	void MeshColliderComponent::SetPhysicsMaterial(const Ref<PhysicsMaterial>& material)
	{
		Material = material;
		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetPhysicsMaterial(material);
	}

	void MeshColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		if (m_Shapes[0]) // No need to enable it for the backside
			m_Shapes[0]->SetShowCollision(bShowCollision);
	}
	
	void MeshColliderComponent::SetCollisionMeshAsset(const Ref<AssetMesh>& meshAsset)
	{
		m_CollisionMeshAsset = meshAsset;

		auto actor = Parent.GetPhysicsActor();
		if (actor)
		{
			for (auto& shape : m_Shapes)
			{
				actor->RemoveCollider(shape);
				shape.reset();
			}

			if (m_CollisionMeshAsset)
				m_Shapes = actor->AddCollider(*this);
		}
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			if (m_CollisionMeshAsset)
				m_Shapes = actor->AddCollider(*this);
			else
			{
				for (auto& shape : m_Shapes)
					shape.reset();
			}
		}

		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetFilterData(actor->GetFilterData());
	}
	
	void MeshColliderComponent::OnInit(Entity entity)
	{
		BaseColliderComponent::OnInit(entity);
		if (Parent && Parent.HasComponent<StaticMeshComponent>())
		{
			auto& comp = Parent.GetComponent<StaticMeshComponent>();
			m_CollisionMeshAsset = comp.GetMeshAsset();
			SetRelativeTransform(comp.GetRelativeTransform());
		}
		
		SetCollisionMeshAsset(m_CollisionMeshAsset);
	}

	void MeshColliderComponent::OnRemoved(Entity entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			for (auto& shape : m_Shapes)
				if (shape)
				{
					actor->RemoveCollider(shape);
					shape.reset();
				}	
		}
	}
	
	void MeshColliderComponent::UpdatePhysicsTransform()
	{
		for (auto& shape : m_Shapes)
		{
			if (shape)
			{
				shape->SetRelativeLocationAndRotation(RelativeTransform);

				const glm::vec3& newSize = WorldTransform.Scale3D;
				const glm::vec3& oldSize = shape->GetColliderScale();
				if (newSize != oldSize)
					shape->SetScale(newSize);
			}
		}
	}
}
