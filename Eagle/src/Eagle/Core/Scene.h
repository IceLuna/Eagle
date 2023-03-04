#pragma once

#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "GUID.h"
#include "Notifications.h"

#include <entt.hpp>

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
	class StaticMeshComponent;

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

		template <typename T>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<T>();
		}

		// void(const Entity);
		template<typename Func>
		void OnEach(Func func)
		{
			const auto myFunc = [&func, this](entt::entity e)
			{
				Entity entity(e, this);
				func(entity);
			};
			m_Registry.each(myFunc);
		}
		
		//Camera
		const CameraComponent* GetRuntimeCamera() const;
		Entity GetPrimaryCameraEntity(); //TODO: Remove
		const EditorCamera& GetEditorCamera() const { return m_EditorCamera; }

		//Static 
		static void SetCurrentScene(const Ref<Scene>& currentScene)
		{
			s_CurrentScene = currentScene;
			if (s_CurrentScene)
			{
				s_CurrentScene->bMeshesDirty = true;
				s_CurrentScene->bMeshTransformsDirty = true;
				s_CurrentScene->bPointLightsDirty = true;
				s_CurrentScene->bSpotLightsDirty = true;
			}
		}

		static Ref<Scene>& GetCurrentScene() { return s_CurrentScene; }

	private:
		void GatherLightsInfo();
		void DestroyPendingEntities();
		void UpdateScripts(Timestep ts);
		void RenderScene();
		CameraComponent* FindOrCreateRuntimeCamera();
		void ConnectSignals();

		void OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e);
		void OnPointLightAdded(entt::registry& r, entt::entity e);
		void OnPointLightRemoved(entt::registry& r, entt::entity e);
		void OnSpotLightAdded(entt::registry& r, entt::entity e);
		void OnSpotLightRemoved(entt::registry& r, entt::entity e);

		// T - is component type
		template<typename T>
		void OnComponentChanged(const T& component, Notification notification)
		{
			// For StaticMeshComponents
			if constexpr (std::is_base_of<StaticMeshComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged)
				{
					bMeshesDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformMeshes.push_back(&component);
				}
				else if (notification == Notification::OnInvalidateMesh)
				{
					bMeshesDirty = true;
					bMeshTransformsDirty = true;
				}
			}

			if constexpr (std::is_base_of<PointLightComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnTransformChanged)
				{
					bPointLightsDirty = true;
				}
			}

			if constexpr (std::is_base_of<SpotLightComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnTransformChanged)
				{
					bSpotLightsDirty = true;
				}
			}
		}

	public:
		bool bCanUpdateEditorCamera = true;

	private:
		static Ref<Scene> s_CurrentScene;
		Ref<PhysicsScene> m_PhysicsScene;
		Ref<PhysicsScene> m_RuntimePhysicsScene;
		Ref<TextureCube> m_IBL;
		EditorCamera m_EditorCamera;

		std::vector<const StaticMeshComponent*> m_DirtyTransformMeshes;

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

		bool bMeshesDirty = true;
		bool bMeshTransformsDirty = true;
		bool bPointLightsDirty = true;
		bool bSpotLightsDirty = true;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
