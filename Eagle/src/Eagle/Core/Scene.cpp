#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/Renderer.h"
#include "Eagle/Renderer/Framebuffer.h"
#include "Eagle/Renderer/Renderer2D.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Audio/AudioEngine.h"

namespace Eagle
{
	Ref<Scene> Scene::s_CurrentScene;
	static DirectionalLightComponent defaultDirectionalLight;

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
	{
		defaultDirectionalLight.LightColor = glm::vec4{ glm::vec3(0.0f), 1.f };
		defaultDirectionalLight.Ambient = glm::vec3(0.0f);
		SetSceneGamma(m_SceneGamma);
		SetSceneExposure(m_SceneExposure);
		m_PhysicsScene = MakeRef<PhysicsScene>(PhysicsSettings());
	}

	Scene::Scene(const Ref<Scene>& other) 
	: m_Cubemap(other->m_Cubemap)
	, bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
	, m_PhysicsScene(other->m_PhysicsScene)
	, m_EditorCamera(other->m_EditorCamera)
	, m_EntitiesToDestroy(other->m_EntitiesToDestroy)
	, m_ViewportWidth(other->m_ViewportWidth)
	, m_ViewportHeight(other->m_ViewportHeight)
	, m_SceneGamma(other->m_SceneGamma)
	, m_SceneExposure(other->m_SceneExposure)
	, bEnableSkybox(other->bEnableSkybox)
	{
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
		SceneAddAndCopyComponent<CameraComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<RigidBodyComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<BoxColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<SphereColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<CapsuleColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<MeshColliderComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<AudioComponent>(this, m_Registry, other->m_Registry, createdEntities);
		SceneAddAndCopyComponent<ReverbComponent>(this, m_Registry, other->m_Registry, createdEntities);
	}

	Scene::~Scene()
	{
		ClearScene();
		delete m_RuntimeCameraHolder;
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

		//Recreating Ownership component
		const auto& srcChildren = source.GetChildren();
		for (auto& child : srcChildren)
		{
			Entity myChild = CreateFromEntity(child);
			myChild.SetParent(result);
		}

		EntityCopyComponent<NativeScriptComponent>(source, result);
		EntityCopyComponent<ScriptComponent>(source, result);
		EntityCopyComponent<PointLightComponent>(source, result);
		EntityCopyComponent<DirectionalLightComponent>(source, result);
		EntityCopyComponent<SpotLightComponent>(source, result);
		EntityCopyComponent<SpriteComponent>(source, result);
		EntityCopyComponent<StaticMeshComponent>(source, result);
		EntityCopyComponent<CameraComponent>(source, result);
		EntityCopyComponent<RigidBodyComponent>(source, result);
		EntityCopyComponent<BoxColliderComponent>(source, result);
		EntityCopyComponent<SphereColliderComponent>(source, result);
		EntityCopyComponent<CapsuleColliderComponent>(source, result);
		EntityCopyComponent<MeshColliderComponent>(source, result);
		EntityCopyComponent<AudioComponent>(source, result);
		EntityCopyComponent<ReverbComponent>(source, result);

		return result;
	}

	void Scene::DestroyEntity(Entity& entity)
	{
		if (bIsPlaying)
		{
			if (entity.HasComponent<NativeScriptComponent>())
			{
				auto& nsc = entity.GetComponent<NativeScriptComponent>();
				if (nsc.Instance)
					nsc.Instance->OnDestroy();
			}

			if (entity.HasComponent<ScriptComponent>())
				if (ScriptEngine::ModuleExists(entity.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnDestroyEntity(entity);
		}
		ScriptEngine::RemoveEntityScript(entity);

		m_AliveEntities.erase(entity.GetComponent<IDComponent>().ID);
		m_EntitiesToDestroy.push_back(entity);
	}

	void Scene::OnUpdateEditor(Timestep ts)
	{
		//Remove entities when a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
		{
			auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
			auto& children = ownershipComponent.Children;
			Entity myParent = ownershipComponent.EntityParent;
			entity.SetParent(Entity::Null);

			while (children.size())
			{
				children[0].SetParent(myParent);
			}
			m_Registry.destroy(entity.GetEnttID());
		}

		m_EntitiesToDestroy.clear();

		if (bCanUpdateEditorCamera)
			m_EditorCamera.OnUpdate(ts);

		std::vector<PointLightComponent*> pointLights;
		pointLights.reserve(MAXPOINTLIGHTS);
		{
			int i = 0;
			auto view = m_Registry.view<PointLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<PointLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					pointLights.push_back(&component);
					++i;

					if (i == MAXPOINTLIGHTS)
						break;
				}
			}
		}
		DirectionalLightComponent* directionalLight = &defaultDirectionalLight;
		{
			auto view = m_Registry.view<DirectionalLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<DirectionalLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					directionalLight = &component;
					break;
				}
			}
		}

		std::vector<SpotLightComponent*> spotLights;
		spotLights.reserve(MAXSPOTLIGHTS);
		{
			int i = 0;
			auto view = m_Registry.view<SpotLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<SpotLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					spotLights.push_back(&component);
					++i;

					if (i == MAXSPOTLIGHTS)
						break;
				}
			}
		}

		//Rendering Static Meshes
		Renderer::BeginScene(m_EditorCamera, pointLights, *directionalLight, spotLights);
		if (bEnableSkybox && m_Cubemap)
			Renderer::DrawSkybox(m_Cubemap);
		{
			auto view = m_Registry.view<StaticMeshComponent>();

			for (auto entity : view)
			{
				auto& smComponent = view.get<StaticMeshComponent>(entity);

				Renderer::DrawMesh(smComponent, (int)entity);
			}
		}

		//Rendering 2D Sprites
		{
			auto view = m_Registry.view<SpriteComponent>();

			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);
				Renderer::DrawSprite(sprite, (int)entity);
			}
		}
		Renderer::EndScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{	
		for (auto& entity : m_EntitiesToDestroy)
		{
			auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
			auto& children = ownershipComponent.Children;
			Entity myParent = ownershipComponent.EntityParent;
			entity.SetParent(Entity::Null);

			while (children.size())
			{
				children[0].SetParent(myParent);
			}
			m_Registry.destroy(entity.GetEnttID());
		}

		m_EntitiesToDestroy.clear();

		//Running Scripts
		{
			auto view = m_Registry.view<NativeScriptComponent>();

			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				
				if (nsc.Instance == nullptr)
				{
					nsc.Instance = nsc.InitScript();
					nsc.Instance->m_Entity = Entity{ entity, this };
					nsc.Instance->OnCreate();
				}

				nsc.Instance->OnUpdate(ts);
			}
		}

		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnUpdateEntity(e, ts);
			}
		}

		m_RuntimeCamera = nullptr;
		//Getting Primary Camera
		{
			auto view = m_Registry.view<CameraComponent>();
			for (auto entity : view)
			{
				auto& cameraComponent = view.get<CameraComponent>(entity);

				if (cameraComponent.Primary)
				{
					m_RuntimeCamera = &cameraComponent;
					break;
				}
			}
		}

		if (!m_RuntimeCamera)
		{
			static bool doneOnce = false;
			if (!doneOnce || (!m_RuntimeCameraHolder))
			{
				doneOnce = true;

				//If user provided primary-camera doesn't exist, provide one and set its transform to match editor camera's transform
				Transform cameraTransform;
				const Transform& editorCameraTransform = m_EditorCamera.GetTransform();
				cameraTransform.Location = editorCameraTransform.Location;
				cameraTransform.Rotation = editorCameraTransform.Rotation;

				Entity temp = CreateEntity("SceneCamera");
				m_RuntimeCameraHolder = new Entity(temp);
				m_RuntimeCamera = &m_RuntimeCameraHolder->AddComponent<CameraComponent>();
				m_RuntimeCameraHolder->AddComponent<NativeScriptComponent>().Bind<CameraController>();

				m_RuntimeCamera->Primary = true;
				m_RuntimeCamera->SetWorldTransform(cameraTransform);
			}
			else
				m_RuntimeCamera = &m_RuntimeCameraHolder->GetComponent<CameraComponent>();
		}
		if (!m_RuntimeCamera->FixedAspectRatio)
		{
			if (m_RuntimeCamera->Camera.GetViewportWidth() != m_ViewportWidth || m_RuntimeCamera->Camera.GetViewportHeight() != m_ViewportHeight)
				m_RuntimeCamera->Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}

		AudioEngine::SetListenerData(m_RuntimeCamera->GetWorldTransform().Location, -m_RuntimeCamera->GetForwardVector(), m_RuntimeCamera->GetUpVector());

		std::vector<PointLightComponent*> pointLights;
		pointLights.reserve(MAXPOINTLIGHTS);
		{
			int i = 0;
			auto view = m_Registry.view<PointLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<PointLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					pointLights.push_back(&component);
					++i;

					if (i == MAXPOINTLIGHTS)
						break;
				}
			}
		}

		DirectionalLightComponent* directionalLight = &defaultDirectionalLight;
		{
			auto view = m_Registry.view<DirectionalLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<DirectionalLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					directionalLight = &component;
					break;
				}
			}
		}

		std::vector<SpotLightComponent*> spotLights;
		pointLights.reserve(MAXSPOTLIGHTS);
		{
			int i = 0;
			auto view = m_Registry.view<SpotLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<SpotLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					spotLights.push_back(&component);
					++i;

					if (i == MAXSPOTLIGHTS)
						break;
				}
			}
		}

		//Rendering Static Meshes
		Renderer::BeginScene(*m_RuntimeCamera, pointLights, *directionalLight, spotLights);
		if (bEnableSkybox && m_Cubemap)
			Renderer::DrawSkybox(m_Cubemap);
		{
			auto view = m_Registry.view<StaticMeshComponent>();

			for (auto entity : view)
			{
				auto& smComponent = view.get<StaticMeshComponent>(entity);

				Renderer::DrawMesh(smComponent, (int)entity);
			}
		}
		/*
		{
			auto view = m_Registry.view<MeshColliderComponent>();

			for (auto entity : view)
			{
				auto& meshCollider = view.get<MeshColliderComponent>(entity);

				Renderer::DrawMesh(meshCollider.DebugMesh, meshCollider.GetWorldTransform(), (int)entity);
			}
		}
		*/
		
		//Rendering 2D Sprites
		{
			auto view = m_Registry.view<SpriteComponent>();

			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);
				Renderer::DrawSprite(sprite, (int)entity);
			}
		}
		Renderer::EndScene();

		m_PhysicsScene->Simulate(ts);
	}

	void Scene::OnRuntimeStart()
	{
		bIsPlaying = true;
		m_PhysicsScene->ConstructFromScene(this);
		ScriptEngine::LoadAppAssembly("Sandbox.dll");
		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::InstantiateEntityClass(e);
			}

			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::OnCreateEntity(e);
			}
		}
		{
			auto view = m_Registry.view<AudioComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				auto& comp = e.GetComponent<AudioComponent>();
				auto& sound = comp.GetSound();
				if (comp.bAutoplay && sound)
					sound->Play();
			}
		}
	}

	void Scene::OnRuntimeStop()
	{
		{
			auto view = m_Registry.view<NativeScriptComponent>();
			for (auto& e : view)
			{
				auto& nsc = m_Registry.get<NativeScriptComponent>(e);
				if (nsc.Instance)
					nsc.Instance->OnDestroy();
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
		}

		bIsPlaying = false;
		m_PhysicsScene->Reset();
	}

	void Scene::OnEventRuntime(Event& e)
	{
		//Running Scripts
		{
			auto view = m_Registry.view<NativeScriptComponent>();

			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);

				if (nsc.Instance == nullptr)
				{
					nsc.Instance = nsc.InitScript();
					nsc.Instance->m_Entity = Entity{ entity, this };
					nsc.Instance->OnCreate();
				}

				nsc.Instance->OnEvent(e);
			}
		}
	}

	void Scene::OnEventEditor(Event& e)
	{
		m_EditorCamera.OnEvent(e);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_EditorCamera.SetViewportSize(width, height);
		Renderer::WindowResized(width, height);
		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	void Scene::ClearScene()
	{
		if (bIsPlaying)
		{
			//Calling on destroy for C++ scripts
			{
				auto view = m_Registry.view<NativeScriptComponent>();
				for (auto entity : view)
				{
					auto& nsc = view.get<NativeScriptComponent>(entity);
					if (nsc.Instance)
					{
						nsc.Instance->OnDestroy();
					}
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

		m_PhysicsScene.reset();
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

	Entity Scene::GetEntityByGUID(const GUID& guid) const
	{
		auto it = m_AliveEntities.find(guid);
		return it != m_AliveEntities.end() ? it->second : Entity::Null;
	}

	const Ref<PhysicsActor>& Scene::GetPhysicsActor(const Entity& entity) const
	{
		return m_PhysicsScene->GetPhysicsActor(entity);
	}

	Ref<PhysicsActor>& Scene::GetPhysicsActor(const Entity& entity)
	{
		return m_PhysicsScene->GetPhysicsActor(entity);
	}

	const CameraComponent* Scene::GetRuntimeCamera() const
	{
		return m_RuntimeCamera;
	}

	void Scene::SetSceneGamma(float gamma)
	{
		m_SceneGamma = gamma;
		Renderer::Gamma() = m_SceneGamma;
	}

	void Scene::SetSceneExposure(float exposure)
	{
		m_SceneExposure = exposure;
		Renderer::Exposure() = exposure;
	}

	int Scene::GetEntityIDAtCoords(int x, int y) const
	{
		auto& framebuffer = Renderer::GetGFramebuffer();
		framebuffer->Bind();
		int result = framebuffer->ReadPixel(4, x, y); //4 - RED_INTEGER
		framebuffer->Unbind();
		return result;
	}

	uint32_t Scene::GetMainColorAttachment(uint32_t index) const
	{
		return Renderer::GetFinalFramebuffer()->GetColorAttachment(index);
	}

	uint32_t Scene::GetGBufferColorAttachment(uint32_t index) const
	{
		return Renderer::GetGFramebuffer()->GetColorAttachment(index);
	}
}
