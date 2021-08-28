#include "egpch.h"

#include "SceneComponent.h"
#include "Eagle/Core/Entity.h"
#include "Components.h"

namespace Eagle
{
	SceneComponent::SceneComponent(SceneComponent&& other) noexcept : Component(std::move(other))
	{
		WorldTransform = std::move(other.WorldTransform);
		RelativeTransform = std::move(other.RelativeTransform);
	}

	SceneComponent& SceneComponent::operator=(SceneComponent&& other) noexcept
	{
		EG_CORE_ASSERT(other.Parent, "No component parent.");

		Component::operator=(std::move(other));
		WorldTransform = std::move(other.WorldTransform);
		RelativeTransform = std::move(other.RelativeTransform);

		return *this;
	}

	void SceneComponent::OnInit(Entity& entity)
	{
		Component::OnInit(entity);

		if (Parent)
		{
			const auto& world = Parent.GetWorldTransform();
			WorldTransform = world;
		}
	}

	void SceneComponent::SetWorldTransform(const Transform& worldTransform)
	{
		const auto& parentWorldTransform = Parent.GetWorldTransform();
		WorldTransform = worldTransform;

		RelativeTransform.Location = WorldTransform.Location - parentWorldTransform.Location;
		RelativeTransform.Rotation = WorldTransform.Rotation - parentWorldTransform.Rotation; //TODO: Figure out rotation calculation
		RelativeTransform.Scale3D = WorldTransform.Scale3D / parentWorldTransform.Scale3D;

		//notify(Notification::OnParentTransformChanged);
	}

	void SceneComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		const auto& parentWorldTransform = Parent.GetWorldTransform();
		RelativeTransform = relativeTransform;

		WorldTransform.Location = parentWorldTransform.Location + RelativeTransform.Location;
		WorldTransform.Rotation = parentWorldTransform.Rotation + RelativeTransform.Rotation; //TODO: Figure out rotation calculation
		WorldTransform.Scale3D = parentWorldTransform.Scale3D * RelativeTransform.Scale3D;

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