#pragma once

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "Eagle/Audio/Sound3D.h"
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
	class SkeletalMeshComponent;
	class ReverbComponent;
	class Sound2D;
	class AssetAudio;
	class AssetEntity;
	class AssetScene;
	class AssetAnimationGraph;

	struct SceneSoundData
	{
		GUID ID;
		Ref<Sound> Sound;
	};

	class Scene
	{
		struct DirtyFlags
		{
			bool bStaticMeshesDirty = true;
			bool bStaticMeshTransformsDirty = true;

			bool bSkeletalMeshesDirty = true;
			bool bSkeletalMeshTransformsDirty = true;

			bool bSpritesDirty = true;
			bool bSpriteTransformsDirty = true;

			bool bPointLightsDirty = true;
			bool bSpotLightsDirty = true;

			bool bTextDirty = true;
			bool bText2DDirty = true;
			bool bTextTransformsDirty = true;
			bool bImage2DDirty = true;

			void SetEverythingDirty(bool bDirty)
			{
				bStaticMeshesDirty = bDirty;
				bStaticMeshTransformsDirty = bDirty;
				bSkeletalMeshesDirty = bDirty;
				bSkeletalMeshTransformsDirty = bDirty;
				bSpritesDirty = bDirty;
				bSpriteTransformsDirty = bDirty;
				bPointLightsDirty = bDirty;
				bSpotLightsDirty = bDirty;
				bTextDirty = bDirty;
				bText2DDirty = bDirty;
				bTextTransformsDirty = bDirty;
				bImage2DDirty = bDirty;
			}
		};

	public:
		// Used by asset manager to store entity assets since they need to be tied to a scene.
		// So don't use it.
		Scene()
			: m_DebugName("Empty") {}

		Scene(const std::string& debugName, const Ref<SceneRenderer>& sceneRenderer = nullptr, bool bRuntime = false);
		Scene(const Ref<Scene>& other, const std::string& debugName);
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithGUID(GUID guid, const std::string& name = std::string());
		Entity CreateFromEntity(const Entity& source);
		Entity CreateFromEntityAsset(const Ref<AssetEntity>& asset);
		void DestroyEntity(Entity entity);

		void ReloadEntitiesCreatedFromAsset(const Ref<AssetEntity>& asset);

		void OnUpdate(Timestep ts, bool bRender = true);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnEventEditor(Event& e);
		void OnEventRuntime(Event& e);

		void OnViewportResize(uint32_t width, uint32_t height);

		void ClearScene();

		bool IsPlaying() const { return bIsPlaying; }

		// Needs to be called every frame
		void DrawDebugLine(const RendererLine& line)
		{
			m_UserDebugLines.push_back(line);
		}

		SceneSoundData SpawnSound2D(const Ref<AssetAudio>& audio, const SoundSettings& settings);
		SceneSoundData SpawnSound3D(const Ref<AssetAudio>& audio, const glm::vec3& position, RollOffModel rollOff = RollOffModel::Default, const SoundSettings& settings = {});
		Ref<Sound> GetSpawnedSound(GUID id) const;

		Ref<PhysicsScene>& GetPhysicsScene() { return m_PhysicsScene; }
		const Ref<PhysicsScene>& GetPhysicsScene() const { return m_PhysicsScene; }

		const Ref<SceneRenderer>& GetSceneRenderer() const { return m_SceneRenderer; }
		Ref<SceneRenderer>& GetSceneRenderer() { return m_SceneRenderer; }

		Entity GetEntityByGUID(const GUID& guid) const;
		const Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity) const;
		Ref<PhysicsActor>& GetPhysicsActor(const Entity& entity);

		const std::string& GetDebugName() const { return m_DebugName; }

		GUID GetGUID() const { return m_GUID; }
		void SetGUID(GUID guid) { m_GUID = guid; }

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

		// Skybox
		void SetSkybox(const Ref<AssetTextureCube>& cubemap);
		const Ref<AssetTextureCube>& GetSkybox() const { return m_Cubemap; }
		void SetSkyboxIntensity(float intensity);
		float GetSkyboxIntensity() const { return m_CubemapIntensity; }

		void SetSkybox(const SkySettings& sky);
		const SkySettings& GetSkySettings() const { return m_Sky; }

		// Also disables/enables sky (not just cubemap)
		void SetSkyboxEnabled(bool bEnabled);
		bool IsSkyboxEnabled() const { return m_bSkyboxEnabled; }

		void SetUseSkyAsBackground(bool value);
		bool GetUseSkyAsBackground() const { return m_bUseSkyAsBackground; }
		
		//Camera
		const CameraComponent* GetRuntimeCamera() const;
		Entity GetPrimaryCameraEntity(); //TODO: Remove
		const EditorCamera& GetEditorCamera() const { return m_EditorCamera; }
		EditorCamera& GetEditorCamera() { return m_EditorCamera; }

		//Static 
		static void SetCurrentScene(const Ref<Scene>& currentScene)
		{
			s_CurrentScene = currentScene;
			if (s_CurrentScene)
				s_CurrentScene->m_DirtyFlags.SetEverythingDirty(true);
		}

		// This call is delayed for 1 frame.
		// So use a callback to know exactly when a scene is ready to use
		// @bReuseCurrentSceneRenderer. If set to true, `s_CurrentScene`s SceneRenderer will be reused. Useful to not recreate GPU resources
		// @bRuntime. Set to true if a scene will be used for runtime simulations.
		static void OpenScene(const Ref<AssetScene>& sceneAsset, bool bReuseCurrentSceneRenderer = true, bool bRuntime = false);

		// @id. It's used to identify the callback function. It can be used to remove a callback.
		// Using the same ID for adding callback will remove the old callback
		static void AddOnSceneOpenedCallback(GUID id, const std::function<void(const Ref<Scene>&)>& func);
		static void RemoveOnSceneOpenedCallback(GUID id);

		static Ref<Scene>& GetCurrentScene() { return s_CurrentScene; }

		void SetStaticMeshesDirty(bool bDirty)
		{
			m_DirtyFlags.bStaticMeshesDirty = bDirty;
		}

		void SetSkeletalMeshesDirty(bool bDirty)
		{
			m_DirtyFlags.bSkeletalMeshesDirty = bDirty;
		}

		void SetTextsDirty(bool bDirty)
		{
			m_DirtyFlags.bTextDirty = bDirty;
			m_DirtyFlags.bText2DDirty = bDirty;
		}

		void UpdateAnimGraphAsset(const Ref<AssetAnimationGraph>& graph);

	private:
		static void OnSceneOpened(const Ref<Scene>& scene);

	private:
		void CopyComponents(Entity source, Entity dest);

		void OnUpdateEditor(Timestep ts, bool bRender);
		void OnUpdateRuntime(Timestep ts, bool bRender);

		void GatherLightsInfo();
		void DestroyPendingEntities();
		void UpdateScripts(Timestep ts);
		void RenderScene(Timestep ts, bool bRender, bool bRuntime);
		CameraComponent* FindOrCreateRuntimeCamera();
		void ConnectSignals();

		void OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e);
		void OnSkeletalMeshComponentRemoved(entt::registry& r, entt::entity e);
		void OnSpriteComponentAddedRemoved(entt::registry& r, entt::entity e);
		void OnPointLightAdded(entt::registry& r, entt::entity e);
		void OnPointLightRemoved(entt::registry& r, entt::entity e);
		void OnSpotLightAdded(entt::registry& r, entt::entity e);
		void OnSpotLightRemoved(entt::registry& r, entt::entity e);
		void OnTextAddedRemoved(entt::registry& r, entt::entity e);
		void OnText2DAddedRemoved(entt::registry& r, entt::entity e);
		void OnImage2DAddedRemoved(entt::registry& r, entt::entity e);

		// T - is component type
		template<typename T>
		void OnComponentChanged(const T& component, Notification notification)
		{
			if constexpr (std::is_base_of<StaticMeshComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnMaterialChanged)
				{
					m_DirtyFlags.bStaticMeshesDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformStaticMeshes.emplace(&component);
					m_DirtyFlags.bStaticMeshTransformsDirty = true;
				}
			}

			if constexpr (std::is_base_of<SkeletalMeshComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnMaterialChanged)
				{
					m_DirtyFlags.bSkeletalMeshesDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformSkeletalMeshes.emplace(&component);
					m_DirtyFlags.bSkeletalMeshTransformsDirty = true;
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
		
			if constexpr (std::is_base_of<ReverbComponent, T>::value)
			{
				if (notification == Notification::OnDebugStateChanged)
				{
					if (component.IsVisualizeRadiusEnabled())
					{
						m_ReverbDebugBoxes.emplace(&component);
						m_ReverbDebugBoxesDirty = true;
					}
					else
					{
						auto it = m_ReverbDebugBoxes.find(&component);
						if (it != m_ReverbDebugBoxes.end())
						{
							m_ReverbDebugBoxes.erase(it);
							m_ReverbDebugBoxesDirty = true;
						}
					}
				}
			}

			if constexpr (std::is_base_of<TextComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged)
				{
					m_DirtyFlags.bTextDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformTexts.emplace(&component);
					m_DirtyFlags.bTextTransformsDirty = true;
				}
			}

			if constexpr (std::is_base_of<Text2DComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged)
				{
					m_DirtyFlags.bText2DDirty = true;
				}
			}

			if constexpr (std::is_base_of<Image2DComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged)
				{
					m_DirtyFlags.bImage2DDirty = true;
				}
			}
		}

	public:
		glm::vec2 ViewportBounds[2] = { glm::vec2(0.f) };
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

		// Skybox
		Ref<AssetTextureCube> m_Cubemap;
		SkySettings m_Sky;
		float m_CubemapIntensity = 1.f;
		bool m_bSkyboxEnabled = true;
		bool m_bUseSkyAsBackground = true;

		std::unordered_map<GUID, Ref<Sound>> m_SpawnedSounds;

		std::unordered_set<const StaticMeshComponent*> m_DirtyTransformStaticMeshes;
		std::unordered_set<const SkeletalMeshComponent*> m_DirtyTransformSkeletalMeshes;
		std::unordered_set<const SpriteComponent*> m_DirtyTransformSprites;
		std::unordered_set<const TextComponent*> m_DirtyTransformTexts;

		std::unordered_map<GUID, Entity> m_AliveEntities;
		std::vector<const PointLightComponent*> m_PointLights;
		std::vector<const SpotLightComponent*> m_SpotLights;
		DirectionalLightComponent* m_DirectionalLight = nullptr;
		std::vector<Entity> m_EntitiesToDestroy;
		entt::registry m_Registry;
		CameraComponent* m_RuntimeCamera = nullptr;

		// It's a pointer because `Entity` is forward declared.
		Entity* m_RuntimeCameraHolder = nullptr; //In case there's no user provided runtime primary-camera

		std::vector<const StaticMeshComponent*> m_Meshes;
		std::vector<SkeletalMeshComponent*> m_SkeletalMeshes;
		std::vector<const SpriteComponent*> m_Sprites;
		std::vector<const BillboardComponent*> m_Billboards;
		std::vector<const TextComponent*> m_Texts;
		std::vector<const Text2DComponent*> m_Texts2D;
		std::vector<const Image2DComponent*> m_Images2D;
		std::string m_DebugName;

		bool bIsPlaying = false;

		DirtyFlags m_DirtyFlags;

		// Debug lines
		std::vector<RendererLine> m_UserDebugLines;
		std::vector<RendererLine> m_DebugLinesToDraw;
		std::vector<RendererLine> m_DebugPointLines;
		std::vector<RendererLine> m_DebugSpotLines;
		std::vector<RendererLine> m_DebugReverbLines;

		std::unordered_set<const PointLightComponent*> m_PointLightsDebugRadii;
		bool m_PointLightsDebugRadiiDirty = true;

		std::unordered_set<const SpotLightComponent*> m_SpotLightsDebugRadii;
		bool m_SpotLightsDebugRadiiDirty = true;

		std::unordered_set<const ReverbComponent*> m_ReverbDebugBoxes;
		bool m_ReverbDebugBoxesDirty = true;

		GUID m_GUID;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
