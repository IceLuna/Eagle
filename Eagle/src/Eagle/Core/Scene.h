#pragma once

#include <entt.hpp>

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/SceneCamera.h"

namespace Eagle
{
	class Entity;
	class Event;
	class EditorCamera;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		void OnUpdateEditor(Timestep ts, const EditorCamera& editorCamera);
		void OnUpdateRuntime(Timestep ts);

		void OnEventEditor(Event& e);
		void OnEventRuntime(Event& e);

		void OnViewportResize(uint32_t width, uint32_t height);

		void ClearScene();

		Entity GetPrimaryCameraEntity(); //TODO: Remove

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
