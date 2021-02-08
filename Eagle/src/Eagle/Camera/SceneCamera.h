#pragma once

#include "Camera.h"

namespace Eagle
{
	class SceneCamera : public Camera
	{
	public:
		SceneCamera();
		virtual ~SceneCamera() = default;

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

		const CameraProjectionMode GetProjectionMode() { return m_ProjectionMode; }
		void SetProjectionMode(CameraProjectionMode type) { m_ProjectionMode = type; RecalculateProjection(); }

	private:
		void RecalculateProjection();

	private:
		CameraProjectionMode m_ProjectionMode = CameraProjectionMode::Perspective;
		
		float m_PerspectiveVerticalFOV = glm::radians(45.f);
		float m_PerspectiveNear = 0.01f;
		float m_PerspectiveFar  = 10000.f;

		float m_OrthographicSize = 10.f;
		float m_OrthographicNear = -100.f;
		float m_OrthographicFar  = 100.f;
		
		float m_AspectRatio = 0.f;

		uint32_t m_ViewportWidth  = 0;
		uint32_t m_ViewportHeight = 0;
	};
}