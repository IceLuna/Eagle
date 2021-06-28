#pragma once

#include "SceneComponent.h"

#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Camera/SceneCamera.h"
#include "Eagle/Math/Math.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Renderer/SubTexture2D.h"
#include "Eagle/Classes/StaticMesh.h"

namespace Eagle
{
	class NotificationComponent : public Component
	{
	public:
		NotificationComponent() = default;
		COMPONENT_DEFAULTS(NotificationComponent);
	protected:
		void OnNotify(Notification notification) override { Owner.OnNotify(notification); }

		friend class Entity;
	};

	class OwnershipComponent : public Component
	{
	public:
		OwnershipComponent() = default;
		COMPONENT_DEFAULTS(OwnershipComponent);

		Entity EntityOwner = Entity::Null;
		std::vector<Entity> Children;
	};

	class EntitySceneNameComponent : public Component
	{
	public:
		EntitySceneNameComponent() = default;
		COMPONENT_DEFAULTS(EntitySceneNameComponent);
		EntitySceneNameComponent(const std::string& name) : Component(name) {}
	};

	class TransformComponent : public Component
	{
	public:
		TransformComponent() = default;
		COMPONENT_DEFAULTS(TransformComponent);
		Transform WorldTransform;
		Transform RelativeTransform;
	};

	class LightComponent : public SceneComponent
	{
	public:
		LightComponent() = default;
		LightComponent(const glm::vec4 lightColor) : LightColor(lightColor) {}
		COMPONENT_DEFAULTS(LightComponent);
		
	public:
		glm::vec4 LightColor = glm::vec4(1.f);
		glm::vec3 Ambient = glm::vec3(0.2f);
		glm::vec3 Specular = glm::vec3(0.5f);
	};

	class PointLightComponent : public LightComponent
	{
	public:
		PointLightComponent() = default;
		COMPONENT_DEFAULTS(PointLightComponent);
		float Distance = 100.f;
	};

	class DirectionalLightComponent : public LightComponent
	{};

	class SpotLightComponent : public LightComponent
	{
	public:
		SpotLightComponent() = default;
		COMPONENT_DEFAULTS(SpotLightComponent);
		float InnerCutOffAngle = 25.f;
		float OuterCutOffAngle = 45.f;
	};

	class SpriteComponent : public SceneComponent
	{
	public:
		SpriteComponent() : Material(Material::Create()) { Material->Shader = ShaderLibrary::GetOrLoad("assets/shaders/UniqueShader.glsl"); }
		COMPONENT_DEFAULTS(SpriteComponent);

		Ref<Eagle::Material> Material;
		Ref<SubTexture2D> SubTexture;
		glm::vec2 SubTextureCoords = {0, 0};
		glm::vec2 SpriteSize = {64, 64};
		glm::vec2 SpriteSizeCoef = {1, 1};
		bool bSubTexture = false;
	};

	class StaticMeshComponent : public SceneComponent
	{
	public:
		StaticMeshComponent() = default;
		COMPONENT_DEFAULTS(StaticMeshComponent);

	public:
		Ref<Eagle::StaticMesh> StaticMesh = MakeRef<Eagle::StaticMesh>();
	};

	class CameraComponent : public SceneComponent
	{
	public:
		CameraComponent() = default;
		COMPONENT_DEFAULTS(CameraComponent);

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

	public:
		SceneCamera Camera;
		bool Primary = false; //TODO: think about moving to Scene
		bool FixedAspectRatio = false;
	};

	class NativeScriptComponent : public Component
	{
	//TODO: Add array of scripts, OnUpdateFunction to update All Scripts and etc.
	public:
		NativeScriptComponent() = default;
		COMPONENT_DEFAULTS(NativeScriptComponent);

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