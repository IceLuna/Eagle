#include "egpch.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsRagdollActor.h"

namespace Eagle
{
	Entity Entity::Null = Entity();

	void Entity::SetParent(Entity parent)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		auto& ownershipComponent = GetComponent<OwnershipComponent>();

		if (ownershipComponent.EntityParent)
			ownershipComponent.EntityParent.RemoveChildren(*this);

		ownershipComponent.EntityParent = parent;

		auto& transformComponent = GetComponent<TransformComponent>();
		if (parent)
		{
			parent.AddChildren(*this);
			SetWorldTransform(GetWorldTransform());
		}
		else
		{
			transformComponent.RelativeTransform = transformComponent.WorldTransform;
		}
	}

	Entity Entity::GetParent()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().EntityParent;
	}

	const Entity Entity::GetParent() const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().EntityParent;
	}

	void Entity::NotifyAllChildren(Notification notification, PhysicsMove physicsMove)
	{
		auto& children = GetComponent<OwnershipComponent>().Children;

		// If the physics moved this entity, children's physics is teleported along
		const PhysicsMove childPhysicsMove = physicsMove == PhysicsMove::Skip ? PhysicsMove::Teleport : physicsMove;

		// Notify Entity-childs
		for (auto& child : children)
		{
			if (notification == Notification::OnParentTransformChanged)
				child.SetRelativeTransform(child.GetRelativeTransform(), childPhysicsMove);
			else
				child.OnNotify(notification);
		}

		// Notify components
		ComponentsNotificationSystem::Notify(*this, Notification::OnParentTransformChanged);
	}

	void Entity::AddChildren(Entity child)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		GetComponent<OwnershipComponent>().Children.push_back(child);
	}

	void Entity::RemoveChildren(Entity child)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		auto& oc = GetComponent<OwnershipComponent>();
		auto it = std::find(oc.Children.begin(), oc.Children.end(), child);
		if (it != oc.Children.end())
		{
			oc.Children.erase(it);
		}
	}

	void Entity::TriggerAnimationEvent(const std::string& name, float time) const
	{
		if (HasComponent<ScriptComponent>() == false)
			return;

		ScriptEngine::OnAnimationEventEntity(*this, name, time);
	}

	void Entity::TriggerSequenceEvent(const std::string& name, float time) const
	{
		if (HasComponent<ScriptComponent>() == false)
			return;

		ScriptEngine::OnSequenceEventEntity(*this, name, time);
	}

	const std::vector<Entity>& Entity::GetChildren() const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Children;
	}

	const Transform& Entity::GetWorldTransform() const
	{
		return GetComponent<TransformComponent>().WorldTransform;
	}

	void Entity::SetWorldTransform(const Transform& worldTransform, PhysicsMove physicsMove)
	{
		auto& transformComponent = GetComponent<TransformComponent>();
		const Transform oldWorldTransform = transformComponent.WorldTransform;
		if (Entity parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform = worldTransform;

			Rotator inverseParentWorldRotation = parentWorldTransform.Rotation.Conjugate();

			myRelativeTransform.Rotation = inverseParentWorldRotation * myWorldTransform.Rotation;
			myRelativeTransform.Scale3D = myWorldTransform.Scale3D / parentWorldTransform.Scale3D;

			myRelativeTransform.Location = inverseParentWorldRotation.GetQuat() * (myWorldTransform.Location - parentWorldTransform.Location);
			myRelativeTransform.Location /= parentWorldTransform.Scale3D;
		}
		else
		{
			transformComponent.WorldTransform = worldTransform;
			transformComponent.RelativeTransform = worldTransform;
		}

		UpdatePhysicsTransform(oldWorldTransform, physicsMove);
		NotifyAllChildren(Notification::OnParentTransformChanged, physicsMove);

#if EG_DEBUG
		EG_CORE_ASSERT(!glm::any(glm::isnan(transformComponent.WorldTransform.Location)));
		EG_CORE_ASSERT(!glm::any(glm::isnan(transformComponent.WorldTransform.Rotation.GetQuat())));
		EG_CORE_ASSERT(!glm::any(glm::isnan(transformComponent.WorldTransform.Scale3D)));
#endif
	}

	void Entity::SetWorldLocation(const glm::vec3& worldLocation, PhysicsMove physicsMove)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Location = worldLocation;
		SetWorldTransform(transform, physicsMove);
	}

	const glm::vec3& Entity::GetWorldLocation() const
	{
		return GetComponent<TransformComponent>().WorldTransform.Location;
	}

	void Entity::SetWorldRotation(const Rotator& worldRotation, PhysicsMove physicsMove)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Rotation = worldRotation;
		SetWorldTransform(transform, physicsMove);
	}

	const Rotator& Entity::GetWorldRotation() const
	{
		return GetComponent<TransformComponent>().WorldTransform.Rotation;
	}

	void Entity::SetWorldScale(const glm::vec3& worldScale, PhysicsMove physicsMove)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Scale3D = worldScale;
		SetWorldTransform(transform, physicsMove);
	}

	const glm::vec3& Entity::GetWorldScale() const
	{
		return GetComponent<TransformComponent>().WorldTransform.Scale3D;
	}

	void Entity::SetRelativeLocation(const glm::vec3& relativeLocation, PhysicsMove physicsMove)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Location = relativeLocation;
		SetRelativeTransform(transform, physicsMove);
	}

	const glm::vec3& Entity::GetRelativeLocation() const
	{
		return GetComponent<TransformComponent>().RelativeTransform.Location;
	}

	void Entity::SetRelativeRotation(const Rotator& relativeRotation, PhysicsMove physicsMove)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Rotation = relativeRotation;
		SetRelativeTransform(transform, physicsMove);
	}

	const Rotator& Entity::GetRelativeRotation() const
	{
		return GetComponent<TransformComponent>().RelativeTransform.Rotation;
	}

	void Entity::SetRelativeScale(const glm::vec3& relativeScale, PhysicsMove physicsMove)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Scale3D = relativeScale;
		SetRelativeTransform(transform, physicsMove);
	}

	const glm::vec3& Entity::GetRelativeScale() const
	{
		return GetComponent<TransformComponent>().RelativeTransform.Scale3D;
	}

	const Transform& Entity::GetRelativeTransform() const
	{
		return GetComponent<TransformComponent>().RelativeTransform;
	}

	void Entity::SetLinearVelocity(const glm::vec3& velocity)
	{
		if (auto actor = GetPhysicsActor())
			return actor->SetLinearVelocity(velocity);
	}

	glm::vec3 Entity::GetLinearVelocity() const
	{
		if (auto actor = GetPhysicsActor())
			return actor->GetLinearVelocity();
		return glm::vec3(0.f);
	}

	void Entity::SetAngularVelocity(const glm::vec3& velocity)
	{
		if (auto actor = GetPhysicsActor())
			return actor->SetAngularVelocity(velocity);
	}

	glm::vec3 Entity::GetAngularVelocity() const
	{
		if (auto actor = GetPhysicsActor())
			return actor->GetAngularVelocity();
		return glm::vec3(0.f);
	}

	void Entity::SetRelativeTransform(const Transform& relativeTransform, PhysicsMove physicsMove)
	{
		if (Entity parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;
			const Transform oldWorldTransform = myWorldTransform;

			myRelativeTransform = relativeTransform;

			myWorldTransform.Rotation = parentWorldTransform.Rotation * myRelativeTransform.Rotation;
			myWorldTransform.Scale3D = parentWorldTransform.Scale3D * myRelativeTransform.Scale3D;

			glm::vec3 rotated = parentWorldTransform.Rotation.GetQuat() * (myRelativeTransform.Location * parentWorldTransform.Scale3D);
			myWorldTransform.Location = parentWorldTransform.Location + rotated;

			UpdatePhysicsTransform(oldWorldTransform, physicsMove);
			NotifyAllChildren(Notification::OnParentTransformChanged, physicsMove);
		}
		else
		{
			SetWorldTransform(relativeTransform, physicsMove);
		}
	}

	void Entity::UpdatePhysicsTransform(const Transform& oldWorldTransform, PhysicsMove physicsMove)
	{
		if (physicsMove == PhysicsMove::Skip)
			return;

		const Transform& worldTransform = GetComponent<TransformComponent>().WorldTransform;
		if (const auto& physicsActor = GetPhysicsActor())
			physicsActor->SetTransform(worldTransform, physicsMove);

		// A ragdoll isn't attached to the entity, so it's teleported separately
		if (physicsMove != PhysicsMove::Move && HasComponent<SkeletalMeshComponent>())
		{
			if (const auto& ragdoll = GetComponent<SkeletalMeshComponent>().GetRagdollActor())
			{
				const bool bResetVelocity = physicsMove == PhysicsMove::Reset;
				ragdoll->Teleport(oldWorldTransform, worldTransform, bResetVelocity);
			}
		}
	}

	bool Entity::HasParent() const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().EntityParent;
	}

	bool Entity::HasChildren() const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Children.size();
	}

	bool Entity::IsParentOf(const Entity& entity) const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");
		if (entity.GetParent() == (*this))
			return true;

		const auto& children = GetChildren();
		for (auto& child : children)
		{
			if (child.IsParentOf(entity))
				return true;
		}
		return false;
	}

	std::vector<Entity> Entity::GetTopmostEntities(const std::vector<Entity>& entities)
	{
		std::vector<Entity> result;
		result.reserve(entities.size());
		for (const Entity& entity : entities)
		{
			const bool bParentListed = std::any_of(entities.begin(), entities.end(), [&entity](const Entity& other) { return other != entity && other.IsParentOf(entity); });
			if (!bParentListed)
				result.push_back(entity);
		}

		return result;
	}

	bool Entity::IsValid() const
	{
		return m_Scene && m_Scene->m_Registry.valid(m_Entity);
	}

	const GUID& Entity::GetGUID() const
	{
		return GetComponent<IDComponent>().ID;
	}

	void Entity::SetName(const std::string& name)
	{
		GetComponent<EntitySceneNameComponent>().SetName(name);
	}

	const std::string& Entity::GetName() const
	{
		return GetComponent<EntitySceneNameComponent>().GetName();
	}

	void Entity::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
		}
	}

	EntityMovement Entity::GetMovement()
	{
		if (HasComponent<CharacterControllerComponent>())
			return EntityMovement::CharacterController;

		if (HasComponent<RigidBodyComponent>())
		{
			const auto& rigidBody = GetComponent<RigidBodyComponent>();
			if (rigidBody.GetBodyType() == PhysicsBodyType::Dynamic && !rigidBody.IsKinematic() && rigidBody.GetPhysicsActor())
				return EntityMovement::DynamicRigidBody;
		}

		return EntityMovement::Location;
	}
}
