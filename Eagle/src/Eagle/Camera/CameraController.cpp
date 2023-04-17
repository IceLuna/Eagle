#include "egpch.h"

#include "CameraController.h"
#include "Eagle/Events/MouseEvent.h"
#include "Eagle/Input/Input.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	void CameraController::OnUpdate(Timestep ts)
	{
		if (m_Entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = m_Entity.GetComponent<CameraComponent>();
			if (cameraComponent.Primary)
			{
				if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				{
					Transform transform = cameraComponent.GetWorldTransform();

					float offsetX = m_MouseX - Input::GetMouseX();
					float offsetY = m_MouseY - Input::GetMouseY();

					// There's a GLFW bug when mouse pos jumps on second frame, so here we're ignoring mouse delta on first two frames
					if (m_NumberOfFramesMoving++ < 2)
						offsetX = offsetY = 0.f;

					if (Input::IsCursorVisible())
						Input::SetShowCursor(false);

					glm::vec3 forward = cameraComponent.GetForwardVector();

					// Limit cameras vertical rotation
					// Shouldn't prevent camera from moving if camera wants to move away from max rotation
					const float cosTheta = glm::dot(forward, glm::vec3(0.f, 1.f, 0.f));
					const bool bMoveInTheSameDir = glm::sign(cosTheta) == glm::sign(offsetY);
					const bool bStopXRotation = (glm::abs(cosTheta) > 0.999f) && bMoveInTheSameDir ? true : false;

					float rotationX = bStopXRotation ? 0.f : glm::radians(offsetY * m_MouseRotationSpeed);
					float rotationY = glm::radians(offsetX * m_MouseRotationSpeed);

					glm::quat rotX = glm::angleAxis(rotationX, glm::vec3(1, 0, 0));
					glm::quat rotY = glm::angleAxis(rotationY, glm::vec3(0, 1, 0));

					glm::quat& rotation = transform.Rotation.GetQuat();
					glm::quat origRotation = rotation;
					rotation = rotation * rotX;
					rotation = rotY * rotation;

					// If we flipped over, reset back
					{
						forward = cameraComponent.GetForwardVector();
						const float cosThetaAfter = glm::dot(forward, glm::vec3(0.f, 1.f, 0.f));
						const bool bLock = bMoveInTheSameDir && glm::abs(cosThetaAfter) < glm::abs(cosTheta);
						if (bLock)
						{
							rotation = origRotation;
							rotation = rotY * rotation;
						}
					}

					glm::vec3 right = cameraComponent.GetRightVector();

					glm::vec3& Location = transform.Location;
					if (Input::IsKeyPressed(Key::W))
						Location += (forward * (m_MoveSpeed * ts));
					if (Input::IsKeyPressed(Key::S))
						Location -= (forward * (m_MoveSpeed * ts));
					if (Input::IsKeyPressed(Key::Q))
						Location.y -= m_MoveSpeed * ts;
					if (Input::IsKeyPressed(Key::E))
						Location.y += m_MoveSpeed * ts;
					if (Input::IsKeyPressed(Key::A))
						Location -= (right * (m_MoveSpeed * ts));
					if (Input::IsKeyPressed(Key::D))
						Location += (right * (m_MoveSpeed * ts));

					m_MouseX = Input::GetMouseX();
					m_MouseY = Input::GetMouseY();

					cameraComponent.SetWorldTransform(transform);
				}
				else
				{
					m_NumberOfFramesMoving = 0;
					if (Input::IsCursorVisible() == false)
					{
						Input::SetShowCursor(true);
					}
				}
			}
		}
	}
}