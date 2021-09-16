#include "egpch.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

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

	const std::vector<Entity>& Entity::GetChildren()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Children;
	}

	const Transform& Entity::GetWorldTransform()
	{
		return GetComponent<TransformComponent>().WorldTransform;
	}

	void Entity::SetWorldLocation(const glm::vec3& worldLocation)
	{
		auto& transformComponent = GetComponent<TransformComponent>();
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform.Location = worldLocation;

			myRelativeTransform.Location = myWorldTransform.Location - parentWorldTransform.Location;
		}
		else
		{
			transformComponent.WorldTransform.Location = worldLocation;
		}
		NotifyAllChildren(Notification::OnParentTransformChanged);
	}

	const glm::vec3& Entity::GetWorldLocation()
	{
		return GetComponent<TransformComponent>().WorldTransform.Location;
	}

	void Entity::SetWorldRotation(const glm::vec3& worldRotation)
	{
		auto& transformComponent = GetComponent<TransformComponent>();
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform.Rotation = worldRotation;

			myRelativeTransform.Rotation = myWorldTransform.Rotation - parentWorldTransform.Rotation; //TODO: Figure out rotation calculation
		}
		else
		{
			transformComponent.WorldTransform.Rotation = worldRotation;
		}
		NotifyAllChildren(Notification::OnParentTransformChanged);
	}

	const glm::vec3& Entity::GetWorldRotation()
	{
		return GetComponent<TransformComponent>().WorldTransform.Rotation;
	}

	void Entity::SetWorldScale(const glm::vec3& worldScale)
	{
		auto& transformComponent = GetComponent<TransformComponent>();
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform.Scale3D = worldScale;

			myRelativeTransform.Scale3D = myWorldTransform.Scale3D / parentWorldTransform.Scale3D;
		}
		else
		{
			transformComponent.WorldTransform.Scale3D = worldScale;
		}
		NotifyAllChildren(Notification::OnParentTransformChanged);
	}

	const glm::vec3& Entity::GetWorldScale()
	{
		return GetComponent<TransformComponent>().WorldTransform.Scale3D;
	}

	void Entity::SetRelativeLocation(const glm::vec3& relativeLocation)
	{
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;

			myRelativeTransform.Location = relativeLocation;

			myWorldTransform.Location = parentWorldTransform.Location + myRelativeTransform.Location;
			NotifyAllChildren(Notification::OnParentTransformChanged);
		}
	}

	const glm::vec3& Entity::GetRelativeLocation()
	{
		return GetComponent<TransformComponent>().RelativeTransform.Location;
	}

	void Entity::SetRelativeRotation(const glm::vec3& relativeRotation)
	{
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;

			myRelativeTransform.Rotation = relativeRotation;

			myWorldTransform.Rotation = parentWorldTransform.Rotation + myRelativeTransform.Rotation; //TODO: Figure out rotation calculation
			
			glm::vec3 radius = myRelativeTransform.Location;
			glm::vec3 rotated = glm::rotate(glm::quat(parentWorldTransform.Rotation), radius);
			myWorldTransform.Location = parentWorldTransform.Location + rotated;

			NotifyAllChildren(Notification::OnParentTransformChanged);
		}
	}

	const glm::vec3& Entity::GetRelativeRotation()
	{
		return GetComponent<TransformComponent>().RelativeTransform.Rotation;
	}

	void Entity::SetRelativeScale(const glm::vec3& relativeScale)
	{
		if (Entity& parent = GetParent())
		{
			const auto& parentWorldTransform = parent.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;

			myRelativeTransform.Scale3D = relativeScale;

			myWorldTransform.Scale3D = parentWorldTransform.Scale3D * myRelativeTransform.Scale3D;
			NotifyAllChildren(Notification::OnParentTransformChanged);
		}
	}

	const glm::vec3& Entity::GetRelativeScale()
	{
		return GetComponent<TransformComponent>().RelativeTransform.Scale3D;
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

			myRelativeTransform.Location = myWorldTransform.Location - parentWorldTransform.Location;
			myRelativeTransform.Rotation = myWorldTransform.Rotation - parentWorldTransform.Rotation; //TODO: Figure out rotation calculation
			myRelativeTransform.Scale3D = myWorldTransform.Scale3D / parentWorldTransform.Scale3D;
		}
		else
		{
			transformComponent.WorldTransform = worldTransform;
		}
		NotifyAllChildren(Notification::OnParentTransformChanged);
	}

	const Transform& Entity::GetRelativeTransform()
	{
		return GetComponent<TransformComponent>().RelativeTransform;
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

			//myWorldTransform.Location = parentWorldTransform.Location + myRelativeTransform.Location;
			myWorldTransform.Rotation = parentWorldTransform.Rotation + myRelativeTransform.Rotation; //TODO: Figure out rotation calculation
			myWorldTransform.Scale3D = parentWorldTransform.Scale3D * myRelativeTransform.Scale3D;

			glm::vec3 radius = myRelativeTransform.Location;
			glm::vec3 rotated = glm::rotate(glm::quat(parentWorldTransform.Rotation), radius);
			myWorldTransform.Location = parentWorldTransform.Location + rotated;

			NotifyAllChildren(Notification::OnParentTransformChanged);
		}
	}

	bool Entity::HasParent()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().EntityParent;
	}

	bool Entity::HasChildren()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Children.size();
	}

	bool Entity::IsParentOf(Entity& entity)
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

	void Entity::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
		}
	}
}
