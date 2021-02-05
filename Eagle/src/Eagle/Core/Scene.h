#pragma once

#include <entt.hpp>

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/SceneCamera.h"

namespace Eagle
{
	class Entity;
	class Event;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		void SetSceneCameraAspectRatio(float aspectRatio);

	private:
		entt::registry m_Registry;
		SceneCamera m_SceneCamera;
		friend class Entity;
		friend class SceneHierarchyPanel;
	};
}
