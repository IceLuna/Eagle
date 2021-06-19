#pragma once

#include <entt.hpp>

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "Eagle/Renderer/Cubemap.h"

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
		void DestroyEntity(Entity& entity);

		void OnUpdateEditor(Timestep ts);
		void OnUpdateRuntime(Timestep ts);

		void OnEventEditor(Event& e);
		void OnEventRuntime(Event& e);

		void OnViewportResize(uint32_t width, uint32_t height);

		void ClearScene();

		const EditorCamera& GetEditorCamera() const { return m_EditorCamera; }
		bool IsSkyboxEnabled() const { return bEnableSkybox; }

		Entity GetPrimaryCameraEntity(); //TODO: Remove

		void SetEnableSkybox(bool bEnable) { bEnableSkybox = bEnable; }

	public:
		Ref<Cubemap> cubemap;

	private:
		bool WasEntityDestroyed(Entity entity);

	private:
		EditorCamera m_EditorCamera;

		std::vector<Entity> m_EntitiesToDestroy;
		std::vector<Entity> m_DestroyedEntities;
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		bool bEnableSkybox = false;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
