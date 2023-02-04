#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Eagle/Core/Timestep.h"

// After changing this, projections needs to be adjusted
#ifndef EG_CASCADES_COUNT
#define EG_CASCADES_COUNT 4
#endif

namespace Eagle
{
	class Event;

	enum class CameraProjectionMode : uint8_t
	{
		Perspective = 0,
		Orthographic = 1
	};

	class Camera
	{
	public:
		Camera() { RecalculateProjection(); }
		virtual ~Camera() = default;

		const glm::mat4& GetProjection() const { return m_Projection; }
		const glm::mat4& GetCascadeProjection(uint32_t cascadeIndex) const { EG_ASSERT(cascadeIndex < EG_CASCADES_COUNT); return m_CascadeProjections[cascadeIndex]; }

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

		float GetCascadeFarPlane(uint32_t index) const { EG_ASSERT(index < EG_CASCADES_COUNT); return m_CascadeFarPlanes[index]; }

	protected:
		void RecalculateProjection();

	protected:
		glm::mat4 m_Projection;
		glm::mat4 m_CascadeProjections[EG_CASCADES_COUNT];

		CameraProjectionMode m_ProjectionMode = CameraProjectionMode::Perspective;

		float m_CascadeFarPlanes[EG_CASCADES_COUNT];

		float m_PerspectiveVerticalFOV = glm::radians(45.f);
		float m_PerspectiveNear = 0.01f;
		float m_PerspectiveFar = 1000.f;

		float m_OrthographicSize = 10.f;
		float m_OrthographicNear = -100.f;
		float m_OrthographicFar = 100.f;

		// Just some default values
		float m_AspectRatio = 0.785398185f;
		uint32_t m_ViewportWidth = 128;
		uint32_t m_ViewportHeight = 128;
	};

}