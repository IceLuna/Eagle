#include "egpch.h"

#include "SceneComponent.h"
#include "Eagle/Core/Entity.h"
#include "Components.h"

namespace Eagle
{
	Component::Component(Component&& other) noexcept 
	: Object(other)
	, Name(std::move(other.Name))
	, Owner(std::move(other.Owner))
	, m_Tags(std::move(other.m_Tags))
	{
		ComponentsNotificationSystem::RemoveObserver(Owner, &other);
		ComponentsNotificationSystem::AddObserver(Owner, this);
	}

	Component& Component::operator=(Component&& other) noexcept
	{
		Object::operator=(std::move(other));

		Name = std::move(other.Name);
		Owner = std::move(other.Owner);
		m_Tags = std::move(other.m_Tags);

		ComponentsNotificationSystem::RemoveObserver(Owner, &other);
		ComponentsNotificationSystem::AddObserver(Owner, this);

		return *this;
	}

	Component::~Component()
	{
		if (Owner)
			ComponentsNotificationSystem::RemoveObserver(Owner, this);
	}

	void Component::OnInit(Entity& entity)
	{
		Owner = entity;
		ComponentsNotificationSystem::AddObserver(Owner, this);
	}

	void Component::AddTag(const std::string& tag)
	{
		m_Tags.insert(tag);
	}

	void Component::RemoveTag(const std::string& tag)
	{
		auto& it = m_Tags.find(tag);
		if (it != m_Tags.end())
		{
			m_Tags.erase(it);
		}
	}
	
	SceneComponent::SceneComponent(SceneComponent&& other) noexcept : Component(std::move(other))
	{
		WorldTransform = std::move(other.WorldTransform);
		RelativeTransform = std::move(other.RelativeTransform);
	}

	SceneComponent& SceneComponent::operator=(SceneComponent&& other) noexcept
	{
		EG_CORE_ASSERT(other.Owner, "No component owner.");

		Component::operator=(std::move(other));
		WorldTransform = std::move(other.WorldTransform);
		RelativeTransform = std::move(other.RelativeTransform);

		return *this;
	}

	void SceneComponent::OnInit(Entity& entity)
	{
		Component::OnInit(entity);

		if (Owner)
		{
			const auto& world = Owner.GetWorldTransform();
			WorldTransform = world;
		}
	}

	void SceneComponent::SetWorldTransform(const Transform& worldTransform)
	{
		const auto& ownerWorldTransform = Owner.GetWorldTransform();
		WorldTransform = worldTransform;

		RelativeTransform.Translation = WorldTransform.Translation - ownerWorldTransform.Translation;
		RelativeTransform.Rotation = WorldTransform.Rotation - ownerWorldTransform.Rotation; //TODO: Figure out rotation calculation
		RelativeTransform.Scale3D = WorldTransform.Scale3D / ownerWorldTransform.Scale3D;

		//notify(Notification::OnParentTransformChanged);
	}

	void SceneComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		const auto& ownerWorldTransform = Owner.GetWorldTransform();
		RelativeTransform = relativeTransform;

		WorldTransform.Translation = ownerWorldTransform.Translation + RelativeTransform.Translation;
		WorldTransform.Rotation = ownerWorldTransform.Rotation + RelativeTransform.Rotation; //TODO: Figure out rotation calculation
		WorldTransform.Scale3D = ownerWorldTransform.Scale3D * RelativeTransform.Scale3D;

		//notify(Notification::OnParentTransformChanged);
	}

	void SceneComponent::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
		}
	}
}