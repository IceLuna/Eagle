#pragma once

#include "Eagle/Renderer/SceneRenderer.h"
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
		struct DirtyFlags
		{
			bool bMeshesDirty = true;
			bool bMeshTransformsDirty = true;
			bool bSpritesDirty = true;
			bool bSpriteTransformsDirty = true;
			bool bPointLightsDirty = true;
			bool bSpotLightsDirty = true;
			bool bTextDirty = true;

			void SetEverythingDirty(bool bDirty)
			{
				bMeshesDirty = bDirty;
				bMeshTransformsDirty = bDirty;
				bSpritesDirty = bDirty;
				bSpriteTransformsDirty = bDirty;
				bPointLightsDirty = bDirty;
				bSpotLightsDirty = bDirty;
				bTextDirty = bDirty;
			}
		};

	public:
		Scene();
		Scene(const Ref<Scene>& other);
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithGUID(GUID guid, const std::string& name = std::string());
		Entity CreateFromEntity(const Entity& source);
		void DestroyEntity(Entity& entity);

		void OnUpdate(Timestep ts, bool bRender = true);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnEventEditor(Event& e);
		void OnEventRuntime(Event& e);

		void OnViewportResize(uint32_t width, uint32_t height);

		void ClearScene();

		bool IsPlaying() const { return bIsPlaying; }

		Ref<PhysicsScene>& GetPhysicsScene() { return m_PhysicsScene; }
		const Ref<PhysicsScene>& GetPhysicsScene() const { return m_PhysicsScene; }

		const Ref<SceneRenderer>& GetSceneRenderer() const { return m_SceneRenderer; }
		Ref<SceneRenderer>& GetSceneRenderer() { return m_SceneRenderer; }

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
				s_CurrentScene->m_DirtyFlags.SetEverythingDirty(true);
		}

		static Ref<Scene>& GetCurrentScene() { return s_CurrentScene; }

	private:
		void OnUpdateEditor(Timestep ts, bool bRender);
		void OnUpdateRuntime(Timestep ts, bool bRender);

		void GatherLightsInfo();
		void DestroyPendingEntities();
		void UpdateScripts(Timestep ts);
		void RenderScene();
		CameraComponent* FindOrCreateRuntimeCamera();
		void ConnectSignals();

		void OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e);
		void OnSpriteComponentAddedRemoved(entt::registry& r, entt::entity e);
		void OnPointLightAdded(entt::registry& r, entt::entity e);
		void OnPointLightRemoved(entt::registry& r, entt::entity e);
		void OnSpotLightAdded(entt::registry& r, entt::entity e);
		void OnSpotLightRemoved(entt::registry& r, entt::entity e);
		void OnTextAddedRemoved(entt::registry& r, entt::entity e);

		// T - is component type
		template<typename T>
		void OnComponentChanged(const T& component, Notification notification)
		{
			// For StaticMeshComponents
			if constexpr (std::is_base_of<StaticMeshComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnMaterialChanged)
				{
					m_DirtyFlags.bMeshesDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformMeshes.emplace(&component);
					m_DirtyFlags.bMeshTransformsDirty = true;
				}
			}

			if constexpr (std::is_base_of<SpriteComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnMaterialChanged)
				{
					m_DirtyFlags.bSpritesDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformSprites.emplace(&component);
					m_DirtyFlags.bSpriteTransformsDirty = true;
				}
			}

			if constexpr (std::is_base_of<PointLightComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnTransformChanged)
				{
					m_DirtyFlags.bPointLightsDirty = true;
				}
				else if (notification == Notification::OnDebugStateChanged)
				{
					// No need to updated if point lights are dirty since all data will be recollected
					if (!m_DirtyFlags.bPointLightsDirty)
					{
						if (component.VisualizeRadiusEnabled())
						{
							m_PointLightsDebugRadii.emplace(&component);
							m_PointLightsDebugRadiiDirty = true;
						}
						else
						{
							auto it = m_PointLightsDebugRadii.find(&component);
							if (it != m_PointLightsDebugRadii.end())
							{
								m_PointLightsDebugRadii.erase(it);
								m_PointLightsDebugRadiiDirty = true;
							}
						}
					}
				}
			}

			if constexpr (std::is_base_of<SpotLightComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnTransformChanged)
				{
					m_DirtyFlags.bSpotLightsDirty = true;
				}

				else if (notification == Notification::OnDebugStateChanged)
				{
					// No need to updated if spot lights are dirty since all data will be recollected
					if (!m_DirtyFlags.bSpotLightsDirty)
					{
						if (component.VisualizeDistanceEnabled())
						{
							m_SpotLightsDebugRadii.emplace(&component);
							m_SpotLightsDebugRadiiDirty = true;
						}
						else
						{
							auto it = m_SpotLightsDebugRadii.find(&component);
							if (it != m_SpotLightsDebugRadii.end())
							{
								m_SpotLightsDebugRadii.erase(it);
								m_SpotLightsDebugRadiiDirty = true;
							}
						}
					}
				}
			}
		
			if constexpr (std::is_base_of<TextComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnTransformChanged)
				{
					m_DirtyFlags.bTextDirty = true;
				}
			}
		}

	public:
		bool bCanUpdateEditorCamera = true;
		bool bDrawMiscellaneous = true;

	private:
		static Ref<Scene> s_CurrentScene;
		Ref<PhysicsScene> m_PhysicsScene;
		Ref<PhysicsScene> m_RuntimePhysicsScene;
		EditorCamera m_EditorCamera;
		uint32_t m_ViewportWidth = 1;
		uint32_t m_ViewportHeight = 1;
		Ref<SceneRenderer> m_SceneRenderer;

		std::set<const StaticMeshComponent*> m_DirtyTransformMeshes;
		std::set<const SpriteComponent*> m_DirtyTransformSprites;

		std::vector<RendererLine> m_DebugLines;
		std::vector<RendererLine> m_DebugPointLines;
		std::vector<RendererLine> m_DebugSpotLines;

		std::map<GUID, Entity> m_AliveEntities;
		std::vector<Entity> m_EntitiesToDestroy;
		std::vector<const PointLightComponent*> m_PointLights;
		std::set<const PointLightComponent*> m_PointLightsDebugRadii;
		bool m_PointLightsDebugRadiiDirty = true;
		std::set<const SpotLightComponent*> m_SpotLightsDebugRadii;
		bool m_SpotLightsDebugRadiiDirty = true;
		std::vector<const SpotLightComponent*> m_SpotLights;
		DirectionalLightComponent* m_DirectionalLight = nullptr;
		entt::registry m_Registry;
		CameraComponent* m_RuntimeCamera = nullptr;
		Entity* m_RuntimeCameraHolder = nullptr; //In case there's no user provided runtime primary-camera

		std::vector<const StaticMeshComponent*> m_Meshes;
		std::vector<const SpriteComponent*> m_Sprites;
		std::vector<const BillboardComponent*> m_Billboards;
		std::vector<const TextComponent*> m_Texts;

		bool bIsPlaying = false;

		DirtyFlags m_DirtyFlags;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
