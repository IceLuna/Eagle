#include "egpch.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsActor.h"

namespace Eagle
{
	Entity Entity::Null = Entity();

	void Entity::SetParent(Entity& parent)
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

	Entity& Entity::GetParent()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().EntityParent;
	}

	void Entity::NotifyAllChildren(Notification notification)
	{
		auto& children = GetComponent<OwnershipComponent>().Children;

		for (auto& child : children)
			child.OnNotify(notification);

		ComponentsNotificationSystem::Notify(*this, Notification::OnParentTransformChanged);
	}

	void Entity::AddChildren(Entity& child)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		GetComponent<OwnershipComponent>().Children.push_back(child);
	}

	void Entity::RemoveChildren(Entity& child)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		auto& oc = GetComponent<OwnershipComponent>();
		auto it = std::find(oc.Children.begin(), oc.Children.end(), child);
		if (it != oc.Children.end())
		{
			oc.Children.erase(it);
		}
	}

	const std::vector<Entity>& Entity::GetChildren() const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Children;
	}

	const Transform& Entity::GetWorldTransform()
	{
		return GetComponent<TransformComponent>().WorldTransform;
	}

	void Entity::SetWorldTransform(const Transform& worldTransform)
	{
		auto& transformComponent = GetComponent<TransformComponent>();
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform = worldTransform;

			Rotator inverseParentWorldRotation = parentWorldTransform.Rotation.Inverse();

			myRelativeTransform.Location = myWorldTransform.Location - parentWorldTransform.Location;
			myRelativeTransform.Rotation = inverseParentWorldRotation * myWorldTransform.Rotation;
			myRelativeTransform.Scale3D = myWorldTransform.Scale3D / parentWorldTransform.Scale3D;

			glm::vec3 radius = myRelativeTransform.Location;
			glm::vec3 rotated = glm::rotate(inverseParentWorldRotation.GetQuat(), radius);
			myRelativeTransform.Location = rotated;
		}
		else
		{
			transformComponent.WorldTransform = worldTransform;
		}

		if (auto physicsActor = GetPhysicsActor())
		{
			physicsActor->SetLocation(transformComponent.WorldTransform.Location);
			physicsActor->SetRotation(transformComponent.WorldTransform.Rotation);
		}

		NotifyAllChildren(Notification::OnParentTransformChanged);
	}

	void Entity::SetWorldLocation(const glm::vec3& worldLocation)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Location = worldLocation;
		SetWorldTransform(transform);
	}

	const glm::vec3& Entity::GetWorldLocation()
	{
		return GetComponent<TransformComponent>().WorldTransform.Location;
	}

	void Entity::SetWorldRotation(const Rotator& worldRotation)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Rotation = worldRotation;
		SetWorldTransform(transform);
	}

	const Rotator& Entity::GetWorldRotation()
	{
		return GetComponent<TransformComponent>().WorldTransform.Rotation;
	}

	void Entity::SetWorldScale(const glm::vec3& worldScale)
	{
		Transform transform = GetComponent<TransformComponent>().WorldTransform;
		transform.Scale3D = worldScale;
		SetWorldTransform(transform);
	}

	const glm::vec3& Entity::GetWorldScale()
	{
		return GetComponent<TransformComponent>().WorldTransform.Scale3D;
	}

	void Entity::SetRelativeLocation(const glm::vec3& relativeLocation)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Location = relativeLocation;
		SetRelativeTransform(transform);
	}

	const glm::vec3& Entity::GetRelativeLocation()
	{
		return GetComponent<TransformComponent>().RelativeTransform.Location;
	}

	void Entity::SetRelativeRotation(const Rotator& relativeRotation)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Rotation = relativeRotation;
		SetRelativeTransform(transform);
	}

	const Rotator& Entity::GetRelativeRotation()
	{
		return GetComponent<TransformComponent>().RelativeTransform.Rotation;
	}

	void Entity::SetRelativeScale(const glm::vec3& relativeScale)
	{
		Transform transform = GetComponent<TransformComponent>().RelativeTransform;
		transform.Scale3D = relativeScale;
		SetRelativeTransform(transform);
	}

	const glm::vec3& Entity::GetRelativeScale()
	{
		return GetComponent<TransformComponent>().RelativeTransform.Scale3D;
	}

	const Transform& Entity::GetRelativeTransform()
	{
		return GetComponent<TransformComponent>().RelativeTransform;
	}

	glm::vec3 Entity::GetLinearVelocity() const
	{
		if (auto actor = GetPhysicsActor())
			return actor->GetLinearVelocity();
		return glm::vec3(0.f);
	}

	void Entity::SetRelativeTransform(const Transform& relativeTransform)
	{
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;

			myRelativeTransform = relativeTransform;

			myWorldTransform.Rotation = myRelativeTransform.Rotation * parentWorldTransform.Rotation;
			myWorldTransform.Scale3D = parentWorldTransform.Scale3D * myRelativeTransform.Scale3D;

			glm::vec3 radius = myRelativeTransform.Location;
			glm::vec3 rotated = glm::rotate(parentWorldTransform.Rotation.GetQuat(), radius);
			myWorldTransform.Location = parentWorldTransform.Location + rotated;

			auto physicsActor = GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetLocation(myWorldTransform.Location);
				physicsActor->SetRotation(myWorldTransform.Rotation);
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

	bool Entity::IsParentOf(Entity& entity) const
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");
		return entity.GetParent() == (*this);
	}

	bool Entity::IsValid() const
	{
		return m_Scene && m_Scene->m_Registry.valid(m_Entity);
	}

	const GUID& Entity::GetGUID() const
	{
		return GetComponent<IDComponent>().ID;
	}

	const std::string& Entity::GetSceneName() const
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
