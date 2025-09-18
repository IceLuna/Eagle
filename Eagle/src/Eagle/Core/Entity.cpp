#include "egpch.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsActor.h"

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
			transformComponent.RelativeTransform = Transform();
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

	void Entity::NotifyAllChildren(Notification notification)
	{
		auto& children = GetComponent<OwnershipComponent>().Children;

		// Notify Entity-childs
		for (auto& child : children)
			child.OnNotify(notification);

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

	void Entity::TriggerAnimationEvent(const std::string& name, float time)
	{
		if (HasComponent<ScriptComponent>() == false)
			return;

		if (ScriptEngine::ModuleExists(GetComponent<ScriptComponent>().ModuleName))
			ScriptEngine::OnAnimationEventEntity(*this, name, time);
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

	void Entity::SetWorldTransform(const Transform& worldTransform, bool bTeleportPhysics)
	{
		auto& transformComponent = GetComponent<TransformComponent>();
		if (Entity parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform = worldTransform;

			Rotator inverseParentWorldRotation = parentWorldTransform.Rotation.Conjugate();

			myRelativeTransform.Location = myWorldTransform.Location - parentWorldTransform.Location;
			myRelativeTransform.Rotation = inverseParentWorldRotation * myWorldTransform.Rotation;
			myRelativeTransform.Scale3D = myWorldTransform.Scale3D / parentWorldTransform.Scale3D;

			myRelativeTransform.Location /= parentWorldTransform.Scale3D; // Undo parent's scaling
			myRelativeTransform.Location = inverseParentWorldRotation.GetQuat() * myRelativeTransform.Location;
		}
		else
		{
			transformComponent.WorldTransform = worldTransform;
		}

		if (bTeleportPhysics)
		{
			if (auto physicsActor = GetPhysicsActor())
			{
				physicsActor->SetTransform(transformComponent.WorldTransform);
			}
		}

		NotifyAllChildren(Notification::OnParentTransformChanged);

#if EG_DEBUG
		EG_CORE_ASSERT(!glm::any(glm::isnan(transformComponent.WorldTransform.Location)));
		EG_CORE_ASSERT(!glm::any(glm::isnan(transformComponent.WorldTransform.Rotation.GetQuat())));
		EG_CORE_ASSERT(!glm::any(glm::isnan(transformComponent.WorldTransform.Scale3D)));
#endif
	}

	void Entity::SetWorldLocation(const glm::vec3& worldLocation, bool bTeleportPhysics)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Location = worldLocation;
		SetWorldTransform(transform, bTeleportPhysics);
	}

	const glm::vec3& Entity::GetWorldLocation() const
	{
		return GetComponent<TransformComponent>().WorldTransform.Location;
	}

	void Entity::SetWorldRotation(const Rotator& worldRotation, bool bTeleportPhysics)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Rotation = worldRotation;
		SetWorldTransform(transform, bTeleportPhysics);
	}

	const Rotator& Entity::GetWorldRotation() const
	{
		return GetComponent<TransformComponent>().WorldTransform.Rotation;
	}

	void Entity::SetWorldScale(const glm::vec3& worldScale, bool bTeleportPhysics)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Scale3D = worldScale;
		SetWorldTransform(transform, bTeleportPhysics);
	}

	const glm::vec3& Entity::GetWorldScale() const
	{
		return GetComponent<TransformComponent>().WorldTransform.Scale3D;
	}

	void Entity::SetRelativeLocation(const glm::vec3& relativeLocation, bool bTeleportPhysics)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Location = relativeLocation;
		SetRelativeTransform(transform, bTeleportPhysics);
	}

	const glm::vec3& Entity::GetRelativeLocation() const
	{
		return GetComponent<TransformComponent>().RelativeTransform.Location;
	}

	void Entity::SetRelativeRotation(const Rotator& relativeRotation, bool bTeleportPhysics)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Rotation = relativeRotation;
		SetRelativeTransform(transform, bTeleportPhysics);
	}

	const Rotator& Entity::GetRelativeRotation() const
	{
		return GetComponent<TransformComponent>().RelativeTransform.Rotation;
	}

	void Entity::SetRelativeScale(const glm::vec3& relativeScale, bool bTeleportPhysics)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Scale3D = relativeScale;
		SetRelativeTransform(transform, bTeleportPhysics);
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

	void Entity::SetRelativeTransform(const Transform& relativeTransform, bool bTeleportPhysics)
	{
		if (Entity parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;

			myRelativeTransform = relativeTransform;

			myWorldTransform.Rotation = parentWorldTransform.Rotation * myRelativeTransform.Rotation;
			myWorldTransform.Scale3D = parentWorldTransform.Scale3D * myRelativeTransform.Scale3D;

			glm::vec3 rotated = parentWorldTransform.Rotation.GetQuat() * (myRelativeTransform.Location * parentWorldTransform.Scale3D);
			myWorldTransform.Location = parentWorldTransform.Location + rotated;

			if (bTeleportPhysics)
			{
				if (auto physicsActor = GetPhysicsActor())
				{
					physicsActor->SetTransform(myWorldTransform);
				}
			}

			NotifyAllChildren(Notification::OnParentTransformChanged);
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

	bool Entity::IsValid() const
	{
		return m_Scene && m_Scene->m_Registry.valid(m_Entity);
	}

	const GUID& Entity::GetGUID() const
	{
		return GetComponent<IDComponent>().ID;
	}

	const std::string& Entity::GetName() const
	{
		return GetComponent<EntitySceneNameComponent>().Name;
	}

	void Entity::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
		}
	}
}
