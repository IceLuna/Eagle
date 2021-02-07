#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Eagle/Core/Timestep.h"

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
		virtual ~Camera() = default;

		virtual const glm::mat4& GetProjection() const { return m_Projection; }

	protected:
		glm::mat4 m_Projection = glm::mat4(1.f);
	};

	class CameraController
	{
	public:
		virtual ~CameraController() = default;

		virtual void OnUpdate(Timestep ts) = 0;
		virtual void OnEvent(Event& e) = 0;

		virtual void SetAspectRatio(float aspectRatio) = 0;

		virtual const Camera& GetCamera() const = 0;
	};
}