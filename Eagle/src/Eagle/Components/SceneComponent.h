#pragma once

#include "Component.h"
#include "Eagle/Core/Transform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
	class SceneComponent : public Component
	{
	public:
		SceneComponent(const std::string& name = std::string("Unnamed Component")) : Component(name) {}
		
		SceneComponent(const SceneComponent&) = delete;
		SceneComponent(SceneComponent&&) noexcept;
		SceneComponent& operator=(const SceneComponent&) = default;
		SceneComponent& operator=(SceneComponent&&) noexcept;

		virtual void OnInit(Entity& entity) override;
		
		const Transform& GetWorldTransform() const { return WorldTransform; }
		const Transform& GetRelativeTransform() const { return RelativeTransform; }

		virtual void SetWorldTransform(const Transform& worldTransform);
		virtual void SetRelativeTransform(const Transform& relativeTransform);

		glm::vec3 GetForwardVector() const
		{
			return glm::rotate(GetOrientation().GetQuat(), glm::vec3(0.f, 0.f, -1.f));
		}

		glm::vec3 GetUpVector() const
		{
			return glm::rotate(GetOrientation().GetQuat(), glm::vec3(0.f, 1.f, 0.f));
		}

		glm::vec3 GetRightVector() const
		{
			return glm::rotate(GetOrientation().GetQuat(), glm::vec3(1.f, 0.f, 0.f));
		}

		const Rotator& GetOrientation() const
		{
			return WorldTransform.Rotation;
		}

	protected:
		virtual void OnNotify(Notification notification) override;

	protected:
		Transform WorldTransform;
		Transform RelativeTransform;

		//TODO: 
		//glm::vec3 m_Velocity;
		//bool m_Visible;
		//bool m_HiddenInGame;
	};
}
