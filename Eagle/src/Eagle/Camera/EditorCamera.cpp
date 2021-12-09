#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "EditorCamera.h"
#include "Eagle/Input/Input.h"
#include "Eagle/Events/MouseEvent.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
	EditorCamera::EditorCamera()
	{
		m_Transform.Location.z = 15.f;

		RecalculateProjection();
		RecalculateView();
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			float offsetX = m_MouseX - Input::GetMouseX();
			float offsetY = Input::GetMouseY() - m_MouseY;

			if (Input::IsCursorVisible())
			{
				Input::SetShowCursor(false);

				offsetX = offsetY = 0.f;
			}

			glm::vec3& Location = m_Transform.Location;
			glm::vec3& Rotation = m_EulerRotation;

			Rotation.x -= glm::radians(offsetY * m_MouseRotationSpeed);
			Rotation.y += glm::radians(offsetX * m_MouseRotationSpeed);
			Rotation.z = 0.f;

			constexpr float maxRadians = glm::radians(89.9f);
			constexpr float minRadians = glm::radians(-89.9f);

			if (Rotation.x >= maxRadians)
				Rotation.x = maxRadians;
			else if (Rotation.x <= minRadians)
				Rotation.x = minRadians;

			m_Transform.Rotation = Rotator::FromEulerAngles(Rotation);

			glm::vec3 forward = GetForwardVector();
			glm::vec3 right = GetRightVector();

			if (Input::IsKeyPressed(Key::W))
			{
				Location += (forward * (m_MoveSpeed * ts));
			}
			if (Input::IsKeyPressed(Key::S))
			{
				Location -= (forward * (m_MoveSpeed * ts));
			}
			if (Input::IsKeyPressed(Key::Q))
			{
				Location.y -= m_MoveSpeed * ts;
			}
			if (Input::IsKeyPressed(Key::E))
			{
				Location.y += m_MoveSpeed * ts;
			}
			if (Input::IsKeyPressed(Key::A))
			{
				Location -= (right * (m_MoveSpeed * ts));
			}
			if (Input::IsKeyPressed(Key::D))
			{
				Location += (right * (m_MoveSpeed * ts));
			}

			m_MouseX = Input::GetMouseX();
			m_MouseY = Input::GetMouseY();

			RecalculateView();
		}
		else
		{
			if (Input::IsCursorVisible() == false)
			{
				Input::SetShowCursor(true);
			}
		}
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(EG_BIND_FN(EditorCamera::OnMouseScrolled));
	}

	void EditorCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		m_ProjectionMode = CameraProjectionMode::Orthographic;
		m_OrthographicSize = size;
		m_OrthographicNear = nearClip;
		m_OrthographicFar = farClip;

		RecalculateProjection();
	}

	void EditorCamera::SetPerspective(float verticalFOV, float nearClip, float farClip)
	{
		m_ProjectionMode = CameraProjectionMode::Perspective;
		m_PerspectiveVerticalFOV = verticalFOV;
		m_PerspectiveNear = nearClip;
		m_PerspectiveFar = farClip;

		RecalculateProjection();
	}

	void EditorCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_AspectRatio = (float)width / (float)height;

		RecalculateProjection();
	}

	glm::vec3 EditorCamera::GetForwardVector() const
	{
		return glm::rotate(GetOrientation().GetQuat(), glm::vec3(0.f, 0.f, -1.f));
	}

	glm::vec3 EditorCamera::GetUpVector() const
	{
		return glm::rotate(GetOrientation().GetQuat(), glm::vec3(0.f, 1.f, 0.f));
	}

	glm::vec3 EditorCamera::GetRightVector() const
	{
		return glm::rotate(GetOrientation().GetQuat(), glm::vec3(1.f, 0.f, 0.f));
	}

	Rotator EditorCamera::GetOrientation() const
	{
		return Rotator::FromEulerAngles({m_EulerRotation.x, m_EulerRotation.y, 0.f});
	}

	void EditorCamera::RecalculateProjection()
	{
		if (m_ProjectionMode == CameraProjectionMode::Perspective)
		{
			m_Projection = glm::perspective(m_PerspectiveVerticalFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
		}
		else
		{
			float orthoLeft = -m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoRight = m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoBottom = -m_OrthographicSize * 0.5f;
			float orthoTop = m_OrthographicSize * 0.5f;

			m_Projection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);
		}
	}

	void EditorCamera::RecalculateView()
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), m_Transform.Location);
		transformMatrix *= m_Transform.Rotation.ToMat4();

		m_ViewMatrix = glm::inverse(transformMatrix);
	}

	bool EditorCamera::OnMouseScrolled(MouseScrolledEvent& e)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			m_MoveSpeed += e.GetYOffset() * 0.5f;
			m_MoveSpeed = std::max(0.f, m_MoveSpeed);
		}
		
		return false;
	}
}
