#pragma once

#include "Camera.h"

namespace Eagle
{
	class SceneCamera
	{
	public:
		SceneCamera(float aspectRatio, CameraType cameraType);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		void SetAspectRatio(float aspectRatio);
		const Camera& GetCamera() { return m_CameraController->GetCamera(); }

	private:
		Ref<CameraController> m_CameraController;
		float m_AspectRatio;
		CameraType m_CameraType;
	};
}