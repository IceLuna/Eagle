#include "egpch.h"
#include "SceneCamera.h"

#include "OrthographicCameraController.h"

namespace Eagle
{
	SceneCamera::SceneCamera(float aspectRatio, CameraProjectionMode cameraType)
	:	m_AspectRatio(aspectRatio)
	,	m_CameraProjectionMode(cameraType)
	{
		switch (m_CameraProjectionMode)
		{
			case CameraProjectionMode::Orthographic:
				m_CameraController = MakeRef<OrthographicCameraController>(m_AspectRatio);
				break;
			case CameraProjectionMode::Perspective:
				EG_CORE_ASSERT(false, "Eagle supports only Orthographic Cameras for now!");
				break;
			default:
				EG_CORE_ASSERT(false, "Invalid CameraProjectionMode!");
		}
	}

	void SceneCamera::OnUpdate(Timestep ts)
	{
		m_CameraController->OnUpdate(ts);
	}

	void SceneCamera::OnEvent(Event& e)
	{
		m_CameraController->OnEvent(e);
	}

	void SceneCamera::SetAspectRatio(float aspectRatio)
	{
		m_CameraController->SetAspectRatio(aspectRatio);
	}
}


