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
		Component::operator=(std::move(other));
		WorldTransform = std::move(other.WorldTransform);
		RelativeTransform = std::move(other.RelativeTransform);

		return *this;
	}

	void SceneComponent::OnInit(Entity entity)
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

		Rotator inverseParentWorldRotation = parentWorldTransform.Rotation.Inverse();

		RelativeTransform.Location = WorldTransform.Location - parentWorldTransform.Location;
		RelativeTransform.Rotation = inverseParentWorldRotation * WorldTransform.Rotation;
		RelativeTransform.Scale3D = WorldTransform.Scale3D / parentWorldTransform.Scale3D;

		const glm::vec3& radius = RelativeTransform.Location;
		glm::vec3 rotated = glm::rotate(inverseParentWorldRotation.GetQuat(), radius);
		RelativeTransform.Location = rotated;
	}

	void SceneComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		const auto& parentWorldTransform = Parent.GetWorldTransform();
		RelativeTransform = relativeTransform;

		WorldTransform.Rotation = RelativeTransform.Rotation * parentWorldTransform.Rotation;
		WorldTransform.Scale3D = parentWorldTransform.Scale3D * RelativeTransform.Scale3D;

		const glm::vec3& radius = RelativeTransform.Location;
		glm::vec3 rotated = glm::rotate(parentWorldTransform.Rotation.GetQuat(), radius);
		WorldTransform.Location = parentWorldTransform.Location + rotated;
	}

	void SceneComponent::OnNotify(Notification notification)
	{
		if (notification == Notification::OnParentTransformChanged)
		{
			SetRelativeTransform(GetRelativeTransform());
		}
	}
}