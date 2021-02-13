#pragma once

#include "Eagle/Camera/Camera.h"
#include "Eagle/Core/Transform.h"

namespace Eagle
{
	class MouseScrolledEvent;

	class EditorCamera : public Camera
	{
	public:
		EditorCamera();

		virtual void OnUpdate(Timestep ts);
		virtual void OnEvent(Event & e);

		void SetOrthographic(float size, float nearClip, float farClip);
		void SetPerspective(float verticalFOV, float nearClip, float farClip);

		void SetViewportSize(uint32_t width, uint32_t height);

		float GetAspectRatio() const { return m_AspectRatio; }
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		float GetPerspectiveVerticalFOV() const { return m_PerspectiveVerticalFOV; }
		void SetPerspectiveVerticalFOV(float verticalFov) { m_PerspectiveVerticalFOV = verticalFov; RecalculateProjection(); }
		float GetPerspectiveNearClip() const { return m_PerspectiveNear; }
		void SetPerspectiveNearClip(float nearClip) { m_PerspectiveNear = nearClip; RecalculateProjection(); }
		float GetPerspectiveFarClip() const { return m_PerspectiveFar; }
		void SetPerspectiveFarClip(float farClip) { m_PerspectiveFar = farClip; RecalculateProjection(); }

		float GetOrthographicSize() const { return m_OrthographicSize; }
		void SetOrthographicSize(float size) { m_OrthographicSize = size; RecalculateProjection(); }
		float GetOrthographicNearClip() const { return m_OrthographicNear; }
		void SetOrthographicNearClip(float nearClip) { m_OrthographicNear = nearClip; RecalculateProjection(); }
		float GetOrthographicFarClip() const { return m_OrthographicFar; }
		void SetOrthographicFarClip(float farClip) { m_OrthographicFar = farClip; RecalculateProjection(); }

		const CameraProjectionMode GetProjectionMode() const { return m_ProjectionMode; }
		void SetProjectionMode(CameraProjectionMode type) { m_ProjectionMode = type; RecalculateProjection(); }
		
		const Transform& GetTransform() const { return m_Transform; }
		void SetTransform(const Transform& transform) { m_Transform = transform; RecalculateView(); }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

		glm::vec3 GetForwardDirection() const;
		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;

		glm::quat GetOrientation() const;
		const glm::vec3& GetTranslation() const { return m_Transform.Translation; };
		const glm::vec3& GetRotation() const { return m_Transform.Rotation; };

		float GetMoveSpeed() const { return m_MoveSpeed; }
		void  SetMoveSpeed(float speed) { m_MoveSpeed = speed; }

		float GetRotationSpeed() const { return m_MouseRotationSpeed; }
		void  SetRotationSpeed(float speed) { m_MouseRotationSpeed = speed; }

	private:
		void RecalculateProjection();
		void RecalculateView();

	protected:
		bool OnMouseScrolled(MouseScrolledEvent& e);

	protected:
		Transform m_Transform;
		glm::mat4 m_ViewMatrix = glm::mat4(1.f);

		CameraProjectionMode m_ProjectionMode = CameraProjectionMode::Perspective;

		float m_PerspectiveVerticalFOV = glm::radians(45.f);
		float m_PerspectiveNear = 0.01f;
		float m_PerspectiveFar = 10000.f;

		float m_OrthographicSize = 10.f;
		float m_OrthographicNear = -100.f;
		float m_OrthographicFar = 100.f;

		float m_AspectRatio = 0.f;

		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		//Input speeds
		float m_MouseX = 0.f;
		float m_MouseY = 0.f;

		float m_MoveSpeed = 1.75f; //0.225f;
		float m_MouseRotationSpeed = 0.032f; //0.225f;
	};
}
