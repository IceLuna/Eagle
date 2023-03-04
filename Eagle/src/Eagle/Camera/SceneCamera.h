#pragma once

#include "Camera.h"

namespace Eagle
{
	class SceneCamera : public Camera
	{
	public:
		SceneCamera() = default;
		SceneCamera(const SceneCamera&) = default;
		SceneCamera(SceneCamera&&) = default;
		SceneCamera(const Camera& other) : Camera(other) {}

		SceneCamera& operator=(const SceneCamera&) = default;
		SceneCamera& operator=(SceneCamera&&) = default;
	};
}