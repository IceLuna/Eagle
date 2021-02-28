#pragma once

#include "SceneComponent.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Camera/SceneCamera.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Renderer/Material.h"

namespace Eagle
{
	class NotificationComponent : public Component
	{
	protected:
		void OnNotify(Notification notification) override { Owner.OnNotify(notification); }

		friend class Entity;
	};

	class OwnershipComponent : public Component
	{
	public:
		Entity Owner = Entity::Null;
		std::vector<Entity> Children;
	};

	class EntitySceneNameComponent : public Component
	{
	public:
		EntitySceneNameComponent() = default;
		EntitySceneNameComponent(const EntitySceneNameComponent&) = default;
		EntitySceneNameComponent(const std::string& name) : Component(name) {}
	};

	class TransformComponent : public Component
	{
	public:
		Transform WorldTransform;
		Transform RelativeTransform;
	};

	class LightComponent : public SceneComponent
	{
	public:
		LightComponent() = default;
		LightComponent(const glm::vec4 lightColor) : LightColor(lightColor) {}
		LightComponent(const LightComponent&) = default;
		
	public:
		glm::vec4 LightColor = glm::vec4(1.f);
		glm::vec3 Ambient = glm::vec3(0.2f);
		glm::vec3 Specular = glm::vec3(0.5f);
		float Distance = 100.f;
	};

	class SpriteComponent : public SceneComponent
	{
	public:
		SpriteComponent() = default;
		SpriteComponent(const SpriteComponent& sprite) = default;
		SpriteComponent(const Eagle::Material& material) : Material(material) {}

		Eagle::Material Material;
	};

	class CameraComponent : public SceneComponent
	{
	public:
		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;

		glm::mat4 GetViewProjection() const
		{
			glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), WorldTransform.Translation);
			transformMatrix *= Math::GetRotationMatrix(WorldTransform.Rotation);

			glm::mat4 ViewMatrix = glm::inverse(transformMatrix);
			glm::mat4 ViewProjection = Camera.GetProjection() * ViewMatrix;

			return ViewProjection;
		}
		
		glm::mat4 GetViewMatrix() const
		{
			glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), WorldTransform.Translation);
			transformMatrix *= glm::toMat4(glm::quat(WorldTransform.Rotation));

			glm::mat4 ViewMatrix = glm::inverse(transformMatrix);
			return ViewMatrix;
		}

		glm::vec3 GetForwardDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(0.f, 0.f, -1.f));
		}

		glm::vec3 GetUpDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(0.f, 1.f, 0.f));
		}

		glm::vec3 GetRightDirection() const
		{
			return glm::rotate(GetOrientation(), glm::vec3(1.f, 0.f, 0.f));
		}

		glm::quat GetOrientation() const
		{
			return glm::quat(glm::vec3(WorldTransform.Rotation.x, WorldTransform.Rotation.y, 0.f));
		}

	public:
		SceneCamera Camera;
		bool Primary = false; //TODO: think about moving to Scene
		bool FixedAspectRatio = false;
	};

	class NativeScriptComponent : public Component
	{
	//TODO: Add array of scripts, OnUpdateFunction to update All Scripts and etc.
	public:

		template<typename T>
		void Bind()
		{
			EG_CORE_ASSERT(InitScript == nullptr, "Initialized Script twice!");
			InitScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}

	protected:
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InitScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		friend class Scene;
		friend void DestroyScript(NativeScriptComponent*);
	};
}