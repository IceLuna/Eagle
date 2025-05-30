#pragma once

#include "Component.h"
#include "Eagle/Math/Transform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
	class SceneComponent : public Component
	{
	public:
		SceneComponent(const Entity& entity) : Component(entity)
		{
			EG_CORE_ASSERT(Parent);
			const auto& world = Parent.GetWorldTransform();
			WorldTransform = world;
		}
		
		SceneComponent(const SceneComponent&) = delete;
		SceneComponent(SceneComponent&&) noexcept;
		SceneComponent& operator=(const SceneComponent&) = default;
		SceneComponent& operator=(SceneComponent&&) noexcept;

		const Transform& GetWorldTransform() const { return WorldTransform; }
		const Transform& GetRelativeTransform() const { return RelativeTransform; }

		virtual void SetWorldTransform(const Transform& worldTransform);
		virtual void SetRelativeTransform(const Transform& relativeTransform);

		// TODO: Maybe cache these values instead of calculating them each time?
		glm::vec3 GetForwardVector() const
		{
			return Math::GetForwardVector(WorldTransform.Rotation);
		}

		glm::vec3 GetUpVector() const
		{
			return Math::GetUpVector(WorldTransform.Rotation);
		}

		glm::vec3 GetRightVector() const
		{
			return Math::GetRightVector(WorldTransform.Rotation);
		}

	protected:
		virtual void OnNotify(Notification notification) override;

	protected:
		Transform WorldTransform;
		Transform RelativeTransform;
	};
}
