#include "egpch.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	Entity Entity::Null = Entity();

	void Entity::SetOwner(Entity& owner)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		auto& ownershipComponent = GetComponent<OwnershipComponent>();

		//if (owner.GetID() == ownershipComponent.Owner.GetID())
		//	return;

		auto& NC = GetComponent<NotificationComponent>();

		if (ownershipComponent.Owner)
		{
			ownershipComponent.Owner.RemoveObserver(&NC);
			ownershipComponent.Owner.RemoveChildren(*this);
		}

		ownershipComponent.Owner = owner;

		auto& transformComponent = GetComponent<TransformComponent>();
		if (owner)
		{
			owner.AddChildren(*this);
			owner.AddObserver(&NC);
			SetWorldTransform(GetWorldTransform());
		}
		else
		{
			transformComponent.RelativeTransform = Transform();
		}
		auto& notifComp = GetComponent<NotificationComponent>();
		notifComp.notify(Notification::OnParentTransformChanged);
	}

	Entity& Entity::GetOwner()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Owner;
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

	void Entity::AddObserver(Observer* observer)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		if (HasComponent<NotificationComponent>())
			GetComponent<NotificationComponent>().AddObserver(observer);
	}

	void Entity::RemoveObserver(Observer* observer)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		if (HasComponent<NotificationComponent>())
			GetComponent<NotificationComponent>().RemoveObserver(observer);
	}

	const Transform& Entity::GetWorldTransform()
	{
		return GetComponent<TransformComponent>().WorldTransform;
	}

	void Entity::SetWorldTransform(const Transform& worldTransform)
	{
		if (Entity& owner = GetOwner())
		{
			const auto& ownerWorldTransform = owner.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myWorldTransform = transformComponent.WorldTransform;
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			myWorldTransform = worldTransform;

			myRelativeTransform.Translation = myWorldTransform.Translation - ownerWorldTransform.Translation;
			myRelativeTransform.Rotation = myWorldTransform.Rotation - ownerWorldTransform.Rotation; //TODO: Figure out rotation calculation
			myRelativeTransform.Scale3D = myWorldTransform.Scale3D / ownerWorldTransform.Scale3D;
		}
		else
		{
			GetComponent<TransformComponent>().WorldTransform = worldTransform;
		}
		auto& notifComp = GetComponent<NotificationComponent>();
		notifComp.notify(Notification::OnParentTransformChanged);
	}

	const Transform& Entity::GetRelativeTransform()
	{
		return GetComponent<TransformComponent>().RelativeTransform;
	}

	void Entity::SetRelativeTransform(const Transform& relativeTransform)
	{
		if (Entity& owner = GetOwner())
		{
			const auto& ownerWorldTransform = owner.GetWorldTransform();
			auto& transformComponent = GetComponent<TransformComponent>();
			auto& myRelativeTransform = transformComponent.RelativeTransform;
			auto& myWorldTransform = transformComponent.WorldTransform;

			myRelativeTransform = relativeTransform;

			myWorldTransform.Translation = ownerWorldTransform.Translation + myRelativeTransform.Translation;
			myWorldTransform.Rotation = ownerWorldTransform.Rotation + myRelativeTransform.Rotation; //TODO: Figure out rotation calculation
			myWorldTransform.Scale3D = ownerWorldTransform.Scale3D * myRelativeTransform.Scale3D;
		}

		auto& notifComp = GetComponent<NotificationComponent>();
		notifComp.notify(Notification::OnParentTransformChanged);
	}

	bool Entity::HasOwner()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Owner;
	}

	bool Entity::HasChildren()
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		return GetComponent<OwnershipComponent>().Children.size();
	}

	bool Entity::IsOwnerOf(Entity& entity)
	{
		EG_CORE_ASSERT(m_Scene, "Invalid Entity");

		auto& children = GetComponent<OwnershipComponent>().Children;
		auto it = std::find(children.begin(), children.end(), entity);

		return (it != children.end());
	}

	bool Entity::IsValid() const
	{
		return (*this != Entity::Null) && (!m_Scene->WasEntityDestroyed(*this));
	}

	void Entity::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
			auto& notifComp = GetComponent<NotificationComponent>();
			notifComp.notify(Notification::OnParentTransformChanged);
		}
	}
}
