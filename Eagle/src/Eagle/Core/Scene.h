#pragma once

#include <entt.hpp>

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "GUID.h"

namespace Eagle
{
	class Entity;
	class Event;
	class CameraComponent;
	class PhysicsScene;
	class PhysicsActor;
	class PointLightComponent;
	class SpotLightComponent;
	class TextureCube;
	class DirectionalLightComponent;

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

		const Ref<TextureCube>& GetIBL() const { return m_IBL; }
		bool IsIBLEnabled() const { return bEnableIBL; }
		bool IsPlaying() const { return bIsPlaying; }

		void SetIBL(const Ref<TextureCube>& ibl) { m_IBL = ibl; }
		void SetEnableIBL(bool bEnable) { bEnableIBL = bEnable; }
		void SetGamma(float gamma);
		void SetExposure(float exposure);
		void SetTonemappingMethod(TonemappingMethod method);
		void SetPhotoLinearTonemappingParams(PhotoLinearTonemappingParams params);
		void SetFilmicTonemappingParams(FilmicTonemappingParams params);
		Ref<PhysicsScene>& GetPhysicsScene() { return m_PhysicsScene; }
		const Ref<PhysicsScene>& GetPhysicsScene() const { return m_PhysicsScene; }

		float GetGamma() const { return m_Gamma; }
		float GetExposure() const { return m_Exposure; }
		TonemappingMethod GetTonemappingMethod() const { return m_TonemappingMethod; }
		PhotoLinearTonemappingParams GetPhotoLinearTonemappingParams() const { return m_PhotoLinearParams; }
		FilmicTonemappingParams GetFilmicTonemappingParams() const { return m_FilmicParams; }

		Entity GetEntityByGUID(const GUID& guid) const;
		const Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity) const;
		Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity);
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

		//Static 
		static void SetCurrentScene(const Ref<Scene>& currentScene) { s_CurrentScene = currentScene; }
		static Ref<Scene>& GetCurrentScene() { return s_CurrentScene; }

	private:
		void GatherLightsInfo();
		void DestroyPendingEntities();
		void UpdateScripts(Timestep ts);
		void RenderScene();
		CameraComponent* FindOrCreateRuntimeCamera();
		void OnStaticMeshComponentAdded(entt::registry& r, entt::entity e);
		void OnStaticMeshComponentUpdated(entt::registry& r, entt::entity e);
		void OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e);

	public:
		bool bCanUpdateEditorCamera = true;

	private:
		static Ref<Scene> s_CurrentScene;
		Ref<PhysicsScene> m_PhysicsScene;
		Ref<PhysicsScene> m_RuntimePhysicsScene;
		Ref<TextureCube> m_IBL;
		EditorCamera m_EditorCamera;

		std::map<GUID, Entity> m_AliveEntities;
		std::vector<Entity> m_EntitiesToDestroy;
		std::vector<PointLightComponent*> m_PointLights;
		std::vector<SpotLightComponent*> m_SpotLights;
		DirectionalLightComponent* m_DirectionalLight = nullptr;
		entt::registry m_Registry;
		CameraComponent* m_RuntimeCamera = nullptr;
		Entity* m_RuntimeCameraHolder = nullptr; //In case there's no user provided runtime primary-camera
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		float m_Gamma = 2.2f;
		float m_Exposure = 1.f;
		TonemappingMethod m_TonemappingMethod = TonemappingMethod::Reinhard;
		PhotoLinearTonemappingParams m_PhotoLinearParams;
		FilmicTonemappingParams m_FilmicParams;
		bool bEnableIBL = false;
		bool bIsPlaying = false;
		bool bDrawMiscellaneous = true;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
