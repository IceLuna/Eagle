#pragma once

#include "SceneComponent.h"

#include "Eagle/Core/ScriptableEntity.h"
#include "Eagle/Camera/SceneCamera.h"

namespace Eagle
{
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
		CameraComponent(float aspectRatio, CameraProjectionMode cameraType) : Camera(aspectRatio, cameraType) {}
		CameraComponent(const CameraComponent&) = default;
		const Camera& GetCamera() const { return Camera.GetCamera(); }

	protected:
		SceneCamera Camera;
		bool m_Primary = true; //TODO: think about moving to Scene
		bool m_FixedAspectRatio = false;
		float m_FOV; //Set; 90
		float m_OrthoWidth; //Set; 512
		float m_OrthoNearClipPlane;  //Set; 0
		float m_OrthoFarClipPlane;  //Set; 2097152
		float m_AspectRatio;  //Set; 1.33333333f
		CameraProjectionMode m_ProjectionMode; //Set; Perspective
	};

	class NativeScriptComponent : public SceneComponent
	{
	public:
		template<typename T>
		void Bind()
		{
			InitScript = []() { return MakeRef<ScriptableEntity>(static_cast<ScriptableEntity*>(new T())); }
			DestroyScript = [](NativeScriptComponent* nsc) { nsc->Instance.reset(nullptr); }
		}

	protected:
		Ref<ScriptableEntity> Instance;

		Ref<ScriptableEntity> (*InitScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		friend class Scene;
	};
}