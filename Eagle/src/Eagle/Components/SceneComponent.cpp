#include "egpch.h"

#include "SceneComponent.h"
#include "Eagle/Core/Entity.h"
#include "Components.h"

namespace Eagle
{
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

	SceneComponent::SceneComponent(const SceneComponent& sc) 
	: Component(sc)
	, WorldTransform(sc.WorldTransform)
	, RelativeTransform(sc.RelativeTransform)
	{
		OnInit(Owner);
	}

	SceneComponent::~SceneComponent()
	{
		if (Owner)
			Owner.RemoveObserver(this);
	}

	void SceneComponent::OnInit(Entity& entity)
	{
		Component::OnInit(entity);

		entity.AddObserver(this);
		const auto& world = entity.GetWorldTransform();
		WorldTransform = world;
	}

	void SceneComponent::SetWorldTransform(const Transform& worldTransform)
	{
		const auto& ownerWorldTransform = Owner.GetWorldTransform();
		WorldTransform = worldTransform;

		RelativeTransform.Translation = WorldTransform.Translation - ownerWorldTransform.Translation;
		RelativeTransform.Rotation = WorldTransform.Rotation - ownerWorldTransform.Rotation; //TODO: Figure out rotation calculation
		RelativeTransform.Scale3D = WorldTransform.Scale3D / ownerWorldTransform.Scale3D;

		notify(Notification::OnParentTransformChanged);
	}

	void SceneComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		const auto& ownerWorldTransform = Owner.GetWorldTransform();
		RelativeTransform = relativeTransform;

		WorldTransform.Translation = ownerWorldTransform.Translation + RelativeTransform.Translation;
		WorldTransform.Rotation = ownerWorldTransform.Rotation + RelativeTransform.Rotation; //TODO: Figure out rotation calculation
		WorldTransform.Scale3D = ownerWorldTransform.Scale3D * RelativeTransform.Scale3D;

		notify(Notification::OnParentTransformChanged);
	}

	void SceneComponent::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
		}
	}
}