#pragma once

#include <entt.hpp>

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "Eagle/Renderer/Cubemap.h"
#include "GUID.h"

namespace Eagle
{
	class Entity;
	class Event;
	class CameraComponent;
	class PhysicsScene;
	class PhysicsActor;

	class Scene
	{
	public:
		Scene();
		Scene(const Ref<Scene>& other);
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithGUID(GUID guid, const std::string& name = std::string());
		Entity CreateFromEntity(const Entity& source);
		void DestroyEntity(Entity& entity);

		void OnUpdateEditor(Timestep ts);
		void OnUpdateRuntime(Timestep ts);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnEventEditor(Event& e);
		void OnEventRuntime(Event& e);

		void OnViewportResize(uint32_t width, uint32_t height);

		void ClearScene();

		bool IsSkyboxEnabled() const { return bEnableSkybox; }
		bool IsPlaying() const { return bIsPlaying; }

		void SetEnableSkybox(bool bEnable) { bEnableSkybox = bEnable; }
		void SetSceneGamma(float gamma);
		void SetSceneExposure(float exposure);

		float GetSceneGamma() const { return m_SceneGamma; }
		float GetSceneExposure() const { return m_SceneExposure; }

		Entity GetEntityByGUID(const GUID& guid) const;
		Ref<PhysicsActor> GetPhysicsActor(const Entity& entity) const;
		const std::map<GUID, Entity>& GetAliveEntities() const { return m_AliveEntities; }
		std::map<GUID, Entity>& GetAliveEntities() { return m_AliveEntities; }

		template <typename T>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<T>();
		}
		
		//Camera
		const CameraComponent* GetRuntimeCamera() const;
		Entity GetPrimaryCameraEntity(); //TODO: Remove
		const EditorCamera& GetEditorCamera() const { return m_EditorCamera; }

		//Slow
		int GetEntityIDAtCoords(int x, int y) const;
		uint32_t GetMainColorAttachment(uint32_t index) const;
		uint32_t GetGBufferColorAttachment(uint32_t index) const;

		//Static 
		static void SetCurrentScene(const Ref<Scene>& currentScene) { s_CurrentScene = currentScene; }
		static Ref<Scene>& GetCurrentScene() { return s_CurrentScene; }

	public:
		Ref<Cubemap> m_Cubemap;
		bool bCanUpdateEditorCamera = true;

	private:
		static Ref<Scene> s_CurrentScene;
		Ref<PhysicsScene> m_PhysicsScene;
		EditorCamera m_EditorCamera;

		std::map<GUID, Entity> m_AliveEntities;
		std::vector<Entity> m_EntitiesToDestroy;
		entt::registry m_Registry;
		CameraComponent* m_RuntimeCamera = nullptr;
		Entity* m_RuntimeCameraHolder = nullptr; //In case there's no user provided runtime primary-camera
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		float m_SceneGamma = 2.2f;
		float m_SceneExposure = 1.f;
		bool bEnableSkybox = false;
		bool bIsPlaying = false;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
