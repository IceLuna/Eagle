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
		void DestroyEntity(Entity entity);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		void OnViewportResize(uint32_t width, uint32_t height);

		void ClearScene();

	private:
		std::vector<Entity> m_EntitiesToDestroy;
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
