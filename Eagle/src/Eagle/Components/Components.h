#pragma once

#include "SceneComponent.h"

#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Camera/SceneCamera.h"

namespace Eagle
{
	class EntitySceneNameComponent
	{
	public:
		EntitySceneNameComponent() = default;
		EntitySceneNameComponent(const EntitySceneNameComponent&) = default;
		EntitySceneNameComponent(const std::string& name) : Name(name) {}
		
		std::string Name;
	};

	class TransformComponent : public SceneComponent
	{
	};

	class SpriteComponent : public SceneComponent
	{
	public:
		SpriteComponent() = default;
		SpriteComponent(const SpriteComponent&) = default;
		SpriteComponent(const glm::vec4 & color) : Color(color) {}

		glm::vec4 Color {1.f};
	};

	class CameraComponent : public SceneComponent
	{
	public:
		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;

		glm::mat4 GetViewProjection() const
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.f), Transform.Translation);

			glm::mat4 View = glm::inverse(transform);
			glm::mat4 ViewProjection = Camera.GetProjection() * View;

			return ViewProjection;
		}

	public:
		SceneCamera Camera;
		bool Primary = true; //TODO: think about moving to Scene
		bool FixedAspectRatio = false;
	};

	class NativeScriptComponent
	{
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