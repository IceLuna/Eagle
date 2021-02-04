#pragma once

#include "Camera.h"

namespace Eagle
{
	class SceneCamera
	{
	public:
		SceneCamera(float aspectRatio, CameraProjectionMode cameraProjectionMode);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		void SetAspectRatio(float aspectRatio);
		const Camera& GetCamera() const { return m_CameraController->GetCamera(); }
		const CameraProjectionMode GetCameraProjectionMode() { return m_CameraProjectionMode; }

	private:
		Ref<CameraController> m_CameraController;
		float m_AspectRatio;
		CameraProjectionMode m_CameraProjectionMode;
	};
}