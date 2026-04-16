#pragma once

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Camera/EditorCamera.h"
#include "Eagle/Audio/Sound3D.h"
#include "Eagle/Physics/PhysicsSettings.h"
#include "Eagle/Animation/Animation.h"
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
	class NavigationMeshComponent;
	class Sound2D;
	class AssetAudio;
	class AssetEntity;
	class AssetScene;
	class AssetAnimationGraph;

	namespace AINavigation
	{
		class Mesh;
		struct CrowdSettings;
	}

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
			bool bDirLightsDirty = true;

			bool bTextDirty = true;
			bool bText2DDirty = true;
			bool bTextTransformsDirty = true;
			bool bImage2DDirty = true;

			bool bDecalsDirty = true;
			bool bDecalTransformsDirty = true;

			bool bRecreateParticleSystems = true;

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
				bDirLightsDirty = bDirty;
				bTextDirty = bDirty;
				bText2DDirty = bDirty;
				bTextTransformsDirty = bDirty;
				bImage2DDirty = bDirty;
				bDecalsDirty = bDirty;
				bDecalTransformsDirty = bDirty;
				bRecreateParticleSystems = bDirty;
			}
		};

	public:
		// Used by asset manager to store entity assets since they need to be tied to a scene.
		// So don't use it.
		Scene();

		Scene(const std::string& debugName, const Ref<SceneRenderer>& sceneRenderer = nullptr, bool bRuntime = false);
		Scene(const Ref<Scene>& other, const std::string& debugName);
		~Scene();

		// Use copy constructor
		Scene(Scene&& other) = delete;
		Scene& operator=(const Scene& other) = delete;
		Scene& operator=(Scene&& other) = delete;

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithGUID(GUID guid, const std::string& name = std::string());
		Entity CreateFromEntity(const Entity& source, bool bCopyGUID = false);
		Entity CreateFromEntityAsset(const Ref<AssetEntity>& asset, bool bCopyGUID = false);
		void DestroyEntity(Entity entity, bool bDestroyChildren = false);

		// Note: Carefule when using it since it will invalidate `entity`.
		// @bDestroyChildren. If set to true, child entities will also be removed from the scene and invalidated
		void DestroyEntityImmediately(Entity entity, bool bDestroyChildren = false);

		void ReloadEntitiesCreatedFromAsset(const Ref<AssetEntity>& asset);

		void OnUpdate(Timestep ts, bool bRender = true, bool bForceAnimationsUpdate = false);

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

		// Needs to be called every frame
		void DrawDebugTriangle(const RendererTriangle& triangle)
		{
			m_UserDebugTriangles.push_back(triangle);
		}

		// Needs to be called every frame
		void DrawAABB(const AABB& aabb, const Transform& transform)
		{
			m_UserAABBs.emplace_back(aabb, transform);
		}

		void DrawBox(const AABB& aabb, const Transform& transform)
		{
			m_UserBoxes.emplace_back(aabb, transform);
		}

		// Needs to be called every frame
		void DrawArrow(const glm::vec3& start, const glm::vec3& end, const glm::vec3& up);
		void DrawCone(const glm::vec3& location, const glm::quat& rotation, float distance, float angleRad);
		void DrawFrustum(const CameraComponent& camera);

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

		void SetGravity(const glm::vec3& gravity);
		const glm::vec3& GetGravity() const { return m_RuntimePhysicsSettings.Gravity; }

		void SetPhysicsUpdateRate(uint32_t updateRate);
		uint32_t GetPhysicsUpdateRate() const { return m_RuntimePhysicsSettings.UpdateRate; }

		void SetPhysicsDebugOnPlay(bool bEnable);
		bool IsPhysicsDebugOnPlayEnabled() const { return m_RuntimePhysicsSettings.bDebugOnPlay; }

		void SetPhysicsDebugType(DebugType type);
		DebugType GetPhysicsDebugType() const { return m_RuntimePhysicsSettings.DebugType; }
		
		void InvalidateCollisionGroups(uint32_t validMasks);

		template <typename... T>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<T...>();
		}

		template <typename... T>
		std::vector<Entity> GetAllEntitiesWith_Vector()
		{
			auto view = m_Registry.view<T...>();
			std::vector<Entity> result;
			result.reserve(view.size());
			for (auto& e : view)
			{
				result.emplace_back(e, this);
			}

			return result;
		}

		// void(const Entity);
		template<typename Func>
		void OnEach(Func func)
		{
			for (auto [entt] : m_Registry.storage<entt::entity>().each())
			{
				Entity entity(entt, this);
				func(entity);
			}
		}

		size_t GetEntitiesCount() const
		{
			return m_Registry.storage<entt::entity>()->size();
		}

		// Skybox
		void SetSkybox(const Ref<AssetTextureCube>& cubemap);
		const Ref<AssetTextureCube>& GetSkybox() const { return m_Cubemap; }
		void SetSkyboxIntensity(float intensity);
		float GetSkyboxIntensity() const { return m_SkyboxIntensity; }

		void SetSkybox(const SkySettings& sky);
		const SkySettings& GetSkySettings() const { return m_Sky; }

		// Also disables/enables sky (not just cubemap)
		void SetSkyboxEnabled(bool bEnabled);
		bool IsSkyboxEnabled() const { return m_bSkyboxEnabled; }
		void SetRenderSkybox(bool bEnabled);
		bool IsRenderSkyboxEnabled() const { return m_bRenderSkybox; }

		void SetUseSkyAsBackground(bool value);
		bool GetUseSkyAsBackground() const { return m_bUseSkyAsBackground; }

		// Currently, scene can only have on NavMesh. So all other NavMeshes are destroyed.
		// Can pass a nullptr to remove all nav meshes & update obstacles properly
		void BuildNavMesh(NavigationMeshComponent* navMesh);
		void RebuildNavMesh(); // Uses current one if available
		const Ref<AINavigation::Mesh>& GetNavMesh() const { return m_CurrentNavMesh; }
		void BuildCrowd(const AINavigation::CrowdSettings& settings); // Builds crowd system for the current nav mesh
		
		//Camera
		CameraComponent* GetRuntimeCamera();
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
		static GUID AddOnSceneOpenedCallback(const std::function<void(const Ref<Scene>&)>& func);
		static void RemoveOnSceneOpenedCallback(GUID id);

		static const Ref<Scene>& GetCurrentScene() { return s_CurrentScene; }

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

		void SetEverythingDirty()
		{
			m_DirtyFlags.SetEverythingDirty(true);
		}

		void AddParticleSystem(const ParticleSystemComponent& system);
		void RemoveParticleSystem(const ParticleSystemComponent& system);
		void UpdateParticleSystem(const ParticleSystemComponent& system);

		void DestroyPendingEntities();
		bool IsPendingDestroy(Entity entity) const;

		size_t GetSpritesCount() const { return m_Sprites.size(); }
		size_t GetStaticMeshesCount() const { return m_Meshes.size(); }
		size_t GetSkeletalMeshesCount() const { return m_SkeletalMeshes.size(); }
		size_t GetPointLightsCount() const { return m_PointLights.size(); }
		size_t GetSpotLightsCount() const { return m_SpotLights.size(); }
		size_t GetDirLightsCount() const { return m_DirectionalLights.size(); }
		bool HasIBL() const { return m_Cubemap && IsSkyboxEnabled(); }

	private:
		static void OnSceneOpened(const Ref<Scene>& scene);

	private:
		void CopyComponents(Entity source, Entity dest);
		void DestroyScripts();

		void OnUpdateEditor(Timestep ts, bool bRender, bool bForceAnimationsUpdate);
		void OnUpdateRuntime(Timestep ts, bool bRender, bool bForceAnimationsUpdate);
		void UpdateNavMesh(Timestep ts);
		void SyncCrowdAgents();
		void SetupOnAppAssemblyReloadedCallback();

		void GatherLightsInfo();
		void GatherSkeletalMeshes();
		void UpdateScripts(Timestep ts);
		void UpdateAnimations(Timestep ts, bool bUseBasePose, bool bApplyRootMotion);
		void RenderScene(Timestep ts, bool bRuntime);
		CameraComponent* FindOrCreateRuntimeCamera();
		void ConnectSignals();
		void RegisterSkeletalParticleIfCan(const ParticleSystemComponent& system);

		void OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e);
		void OnSkeletalMeshComponentRemoved(entt::registry& r, entt::entity e);
		void OnSpriteComponentAddedRemoved(entt::registry& r, entt::entity e);
		void OnDecalComponentAddedRemoved(entt::registry& r, entt::entity e);
		void OnPointLightAdded(entt::registry& r, entt::entity e);
		void OnPointLightRemoved(entt::registry& r, entt::entity e);
		void OnSpotLightAdded(entt::registry& r, entt::entity e);
		void OnSpotLightRemoved(entt::registry& r, entt::entity e);
		void OnTextAddedRemoved(entt::registry& r, entt::entity e);
		void OnText2DAddedRemoved(entt::registry& r, entt::entity e);
		void OnImage2DAddedRemoved(entt::registry& r, entt::entity e);
		void OnParticleSystemAdded(entt::registry& r, entt::entity e);
		void OnParticleSystemRemoved(entt::registry& r, entt::entity e);
		void OnNavMeshRemoved(entt::registry& r, entt::entity e);
		void OnBoxColliderRemoved(entt::registry& r, entt::entity e);
		void OnSphereColliderRemoved(entt::registry& r, entt::entity e);
		void OnCapsuleColliderRemoved(entt::registry& r, entt::entity e);
		void OnCrowdAgentAdded(entt::registry& r, entt::entity e);
		void OnCrowdAgentRemoved(entt::registry& r, entt::entity e);
		void OnCameraRemoved(entt::registry& r, entt::entity e);
		void OnReverbRemoved(entt::registry& r, entt::entity e);
		void OnDirectionalLightRemoved(entt::registry& r, entt::entity e);

		// T - is component type
		template<typename T>
		void OnComponentChanged(T& component, Notification notification)
		{
			if constexpr (std::is_base_of<StaticMeshComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnMaterialChanged)
				{
					m_DirtyFlags.bStaticMeshesDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformStaticMeshes.emplace(component.Parent.GetID());
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
					m_DirtyTransformSkeletalMeshes.emplace(component.Parent.GetID());
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
					m_DirtyTransformSprites.emplace(component.Parent.GetID());
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
					if (component.VisualizeRadiusEnabled())
					{
						m_PointLightsDebugRadii.emplace(component.Parent.GetID());
					}
					else
					{
						m_PointLightsDebugRadii.erase(component.Parent.GetID());
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
					if (component.VisualizeDistanceEnabled())
					{
						m_SpotLightsDebugRadii.emplace(component.Parent.GetID());
					}
					else
					{
						m_SpotLightsDebugRadii.erase(component.Parent.GetID());
					}
				}
			}

			if constexpr (std::is_base_of<DirectionalLightComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnTransformChanged)
				{
					m_DirtyFlags.bDirLightsDirty = true;
				}

				else if (notification == Notification::OnDebugStateChanged)
				{
					if (component.IsVisualizeDirectionEnabled())
					{
						m_DirLightsDebugDirection.emplace(component.Parent.GetID());
					}
					else
					{
						m_DirLightsDebugDirection.erase(component.Parent.GetID());
					}
				}
			}
		
			if constexpr (std::is_base_of<ReverbComponent, T>::value)
			{
				if (notification == Notification::OnDebugStateChanged)
				{
					if (component.IsVisualizeRadiusEnabled())
					{
						m_ReverbDebugBoxes.emplace(component.Parent.GetID());
					}
					else
					{
						m_ReverbDebugBoxes.erase(component.Parent.GetID());
					}
				}
			}

			if constexpr (std::is_base_of<TextComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged || notification == Notification::OnMaterialChanged)
				{
					m_DirtyFlags.bTextDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformTexts.emplace(component.Parent.GetID());
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

			if constexpr (std::is_base_of<ParticleSystemComponent, T>::value)
			{
				if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformParticles.emplace(component.Parent.GetID());
				}
			}

			if constexpr (std::is_base_of<DecalComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged)
				{
					m_DirtyFlags.bDecalsDirty = true;
				}
				else if (notification == Notification::OnTransformChanged)
				{
					m_DirtyTransformDecals.emplace(component.Parent.GetID());
					m_DirtyFlags.bDecalTransformsDirty = true;
				}
			}

			if constexpr (std::is_base_of<NavigationMeshComponent, T>::value)
			{
				if (notification == Notification::OnStateChanged)
				{
					if (component.bAutoRebuild)
					{
						auto& navMesh = component.GetNavMesh();
						if (navMesh && navMesh == m_CurrentNavMesh)
							BuildNavMesh(&component);
					}
				}
			}

			if constexpr (std::is_base_of<CameraComponent, T>::value)
			{
				if (notification == Notification::OnDebugStateChanged)
				{
					if (component.IsDebugFrustumCullingEnabled())
					{
						m_DebugCameras.emplace(component.Parent.GetID());
					}
					else
					{
						m_DebugCameras.erase(component.Parent.GetID());
					}
				}
			}
		}

	public:
		glm::vec2 ViewportBounds[2] = { glm::vec2(0.f) };
		bool bCanUpdateEditorCamera = false;
		bool bDrawMiscellaneous = true;
		bool bDrawNavMesh = false;
		bool bDrawMeshAABBs = false;
		bool bDrawBones = false;

	private:
		static Ref<Scene> s_CurrentScene;
		Ref<PhysicsScene> m_PhysicsScene;
		Ref<PhysicsScene> m_RuntimePhysicsScene;
		PhysicsSettings m_RuntimePhysicsSettings{};
		EditorCamera m_EditorCamera;
		uint32_t m_ViewportWidth = 1;
		uint32_t m_ViewportHeight = 1;
		Ref<SceneRenderer> m_SceneRenderer;

		// Key - mesh ID (entity ID)
		std::unordered_map<uint32_t, std::vector<glm::mat4>> m_AnimationTransforms;
		// Key - system ID; Value - transforms per emitter
		std::unordered_map<GUID, std::unordered_map<GUID, std::vector<glm::mat4>>> m_SkeletalParticlesAnimationTransforms;

		// Skybox
		Ref<AssetTextureCube> m_Cubemap;
		SkySettings m_Sky;
		float m_SkyboxIntensity = 1.f;
		bool m_bSkyboxEnabled = true;
		bool m_bRenderSkybox = true;
		bool m_bUseSkyAsBackground = true;

		std::unordered_map<GUID, Ref<Sound>> m_SpawnedSounds;

		std::vector<AnimationEventData> m_AnimationsToTrigger;
		std::vector<ParticleSystemComponent*> m_SystemsToUpdateAnims;

		// entt::entity. Can't store Entity (forward declaration)
		std::unordered_set<uint32_t> m_DirtyTransformStaticMeshes;
		std::unordered_set<uint32_t> m_DirtyTransformSkeletalMeshes;
		std::unordered_set<uint32_t> m_DirtyTransformSprites;
		std::unordered_set<uint32_t> m_DirtyTransformTexts;
		std::unordered_set<uint32_t> m_DirtyTransformDecals;

		// entt::entity. Can't store Entity (forward declaration)
		std::unordered_set<uint32_t> m_SkeletalParticles; // Particles that use animated mesh
		std::unordered_set<uint32_t> m_DirtyTransformParticles;

		std::unordered_set<const ParticleSystemComponent*> m_TempParticleSystems; // Used to update and to avoid reallocation of this data structure

		std::unordered_map<GUID, Entity> m_AliveEntities;
		std::vector<const PointLightComponent*> m_PointLights;
		std::vector<const SpotLightComponent*> m_SpotLights;
		std::vector<const DirectionalLightComponent*> m_DirectionalLights;
		std::vector<std::pair<Entity, bool>> m_EntitiesToDestroy; // 1st - Entity to destroy; 2nd - whether to destroy its children
		entt::registry m_Registry;
		CameraComponent* m_RuntimeCamera = nullptr;

		// It's a scope-pointer because `Entity` is forward declared.
		Scope<Entity> m_RuntimeCameraHolder = nullptr; // In case there's no user provided runtime primary-camera

		std::vector<const StaticMeshComponent*> m_Meshes;
		std::vector<SkeletalMeshComponent*> m_SkeletalMeshes;
		std::vector<const SpriteComponent*> m_Sprites;
		std::vector<const BillboardComponent*> m_Billboards;
		std::vector<const TextComponent*> m_Texts;
		std::vector<const Text2DComponent*> m_Texts2D;
		std::vector<const Image2DComponent*> m_Images2D;
		std::vector<const DecalComponent*> m_Decals;
		std::string m_DebugName;

		bool bIsPlaying = false;

		DirtyFlags m_DirtyFlags;

		// Dirty fix but here's the problem it fixes:
		// 1. In order for `GetBoneWorldTransform()` to return the most up-to-date info for scripts, we need to update animations before running scripts.
		// 2. In order to update animations, we need to gather all skeletal meshes.
		// So, we Gather Skeletals -> Update Anims -> Run scrips.
		// But the problem is that scripts might spawn/delete skeletal meshes, which will invalidate gathered skeletal mesh info.
		// And there'll be a mismatch between gathered skeletal mesh data and animation data.
		// So here's the fix: if scripts invalidated skeletal meshes data, we just ignore it for the current frame, and force it to be updated on the next one.
		// This way we keep animation data and skeletal data in sync.
		bool bForceSkeletalMeshUpdateNextFrame = false;

		// Debug data
		std::vector<RendererLine> m_UserDebugLines;
		std::vector<RendererLine> m_DebugLinesToDraw;
		std::vector<RendererTriangle> m_DebugTrianglesToDraw;
		std::vector<RendererTriangle> m_UserDebugTriangles;
		std::vector<std::pair<AABB, Transform>> m_UserBoxes; // Box and its world transform
		std::vector<std::pair<AABB, Transform>> m_UserAABBs; // AABB and its world transform

		// entt::entity. Can't store Entity (forward declaration)
		std::unordered_set<uint32_t> m_PointLightsDebugRadii;
		std::unordered_set<uint32_t> m_SpotLightsDebugRadii;
		std::unordered_set<uint32_t> m_DirLightsDebugDirection;
		std::unordered_set<uint32_t> m_ReverbDebugBoxes;
		std::unordered_set<uint32_t> m_DebugCameras;

		GUID m_GUID;

		Ref<AINavigation::Mesh> m_CurrentNavMesh;
		GUID m_CurrentNavMeshEntityGUID = GUID(0, 0);

		friend class Entity;
	};
}
