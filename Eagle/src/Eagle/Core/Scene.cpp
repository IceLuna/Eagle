#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Physics/PhysicsUtils.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/AINavigation/AINavigationDebugDraw.h"

namespace Eagle
{
	namespace Utils
	{
		constexpr uint32_t s_SphereLinesCount = 24;
		constexpr float s_2PI = 2.f * glm::pi<float>();

		void DrawSphere(std::vector<RendererLine>& buffer, const glm::vec3& center, const glm::vec3& color, float radius)
		{
			for (uint32_t i = 0; i < s_SphereLinesCount; ++i)
			{
				const float angle1 = (float(i) / s_SphereLinesCount) * s_2PI;
				const float angle2 = (float(i + 1) / s_SphereLinesCount) * s_2PI;
				const float cosAngle1 = glm::cos(angle1);
				const float cosAngle2 = glm::cos(angle2);
				const float sinAngle1 = glm::sin(angle1);
				const float sinAngle2 = glm::sin(angle2);
				constexpr float cos45 = 0.707106f;
				constexpr float cosMinus45 = -0.707106f;

				auto& line = buffer.emplace_back();
				line.Start.Location = center + radius * glm::vec3(cosAngle1, sinAngle1, 0.f);
				line.End.Location = center + radius * glm::vec3(cosAngle2, sinAngle2, 0.f);
				line.Start.Color = color;
				line.End.Color = color;

				auto& line2 = buffer.emplace_back();
				line2.Start.Location = center + radius * glm::vec3(0.f, cosAngle1, sinAngle1);
				line2.End.Location = center + radius * glm::vec3(0.f, cosAngle2, sinAngle2);
				line2.Start.Color = color;
				line2.End.Color = color;

				auto& line3 = buffer.emplace_back();
				line3.Start.Location = center + radius * glm::vec3(cos45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line3.End.Location = center + radius * glm::vec3(cos45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line3.Start.Color = color;
				line3.End.Color = color;

				auto& line4 = buffer.emplace_back();
				line4.Start.Location = center + radius * glm::vec3(cosMinus45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line4.End.Location = center + radius * glm::vec3(cosMinus45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line4.Start.Color = color;
				line4.End.Color = color;
			}
		}

		void DrawBones_Internal(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& baseTransform)
		{
			glm::mat4 tr;
			if (auto it = currentPose.Bones.find(node.Name); it != currentPose.Bones.end())
			{
				const auto& bone = it->second;
				const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
				tr = baseTransform * boneTransform;
			}
			else
				tr = baseTransform * node.Transformation;

			const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(tr).Location;
			for (const auto& child : node.Children)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = parentLocation;
				line.Start.Color = glm::vec3(0, 1, 0);
				
				const glm::vec3 location = Math::DecomposeTransformMatrix(tr * child.Transformation).Location;
				line.End.Location = location;
				line.End.Color = glm::vec3(1, 1, 0);
			}

			for (const auto& child : node.Children)
				DrawBones_Internal(buffer, child, currentPose, tr);
		}

		void DrawRagdollBones_Internal(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& worldTransform, const glm::mat4& baseTransform)
		{
			glm::mat4 tr;
			if (auto it = currentPose.Bones.find(node.Name); it != currentPose.Bones.end())
			{
				const auto& bone = it->second;
				tr = Math::ToTransformMatrix(bone); // Ragdoll is already a global transform
			}
			else
				tr = baseTransform * node.Transformation;

			const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(worldTransform * tr).Location;
			for (const auto& child : node.Children)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = parentLocation;
				line.Start.Color = glm::vec3(0, 1, 0);

				const glm::vec3 location = Math::DecomposeTransformMatrix(worldTransform * (tr * child.Transformation)).Location;
				line.End.Location = location;
				line.End.Color = glm::vec3(1, 1, 0);
			}

			for (const auto& child : node.Children)
				DrawRagdollBones_Internal(buffer, child, currentPose, worldTransform, tr);
		}

		void DrawBones(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, bool bRagdoll, const glm::mat4& baseTransform)
		{
			if (bRagdoll)
				DrawRagdollBones_Internal(buffer, node, currentPose, baseTransform, baseTransform);
			else
				DrawBones_Internal(buffer, node, currentPose, baseTransform);
		}
	
		void DrawBox(std::vector<RendererLine>& buffer, AABB aabb, const Transform& worldTr, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			const glm::mat4 trMat = Math::ToTransformMatrix(worldTr);
			const size_t startIdx = buffer.size();

			for (glm::length_t i = 0; i < aabb.Min.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;

				line.End.Location = aabb.Min;
				line.End.Location[i] = aabb.Max[i];
			}

			for (glm::length_t i = 0; i < aabb.Max.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Max;

				line.End.Location = aabb.Max;
				line.End.Location[i] = aabb.Min[i];
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}
		
			for (size_t i = startIdx; i < buffer.size(); ++i)
			{
				auto& line = buffer[i];
				line.Start.Location = trMat * glm::vec4(line.Start.Location, 1.f);
				line.End.Location = trMat * glm::vec4(line.End.Location, 1.f);
			}
		}

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
	static void SceneAddAndCopyComponent(Scene* destScene, entt::registry& destRegistry, entt::registry& srcRegistry, const std::unordered_map<entt::entity, entt::entity>& createdEntities)
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
		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(PhysicsSettings());
		m_PhysicsScene = m_RuntimePhysicsScene;
	}

	Scene::Scene(const std::string& debugName, const Ref<SceneRenderer>& sceneRenderer, bool bRuntime)
		: m_DebugName(debugName)
	{
		if (sceneRenderer)
		{
			glm::uvec2 size = sceneRenderer->GetViewportSize();
			m_SceneRenderer = sceneRenderer;
			m_ViewportWidth = size.x;
			m_ViewportHeight = size.y;
		}
		else
		{
			m_SceneRenderer = MakeRef<SceneRenderer>(glm::uvec2{ m_ViewportWidth, m_ViewportHeight });
		}
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);
		ConnectSignals();

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

	Scene::Scene(const Ref<Scene>& other, const std::string& debugName)
	: bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
	, m_RuntimePhysicsScene(other->m_RuntimePhysicsScene)
	, m_PhysicsScene(other->m_RuntimePhysicsScene)
	, m_EditorCamera(other->m_EditorCamera)
	, m_EntitiesToDestroy(other->m_EntitiesToDestroy)
	, m_ViewportWidth(other->m_ViewportWidth)
	, m_ViewportHeight(other->m_ViewportHeight)
	, m_DebugName(debugName)
	, m_RuntimePhysicsSettings(other->m_RuntimePhysicsSettings)
	, bDrawMiscellaneous(other->bDrawMiscellaneous)
	, bDrawNavMesh(other->bDrawNavMesh)
	, bDrawBones(other->bDrawBones)
	, m_Cubemap(other->m_Cubemap)
	, m_Sky(other->m_Sky)
	, m_SkyboxIntensity(other->m_SkyboxIntensity)
	, m_bSkyboxEnabled(other->m_bSkyboxEnabled)
	, m_bRenderSkybox(other->m_bRenderSkybox)
	, m_bUseSkyAsBackground(other->m_bUseSkyAsBackground)
	{
		// Reuse renderer so that we don't allocate additional GPU resources
		m_SceneRenderer = other->m_SceneRenderer;
		SetUseSkyAsBackground(m_bUseSkyAsBackground);
		SetRenderSkybox(m_bRenderSkybox);
		SetSkyboxEnabled(m_bSkyboxEnabled);
		SetSkyboxIntensity(m_SkyboxIntensity);
		SetSkybox(m_Sky);

		std::unordered_map<entt::entity, entt::entity> createdEntities;
		createdEntities.reserve(other->m_Registry.size());
		for (auto entt : other->m_Registry.view<TransformComponent>())
		{
			const std::string& sceneName = other->m_Registry.get<EntitySceneNameComponent>(entt).Name;
			const GUID& guid  = other->m_Registry.get<IDComponent>(entt).ID;
			Entity entity = CreateEntityWithGUID(guid, sceneName);
			createdEntities[entt] = entity.GetEnttID();
		}

		SceneAddAndCopyComponent<TransformComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<OwnershipComponent>(this, m_Registry, other->m_Registry, createdEntities);

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
		m_DirtyFlags.SetEverythingDirty(true);
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
		m_Registry.clear();
		m_SpawnedSounds.clear();
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
		entity.AddComponent<OwnershipComponent>();

		m_AliveEntities[guid] = entity;

		return entity;
	}

	Entity Scene::CreateFromEntity(const Entity& source)
	{
		Entity result = CreateEntity(source.GetComponent<EntitySceneNameComponent>().Name);
		EntityCopyComponent<TransformComponent>(source, result); //Copying TransformComponent to set childrens transform correctly

		// Recreating Ownership component
		const auto& srcChildren = source.GetChildren();
		for (auto& child : srcChildren)
		{
			Entity myChild = CreateFromEntity(child);
			myChild.SetParent(result);
		}

		CopyComponents(source, result);

		if (bIsPlaying && result.HasComponent<ScriptComponent>())
		{
			if (ScriptEngine::ModuleExists(result.GetComponent<ScriptComponent>().ModuleName))
			{
				ScriptEngine::InstantiateEntityClass(result);
				ScriptEngine::OnCreateEntity(result);
			}
		}

		return result;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity)
			return;

		if (bIsPlaying)
		{
			if (entity.HasComponent<NativeScriptComponent>())
			{
				auto& nsc = entity.GetComponent<NativeScriptComponent>();
				nsc.Destroy();
			}

			if (entity.HasComponent<ScriptComponent>())
				if (ScriptEngine::ModuleExists(entity.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnDestroyEntity(entity);
		}

		m_EntitiesToDestroy.push_back(entity);

		// EG_CORE_TRACE("Destroyed Entity: {}", entity.GetComponent<EntitySceneNameComponent>().Name);
	}

	void Scene::OnUpdate(Timestep ts, bool bRender, bool bForceAnimationsUpdate)
	{
		if (bIsPlaying)
			OnUpdateRuntime(ts, bRender, bForceAnimationsUpdate);
		else
			OnUpdateEditor(ts, bRender, bForceAnimationsUpdate);
	}

	void Scene::OpenScene(const Ref<AssetScene>& sceneAsset, bool bReuseCurrentSceneRenderer, bool bRuntime)
	{
		auto func = [path = sceneAsset ? sceneAsset->GetPath() : "", bReuseCurrentSceneRenderer, bRuntime]()
		{
			ComponentsNotificationSystem::Reset();
			ScriptEngine::Reset();
			RenderManager::Wait();
			Ref<Scene> scene = MakeRef<Scene>(path.u8string(), (bReuseCurrentSceneRenderer && s_CurrentScene) ? s_CurrentScene->GetSceneRenderer() : nullptr, bRuntime);
			if (Application::Get().IsGame())
			{
				YAML::Node sceneNode;
				if (AssetManager::GetRuntimeAssetNode(path, &sceneNode))
				{
					AssetManager::ResetGameAssets();
					AssetManager::ResetRuntimeAsset();
					SceneSerializer serializer(scene);
					serializer.Deserialize(sceneNode);
					OnSceneOpened(scene);
				}
				else
					EG_CORE_ERROR("Failed to open the scene: {}", path.u8string());
			}
			else
			{
				if (std::filesystem::exists(path))
				{
					SceneSerializer serializer(scene);
					serializer.Deserialize(path);
				}
				AssetManager::ResetRuntimeAsset();
				OnSceneOpened(scene);
			}
		};

		Application::Get().CallNextFrame(func);
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

		auto agentComponents = m_Registry.view<NavigationCrowdAgentComponent>();

		// Go through all agents and delete them
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

		m_EditorCamera.OnUpdate(ts, bCanUpdateEditorCamera);

		GatherSkeletalMeshes();
		UpdateAnimations(ts, !bForceAnimationsUpdate, false);
		UpdateNavMesh(ts);
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
		AudioEngine::SetListenerData(m_RuntimeCamera->GetWorldTransform().Location, m_RuntimeCamera->GetForwardVector(), m_RuntimeCamera->GetUpVector());
		if (bRender)
			RenderScene(ts, true);
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

	void Scene::CollectParticleSystems(const std::unordered_set<uint32_t>& entities)
	{
		static_assert(std::is_same<uint32_t, EntityIDType>::value);

		m_TempParticleSystems.clear();
		for (const auto& entityID : entities)
		{
			entt::entity entity = (entt::entity)entityID;
			if (m_Registry.valid(entity) && m_Registry.all_of<ParticleSystemComponent>(entity))
			{
				m_TempParticleSystems.emplace(&m_Registry.get<ParticleSystemComponent>(entity));
			}
		}
	}

	void Scene::GatherLightsInfo()
	{
		EG_CPU_TIMING_SCOPED("Scene. Gather Lights Info");

		if (m_DirtyFlags.bPointLightsDirty)
		{
			auto view = m_Registry.view<PointLightComponent>();
			m_PointLights.clear();
			m_PointLightsDebugRadii.clear();
			m_PointLightsDebugRadiiDirty = true;

			for (auto entity : view)
			{
				auto& component = view.get<PointLightComponent>(entity);
				if (component.VisualizeRadiusEnabled())
					m_PointLightsDebugRadii.emplace(&component);
				if (component.DoesAffectWorld())
					m_PointLights.push_back(&component);
			}
		}

		m_DirectionalLight = nullptr;
		{
			auto view = m_Registry.view<DirectionalLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<DirectionalLightComponent>(entity);
				if (component.DoesAffectWorld())
				{
					m_DirectionalLight = &component;
					break;
				}
			}
		}

		if (m_DirtyFlags.bSpotLightsDirty)
		{
			auto view = m_Registry.view<SpotLightComponent>();
			m_SpotLights.clear();
			m_SpotLightsDebugRadii.clear();
			m_SpotLightsDebugRadiiDirty = true;

			for (auto entity : view)
			{
				auto& component = view.get<SpotLightComponent>(entity);
				if (component.VisualizeDistanceEnabled())
					m_SpotLightsDebugRadii.emplace(&component);
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
				if (mesh.GetMeshAsset())
					m_SkeletalMeshes.push_back(&mesh);
			}
		}
	}

	void Scene::DestroyPendingEntities()
	{
		EG_CPU_TIMING_SCOPED("Scene. Destroy Pending Entities");

		//Remove entities when a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
		{
			ScriptEngine::RemoveEntityScript(entity);
			auto& actor = entity.GetPhysicsActor();
			if (actor)
				m_PhysicsScene->RemovePhysicsActor(actor);

			auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
			std::vector<Entity> children = ownershipComponent.Children; // Copy
			Entity myParent = ownershipComponent.EntityParent;
			entity.SetParent(Entity::Null);

			for (size_t i = 0; i < children.size(); ++i)
				children[i].SetParent(myParent);

			m_AliveEntities.erase(entity.GetGUID());
			m_Registry.destroy(entity.GetEnttID());
		}
		m_EntitiesToDestroy.clear();
	}

	void Scene::UpdateScripts(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("Scene. Run Scripts");

		const bool bDirtyBefore = m_DirtyFlags.bSkeletalMeshesDirty;
		m_DirtyFlags.bSkeletalMeshesDirty = false;

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
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnUpdateEntity(e, ts);
			}
		}

		bForceSkeletalMeshUpdateNextFrame = m_DirtyFlags.bSkeletalMeshesDirty;
		m_DirtyFlags.bSkeletalMeshesDirty = bDirtyBefore;
	}

	void Scene::UpdateAnimations(Timestep ts, bool bUseBasePose, bool bApplyRootMotion)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update animations");
		if (bUseBasePose)
		{
			m_AnimationTransforms = AnimationSystem::UpdateBasePose(m_SkeletalMeshes, ts);
		}
		else
		{
			m_AnimationTransforms = AnimationSystem::Update(m_SkeletalMeshes, ts, bApplyRootMotion);
		}

		std::vector<ParticleSystemComponent*> systems;
		systems.reserve(m_SkeletalParticles.size());
		for (const auto& entityID : m_SkeletalParticles)
		{
			entt::entity entity = (entt::entity)entityID;
			EG_CORE_ASSERT(m_Registry.valid(entity) && m_Registry.all_of<ParticleSystemComponent>(entity));
			systems.push_back(&m_Registry.get<ParticleSystemComponent>(entity));
		}
		m_SkeletalParticlesAnimationTransforms = AnimationSystem::Update(systems, ts);
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
				cameraComp.Camera = m_EditorCamera;
				cameraComp.Primary = true;
				cameraComp.SetWorldTransform(m_EditorCamera.GetTransform());
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
				m_Decals.push_back(&decal);
			}
		}

		// If meshes are dirty, there's not point in updating specific transforms
		// Since meshes are going to be fully updated anyway
		if (m_DirtyFlags.bStaticMeshTransformsDirty && !m_DirtyFlags.bStaticMeshesDirty)
			m_SceneRenderer->UpdateMeshesTransforms(m_DirtyTransformStaticMeshes);

		// Same for skeletals
		if (m_DirtyFlags.bSkeletalMeshTransformsDirty && !m_DirtyFlags.bSkeletalMeshesDirty)
			m_SceneRenderer->UpdateSkeletalMeshesTransforms(m_DirtyTransformSkeletalMeshes);

		// Same for sprites
		if (m_DirtyFlags.bSpriteTransformsDirty && !m_DirtyFlags.bSpritesDirty)
			m_SceneRenderer->UpdateSpritesTransforms(m_DirtyTransformSprites);

		// Same for decals
		if (m_DirtyFlags.bDecalTransformsDirty && !m_DirtyFlags.bDecalsDirty)
			m_SceneRenderer->UpdateDecalsTransforms(m_DirtyTransformDecals);

		// Same for texts
		if (m_DirtyFlags.bTextTransformsDirty && !m_DirtyFlags.bTextDirty)
			m_SceneRenderer->UpdateTextsTransforms(m_DirtyTransformTexts);

		// Gather billboards
		{
			auto view = m_Registry.view<BillboardComponent>();
			m_Billboards.clear();
			for (auto entity : view)
			{
				auto& billboard = view.get<BillboardComponent>(entity);
				m_Billboards.push_back(&billboard);
			}
		}

		// Gather Debug data
		{
			// Debug point lights attenuation radii
			if (m_PointLightsDebugRadiiDirty)
			{
				m_DebugPointLines.clear();
				for (auto& light : m_PointLightsDebugRadii)
				{
					const glm::vec3& center = light->GetWorldTransform().Location;
					const float radius = light->GetRadius();
					Utils::DrawSphere(m_DebugPointLines, center, glm::vec3(0, 1, 0), radius);
				}
				m_PointLightsDebugRadiiDirty = false;
			}

			// Debug spot lights attenuation distance
			if (m_SpotLightsDebugRadiiDirty)
			{
				m_DebugSpotLines.clear();
				for (auto& light : m_SpotLightsDebugRadii)
				{
					const glm::vec3& location = light->GetWorldTransform().Location;
					const float distance = light->GetDistance();
					const glm::vec3 center = location + light->GetForwardVector() * distance;
					const glm::quat quat = light->GetWorldTransform().Rotation.GetQuat();
					const float innerRadius = distance * glm::tan(glm::radians(light->GetInnerCutOffAngle()));
					const float outerRadius = distance * glm::tan(glm::radians(light->GetOuterCutOffAngle()));

					for (uint32_t i = 0; i < Utils::s_SphereLinesCount; ++i)
					{
						const float angle1 = (float(i) / Utils::s_SphereLinesCount) * Utils::s_2PI;
						const float angle2 = (float(i + 1) / Utils::s_SphereLinesCount) * Utils::s_2PI;
						const float cosAngle1 = glm::cos(angle1);
						const float cosAngle2 = glm::cos(angle2);
						const float sinAngle1 = glm::sin(angle1);
						const float sinAngle2 = glm::sin(angle2);

						auto& innerCircleLine = m_DebugSpotLines.emplace_back();
						innerCircleLine.Start.Location = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
						innerCircleLine.End.Location = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));

						auto& toInnerLine = m_DebugSpotLines.emplace_back();
						toInnerLine.Start.Location = location;
						toInnerLine.End.Location = innerCircleLine.Start.Location;

						auto& outerCircleLine = m_DebugSpotLines.emplace_back();
						outerCircleLine.Start.Location = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
						outerCircleLine.End.Location = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));
						outerCircleLine.Start.Color = glm::vec3(0.75, 0.75f, 0.f);
						outerCircleLine.End.Color = glm::vec3(0.75, 0.75f, 0.f);

						auto& toOuterLine = m_DebugSpotLines.emplace_back();
						toOuterLine.Start.Location = location;
						toOuterLine.End.Location = outerCircleLine.Start.Location;
						toOuterLine.Start.Color = glm::vec3(0.75, 0.75f, 0.f);
						toOuterLine.End.Color = glm::vec3(0.75, 0.75f, 0.f);
					}
				}
				m_SpotLightsDebugRadiiDirty = false;
			}

			// Debug spot lights attenuation distance
			if (m_ReverbDebugBoxesDirty)
			{
				m_DebugReverbLines.clear();
				for (auto& reverb : m_ReverbDebugBoxes)
				{
					const glm::vec3& center = reverb->GetReverb()->GetPosition();
					Utils::DrawSphere(m_DebugReverbLines, center, glm::vec3(0, 1, 0), reverb->GetMinDistance());
					Utils::DrawSphere(m_DebugReverbLines, center, glm::vec3(1, 0, 0), reverb->GetMaxDistance());
				}
				m_ReverbDebugBoxesDirty = false;
			}

			auto& rb = m_PhysicsScene->GetRenderBuffer();
			const uint32_t debugCollisionsLinesSize = rb.getNbLines();

			constexpr size_t linesPerDirLight = 3ull;
			size_t debugDirLightLinesCount = 0;
			auto dirLightsView = m_Registry.view<DirectionalLightComponent>();
			debugDirLightLinesCount = dirLightsView.size() * linesPerDirLight;

			m_DebugLinesToDraw.clear();
			m_DebugLinesToDraw.reserve(debugCollisionsLinesSize + m_DebugPointLines.size() + m_DebugSpotLines.size() + m_DebugReverbLines.size() + m_UserDebugLines.size() + debugDirLightLinesCount);
			m_DebugLinesToDraw = m_DebugPointLines;
			m_DebugLinesToDraw.insert(m_DebugLinesToDraw.end(), m_DebugSpotLines.begin(), m_DebugSpotLines.end());
			m_DebugLinesToDraw.insert(m_DebugLinesToDraw.end(), m_DebugReverbLines.begin(), m_DebugReverbLines.end());
			m_DebugTrianglesToDraw.clear();

			for (auto entity : dirLightsView)
			{
				auto& dir = dirLightsView.get<DirectionalLightComponent>(entity);
				if (dir.bVisualizeDirection)
				{
					const glm::vec3& location = dir.GetWorldTransform().Location;
					const glm::vec3 forward = dir.GetForwardVector();
					const glm::vec3 endLocation = location + forward * 0.2f;

					DrawArrow(location, endLocation, dir.GetUpVector());
				}
			}

			// Debug collisions
			if (debugCollisionsLinesSize)
			{
				const physx::PxDebugLine* physicsLines = rb.getLines();

				for (uint32_t i = 0; i < debugCollisionsLinesSize; ++i)
				{
					auto& line = physicsLines[i];
					RendererLine rendererLine;
					rendererLine.Start.Location = PhysXUtils::FromPhysXVector(line.pos0);
					rendererLine.End.Location = PhysXUtils::FromPhysXVector(line.pos1);
					m_DebugLinesToDraw.push_back(rendererLine);
				}
			}

			// Bones
			if (bDrawBones)
			{
				auto view = m_Registry.view<SkeletalMeshComponent>();
				for (auto entity : view)
				{
					auto& skeletal = view.get<SkeletalMeshComponent>(entity);
					if (auto& asset = skeletal.GetMeshAsset())
						Utils::DrawBones(m_DebugLinesToDraw, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, skeletal.LastPose, skeletal.IsRagdollEnabled(), Math::ToTransformMatrix(skeletal.GetWorldTransform()));
				}
			}

			// AABBs
			if (true)
			{
				if (false)
				{
					auto view = m_Registry.view<SkeletalMeshComponent>();
					for (auto entity : view)
					{
						const auto& skeletal = view.get<SkeletalMeshComponent>(entity);
						if (const auto& asset = skeletal.GetMeshAsset())
							Utils::DrawBox(m_DebugLinesToDraw, asset->GetMesh()->GetAABB(), skeletal.GetWorldTransform());
					}
				}
				if (false)
				{
					auto view = m_Registry.view<StaticMeshComponent>();
					for (auto entity : view)
					{
						const auto& staticMesh = view.get<StaticMeshComponent>(entity);
						if (const auto& asset = staticMesh.GetMeshAsset())
							Utils::DrawBox(m_DebugLinesToDraw, asset->GetMesh()->GetAABB(), staticMesh.GetWorldTransform());
					}
				}
				if (false)
				{
					auto view = m_Registry.view<ParticleSystemComponent>();
					for (auto entity : view)
					{
						const auto& system = view.get<ParticleSystemComponent>(entity);
						const auto& asset = system.GetAsset();
						if (!asset)
							continue;

						for (const auto& emitter : asset->GetEmitters())
						{
							Utils::DrawBox(m_DebugLinesToDraw, emitter.VisibilityAABB, system.GetWorldTransform());
						}
					}
				}
				if (bDrawNavMesh)
				{
					auto view = m_Registry.view<NavigationMeshComponent>();
					for (auto entity : view)
					{
						const auto& navigation = view.get<NavigationMeshComponent>(entity);
						const auto& settings = navigation.GetSettings();
						const auto& aabb = settings.AABB;
						Utils::DrawBox(m_DebugLinesToDraw, aabb, navigation.GetWorldTransform(), glm::vec3(1, 0, 0));

						AINavigation::DebugDraw debugDraw(m_DebugLinesToDraw, m_DebugTrianglesToDraw);
						navigation.GetNavMeshDebugDraw(&debugDraw);
					}
				}
				if (false)
				{
					if (m_CurrentNavMesh)
					{
						const glm::vec3 start = { 2.1703f, 0.f, -2.2557f };
						const glm::vec3 end = { -2.1192f, 0.f, 2.3706 };
						std::vector<glm::vec3> path = m_CurrentNavMesh->FindSmoothPath(start, end);
						if (!path.empty())
						{
							const size_t count = path.size();
							glm::vec3 startPos = path.front();
							for (size_t i = 1; i < count; i++)
							{
								RendererLine line;
								line.Start.Location = startPos;
								line.End.Location = path[i];
								DrawDebugLine(line);
								startPos = path[i];
							}
						}
					}
				}
				if (false)
				{
					auto view = m_Registry.view<BoxColliderComponent>();
					for (auto entity : view)
					{
						const auto& component = view.get<BoxColliderComponent>(entity);
						const auto& shape = component.GetShape();
						std::vector<glm::vec3> vertices;
						std::vector<uint32_t> indices;

						Transform pose = shape->GetLocalTransform();
						Transform tBody = shape->GetGlobalTransform();
						glm::mat4 t = Math::ToTransformMatrix(tBody + pose);

						shape->GetGeometry(vertices, indices);

						for (uint32_t i = 0; i < indices.size(); i += 3)
						{
							auto& line1 = m_DebugLinesToDraw.emplace_back();
							line1.Start.Location = t * glm::vec4(vertices[indices[i]], 1.f);
							line1.End.Location = t * glm::vec4(vertices[indices[i + 1]], 1.f);

							auto& line2 = m_DebugLinesToDraw.emplace_back();
							line2.Start.Location = t * glm::vec4(vertices[indices[i]], 1.f);
							line2.End.Location = t * glm::vec4(vertices[indices[i + 2]], 1.f);

							auto& line3 = m_DebugLinesToDraw.emplace_back();
							line3.Start.Location = t * glm::vec4(vertices[indices[i + 1]], 1.f);
							line3.End.Location = t * glm::vec4(vertices[indices[i + 2]], 1.f);
						}
					}
				}
				if (false)
				{
					auto view = m_Registry.view<CapsuleColliderComponent>();
					for (auto entity : view)
					{
						const auto& component = view.get<CapsuleColliderComponent>(entity);
						const auto& shape = component.GetShape();
						const glm::vec3& location = component.GetWorldTransform().Location;

						const glm::vec3& scale = shape->GetColliderScale();
						const float& radius = scale.x;
						const float& height = scale.y;

						const glm::vec3 halfExtent1 = glm::vec3(radius, 0.f, radius);
						const glm::vec3 halfExtent2 = glm::vec3(radius, height, radius);
						AABB aabb(location - halfExtent1, location + halfExtent2);

						Utils::DrawBox(m_DebugLinesToDraw, aabb, {}, glm::vec3(0, 0, 1));
					}
				}
				for (const auto& [aabb, transform] :m_UserAABBs)
				{
					Utils::DrawBox(m_DebugLinesToDraw, aabb, transform);
				}
				m_UserAABBs.clear();
			}

			// Append user provided lines
			m_DebugLinesToDraw.insert(m_DebugLinesToDraw.end(), m_UserDebugLines.begin(), m_UserDebugLines.end());
			m_DebugTrianglesToDraw.insert(m_DebugTrianglesToDraw.end(), m_UserDebugTriangles.begin(), m_UserDebugTriangles.end());
			// User provided lines need to provided each frame. So clear it.
			m_UserDebugLines.clear();
			m_UserDebugTriangles.clear();
		}

		// Text components
		if (m_DirtyFlags.bTextDirty)
		{
			auto view = m_Registry.view<TextComponent>();
			m_Texts.clear();

			for (auto entity : view)
			{
				auto& text = view.get<TextComponent>(entity);
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
			m_ParticlesToAdd.clear();
			m_ParticlesToRemove.clear();
			m_ParticlesToUpdate.clear();
			m_TempParticleSystems.clear();
			m_SkeletalParticles.clear();

			m_SceneRenderer->RemoveAllParticleSystems();

			auto view = m_Registry.view<ParticleSystemComponent>();
			for (auto entity : view)
			{
				auto& ps = view.get<ParticleSystemComponent>(entity);
				if (ps.bAutospawn)
				{
					m_TempParticleSystems.insert(&ps);
					RegisterSkeletalParticleIfCan(&ps);
				}
			}
			m_SceneRenderer->AddParticleSystems(m_TempParticleSystems);
			m_TempParticleSystems.clear();
		}
		else
		{
			if (m_DirtyTransformParticles.size())
			{
				CollectParticleSystems(m_DirtyTransformParticles);
				m_SceneRenderer->UpdateParticleTransforms(m_TempParticleSystems);
				m_DirtyTransformParticles.clear();
			}
			if (m_ParticlesToAdd.size())
			{
				CollectParticleSystems(m_ParticlesToAdd);
				m_SceneRenderer->AddParticleSystems(m_TempParticleSystems);
				m_ParticlesToAdd.clear();
			}
			if (m_ParticlesToRemove.size())
			{
				m_SceneRenderer->RemoveParticleSystems(m_ParticlesToRemove);
				m_ParticlesToRemove.clear();
			}
			if (m_ParticlesToUpdate.size())
			{
				CollectParticleSystems(m_ParticlesToUpdate);
				m_SceneRenderer->UpdateParticleSystems(m_TempParticleSystems);
				m_ParticlesToUpdate.clear();
			}
		}

		const Camera* camera = bIsPlaying ? (Camera*)&m_RuntimeCamera->Camera : (Camera*)&m_EditorCamera;
		m_SceneRenderer->SetPointLights(m_PointLights, m_DirtyFlags.bPointLightsDirty);
		m_SceneRenderer->SetSpotLights(m_SpotLights, m_DirtyFlags.bSpotLightsDirty);
		m_SceneRenderer->SetDirectionalLight(m_DirectionalLight);
		m_SceneRenderer->SetMeshes(m_Meshes, m_DirtyFlags.bStaticMeshesDirty);
		m_SceneRenderer->SetSkeletalMeshes(m_SkeletalMeshes, m_DirtyFlags.bSkeletalMeshesDirty);
		m_SceneRenderer->SetSprites(m_Sprites, m_DirtyFlags.bSpritesDirty);
		m_SceneRenderer->SetDebugLines(m_DebugLinesToDraw);
		m_SceneRenderer->SetDebugTriangles(m_DebugTrianglesToDraw);
		m_SceneRenderer->SetBillboards(m_Billboards);
		m_SceneRenderer->SetTexts(m_Texts, m_DirtyFlags.bTextDirty);
		m_SceneRenderer->SetTexts2D(m_Texts2D, m_DirtyFlags.bText2DDirty);
		m_SceneRenderer->SetImages2D(m_Images2D, m_DirtyFlags.bImage2DDirty);
		m_SceneRenderer->SetIsRuntime(bIsPlaying);
		m_SceneRenderer->SetMeshesAnimationTransforms(std::move(m_AnimationTransforms));
		m_SceneRenderer->SetSkeletalParticleAnimationTransforms(std::move(m_SkeletalParticlesAnimationTransforms));
		m_SceneRenderer->SetGravity(m_RuntimePhysicsSettings.Gravity);
		m_SceneRenderer->SetDecals(m_Decals, m_DirtyFlags.bDecalsDirty);

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
			if (m_DirectionalLight)
			{
				transform = m_DirectionalLight->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);
				m_SceneRenderer->AddAdditionalBillboard(transform, Texture2D::DirectionalLightIcon, (int)m_DirectionalLight->Parent.GetID());
			}
		}

		const glm::mat4& viewMatrix = bIsPlaying ? m_RuntimeCamera->GetViewMatrix() : m_EditorCamera.GetViewMatrix();
		const glm::vec3& viewPos = bIsPlaying ? m_RuntimeCamera->GetWorldTransform().Location : m_EditorCamera.GetLocation();
		const glm::vec3& viewDir = bIsPlaying ? m_RuntimeCamera->GetForwardVector() : m_EditorCamera.GetForwardVector();
		{
			EG_CPU_TIMING_SCOPED("Scene. Render");
			m_SceneRenderer->Render(camera, viewMatrix, viewPos, viewDir);
		}

		m_DirtyTransformStaticMeshes.clear();
		m_DirtyTransformSkeletalMeshes.clear();
		m_DirtyTransformSprites.clear();
		m_DirtyTransformTexts.clear();
		m_DirtyFlags.SetEverythingDirty(false);
	}

	void Scene::OnRuntimeStart()
	{
		EG_CORE_TRACE("Runtime started");

		bIsPlaying = true;

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
		
		// Update C# scripts
		{
			auto view = m_Registry.view<ScriptComponent>();

			// Instantiate all entities
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::InstantiateEntityClass(e);
			}

			// When all entities were instantiated,
			// call 'OnCreate'
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
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
		EG_CORE_TRACE("Runtime stopped");

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
				if (ScriptEngine::ModuleExists(sc.ModuleName))
					ScriptEngine::OnDestroyEntity(Entity{e, this});
			}

			// Destroy script instances
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::RemoveEntityScript(e);
			}
		}

		bIsPlaying = false;
		m_PhysicsScene->StopDebugging();
		m_PhysicsScene->Reset();
	}

	Entity Scene::CreateFromEntityAsset(const Ref<AssetEntity>& asset)
	{
		Entity createdEntity = CreateFromEntity(*asset->GetEntity().get());
		createdEntity.AddComponent<EntityAssetComponent>().AssetGUID = asset->GetGUID();

		return createdEntity;
	}

	void Scene::ReloadEntitiesCreatedFromAsset(const Ref<AssetEntity>& asset)
	{
		auto view = m_Registry.view<EntityAssetComponent>();
		const GUID assetID = asset->GetGUID();
		const Entity assetEntity = *asset->GetEntity().get();

		// TODO: do we need to recreate ownership component?
		for (auto& e : view)
		{
			const auto& id = m_Registry.get<EntityAssetComponent>(e).AssetGUID;
			if (id == assetID)
			{
				Entity thisEntity = Entity(e, this);
				CopyComponents(assetEntity, thisEntity);
				thisEntity.SetWorldTransform(thisEntity.GetWorldTransform()); // Forcing components to update
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
			std::array params = e.GetData();
			void* eventObject = ScriptEngine::Construct(e.GetCSharpCtor(), true, params.data());

			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnEventEntity(e, eventObject);
			}
		}
	}

	void Scene::OnEventEditor(Event& e)
	{
		m_EditorCamera.OnEvent(e);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_EditorCamera.SetViewportSize(width, height);
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
					if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
						ScriptEngine::OnDestroyEntity(e);
				}
			}
		}
	}

	void Scene::ClearScene()
	{
		DestroyScripts();

		m_PhysicsScene->Reset();
		m_Registry.clear();
		m_CurrentNavMesh.reset();
		m_SpawnedSounds.clear();
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

	void Scene::DrawArrow(const glm::vec3& start, const glm::vec3& end, const glm::vec3& up)
	{
		const glm::vec3 dir = glm::normalize(end - start);

		RendererLine line;
		line.Start.Location = start;
		line.End.Location = end;
		m_UserDebugLines.push_back(line);

		line.Start.Location = start + dir * 0.15f + up * 0.05f;
		m_UserDebugLines.push_back(line);

		line.Start.Location = start + dir * 0.15f + up * -0.05f;
		m_UserDebugLines.push_back(line);
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
		Utils::InvalidateCollisionGroups<BoxColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<SphereColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<CapsuleColliderComponent>(m_Registry, validMasks);
		Utils::InvalidateCollisionGroups<MeshColliderComponent>(m_Registry, validMasks);
	}

	CameraComponent* Scene::GetRuntimeCamera()
	{
		return m_RuntimeCamera;
	}

	void Scene::AddParticleSystem(const ParticleSystemComponent* system)
	{
		m_ParticlesToAdd.emplace(system->Parent.GetID());
		RegisterSkeletalParticleIfCan(system);
	}

	void Scene::RemoveParticleSystem(const ParticleSystemComponent* system)
	{
		m_ParticlesToRemove.emplace(system->GetSystemID());
		m_SkeletalParticles.erase(system->Parent.GetID());
	}

	void Scene::UpdateParticleSystem(const ParticleSystemComponent* system)
	{
		m_ParticlesToUpdate.emplace(system->Parent.GetID());
		m_DirtyTransformParticles.erase(system->Parent.GetID()); // No need to update transform separately

		m_SkeletalParticles.erase(system->Parent.GetID());
		RegisterSkeletalParticleIfCan(system);
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
	}

	void Scene::RegisterSkeletalParticleIfCan(const ParticleSystemComponent* system)
	{
		const auto& emitters = system->GetAsset()->GetEmitters();
		for (const auto& emitter : emitters)
		{
			if (emitter.IsSkeletalMeshUsed())
			{
				m_SkeletalParticles.emplace(system->Parent.GetID());
				break;
			}
		}
	}

	void Scene::CopyComponents(Entity source, Entity dest)
	{
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
		EntityCopyComponent<DecalComponent>(source, dest);
		EntityCopyComponent<NavigationCrowdAgentComponent>(source, dest);
		EntityCopyComponent<NavigationMeshComponent>(source, dest);
	}
}
