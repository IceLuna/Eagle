#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/AI/NavigationDebugDraw.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Core/AsyncSceneLoader.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Input/Input.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/SceneSequence/SequenceTrack.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Script/ScriptUserComponents.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Physics/PhysicsUtils.h"
#include "Eagle/Debug/CPUTimings.h"

namespace Eagle
{
	namespace Utils
	{
		template <typename Comp>
		void InvalidateCollisionGroups(entt::registry& registry, uint32_t validMasks)
		{
			auto view = registry.view<Comp>();
			for (auto entityID : view)
			{
				auto& comp = view.get<Comp>(entityID);
				comp.SetCollisionGroup(CollisionGroup(uint32_t(comp.GetCollisionGroup()) & validMasks));
				comp.SetInteractingCollisionGroup(CollisionGroup(uint32_t(comp.GetInteractingCollisionGroup()) & validMasks));
			}
		}
	}

	Ref<Scene> Scene::s_CurrentScene;
	static Ref<PhysicsActor> s_NullPhysicsActor;

	static std::unordered_map<GUID, std::function<void(const Ref<Scene>&)>> s_OnSceneOpenedCallbacks;

	template<typename T>
	static void SceneAddAndCopyComponent(Scene* destScene, entt::registry& destRegistry, entt::registry& srcRegistry, const ankerl::unordered_dense::map<entt::entity, entt::entity>& createdEntities)
	{
		auto entities = srcRegistry.view<T>();
		for (auto srcEntity : entities)
		{
			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			Entity destEntity(createdEntities.at(srcEntity), destScene);
			if (destEntity.HasComponent<T>())
			{
				T& comp = destEntity.GetComponent<T>();
				comp = srcComponent;
			}
			else
			{
				T& comp = destEntity.AddComponent<T>();
				comp = srcComponent;
			}
		}
	}

	template<typename T>
	static void EntityCopyComponent(const Entity& src, Entity& destination)
	{
		if (src.HasComponent<T>())
		{
			if (!destination.HasComponent<T>())
				destination.AddComponent<T>();

			destination.GetComponent<T>() = src.GetComponent<T>();
		}
	}

	Scene::Scene()
		: m_DebugName("Empty")
	{
		// No scene renderer on purpose
		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(PhysicsSettings());
		m_PhysicsScene = m_RuntimePhysicsScene;
		SetupOnAppAssemblyReloadedCallback();

		// Not enough bits in `m_UIPressedButtons` to store the state for each mouse button
		static_assert(magic_enum::enum_count<Mouse>() <= sizeof(m_UIPressedButtons) * 8);
		static_assert(magic_enum::enum_count<Mouse>() == s_MouseButtonsCount, "Update s_MouseButtonsCount");
	}

	Scene::Scene(const std::string& debugName, bool bRuntime)
		: m_DebugName(debugName)
	{
		// No scene renderer on purpose
		ConnectSignals();
		InitPhysicsScenes(bRuntime);
		SetupOnAppAssemblyReloadedCallback();
	}

	Scene::Scene(const std::string& debugName, const Ref<SceneRenderer>& sceneRenderer, bool bRuntime)
		: m_DebugName(debugName)
	{
		if (sceneRenderer)
		{
			m_SceneRenderer = sceneRenderer;
		}
		else
		{
			m_SceneRenderer = MakeRef<SceneRenderer>(glm::uvec2{ m_ViewportWidth, m_ViewportHeight });
		}
		OnViewportResize(m_SceneRenderer->GetViewportSize().x, m_SceneRenderer->GetViewportSize().y);
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);
		ConnectSignals();
		InitPhysicsScenes(bRuntime);
		SetupOnAppAssemblyReloadedCallback();
	}

	Scene::Scene(const Ref<Scene>& other, const std::string& debugName)
	: bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
	, m_RuntimePhysicsScene(other->m_RuntimePhysicsScene)
	, m_PhysicsScene(other->m_RuntimePhysicsScene)
	, EditorCamera(other->EditorCamera)
	, m_ViewportWidth(other->m_ViewportWidth)
	, m_ViewportHeight(other->m_ViewportHeight)
	, m_DebugName(debugName)
	, m_RuntimePhysicsSettings(other->m_RuntimePhysicsSettings)
	, bDrawMiscellaneous(other->bDrawMiscellaneous)
	, bDrawNavMesh(other->bDrawNavMesh)
	, bDrawMeshAABBs(other->bDrawMeshAABBs)
	, bDrawBones(other->bDrawBones)
	, m_Cubemap(other->m_Cubemap)
	, m_Sky(other->m_Sky)
	, m_SkyboxIntensity(other->m_SkyboxIntensity)
	, m_bSkyboxEnabled(other->m_bSkyboxEnabled)
	, m_bRenderSkybox(other->m_bRenderSkybox)
	, m_bUseSkyAsBackground(other->m_bUseSkyAsBackground)
	, m_CurrentNavMeshEntityGUID(other->m_CurrentNavMeshEntityGUID)
	{
		// Reuse renderer so that we don't allocate additional GPU resources
		m_SceneRenderer = other->m_SceneRenderer;
		OnViewportResize(m_SceneRenderer->GetViewportSize().x, m_SceneRenderer->GetViewportSize().y);
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);

		ankerl::unordered_dense::map<entt::entity, entt::entity> createdEntities;
		createdEntities.reserve(other->GetEntitiesCount());
		for (auto entt : other->m_Registry.view<TransformComponent>())
		{
			const std::string& sceneName = other->m_Registry.get<EntitySceneNameComponent>(entt).GetName();
			const GUID& guid  = other->m_Registry.get<IDComponent>(entt).ID;
			Entity entity = CreateEntityWithGUID(guid, sceneName);
			createdEntities[entt] = entity.GetEnttID();
		}

		SceneAddAndCopyComponent<TransformComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<TagComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<OwnershipComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<EntityAssetComponent>(this, m_Registry, other->m_Registry, createdEntities);

		SceneAddAndCopyComponent<NativeScriptComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ScriptComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<PointLightComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<DirectionalLightComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SpotLightComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SpriteComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<StaticMeshComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SkeletalMeshComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<BillboardComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<CameraComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<CharacterControllerComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<RigidBodyComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<BoxColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SphereColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<CapsuleColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<MeshColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<AudioComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ReverbComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<TextComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<Text2DComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<Image2DComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ParticleSystemComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SceneSequenceComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<DecalComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<NavigationCrowdAgentComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<NavigationMeshComponent>(this, m_Registry, other->m_Registry, createdEntities);

		for (auto entt : m_Registry.view<RigidBodyComponent>())
		{
			Entity entity{entt, this};
			if (!entity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>())
				m_PhysicsScene->CreatePhysicsActor(entity);
		}

		ConnectSignals();
		SetupOnAppAssemblyReloadedCallback();
		m_DirtyFlags.SetEverythingDirty(true);
		SetForceShowCollision(other->IsForcingShowCollision());
	}

	Scene::~Scene()
	{
		if (IsPlaying())
			OnRuntimeStop();

		m_RuntimeCameraHolder.reset();

		DestroyScripts();

		m_CurrentNavMesh.reset();
		m_PhysicsScene.reset();
		m_RuntimePhysicsScene.reset();
		m_SpawnedSounds.clear();
		m_Registry.clear();

		ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_GUID);
	}

	void Scene::InitPhysicsScenes(bool bRuntime)
	{
		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(m_RuntimePhysicsSettings);
		if (bRuntime)
		{
			m_PhysicsScene = m_RuntimePhysicsScene;
		}
		else
		{
			PhysicsSettings editorSettings;
			editorSettings.UpdateRate = 30u;
			editorSettings.Gravity = glm::vec3{ 0.f };
			editorSettings.bDebugOnPlay = false;
			editorSettings.bEditorScene = true;
			m_PhysicsScene = MakeRef<PhysicsScene>(editorSettings);
		}
	}

	void Scene::SetSceneRenderer(const Ref<SceneRenderer>& sceneRenderer)
	{
		EG_CORE_ASSERT(sceneRenderer);
		m_SceneRenderer = sceneRenderer;

		const glm::uvec2 viewportSize = m_SceneRenderer->GetViewportSize();
		OnViewportResize(viewportSize.x, viewportSize.y);
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);
		SetSkybox(m_Cubemap);

		m_DirtyFlags.SetEverythingDirty(true);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithGUID(GUID(), name);
	}

	Entity Scene::CreateEntityWithGUID(GUID guid, const std::string& name)
	{
		const std::string& sceneName = name.empty() ? "Unnamed Entity" : name;
		Entity entity = Entity(m_Registry.create(), this);
		entity.AddComponent<IDComponent>(guid);
		entity.AddComponent<EntitySceneNameComponent>(sceneName);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<TagComponent>();
		entity.AddComponent<OwnershipComponent>();

		m_AliveEntities[guid] = entity;
		bEntityListChanged = true;

		return entity;
	}

	// Outputs a mapping of src->dst entityIDs
	Entity Scene::CreateFromEntity_Internal(const Entity& source, bool bCopyGUID, ankerl::unordered_dense::map<GUID, GUID>* mapping)
	{
		const GUID& srcGUID = source.GetComponent<IDComponent>().ID;
		const GUID guid = bCopyGUID ? srcGUID : GUID{};
		Entity result = CreateEntityWithGUID(guid, source.GetName());
		EntityCopyComponent<TransformComponent>(source, result); //Copying TransformComponent to set childrens transform correctly
		if (mapping)
		{
			(*mapping)[srcGUID] = guid;
		}

		// Recreating Ownership component
		const auto& srcChildren = source.GetChildren();
		for (auto& child : srcChildren)
		{
			// Don't copy entities that are about to be destroyed
			if (child.GetScene()->IsPendingDestroy(child))
				continue;

			Entity myChild = CreateFromEntity_Internal(child, bCopyGUID, mapping);
			myChild.SetParent(result);
		}

		CopyComponents(source, result);

		return result;
	}

	void Scene::ResolveCopiedPublicFields(Entity dst, const ankerl::unordered_dense::map<GUID, GUID>& mapping)
	{
		if (dst.HasComponent<ScriptComponent>())
		{
			auto& dstScript = dst.GetComponent<ScriptComponent>();
			const size_t fieldsCount = dstScript.PublicFields.size();

			bool bUpdated = false;
			for (size_t i = 0; i < fieldsCount; ++i)
			{
				auto& dstField = dstScript.PublicFields[i];
				if (dstField.Type == FieldType::Entity)
				{
					auto it = mapping.find(dstField.GetStoredValue<GUID>());
					if (it != mapping.end())
					{
						// The src entity was poiniting to another copied entity.
						// So, dst entity should point to this new entity, instead of the source one
						dstField.SetStoredValue(it->second);
						bUpdated = true;
					}
				}
			}

			if (bIsPlaying && bUpdated)
			{
				ScriptEngine::CopyFieldsToRuntime(dst);
			}
		}

		const auto& dstChildren = dst.GetChildren();
		for (auto& child : dstChildren)
		{
			ResolveCopiedPublicFields(child, mapping);
		}
	}

	void Scene::CallOnDestroyScripts(Entity entity, bool bPropagateToChildren)
	{
		if (bPropagateToChildren)
		{
			const auto& children = entity.GetChildren();
			for (auto& child : children)
				CallOnDestroyScripts(child, bPropagateToChildren);
		}

		if (entity.HasComponent<NativeScriptComponent>())
			entity.GetComponent<NativeScriptComponent>().Destroy();

		if (entity.HasComponent<ScriptComponent>())
			ScriptEngine::OnDestroyEntity(entity);
	}

	Entity Scene::CreateFromEntity(const Entity& source, bool bCopyGUID)
	{
		// Key -> src entity GUID; Value -> dst entity GUID
		// When an entity, that has C# public fields of type `Entity`, is copied,
		// we need to properly resolve these copies.
		// For example, SrcEntity1 has child SrcEntity2, and SrcEntity1 has a C# public Entity field that points to SrcEntity2.
		// In this case, copied entity DstEntity1 needs to point to DstEntity2, instead of SrcEntity2.
		// If SrcEntity was pointing to any other entity on the scene (that wasn't involved in this copy operation),
		// then it's left as is.
		ankerl::unordered_dense::map<GUID, GUID> createdEntities;

		// Not point to resolve public fields if GUIDs are copied. They'll be already correct.
		Entity dst = CreateFromEntity_Internal(source, bCopyGUID, bCopyGUID ? nullptr : &createdEntities);
		if (!bCopyGUID)
		{
			ResolveCopiedPublicFields(dst, createdEntities);
		}

		return dst;
	}

	void Scene::DestroyEntity(Entity entity, bool bDestroyChildren)
	{
		if (!entity)
			return;

		// Repeated requests are merged
		auto it = m_EntitiesToDestroy.find(entity.GetEnttID());
		if (it != m_EntitiesToDestroy.end())
		{
			if (bDestroyChildren && !it->second)
			{
				it->second = true;
				if (bIsPlaying)
					CallOnDestroyScripts(entity, true); // `OnDestroy` is called once per script, already notified ones are skipped
			}
			return;
		}

		if (IsPendingDestroy(entity))
			return;

		// Scheduled before notifying scripts, so destroy requests made from `OnDestroy` are merged
		m_EntitiesToDestroy.emplace(entity.GetEnttID(), bDestroyChildren);

		if (bIsPlaying)
		{
			CallOnDestroyScripts(entity, bDestroyChildren);
		}

		// EG_CORE_TRACE("Destroyed Entity: {}", entity.GetComponent<EntitySceneNameComponent>().Name);
	}

	void Scene::DestroyEntityImmediately_Internal(Entity entity, bool bDestroyChildren)
	{
		ScriptEngine::RemoveEntityScript(entity);
		auto& actor = entity.GetPhysicsActor();
		if (actor)
			m_PhysicsScene->RemovePhysicsActor(actor);

		auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
		std::vector<Entity> children = ownershipComponent.Children; // Copy, otherwise it'll be modified when we iterate over it
		Entity myParent = ownershipComponent.EntityParent;
		entity.SetParent(Entity::Null);

		if (bDestroyChildren)
		{
			for (size_t i = 0; i < children.size(); ++i)
				DestroyEntityImmediately_Internal(children[i], bDestroyChildren);
		}
		else
		{
			for (size_t i = 0; i < children.size(); ++i)
				children[i].SetParent(myParent);
		}

		if (m_EntityOutlines.erase(entity.GetID()) > 0)
			m_DirtyFlags.bOutlinesDirty = true;

		m_AliveEntities.erase(entity.GetGUID());
		m_Registry.destroy(entity.GetEnttID());

		bEntityListChanged = true;
	}

	void Scene::DestroyEntityImmediately(Entity entity, bool bDestroyChildren)
	{
		if (!entity)
			return;

		if (bIsPlaying)
		{
			CallOnDestroyScripts(entity, bDestroyChildren);
		}

		m_EntitiesToDestroy.erase(entity.GetEnttID());
		DestroyEntityImmediately_Internal(entity, bDestroyChildren);
	}

	void Scene::OnUpdate(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{
		bEntityListChanged = false;
		bAnyEntityNameChanged = false;

		// Object picking. Done before scripts run so that they see the freshest data
		// The pixel under the mouse is always read back, so hover/click queries at the mouse are always answered
		if (m_SceneRenderer)
		{
			const glm::vec2 viewportSize = ViewportBounds[1] - ViewportBounds[0];
			const glm::ivec2 mouse = glm::ivec2(ImGuiLayer::GetMousePos() - ViewportBounds[0]);
			const bool bMouseInViewport = mouse.x >= 0 && mouse.y >= 0 && mouse.x < (int)viewportSize.x && mouse.y < (int)viewportSize.y;
			m_SceneRenderer->SetMousePickCoord(bMouseInViewport ? std::optional<glm::ivec2>(mouse) : std::nullopt);
			m_SceneRenderer->ResolveObjectPicking();
		}

		if (bIsPlaying)
			OnUpdateRuntime(ts, bRender, bForceAnimationsUpdate);
		else
			OnUpdateEditor(ts, bRender, bForceAnimationsUpdate);
	}

	void Scene::OpenScene(const Ref<AssetScene>& sceneAsset, bool bReuseCurrentSceneRenderer, bool bRuntime)
	{
		// Cancelled here (and not in `func`), so that `OpenScene(loadingScreen)` followed by `OpenSceneAsync()` keeps the async load
		AsyncSceneLoader::CancelAll(false);

		auto func = [path = sceneAsset ? sceneAsset->GetPath() : "", bReuseCurrentSceneRenderer, bRuntime]()
		{
			// Note: when this function is modified, don't forget to update `AsyncSceneLoader::LoadScene`
			RenderManager::Wait();
			Ref<Scene> scene = MakeRef<Scene>(Utils::AsString(path), (bReuseCurrentSceneRenderer && s_CurrentScene) ? s_CurrentScene->GetSceneRenderer() : nullptr, bRuntime);
			scene->SetSkybox(SkySettings{});
			scene->SetSkybox(nullptr);
			scene->SetSkyboxIntensity(1.f);

			if (Application::Get().IsGame())
			{
				Ref<ScopedDataBuffer> sceneData;
				if (AssetManager::GetRuntimeAssetData(path, &sceneData))
				{
					AssetManager::ResetGameAssets();
					AssetManager::ResetRuntimeAsset();
					SceneSerializer::Deserialize(scene, sceneData->GetDataBuffer());
					OnSceneOpened(scene);
				}
				else
					EG_CORE_ERROR("Failed to open the scene. The asset is not found: {}", path);
			}
			else
			{
				if (std::filesystem::exists(path))
				{
					SceneSerializer::Deserialize(scene, path);
				}
				AssetManager::ResetRuntimeAsset();
				OnSceneOpened(scene);
			}
		};

		Application::Get().CallNextFrame(func);
	}

	Ref<SceneLoadRequest> Scene::OpenSceneAsync(const Ref<AssetScene>& sceneAsset, bool bActivateWhenReady, bool bReuseCurrentSceneRenderer, bool bRuntime)
	{
		return AsyncSceneLoader::Load(sceneAsset, bActivateWhenReady, bReuseCurrentSceneRenderer, bRuntime);
	}

	void Scene::SetForceShowCollision(bool bForce)
	{
		if (bForceShowCollision == bForce)
			return;

		bForceShowCollision = bForce;

		// Box
		{
			auto view = m_Registry.view<BoxColliderComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<BoxColliderComponent>(entity);
				component.UpdateShowCollisionState();
			}
		}

		// Sphere
		{
			auto view = m_Registry.view<SphereColliderComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<SphereColliderComponent>(entity);
				component.UpdateShowCollisionState();
			}
		}

		// Capsule
		{
			auto view = m_Registry.view<CapsuleColliderComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<CapsuleColliderComponent>(entity);
				component.UpdateShowCollisionState();
			}
		}

		// Mesh
		{
			auto view = m_Registry.view<MeshColliderComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<MeshColliderComponent>(entity);
				component.UpdateShowCollisionState();
			}
		}
	}

	void Scene::SetSkybox(const Ref<AssetTextureCube>& cubemap)
	{
		m_Cubemap = cubemap;
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkybox(m_Cubemap);
	}

	void Scene::SetSkybox(const SkySettings& sky)
	{
		m_Sky = sky;
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkybox(m_Sky);
	}

	void Scene::SetSkyboxIntensity(float intensity)
	{
		m_SkyboxIntensity = glm::max(0.f, intensity);
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkyboxIntensity(intensity);
	}

	void Scene::SetSkyboxEnabled(bool bEnabled)
	{
		m_bSkyboxEnabled = bEnabled;
		if (m_SceneRenderer)
			m_SceneRenderer->SetSkyboxEnabled(m_bSkyboxEnabled);
	}

	void Scene::SetRenderSkybox(bool bEnabled)
	{
		m_bRenderSkybox = bEnabled;
		if (m_SceneRenderer)
			m_SceneRenderer->SetRenderSkybox(m_bRenderSkybox);
	}

	void Scene::SetUseSkyAsBackground(bool value)
	{
		m_bUseSkyAsBackground = value;
		if (m_SceneRenderer)
			m_SceneRenderer->SetUseSkyAsBackground(m_bUseSkyAsBackground);
	}

	void Scene::BuildNavMesh(NavigationMeshComponent* navMesh)
	{
		std::vector<BaseColliderComponent*> obstacleColliders;
		obstacleColliders.reserve(100u);
		// Collect obstacle colliders
		{
			// Box colliders
			{
				auto view = m_Registry.view<BoxColliderComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<BoxColliderComponent>(entity);
					if (component.IsObstacle())
						obstacleColliders.push_back(&component);
				}
			}
			// Sphere colliders
			{
				auto view = m_Registry.view<SphereColliderComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<SphereColliderComponent>(entity);
					if (component.IsObstacle())
						obstacleColliders.push_back(&component);
				}
			}
			// Capsule colliders
			{
				auto view = m_Registry.view<CapsuleColliderComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<CapsuleColliderComponent>(entity);
					if (component.IsObstacle())
						obstacleColliders.push_back(&component);
				}
			}
		}

		// Go through all colliders and delete obstacles
		for (BaseColliderComponent* collider : obstacleColliders)
			collider->SetIsObstacle(false);

		// Go through all agents and delete them
		auto agentComponents = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.RemoveAgent();
		}

		// Destroy NavMeshes
		{
			m_CurrentNavMesh.reset();
			auto view = m_Registry.view<NavigationMeshComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<NavigationMeshComponent>(entity);
				component.DestroyNavMesh();
			}
		}

		// Build a new nav mesh
		if (navMesh)
		{
			navMesh->Build();
			m_CurrentNavMesh = navMesh->GetNavMesh();
			m_CurrentNavMeshEntityGUID = navMesh->Parent.GetGUID();
		}
		else
		{
			m_CurrentNavMeshEntityGUID = GUID(0, 0);
		}

		// Go through all agents and create them back
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.CreateAgent(component.Parent.GetWorldLocation());
		}

		// Go through all colliders and generate obstacles back
		for (BaseColliderComponent* collider : obstacleColliders)
			collider->SetIsObstacle(true);
	}

	void Scene::RebuildNavMesh()
	{
		if (m_CurrentNavMeshEntityGUID.IsNull())
			return;

		Entity entity = GetEntityByGUID(m_CurrentNavMeshEntityGUID);
		if (!entity)
			return;

		if (!entity.HasComponent<NavigationMeshComponent>())
			return;

		auto& comp = entity.GetComponent<NavigationMeshComponent>();
		if (comp.bAutoRebuild)
			BuildNavMesh(&comp);
	}

	void Scene::BuildCrowd(const AINavigation::CrowdSettings& settings)
	{
		if (!m_CurrentNavMesh)
			return;

		auto agentComponents = m_Registry.view<NavigationCrowdAgentComponent>();

		// Go through all agents and delete them
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.RemoveAgent();
		}

		// Recreate crowd system
		m_CurrentNavMesh->GetCrowd().SetSettings(settings);

		// Go through all agents and create them back
		for (auto entity : agentComponents)
		{
			auto& component = agentComponents.get<NavigationCrowdAgentComponent>(entity);
			component.CreateAgent(component.Parent.GetWorldLocation());
		}
	}

	GUID Scene::AddOnSceneOpenedCallback(const std::function<void(const Ref<Scene>&)>& func)
	{
		GUID id{};
		s_OnSceneOpenedCallbacks[id] = func;
		return id;
	}

	void Scene::RemoveOnSceneOpenedCallback(GUID id)
	{
		s_OnSceneOpenedCallbacks.erase(id);
	}

	void Scene::OnSceneOpened(const Ref<Scene>& scene)
	{
		for (auto& [id, func] : s_OnSceneOpenedCallbacks)
			func(scene);
	}

	void Scene::OnUpdateEditor(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{
		DestroyPendingEntities();

		// While something else drives what's rendered (the sequencer previewing a cutscene, for example),
		// the editor camera isn't what's on screen. Moving it would do nothing visible until the override
		// is released, so its input is ignored for as long as the override lasts
		EditorCamera.OnUpdate(ts, bCanUpdateEditorCamera && !bCameraOverrideValid);

		GatherSkeletalMeshes();
		UpdateAnimations(ts, !bForceAnimationsUpdate, false);
		UpdateNavMesh(ts);
		UpdateSceneSequences(ts);
		m_PhysicsScene->Simulate(ts, false);
		if (bRender)
			RenderScene(ts, false);
	}

	void Scene::OnUpdateRuntime(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{	
		DestroyPendingEntities();

		m_RuntimeCamera = FindOrCreateRuntimeCamera();
		if (!m_RuntimeCamera->FixedAspectRatio)
		{
			if (m_RuntimeCamera->Camera.GetViewportWidth() != m_ViewportWidth || m_RuntimeCamera->Camera.GetViewportHeight() != m_ViewportHeight)
				m_RuntimeCamera->Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}

		GatherSkeletalMeshes();
		UpdateAnimations(ts, false, true);
		UpdateNavMesh(ts);
		SyncCrowdAgents();
		m_PhysicsScene->Simulate(ts, true);
		UpdateScripts(ts);
		UpdateSceneSequences(ts);

		if (bCameraOverrideValid)
			AudioEngine::SetListenerData(m_OverrideViewPos, m_OverrideViewDir, m_OverrideViewUp);
		else
			AudioEngine::SetListenerData(m_RuntimeCamera->GetWorldTransform().Location, m_RuntimeCamera->GetForwardVector(), m_RuntimeCamera->GetUpVector());
		if (bRender)
			RenderScene(ts, true);
	}

	void Scene::UpdateSceneSequences(Timestep ts)
	{
		auto view = m_Registry.view<SceneSequenceComponent>();
		if (view.begin() == view.end())
			return;

		EG_CPU_TIMING_SCOPED("Scene. Update Scene Sequences");

		const bool bDrawPaths = !bIsPlaying && bDrawMiscellaneous;
		for (auto entity : view)
		{
			auto& component = view.get<SceneSequenceComponent>(entity);
			component.OnUpdate(ts);

			if (bIsPlaying)
			{
				const auto& events = component.GetPlayer().GetFiredEvents();
				if (!events.empty())
				{
					auto& triggerData = m_SequenceEventsToTrigger.emplace_back();
					triggerData.EntityID = uint32_t(entity);
					triggerData.Events.reserve(events.size());
					for (const auto& event : events)
						triggerData.Events.push_back(event);
				}
			}

			// Skipped for the sequence we're currently looking through,
			// otherwise its own path would be drawn across the screen
			if (bDrawPaths && component.bDrawPathInEditor)
			{
				const bool bIsViewingThrough = bCameraOverrideValid && m_CameraOverrideOwner == component.Parent.GetGUID();
				if (!bIsViewingThrough)
				{
					const Transform base = component.bPlayInWorldSpace ? Transform{} : component.GetWorldTransform();
					DrawSequencePath(component.GetAsset(), base, component.GetTime());
				}
			}
		}
	}

	void Scene::UpdateNavMesh(Timestep ts)
	{
		if (!m_CurrentNavMesh)
			return;

		EG_CPU_TIMING_SCOPED("Scene. Update NavMesh");
		m_CurrentNavMesh->Update(ts);
	}

	void Scene::SyncCrowdAgents()
	{
		if (!m_CurrentNavMesh)
			return;

		auto view = m_Registry.view<NavigationCrowdAgentComponent>();
		for (auto& e : view)
		{
			Entity entity = Entity(e, this);
			auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
			glm::vec3 location;
			if (component.GetLocation(&location))
			{
				entity.SetWorldLocation(location);
			}
		}
	}

	void Scene::SetupOnAppAssemblyReloadedCallback()
	{
		ScriptEngine::AddOnAppAssemblyReloadedCallback(m_GUID, [this]()
		{
			// Update entity public fields
			auto view = GetAllEntitiesWith<ScriptComponent>();
			for (auto entityID : view)
			{
				Entity entity{ entityID, this };
				ScriptEngine::UpdateEntityPublicFields(entity);
			}
		});
	}

	void Scene::GatherLightsInfo()
	{
		EG_CPU_TIMING_SCOPED("Scene. Gather Lights Info");

		if (m_DirtyFlags.bPointLightsDirty)
		{
			auto view = m_Registry.view<PointLightComponent>();
			m_PointLights.clear();

			for (auto entity : view)
			{
				auto& component = view.get<PointLightComponent>(entity);
				if (component.DoesAffectWorld())
					m_PointLights.push_back(&component);
			}
		}

		if (m_DirtyFlags.bDirLightsDirty)
		{
			m_DirectionalLights.clear();

			auto view = m_Registry.view<DirectionalLightComponent>();
			for (auto entity : view)
			{
				auto& component = view.get<DirectionalLightComponent>(entity);
				if (component.DoesAffectWorld())
				{
					m_DirectionalLights.push_back(&component);
				}
			}
		}

		if (m_DirtyFlags.bSpotLightsDirty)
		{
			auto view = m_Registry.view<SpotLightComponent>();
			m_SpotLights.clear();

			for (auto entity : view)
			{
				auto& component = view.get<SpotLightComponent>(entity);
				if (component.DoesAffectWorld())
					m_SpotLights.push_back(&component);
			}
		}
	}

	void Scene::GatherSkeletalMeshes()
	{
		if (bForceSkeletalMeshUpdateNextFrame)
		{
			m_DirtyFlags.bSkeletalMeshesDirty = true;
			bForceSkeletalMeshUpdateNextFrame = false;
		}
		if (m_DirtyFlags.bSkeletalMeshesDirty)
		{
			// TODO: Maybe update the list in callbacks? 
			m_SkeletalMeshes.clear();
			auto view = m_Registry.view<SkeletalMeshComponent>();
			for (auto entity : view)
			{
				auto& mesh = view.get<SkeletalMeshComponent>(entity);
				if (mesh.GetMeshAsset() && mesh.IsVisible())
					m_SkeletalMeshes.push_back(&mesh);
			}
		}
	}

	void Scene::DestroyPendingEntities()
	{
		EG_CPU_TIMING_SCOPED("Scene. Destroy Pending Entities");

		// Remove entities when a new frame begins
		for (const auto& [enttID, bDestroyChildren] : m_EntitiesToDestroy)
		{
			Entity entity{ enttID, this };
			if (!entity.IsValid())
				continue;

			DestroyEntityImmediately_Internal(entity, bDestroyChildren);
		}
		m_EntitiesToDestroy.clear();
	}

	bool Scene::IsPendingDestroy(Entity entity) const
	{
		if (m_EntitiesToDestroy.empty() || !entity)
			return false;

		if (m_EntitiesToDestroy.find(entity.GetEnttID()) != m_EntitiesToDestroy.end())
			return true;

		// Scheduled through the parent that destroys its children
		for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
		{
			auto it = m_EntitiesToDestroy.find(parent.GetEnttID());
			if (it != m_EntitiesToDestroy.end() && it->second)
				return true;
		}
		return false;
	}

	void Scene::UpdateScripts(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("Scene. Run Scripts");

		UpdateUIMouse();

		const bool bDirtyBefore = m_DirtyFlags.bSkeletalMeshesDirty;
		m_DirtyFlags.bSkeletalMeshesDirty = false;

		// C# animation events
		{
			for (const auto& [entityID, events] : m_AnimationsToTrigger)
			{
				for (const auto& event : events)
				{
					Entity entity((entt::entity)entityID, this);
					entity.TriggerAnimationEvent(event.Name, event.Time);
				}
			}
			m_AnimationsToTrigger.clear();
		}

		// C# scene sequence events
		{
			for (const auto& data : m_SequenceEventsToTrigger)
			{
				Entity entity((entt::entity)data.EntityID, this);
				if (entity)
				{
					for (const auto& event : data.Events)
					{
						entity.TriggerSequenceEvent(event.Name, event.Time);
					}
				}
			}
			m_SequenceEventsToTrigger.clear();
		}

		// C++ scripts
		{
			auto view = m_Registry.view<NativeScriptComponent>();

			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				nsc.OnUpdate(Entity{ entity, this }, ts);
			}
		}

		// C# scripts
		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnUpdateEntity(e, ts);
			}
		}

		bForceSkeletalMeshUpdateNextFrame = m_DirtyFlags.bSkeletalMeshesDirty;
		m_DirtyFlags.bSkeletalMeshesDirty = bDirtyBefore;
	}

	void Scene::UpdateAnimations(Timestep ts, bool bUseBasePose, bool bApplyRootMotion)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update animations");

		m_AnimationsToTrigger.clear();
		if (bUseBasePose)
		{
			m_AnimationTransforms = AnimationSystem::UpdateBasePose(m_SkeletalMeshes, ts);
		}
		else
		{
			m_AnimationTransforms = AnimationSystem::Update(m_SkeletalMeshes, ts, bApplyRootMotion, &m_AnimationsToTrigger);
		}

		m_SystemsToUpdateAnims.clear();
		for (const auto& entityID : m_SkeletalParticles)
		{
			entt::entity entity = (entt::entity)entityID;
			EG_CORE_ASSERT(m_Registry.valid(entity) && m_Registry.all_of<ParticleSystemComponent>(entity));
			m_SystemsToUpdateAnims.push_back(&m_Registry.get<ParticleSystemComponent>(entity));
		}
		m_SkeletalParticlesAnimationTransforms = AnimationSystem::Update(m_SystemsToUpdateAnims, ts, &m_AnimationsToTrigger);
	}

	CameraComponent* Scene::FindOrCreateRuntimeCamera()
	{
		EG_CPU_TIMING_SCOPED("Scene. Find or Create runtime camera");

		CameraComponent* camera = nullptr;
		// Looking for Primary Camera
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			// Ignore engine provided camera
			if (m_RuntimeCameraHolder && entity == m_RuntimeCameraHolder->GetEnttID())
				continue;

			auto& cameraComponent = view.get<CameraComponent>(entity);

			if (cameraComponent.Primary)
			{
				camera = &cameraComponent;
				break;
			}
		}

		// If didn't find camera, create one
		if (!camera)
		{
			if (!m_RuntimeCameraHolder)
			{
				//If user provided primary-camera doesn't exist, provide one and set its transform to match editor camera's transform
				m_RuntimeCameraHolder = MakeScope<Entity>(CreateEntity("EAGLE:RuntimeCamera"));
				m_RuntimeCameraHolder->AddComponent<NativeScriptComponent>().Bind<CameraController>();
				m_RuntimeCameraHolder->RemoveComponent<EntitySceneNameComponent>(); // Delete it so it doesn't show up in the Scene hierarchy

				auto& cameraComp = m_RuntimeCameraHolder->AddComponent<CameraComponent>();
				cameraComp.Camera = EditorCamera;
				cameraComp.Primary = true;
				cameraComp.SetWorldTransform(EditorCamera.GetTransform());
			}
			camera = &m_RuntimeCameraHolder->GetComponent<CameraComponent>();
		}

		return camera;
	}

	void Scene::RenderScene(Timestep ts, bool bRuntime)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update Scene");

		GatherLightsInfo();

		if (m_DirtyFlags.bStaticMeshesDirty)
		{
			auto view = m_Registry.view<StaticMeshComponent>();
			m_Meshes.clear();
			for (auto entity : view)
			{
				auto& mesh = view.get<StaticMeshComponent>(entity);
				if (mesh.GetMeshAsset() && mesh.IsVisible())
					m_Meshes.push_back(&mesh);
			}
		}
		if (m_DirtyFlags.bSpritesDirty)
		{
			auto view = m_Registry.view<SpriteComponent>();
			m_Sprites.clear();
			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);
				if (sprite.IsVisible())
					m_Sprites.push_back(&sprite);
			}
		}
		if (m_DirtyFlags.bDecalsDirty)
		{
			auto view = m_Registry.view<DecalComponent>();
			m_Decals.clear();
			for (auto entity : view)
			{
				auto& decal = view.get<DecalComponent>(entity);
				if (decal.IsVisible())
					m_Decals.push_back(&decal);
			}
		}

		// If meshes are dirty, there's not point in updating specific transforms
		// Since meshes are going to be fully updated anyway
		if (m_DirtyFlags.bStaticMeshTransformsDirty && !m_DirtyFlags.bStaticMeshesDirty)
		{
			std::vector<const StaticMeshComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformStaticMeshes.size());
			for (auto entityID : m_DirtyTransformStaticMeshes)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<StaticMeshComponent>())
				{
					const auto& comp = e.GetComponent<StaticMeshComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateMeshesTransforms(dirtyComponents);
		}

		// Same for skeletals
		if (m_DirtyFlags.bSkeletalMeshTransformsDirty && !m_DirtyFlags.bSkeletalMeshesDirty)
		{
			std::vector<const SkeletalMeshComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformSkeletalMeshes.size());
			for (auto entityID : m_DirtyTransformSkeletalMeshes)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<SkeletalMeshComponent>())
				{
					const auto& comp = e.GetComponent<SkeletalMeshComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateSkeletalMeshesTransforms(dirtyComponents);
		}

		// Same for sprites
		if (m_DirtyFlags.bSpriteTransformsDirty && !m_DirtyFlags.bSpritesDirty)
		{
			std::vector<const SpriteComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformSprites.size());
			for (auto entityID : m_DirtyTransformSprites)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<SpriteComponent>())
				{
					const auto& comp = e.GetComponent<SpriteComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateSpritesTransforms(dirtyComponents);
		}

		// Same for decals
		if (m_DirtyFlags.bDecalTransformsDirty && !m_DirtyFlags.bDecalsDirty)
		{
			std::vector<const DecalComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformDecals.size());
			for (auto entityID : m_DirtyTransformDecals)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<DecalComponent>())
				{
					const auto& comp = e.GetComponent<DecalComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}
			m_SceneRenderer->UpdateDecalsTransforms(dirtyComponents);
		}

		// Same for texts
		if (m_DirtyFlags.bTextTransformsDirty && !m_DirtyFlags.bTextDirty)
		{
			std::vector<const TextComponent*> dirtyComponents;
			dirtyComponents.reserve(m_DirtyTransformTexts.size());
			for (auto entityID : m_DirtyTransformTexts)
			{
				Entity e{ (entt::entity)entityID , this };
				if (e.HasComponent<TextComponent>())
				{
					const auto& comp = e.GetComponent<TextComponent>();
					dirtyComponents.emplace_back(&comp);
				}
			}

			m_SceneRenderer->UpdateTextsTransforms(dirtyComponents);
		}

		// Gather billboards
		{
			auto view = m_Registry.view<BillboardComponent>();
			m_Billboards.clear();
			for (auto entity : view)
			{
				auto& billboard = view.get<BillboardComponent>(entity);
				if (billboard.bVisible)
					m_Billboards.push_back(&billboard);
			}
		}

		if (!GatherDebugDrawData())
			return;

		// Text components
		if (m_DirtyFlags.bTextDirty)
		{
			auto view = m_Registry.view<TextComponent>();
			m_Texts.clear();

			for (auto entity : view)
			{
				auto& text = view.get<TextComponent>(entity);
				if (text.IsVisible())
					m_Texts.push_back(&text);
			}
		}

		// Text2D components
		if (m_DirtyFlags.bText2DDirty)
		{
			auto view = m_Registry.view<Text2DComponent>();
			m_Texts2D.clear();

			for (auto entity : view)
			{
				auto& text = view.get<Text2DComponent>(entity);
				if (text.IsVisible())
					m_Texts2D.push_back(&text);
			}
		}

		// Image2D components
		if (m_DirtyFlags.bImage2DDirty)
		{
			auto view = m_Registry.view<Image2DComponent>();
			m_Images2D.clear();

			for (auto entity : view)
			{
				auto& image2D = view.get<Image2DComponent>(entity);
				if (image2D.IsVisible())
					m_Images2D.push_back(&image2D);
			}
		}

		// Particle Systems
		if (m_DirtyFlags.bRecreateParticleSystems)
		{
			m_DirtyTransformParticles.clear();
			m_SkeletalParticles.clear();

			m_SceneRenderer->RemoveAllParticleSystems();

			auto view = m_Registry.view<ParticleSystemComponent>();
			for (auto entity : view)
			{
				auto& ps = view.get<ParticleSystemComponent>(entity);
				if (ps.bAutospawn)
				{
					RegisterSkeletalParticleIfCan(ps);
					m_SceneRenderer->AddParticleSystem(ps);
				}
			}
		}
		else
		{
			if (!m_DirtyTransformParticles.empty())
			{
				static_assert(std::is_same<uint32_t, EntityIDType>::value);

				m_TempParticleSystems.clear();
				for (const auto& entityID : m_DirtyTransformParticles)
				{
					entt::entity entity = (entt::entity)entityID;
					if (m_Registry.valid(entity) && m_Registry.all_of<ParticleSystemComponent>(entity))
					{
						m_TempParticleSystems.emplace(&m_Registry.get<ParticleSystemComponent>(entity));
					}
				}

				m_SceneRenderer->UpdateParticleTransforms(m_TempParticleSystems);
				m_DirtyTransformParticles.clear();
			}
		}

		if (bCameraOverrideValid)
			m_OverrideCamera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);

		const Camera* camera = bCameraOverrideValid ? (Camera*)&m_OverrideCamera
			: (bIsPlaying ? (Camera*)&m_RuntimeCamera->Camera : (Camera*)&EditorCamera);
		if (m_DirtyFlags.bPointLightsDirty)
			m_SceneRenderer->SetPointLights(m_PointLights);
		if (m_DirtyFlags.bSpotLightsDirty)
			m_SceneRenderer->SetSpotLights(m_SpotLights);
		m_SceneRenderer->SetDirectionalLight(m_DirectionalLights.empty() ? nullptr : m_DirectionalLights[0]);
		if (m_DirtyFlags.bStaticMeshesDirty)
			m_SceneRenderer->SetMeshes(m_Meshes);
		if (m_DirtyFlags.bSkeletalMeshesDirty)
			m_SceneRenderer->SetSkeletalMeshes(m_SkeletalMeshes);
		if (m_DirtyFlags.bSpritesDirty)
			m_SceneRenderer->SetSprites(m_Sprites);
		m_SceneRenderer->SetDebugLines(m_DebugLinesToDraw);
		m_SceneRenderer->SetDebugLines2D(m_DebugLines2DToDraw);
		m_SceneRenderer->SetDebugTriangles(m_DebugTrianglesToDraw);
		m_SceneRenderer->SetBillboards(m_Billboards);
		if (m_DirtyFlags.bTextDirty)
			m_SceneRenderer->SetTexts(m_Texts);
		if (m_DirtyFlags.bText2DDirty)
			m_SceneRenderer->SetTexts2D(m_Texts2D);
		if (m_DirtyFlags.bImage2DDirty)
			m_SceneRenderer->SetImages2D(m_Images2D);
		if (const auto& uiScale = Project::GetProjectInfo().UIScale; uiScale != m_SceneRenderer->GetProjectUIScaleSettings())
			m_SceneRenderer->SetProjectUIScaleSettings(uiScale);
		m_SceneRenderer->SetIsRuntime(bIsPlaying);
		m_SceneRenderer->SetMeshesAnimationTransforms(std::move(m_AnimationTransforms));
		m_SceneRenderer->SetSkeletalParticleAnimationTransforms(std::move(m_SkeletalParticlesAnimationTransforms));
		m_SceneRenderer->SetGravity(m_RuntimePhysicsSettings.Gravity);
		if (m_DirtyFlags.bDecalsDirty)
			m_SceneRenderer->SetDecals(m_Decals);
		if (m_DirtyFlags.bOutlinesDirty)
			UpdateOutlines();

		if (!m_DebugCameras.empty())
		{
			const float aspect = float(m_ViewportWidth) / m_ViewportHeight;
			const uint32_t cameraEntityID = *m_DebugCameras.begin();
			Entity entity((entt::entity)cameraEntityID, this);

			EG_CORE_ASSERT(entity.HasComponent<CameraComponent>());
			if (entity.HasComponent<CameraComponent>())
			{
				const auto& camera = entity.GetComponent<CameraComponent>();
				m_SceneRenderer->SetDebugFrustumCulling(camera.GetWorldTransform().Location, camera.GetViewMatrix(), camera.Camera, aspect);
			}
		}

		const bool bDrawEditorHelpers = !bIsPlaying && bDrawMiscellaneous;
		m_SceneRenderer->SetGridEnabled(bDrawEditorHelpers);

		// Add engine billboards if necessary
		if (bDrawEditorHelpers)
		{
			Transform transform;
			transform.Scale3D = glm::vec3(0.25f);
			for (auto& point : m_PointLights)
			{
				transform.Location = point->GetWorldTransform().Location;
				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::PointLightIcon, (int)point->Parent.GetID());
			}
			for (auto& spot : m_SpotLights)
			{
				transform = spot->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);

				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::SpotLightIcon, (int)spot->Parent.GetID());
			}
			for (const auto& dir : m_DirectionalLights)
			{
				transform = dir->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);
				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::DirectionalLightIcon, (int)dir->Parent.GetID());
			}
		}

		const glm::mat4& viewMatrix = bCameraOverrideValid ? m_OverrideViewMatrix
			: (bIsPlaying ? m_RuntimeCamera->GetViewMatrix() : EditorCamera.GetViewMatrix());
		const glm::vec3& viewPos = bCameraOverrideValid ? m_OverrideViewPos
			: (bIsPlaying ? m_RuntimeCamera->GetWorldTransform().Location : EditorCamera.GetLocation());
		const glm::vec3& viewDir = bCameraOverrideValid ? m_OverrideViewDir
			: (bIsPlaying ? m_RuntimeCamera->GetForwardVector() : EditorCamera.GetForwardVector());
		{
			EG_CPU_TIMING_SCOPED("Scene. Render");
			m_SceneRenderer->Render(ts, camera, viewMatrix, viewPos, viewDir);
		}

		m_DirtyTransformStaticMeshes.clear();
		m_DirtyTransformSkeletalMeshes.clear();
		m_DirtyTransformSprites.clear();
		m_DirtyTransformTexts.clear();
		m_DirtyFlags.SetEverythingDirty(false);
	}

	// Inverse of the 2D UI placement (`UI2D_GetClipPosition` in utils.h): `pixel = anchor * viewportSize + uiScale * (offset + Rotate2D(position, rotation))`.
	static bool UIRectContainsPoint(glm::vec2 pixel, glm::vec2 viewportSize, float uiScale,
		glm::vec2 anchor, glm::vec2 offset, float rotationDegrees, glm::vec2 rectMin, glm::vec2 rectMax)
	{
		const glm::vec2 rotated = (pixel - anchor * viewportSize) / uiScale - offset;
		const float radians = glm::radians(rotationDegrees);
		const float c = glm::cos(radians);
		const float s = glm::sin(radians);
		const glm::vec2 position = glm::vec2(c * rotated.x - s * rotated.y, s * rotated.x + c * rotated.y);

		const glm::vec2 boxMin = glm::min(rectMin, rectMax);
		const glm::vec2 boxMax = glm::max(rectMin, rectMax);
		return glm::all(glm::greaterThanEqual(position, boxMin)) && glm::all(glm::lessThanEqual(position, boxMax));
	}

	Entity Scene::GetUIElementAt(glm::vec2 viewportPosition)
	{
		if (!m_SceneRenderer)
			return Entity::Null;

		const glm::vec2 viewportSize = glm::vec2(m_SceneRenderer->GetViewportSize());
		const float uiScale = m_SceneRenderer->GetUIScale();
		if (uiScale <= 0.f)
			return Entity::Null;

		// Same order as they're drawn: images, then texts. Each in the reversed order of their views to test topmost elements first

		auto texts = m_Registry.view<Text2DComponent>();
		for (auto it = texts.rbegin(); it != texts.rend(); ++it)
		{
			const auto& text = texts.get<Text2DComponent>(*it);
			const float fontSize = text.GetFontSize();
			if (!text.IsVisible() || !text.IsHitTestable() || fontSize <= 0.f)
				continue;

			const Ref<const TextLayout> layout = text.GetLayout();
			if (!layout)
				continue;

			const glm::vec2 size = (layout->BoxMax - layout->BoxMin) * fontSize;
			const glm::vec2 rectMin = -text.GetActualPivot() * size;
			if (UIRectContainsPoint(viewportPosition, viewportSize, uiScale, text.GetAnchor(), text.GetOffset(), text.GetRotation(), rectMin, rectMin + size))
				return Entity(*it, this);
		}

		auto images = m_Registry.view<Image2DComponent>();
		for (auto it = images.rbegin(); it != images.rend(); ++it)
		{
			const auto& image = images.get<Image2DComponent>(*it);
			if (!image.IsVisible() || !image.IsHitTestable())
				continue;

			const glm::vec2 size = image.GetSize();
			const glm::vec2 rectMin = -image.GetActualPivot() * size;
			if (UIRectContainsPoint(viewportPosition, viewportSize, uiScale, image.GetAnchor(), image.GetOffset(), image.GetRotation(), rectMin, rectMin + size))
				return Entity(*it, this);
		}

		return Entity::Null;
	}

	Entity Scene::GetHoveredUIElement() const
	{
		return m_UIHoveredEntity.IsNull() ? Entity::Null : GetEntityByGUID(m_UIHoveredEntity);
	}

	std::optional<glm::vec2> Scene::GetMousePositionInViewport() const
	{
		if (!bViewportHovered)
			return {};

		const glm::vec2 position = ImGuiLayer::GetMousePos() - ViewportBounds[0];
		const glm::vec2 size = ViewportBounds[1] - ViewportBounds[0];
		if (glm::any(glm::lessThan(position, glm::vec2(0.f))) || glm::any(glm::greaterThanEqual(position, size)))
			return {};
		return position;
	}

	void Scene::UpdateUIMouse()
	{
		EG_CPU_TIMING_SCOPED("Scene. Update UI Mouse");

		Entity hovered;
		if (const auto position = GetMousePositionInViewport())
			hovered = GetUIElementAt(*position);

		const GUID previouslyHovered = m_UIHoveredEntity;
		m_UIHoveredEntity = hovered ? hovered.GetGUID() : GUID(0, 0);

		if (previouslyHovered != m_UIHoveredEntity)
		{
			if (Entity entity = GetEntityByGUID(previouslyHovered))
			{
				ScriptEngine::OnUIMouseEvent(entity, UIMouseEvent::Exit);
			}
			if (GetEntityByGUID(m_UIHoveredEntity))
			{
				ScriptEngine::OnUIMouseEvent(hovered, UIMouseEvent::Enter);
			}
		}

		for (uint32_t i = 0; i < magic_enum::enum_count<Mouse>(); ++i)
		{
			const Mouse button = magic_enum::enum_value<Mouse>(i);;
			const uint32_t bit = 1u << i;
			const bool bPressed = Input::IsMouseButtonPressed(button);
			if (bPressed == ((m_UIPressedButtons & bit) != 0u))
				continue;
			m_UIPressedButtons ^= bit;

			if (bPressed)
			{
				m_UIPressedEntities[i] = m_UIHoveredEntity;
				if (hovered)
					ScriptEngine::OnUIMouseEvent(hovered, UIMouseEvent::Down, button);
				continue;
			}

			const GUID pressed = m_UIPressedEntities[i];
			m_UIPressedEntities[i] = GUID(0, 0);
			if (pressed.IsNull())
				continue;

			Entity entity = GetEntityByGUID(pressed);
			if (!entity)
				continue;
			
			ScriptEngine::OnUIMouseEvent(entity, UIMouseEvent::Up, button);
			if (entity == hovered) // Only if it's released over the same entity
				ScriptEngine::OnUIMouseEvent(entity, UIMouseEvent::Click, button);
		}
	}

	void Scene::OnRuntimeStart()
	{
		bIsPlaying = true;

		ScriptEngine::Reset();

		// Update Audio
		{
			auto view = m_Registry.view<AudioComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				auto& comp = e.GetComponent<AudioComponent>();
				if (comp.bAutoplay)
					comp.Play();
			}
		}
		
		// Start auto-playing cutscenes
		{
			auto view = m_Registry.view<SceneSequenceComponent>();
			for (auto entity : view)
			{
				auto& comp = view.get<SceneSequenceComponent>(entity);
				if (comp.bAutoPlay)
					comp.Play();
			}
		}

		// Update C# scripts
		{
			auto view = m_Registry.view<ScriptComponent>();

			// Instantiate all entities
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::InstantiateEntityClass(e);
			}

			// When all entities were instantiated,
			// call 'OnCreate'
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnCreateEntity(e);
			}
		}

		m_PhysicsScene->SetGravity(m_RuntimePhysicsSettings.Gravity);
		m_PhysicsScene->SetUpdateRate(m_RuntimePhysicsSettings.UpdateRate);
		m_PhysicsScene->SetDebugOnPlay(m_RuntimePhysicsSettings.bDebugOnPlay);
		m_PhysicsScene->SetDebugType(m_RuntimePhysicsSettings.DebugType);
		m_PhysicsScene->StartDebugging();
	}

	void Scene::OnRuntimeStop()
	{
		{
			auto view = m_Registry.view<NativeScriptComponent>();
			for (auto& e : view)
			{
				auto& nsc = m_Registry.get<NativeScriptComponent>(e);
				nsc.Destroy();
			}
		}
		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto& e : view)
			{
				auto& sc = m_Registry.get<ScriptComponent>(e);
				ScriptEngine::OnDestroyEntity(Entity{e, this});
			}

			// Destroy script instances
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::RemoveEntityScript(e);
			}

			// Release user C# components (they're runtime-only and never copied back to the editor scene)
			m_Registry.clear<ScriptUserComponents>();
		}

		{
			auto view = m_Registry.view<SceneSequenceComponent>();
			for (auto entity : view)
				view.get<SceneSequenceComponent>(entity).Stop();
		}

		bIsPlaying = false;
		m_PhysicsScene->StopDebugging();
		m_PhysicsScene->Reset();
	}

	Entity Scene::CreateFromEntityAsset(const Ref<AssetEntity>& asset, bool bCopyGUID)
	{
		Entity createdEntity = CreateFromEntity(*asset->GetEntity().get(), bCopyGUID);
		createdEntity.SetName(Utils::AsString(asset->GetPath().stem()));
		if (createdEntity.HasComponent<EntityAssetComponent>())
		{
			createdEntity.GetComponent<EntityAssetComponent>().AssetGUID = asset->GetGUID();
		}
		else
		{
			createdEntity.AddComponent<EntityAssetComponent>().AssetGUID = asset->GetGUID();
		}

		return createdEntity;
	}

	void Scene::ResolveEntityReload(const GUID& oldGUID, const GUID& newGUID)
	{
		auto view = m_Registry.view<ScriptComponent>();
		for (auto& e : view)
		{
			ScriptComponent& script = m_Registry.get<ScriptComponent>(e);
			const size_t fieldsCount = script.PublicFields.size();

			bool bUpdated = false;
			for (size_t i = 0; i < fieldsCount; ++i)
			{
				auto& dstField = script.PublicFields[i];
				if (dstField.Type == FieldType::Entity)
				{
					if (dstField.GetStoredValue<GUID>() == oldGUID)
					{
						dstField.SetStoredValue(newGUID);
						bUpdated = true;
					}
				}
			}
		}

		// Note: childrent are not resolved at all.
		// Currently, there's no way to detect whether a child came from the asset or it was created manually by the user
	}

	Entity Scene::ReloadEntityCreatedFromAsset(Entity entity)
	{
		if (!entity.HasComponent<EntityAssetComponent>())
			return Entity::Null;

		const auto& component = entity.GetComponent<EntityAssetComponent>();
		Ref<Asset> asset;
		if (!AssetManager::Get(component.AssetGUID, &asset))
		{
			EG_CORE_ERROR("Failed to reload entity '{}' from its entity asset. The asset wasn't found", entity.GetName());
			return Entity::Null;
		}

		auto assetEntity = Cast<AssetEntity>(asset);
		if (!assetEntity)
		{
			EG_CORE_ERROR("Failed to reload entity '{}' from its entity asset. The asset isn't a entity asset: {}", entity.GetName(), Utils::AsString(asset->GetPath()));
			return Entity::Null;
		}

		std::string oldName = entity.GetName();
		Transform transform = entity.GetWorldTransform();
		GUID oldGUID = entity.GetGUID();
		DestroyEntity(entity, true);

		Entity newEntity = CreateFromEntityAsset(assetEntity);
		newEntity.SetWorldTransform(transform);
		newEntity.SetName(oldName);

		ResolveEntityReload(oldGUID, newEntity.GetGUID());

		return newEntity;
	}

	void Scene::ReloadEntitiesCreatedFromAsset(const Ref<AssetEntity>& asset)
	{
		auto view = m_Registry.view<EntityAssetComponent>();
		const GUID assetID = asset->GetGUID();
		const Entity assetEntity = *asset->GetEntity().get();

		for (auto& e : view)
		{
			const auto& id = m_Registry.get<EntityAssetComponent>(e).AssetGUID;
			if (id == assetID)
			{
				ReloadEntityCreatedFromAsset(Entity(e, this));
			}
		}
	}

	void Scene::OnEventRuntime(Event& e)
	{
		//Running Scripts
		{
			auto view = m_Registry.view<NativeScriptComponent>();

			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				nsc.OnEvent(Entity{ entity, this }, e);
			}
		}

		// C# scripts
		{
			// Create a single event object for all `OnEvent` invocations
			std::array params = e.GetData();
			void* eventObject = ScriptEngine::Construct(e.GetCSharpCtor(), true, params.data());

			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				ScriptEngine::OnEventEntity(e, eventObject);
			}
		}
	}

	void Scene::OnEventEditor(Event& e)
	{
		EditorCamera.OnEvent(e);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		EditorCamera.SetViewportSize(width, height);
		if (m_SceneRenderer)
			m_SceneRenderer->SetViewportSize({ width, height });
	}

	void Scene::DestroyScripts()
	{
		if (bIsPlaying)
		{
			//Calling on destroy for C++ scripts
			{
				auto view = m_Registry.view<NativeScriptComponent>();
				for (auto entity : view)
				{
					auto& nsc = view.get<NativeScriptComponent>(entity);
					nsc.Destroy();
				}
			}

			//Calling on destroy for C# scripts
			if (bIsPlaying)
			{
				auto view = m_Registry.view<ScriptComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };
					ScriptEngine::OnDestroyEntity(e);
				}
			}
		}
	}

	void Scene::UpdateOutlines()
	{
		ankerl::unordered_dense::map<uint32_t, OutlineSettings> outlines;
		outlines.reserve(m_EntityOutlines.size());
		for (const auto& [entityID, layers] : m_EntityOutlines)
		{
			// The highest layer has priority
			for (auto it = layers.rbegin(); it != layers.rend(); ++it)
			{
				if (it->has_value())
				{
					outlines.emplace(entityID, **it);
					break;
				}
			}
		}

		m_SceneRenderer->SetOutlines(std::move(outlines));
		m_DirtyFlags.bOutlinesDirty = false;
	}

	void Scene::SetEntityOutline(const Entity& entity, const OutlineSettings& settings, OutlineLayer layer)
	{
		if (!entity || entity.GetScene() != this)
			return;

		auto& outline = m_EntityOutlines[entity.GetID()][(size_t)layer];
		if (outline == settings)
			return;

		outline = settings;
		m_DirtyFlags.bOutlinesDirty = true;
	}

	void Scene::RemoveEntityOutline(const Entity& entity, OutlineLayer layer)
	{
		if (!entity || entity.GetScene() != this)
			return;

		auto it = m_EntityOutlines.find(entity.GetID());
		if (it == m_EntityOutlines.end())
			return;

		auto& layers = it->second;
		auto& outline = layers[(size_t)layer];
		if (!outline)
			return;

		outline.reset();
		if (std::none_of(layers.begin(), layers.end(), [](const auto& value) { return value.has_value(); }))
			m_EntityOutlines.erase(it);

		m_DirtyFlags.bOutlinesDirty = true;
	}

	void Scene::ClearEntityOutlines(OutlineLayer layer)
	{
		const size_t layerIndex = (size_t)layer;
		std::vector<uint32_t> emptyEntries;
		bool bChanged = false;
		for (auto& [entityID, layers] : m_EntityOutlines)
		{
			if (!layers[layerIndex])
				continue;

			layers[layerIndex].reset();
			bChanged = true;
			if (std::none_of(layers.begin(), layers.end(), [](const auto& value) { return value.has_value(); }))
				emptyEntries.push_back(entityID);
		}

		for (const uint32_t entityID : emptyEntries)
			m_EntityOutlines.erase(entityID);

		if (bChanged)
			m_DirtyFlags.bOutlinesDirty = true;
	}

	bool Scene::HasEntityOutline(const Entity& entity, OutlineLayer layer) const
	{
		return GetEntityOutline(entity, layer) != nullptr;
	}

	const OutlineSettings* Scene::GetEntityOutline(const Entity& entity, OutlineLayer layer) const
	{
		if (!entity || entity.GetScene() != this)
			return nullptr;

		auto it = m_EntityOutlines.find(entity.GetID());
		if (it == m_EntityOutlines.end())
			return nullptr;

		const auto& outline = it->second[(size_t)layer];
		return outline ? &outline.value() : nullptr;
	}

	void Scene::ClearScene()
	{
		DestroyScripts();

		m_PhysicsScene->Reset();
		m_CurrentNavMesh.reset();
		m_SpawnedSounds.clear();
		m_EntityOutlines.clear();
		m_Registry.clear();
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();

		for (auto entityID : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entityID);
			if (cameraComponent.Primary)
			{
				return Entity{entityID, this};
			}
		}

		return Entity::Null;
	}

	SceneSoundData Scene::SpawnSound2D(const Ref<AssetAudio>& audio, const SoundSettings& settings)
	{
		SceneSoundData result;
		result.Sound = Sound2D::Create(audio->GetAudio(), settings);
		m_SpawnedSounds[result.ID] = result.Sound;
		return result;
	}

	SceneSoundData Scene::SpawnSound3D(const Ref<AssetAudio>& audio, const glm::vec3& position, RollOffModel rollOff, const SoundSettings& settings)
	{
		SceneSoundData result;
		result.Sound = Sound3D::Create(audio->GetAudio(), position, rollOff, settings);
		m_SpawnedSounds[result.ID] = result.Sound;
		return result;
	}

	Ref<Sound> Scene::GetSpawnedSound(GUID id) const
	{
		auto it = m_SpawnedSounds.find(id);
		return it != m_SpawnedSounds.end() ? it->second : nullptr;
	}

	Entity Scene::GetEntityByGUID(const GUID& guid) const
	{
		if (guid.IsNull())
			return Entity::Null;
		auto it = m_AliveEntities.find(guid);
		return it != m_AliveEntities.end() ? it->second : Entity::Null;
	}

	const Ref<PhysicsActor>& Scene::GetPhysicsActor(const Entity& entity) const
	{
		if (m_PhysicsScene)
			return m_PhysicsScene->GetPhysicsActor(entity);

		return s_NullPhysicsActor;
	}

	Ref<PhysicsActor>& Scene::GetPhysicsActor(const Entity& entity)
	{
		if (m_PhysicsScene)
			return m_PhysicsScene->GetPhysicsActor(entity);

		return s_NullPhysicsActor;
	}

	void Scene::SetGUID(GUID guid)
	{
		ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_GUID);

		m_GUID = guid;
		SetupOnAppAssemblyReloadedCallback();
	}

	void Scene::SetGravity(const glm::vec3& gravity)
	{
		m_RuntimePhysicsSettings.Gravity = gravity;
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetGravity(m_RuntimePhysicsSettings.Gravity);
	}

	void Scene::SetPhysicsUpdateRate(uint32_t updateRate)
	{
		m_RuntimePhysicsSettings.UpdateRate = glm::clamp(updateRate, PhysicsSettings::s_MinUpdateRate, PhysicsSettings::s_MaxUpdateRate);
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetUpdateRate(m_RuntimePhysicsSettings.UpdateRate);
	}

	void Scene::SetPhysicsDebugOnPlay(bool bEnable)
	{
		m_RuntimePhysicsSettings.bDebugOnPlay = bEnable;
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetDebugOnPlay(bEnable);
	}

	void Scene::SetPhysicsDebugType(DebugType type)
	{
		m_RuntimePhysicsSettings.DebugType = type;
		if (m_PhysicsScene && m_PhysicsScene == m_RuntimePhysicsScene)
			m_PhysicsScene->SetDebugType(type);
	}

	void Scene::InvalidateCollisionGroups(uint32_t validMasks)
	{
		Utils::InvalidateCollisionGroups<CharacterControllerComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<BoxColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<SphereColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<CapsuleColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<MeshColliderComponent>(m_Registry, validMasks);
	}

	void Scene::SetCameraOverride(const GUID& owner, const CameraOverrideData& data)
	{
		m_CameraOverrideOwner = owner;
		bCameraOverrideValid = true;

		m_OverrideCamera.SetProjectionMode(CameraProjectionMode::Perspective);
		m_OverrideCamera.SetPerspective(data.VerticalFOVRadians, data.NearClip, data.FarClip);
		m_OverrideCamera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);

		const glm::mat4 R = data.WorldTransform.Rotation.ToMat4();
		const glm::mat4 T = glm::translate(glm::mat4(1.0f), data.WorldTransform.Location);
		m_OverrideViewMatrix = glm::inverse(T * R);

		m_OverrideViewPos = data.WorldTransform.Location;
		m_OverrideViewDir = Math::GetForwardVector(data.WorldTransform.Rotation);
		m_OverrideViewUp = Math::GetUpVector(data.WorldTransform.Rotation);
	}

	void Scene::ClearCameraOverride(const GUID& owner)
	{
		if (!bCameraOverrideValid)
			return;

		if (m_CameraOverrideOwner != owner)
			return;

		ClearCameraOverride();
	}

	void Scene::ClearCameraOverride()
	{
		bCameraOverrideValid = false;
		m_CameraOverrideOwner = GUID(0, 0);
	}

	void Scene::SetPostProcessOverride(const GUID& owner, const PostProcessOverride& postProcess)
	{
		if (m_SceneRenderer)
			m_SceneRenderer->SetPostProcessOverride(owner, postProcess);
	}

	void Scene::ClearPostProcessOverride(const GUID& owner)
	{
		if (m_SceneRenderer)
			m_SceneRenderer->ClearPostProcessOverride(owner);
	}

	bool Scene::IsPostProcessOverridden() const
	{
		return m_SceneRenderer && m_SceneRenderer->IsPostProcessOverridden();
	}

	CameraComponent* Scene::GetRuntimeCamera()
	{
		return m_RuntimeCamera;
	}

	void Scene::AddParticleSystem(const ParticleSystemComponent& system)
	{
		RegisterSkeletalParticleIfCan(system);
		if (m_SceneRenderer)
			m_SceneRenderer->AddParticleSystem(system);
	}

	void Scene::RemoveParticleSystem(const ParticleSystemComponent& system, bool bForceImmediateRemoval)
	{
		m_SkeletalParticles.erase(system.Parent.GetID());
		if (m_SceneRenderer)
			m_SceneRenderer->RemoveParticleSystem(system, bForceImmediateRemoval);
	}

	void Scene::UpdateParticleSystem(const ParticleSystemComponent& system)
	{
		m_DirtyTransformParticles.erase(system.Parent.GetID()); // No need to update transform separately
		m_SkeletalParticles.erase(system.Parent.GetID());
		RegisterSkeletalParticleIfCan(system);
		if (m_SceneRenderer)
			m_SceneRenderer->UpdateParticleSystem(system);
	}

	void Scene::OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& sm = entity.GetComponent<StaticMeshComponent>().GetMeshAsset();
		if (sm && sm->GetMesh()->IsValid())
		{
			m_DirtyFlags.bStaticMeshesDirty = true;
			m_DirtyFlags.bStaticMeshTransformsDirty = true;
		}
	}

	void Scene::OnSkeletalMeshComponentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& sm = entity.GetComponent<SkeletalMeshComponent>().GetMeshAsset();
		if (sm && sm->GetMesh()->IsValid())
		{
			m_DirtyFlags.bSkeletalMeshesDirty = true;
			m_DirtyFlags.bSkeletalMeshTransformsDirty = true;
		}
	}

	void Scene::OnSpriteComponentAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bSpritesDirty = true;
		m_DirtyFlags.bSpriteTransformsDirty = true;
	}

	void Scene::OnDecalComponentAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bDecalsDirty = true;
		m_DirtyFlags.bDecalTransformsDirty = true;
	}

	void Scene::OnPointLightAdded(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bPointLightsDirty = true;
	}

	void Scene::OnPointLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<PointLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bPointLightsDirty = true;
		}
		m_PointLightsDebugRadii.erase(entity.GetID());
	}

	void Scene::OnSpotLightAdded(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bSpotLightsDirty = true;
	}

	void Scene::OnSpotLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<SpotLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bSpotLightsDirty = true;
		}
		m_SpotLightsDebugRadii.erase(entity.GetID());
	}

	void Scene::OnTextAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bTextDirty = true;
		m_DirtyFlags.bTextTransformsDirty = true;
	}

	void Scene::OnText2DAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bText2DDirty = true;
	}

	void Scene::OnImage2DAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bImage2DDirty = true;
	}

	void Scene::OnParticleSystemAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& comp = entity.GetComponent<ParticleSystemComponent>();
		if (comp.bAutospawn)
			comp.Spawn();
	}

	void Scene::OnParticleSystemRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<ParticleSystemComponent>().Destroy();
	}

	void Scene::OnNavMeshRemoved(entt::registry& r, entt::entity e)
	{
		if (!m_CurrentNavMesh)
			return;

		Entity entity(e, this);
		const auto& navMesh = entity.GetComponent<NavigationMeshComponent>().GetNavMesh();
		if (m_CurrentNavMesh == navMesh)
		{
			BuildNavMesh(nullptr);
		}
	}

	void Scene::OnBoxColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<BoxColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnSphereColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<SphereColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnCapsuleColliderRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		entity.GetComponent<CapsuleColliderComponent>().SetIsObstacle(false);
	}

	void Scene::OnCrowdAgentAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
		component.CreateAgent(entity.GetWorldLocation());
	}

	void Scene::OnCrowdAgentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& component = entity.GetComponent<NavigationCrowdAgentComponent>();
		component.RemoveAgent();
	}

	void Scene::OnCameraRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_DebugCameras.erase(entity.GetID());
	}

	void Scene::OnReverbRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_ReverbDebugBoxes.erase(entity.GetID());
	}

	void Scene::OnDirectionalLightAdded(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<DirectionalLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bDirLightsDirty = true;
		}
	}

	void Scene::OnDirectionalLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_DirLightsDebugDirection.erase(entity.GetID());
		auto& light = entity.GetComponent<DirectionalLightComponent>();
		if (light.DoesAffectWorld())
		{
			m_DirtyFlags.bDirLightsDirty = true;
		}
	}

	void Scene::OnCharacterControllerRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		m_CharacterControllerDebug.erase(entity.GetID());
	}

	void Scene::ConnectSignals()
	{
		m_Registry.on_destroy<StaticMeshComponent>().connect<&Scene::OnStaticMeshComponentRemoved>(*this);
		m_Registry.on_destroy<SkeletalMeshComponent>().connect<&Scene::OnSkeletalMeshComponentRemoved>(*this);
		m_Registry.on_construct<SpriteComponent>().connect<&Scene::OnSpriteComponentAddedRemoved>(*this);
		m_Registry.on_destroy<SpriteComponent>().connect<&Scene::OnSpriteComponentAddedRemoved>(*this);
		m_Registry.on_construct<DecalComponent>().connect<&Scene::OnDecalComponentAddedRemoved>(*this);
		m_Registry.on_destroy<DecalComponent>().connect<&Scene::OnDecalComponentAddedRemoved>(*this);
		m_Registry.on_construct<PointLightComponent>().connect<&Scene::OnPointLightAdded>(*this);
		m_Registry.on_destroy<PointLightComponent>().connect<&Scene::OnPointLightRemoved>(*this);
		m_Registry.on_construct<SpotLightComponent>().connect<&Scene::OnSpotLightAdded>(*this);
		m_Registry.on_destroy<SpotLightComponent>().connect<&Scene::OnSpotLightRemoved>(*this);
		m_Registry.on_construct<TextComponent>().connect<&Scene::OnTextAddedRemoved>(*this);
		m_Registry.on_destroy<TextComponent>().connect<&Scene::OnTextAddedRemoved>(*this);
		m_Registry.on_construct<Text2DComponent>().connect<&Scene::OnText2DAddedRemoved>(*this);
		m_Registry.on_destroy<Text2DComponent>().connect<&Scene::OnText2DAddedRemoved>(*this);
		m_Registry.on_construct<Image2DComponent>().connect<&Scene::OnImage2DAddedRemoved>(*this);
		m_Registry.on_destroy<Image2DComponent>().connect<&Scene::OnImage2DAddedRemoved>(*this);
		m_Registry.on_construct<ParticleSystemComponent>().connect<&Scene::OnParticleSystemAdded>(*this);
		m_Registry.on_destroy<ParticleSystemComponent>().connect<&Scene::OnParticleSystemRemoved>(*this);
		m_Registry.on_destroy<NavigationMeshComponent>().connect<&Scene::OnNavMeshRemoved>(*this);
		m_Registry.on_destroy<BoxColliderComponent>().connect<&Scene::OnBoxColliderRemoved>(*this);
		m_Registry.on_destroy<SphereColliderComponent>().connect<&Scene::OnSphereColliderRemoved>(*this);
		m_Registry.on_destroy<CapsuleColliderComponent>().connect<&Scene::OnCapsuleColliderRemoved>(*this);
		m_Registry.on_construct<NavigationCrowdAgentComponent>().connect<&Scene::OnCrowdAgentAdded>(*this);
		m_Registry.on_destroy<NavigationCrowdAgentComponent>().connect<&Scene::OnCrowdAgentRemoved>(*this);
		m_Registry.on_destroy<CameraComponent>().connect<&Scene::OnCameraRemoved>(*this);
		m_Registry.on_destroy<ReverbComponent>().connect<&Scene::OnReverbRemoved>(*this);
		m_Registry.on_construct<DirectionalLightComponent>().connect<&Scene::OnDirectionalLightAdded>(*this);
		m_Registry.on_destroy<DirectionalLightComponent>().connect<&Scene::OnDirectionalLightRemoved>(*this);
		m_Registry.on_destroy<CharacterControllerComponent>().connect<&Scene::OnCharacterControllerRemoved>(*this);
	}

	void Scene::RegisterSkeletalParticleIfCan(const ParticleSystemComponent& system)
	{
		const auto& asset = system.GetAsset();
		if (!asset)
			return;

		const auto& emitters = asset->GetEmitters();
		for (const auto& emitter : emitters)
		{
			if (emitter.IsSkeletalMeshUsed())
			{
				m_SkeletalParticles.emplace(system.Parent.GetID());
				break;
			}
		}
	}

	void Scene::CopyComponents(Entity source, Entity dest)
	{
		EntityCopyComponent<EntityAssetComponent>(source, dest);
		EntityCopyComponent<TagComponent>(source, dest);
		EntityCopyComponent<NativeScriptComponent>(source, dest);
		EntityCopyComponent<ScriptComponent>(source, dest);
		EntityCopyComponent<PointLightComponent>(source, dest);
		EntityCopyComponent<DirectionalLightComponent>(source, dest);
		EntityCopyComponent<SpotLightComponent>(source, dest);
		EntityCopyComponent<SpriteComponent>(source, dest);
		EntityCopyComponent<StaticMeshComponent>(source, dest);
		EntityCopyComponent<SkeletalMeshComponent>(source, dest);
		EntityCopyComponent<BillboardComponent>(source, dest);
		EntityCopyComponent<CameraComponent>(source, dest);
		EntityCopyComponent<CharacterControllerComponent>(source, dest);
		EntityCopyComponent<RigidBodyComponent>(source, dest);
		EntityCopyComponent<BoxColliderComponent>(source, dest);
		EntityCopyComponent<SphereColliderComponent>(source, dest);
		EntityCopyComponent<CapsuleColliderComponent>(source, dest);
		EntityCopyComponent<MeshColliderComponent>(source, dest);
		EntityCopyComponent<AudioComponent>(source, dest);
		EntityCopyComponent<ReverbComponent>(source, dest);
		EntityCopyComponent<TextComponent>(source, dest);
		EntityCopyComponent<Text2DComponent>(source, dest);
		EntityCopyComponent<Image2DComponent>(source, dest);
		EntityCopyComponent<ParticleSystemComponent>(source, dest);
		EntityCopyComponent<SceneSequenceComponent>(source, dest);
		EntityCopyComponent<DecalComponent>(source, dest);
		EntityCopyComponent<NavigationCrowdAgentComponent>(source, dest);
		EntityCopyComponent<NavigationMeshComponent>(source, dest);

		if (bIsPlaying)
		{
			if (dest.HasComponent<AudioComponent>())
			{
				auto& comp = dest.GetComponent<AudioComponent>();
				if (comp.bAutoplay)
					comp.Play();
			}

			if (dest.HasComponent<ScriptComponent>())
			{
				if (!dest.GetComponent<ScriptComponent>().ModuleName.empty() && ScriptEngine::InstantiateEntityClass(dest))
				{
					ScriptEngine::OnCreateEntity(dest);
				}
			}
		}
	}
}
