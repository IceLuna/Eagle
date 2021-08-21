#pragma once

#include <entt.hpp>

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "Eagle/Renderer/Cubemap.h"

namespace Eagle
{
	class Entity;
	class Event;
	class CameraComponent;

	class Scene
	{
	public:
		Scene();
		Scene(const Ref<Scene>& other);
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
		const CameraComponent* GetRuntimeCamera();

		void SetEnableSkybox(bool bEnable) { bEnableSkybox = bEnable; }
		void SetSceneGamma(float gamma);
		void SetSceneExposure(float exposure);
		float GetSceneGamma() const { return m_SceneGamma; }
		float GetSceneExposure() const { return m_SceneExposure; }

		//Slow
		int GetEntityIDAtCoords(int x, int y);
		uint32_t GetMainColorAttachment(uint32_t index);
		uint32_t GetGBufferColorAttachment(uint32_t index);

	public:
		Ref<Cubemap> m_Cubemap;
		bool bCanUpdateEditorCamera = true;

	private:
		EditorCamera m_EditorCamera;

		std::vector<Entity> m_EntitiesToDestroy;
		entt::registry m_Registry;
		CameraComponent* m_RuntimeCamera = nullptr;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		float m_SceneGamma = 2.2f;
		float m_SceneExposure = 1.f;
		bool bEnableSkybox = false;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
