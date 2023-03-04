#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/Renderer.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Debug/CPUTimings.h"

namespace Eagle
{
	Ref<Scene> Scene::s_CurrentScene;

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
		ConnectSignals();
		SetGamma(m_Gamma);
		SetExposure(m_Exposure);
		SetTonemappingMethod(m_TonemappingMethod);
		SetPhotoLinearTonemappingParams(m_PhotoLinearParams);

		PhysicsSettings editorSettings;
		editorSettings.FixedTimeStep = 1/30.f;
		editorSettings.SolverIterations = 1;
		editorSettings.SolverVelocityIterations = 1;
		editorSettings.Gravity = glm::vec3{0.f};
		editorSettings.DebugOnPlay = false;
		editorSettings.EditorScene = true;
		m_PhysicsScene = MakeRef<PhysicsScene>(editorSettings);
		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(PhysicsSettings());
	}

	Scene::Scene(const Ref<Scene>& other) 
	: bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
	, m_PhysicsScene(other->m_RuntimePhysicsScene)
	, m_IBL(other->m_IBL)
	, m_EditorCamera(other->m_EditorCamera)
	, m_EntitiesToDestroy(other->m_EntitiesToDestroy)
	, m_ViewportWidth(other->m_ViewportWidth)
	, m_ViewportHeight(other->m_ViewportHeight)
	, m_Gamma(other->m_Gamma)
	, m_Exposure(other->m_Exposure)
	, m_TonemappingMethod(other->m_TonemappingMethod)
	, bEnableIBL(other->bEnableIBL)
	{
		ConnectSignals();

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

		for (auto entt : m_Registry.view<RigidBodyComponent>())
		{
			Entity entity{entt, this};
			if (!entity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>())
				m_PhysicsScene->CreatePhysicsActor(entity);
		}
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

		auto& actor = entity.GetPhysicsActor();
		if (actor)
			m_PhysicsScene->RemovePhysicsActor(actor);

		m_EntitiesToDestroy.push_back(entity);
	}

	void Scene::OnUpdateEditor(Timestep ts)
	{
		DestroyPendingEntities();

		if (bCanUpdateEditorCamera)
			m_EditorCamera.OnUpdate(ts);

		m_PhysicsScene->Simulate(ts);
		RenderScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{	
		DestroyPendingEntities();
		UpdateScripts(ts);

		m_RuntimeCamera = FindOrCreateRuntimeCamera();
		if (!m_RuntimeCamera->FixedAspectRatio)
		{
			if (m_RuntimeCamera->Camera.GetViewportWidth() != m_ViewportWidth || m_RuntimeCamera->Camera.GetViewportHeight() != m_ViewportHeight)
				m_RuntimeCamera->Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}

		AudioEngine::SetListenerData(m_RuntimeCamera->GetWorldTransform().Location, -m_RuntimeCamera->GetForwardVector(), m_RuntimeCamera->GetUpVector());

		m_PhysicsScene->Simulate(ts);
		RenderScene();
	}

	void Scene::GatherLightsInfo()
	{
		EG_CPU_TIMING_SCOPED("Scene. Gather Lights Info");

		if (bPointLightsDirty)
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

		m_DirectionalLight = nullptr;
		{
			auto view = m_Registry.view<DirectionalLightComponent>();

			for (auto entity : view)
			{
				auto& component = view.get<DirectionalLightComponent>(entity);
				if (component.bAffectsWorld)
				{
					m_DirectionalLight = &component;
					break;
				}
			}
		}

		if (bSpotLightsDirty)
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

	void Scene::DestroyPendingEntities()
	{
		EG_CPU_TIMING_SCOPED("Scene. Destroy Pending Entities");

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
			m_AliveEntities.erase(entity.GetGUID());
			m_Registry.destroy(entity.GetEnttID());
		}
		m_EntitiesToDestroy.clear();
	}

	void Scene::UpdateScripts(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("Scene. Update Scripts");

		// C++ scripts
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
	}

	CameraComponent* Scene::FindOrCreateRuntimeCamera()
	{
		EG_CPU_TIMING_SCOPED("Scene. Find or Create runtime camera");

		CameraComponent* camera = nullptr;
		// Looking for Primary Camera
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
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
			static bool doneOnce = false;
			if (!doneOnce || (!m_RuntimeCameraHolder))
			{
				doneOnce = true;

				//If user provided primary-camera doesn't exist, provide one and set its transform to match editor camera's transform
				m_RuntimeCameraHolder = new Entity(CreateEntity("Runtime Camera"));
				camera = &m_RuntimeCameraHolder->AddComponent<CameraComponent>();
				camera->Camera = m_EditorCamera;
				m_RuntimeCameraHolder->AddComponent<NativeScriptComponent>().Bind<CameraController>();
				m_RuntimeCameraHolder->RemoveComponent<EntitySceneNameComponent>(); // Delete it so it won't showup in Scene hierarchy

				camera->Primary = true;
				camera->SetWorldTransform(m_EditorCamera.GetTransform());
			}
			else
				camera = &m_RuntimeCameraHolder->GetComponent<CameraComponent>();
		}

		return camera;
	}

	void Scene::RenderScene()
	{
		EG_CPU_TIMING_SCOPED("Scene. Render Scene");

		GatherLightsInfo();

		// If a mesh was added/removed, both bMeshTransformsDirty & bMeshesDirty are true
		//    -> Transforms and meshes are marked dirty, meaning they need to recollected
		//       So there's no point in setting specific transforms dirty since they're gonna be recollected anyway
		// Otherwise bMeshTransformsDirty set to true if transforms were changed
		//    -> Updated specific transforms
		if (!bMeshTransformsDirty)
		{
			if (!m_DirtyTransformMeshes.empty())
			{
				Renderer::SetDirtyMeshTransforms(m_DirtyTransformMeshes);
				m_DirtyTransformMeshes.clear();
			}
		}
		else
		{
			Renderer::MakeMeshTransformBufferDirty();
		}

		if (bMeshesDirty)
			Renderer::MakeMeshBuffersDirty();
		if (bPointLightsDirty)
			Renderer::MakePointLightsDirty();
		if (bSpotLightsDirty)
			Renderer::MakeSpotLightsDirty();

		//Rendering Static Meshes
		if (bIsPlaying)
			Renderer::BeginScene(*m_RuntimeCamera, m_PointLights, m_DirectionalLight, m_SpotLights);
		else
			Renderer::BeginScene(m_EditorCamera, m_PointLights, m_DirectionalLight, m_SpotLights);
		Renderer::DrawSkybox(bEnableIBL ? m_IBL : nullptr);

		//Rendering static meshes
		if (bMeshesDirty || bMeshTransformsDirty)
		{
			auto view = m_Registry.view<StaticMeshComponent>();

			for (auto entity : view)
			{
				auto& smComponent = view.get<StaticMeshComponent>(entity);

				Renderer::DrawMesh(smComponent);
			}
		}

		//Rendering 2D Sprites
		{
			auto view = m_Registry.view<SpriteComponent>();

			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);
				Renderer::DrawSprite(sprite);
			}
		}

		//Rendering Collisions
		{
			auto& rb = m_PhysicsScene->GetRenderBuffer();
			auto lines = rb.getLines();
			const uint32_t linesSize = rb.getNbLines();
			for (uint32_t i = 0; i < linesSize; ++i)
			{
				auto& line = lines[i];
				Renderer::DrawDebugLine(*(glm::vec3*)(&line.pos0), *(glm::vec3*)(&line.pos1), { 0.f, 1.f, 0.f, 1.f });
			}
		}

		//Rendering billboards if enabled and not playing
		if (!bIsPlaying && bDrawMiscellaneous)
		{
			Transform transform;
			transform.Scale3D = glm::vec3(0.25f);
			for (auto& point : m_PointLights)
			{
				transform.Location = point->GetWorldTransform().Location;
				Renderer::DrawBillboard(transform, Texture2D::PointLightIcon);
			}
			for (auto& spot : m_SpotLights)
			{
				transform = spot->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);

				Renderer::DrawBillboard(transform, Texture2D::SpotLightIcon);
			}
			if (m_DirectionalLight)
			{
				transform = m_DirectionalLight->GetWorldTransform();
				transform.Scale3D = glm::vec3(0.25f);
				Renderer::DrawBillboard(transform, Texture2D::DirectionalLightIcon);
			}
		}

		Renderer::EndScene();
		bMeshesDirty = false;
		bMeshTransformsDirty = false;
		bPointLightsDirty = false;
		bSpotLightsDirty = false;
	}

	void Scene::OnRuntimeStart()
	{
		bIsPlaying = true;
		
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

		// Update Audio
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
		if (e.GetEventType() == EventType::KeyPressed)
		{
			KeyPressedEvent& keyEvent = (KeyPressedEvent&)e;
			if (keyEvent.GetKey() == Key::G)
				bDrawMiscellaneous = !bDrawMiscellaneous;
		}

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

	void Scene::SetGamma(float gamma)
	{
		m_Gamma = gamma;
		Renderer::SetGamma(gamma);
	}

	void Scene::SetExposure(float exposure)
	{
		m_Exposure = exposure;
		Renderer::Exposure() = exposure;
	}

	void Scene::SetTonemappingMethod(TonemappingMethod method)
	{
		m_TonemappingMethod = method;
		Renderer::TonemappingMethod() = method;
	}
	
	void Scene::SetPhotoLinearTonemappingParams(PhotoLinearTonemappingParams params)
	{
		m_PhotoLinearParams = params;
		Renderer::SetPhotoLinearTonemappingParams(params);
	}
	
	void Scene::SetFilmicTonemappingParams(FilmicTonemappingParams params)
	{
		m_FilmicParams = params;
		Renderer::FilmicTonemappingParams() = params;
	}

	void Scene::OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& sm = entity.GetComponent<StaticMeshComponent>().GetStaticMesh();
		if (sm && sm->IsValid())
		{
			bMeshesDirty = true;
			bMeshTransformsDirty = true;
		}
	}

	void Scene::OnPointLightAdded(entt::registry& r, entt::entity e)
	{
		bPointLightsDirty = true;
	}

	void Scene::OnPointLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<PointLightComponent>();
		if (light.DoesAffectWorld())
		{
			bPointLightsDirty = true;
		}
	}

	void Scene::OnSpotLightAdded(entt::registry& r, entt::entity e)
	{
		bSpotLightsDirty = true;
	}

	void Scene::OnSpotLightRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& light = entity.GetComponent<SpotLightComponent>();
		if (light.DoesAffectWorld())
		{
			bSpotLightsDirty = true;
		}
	}

	void Scene::ConnectSignals()
	{
		m_Registry.on_destroy<StaticMeshComponent>().connect<&Scene::OnStaticMeshComponentRemoved>(*this);
		m_Registry.on_construct<PointLightComponent>().connect<&Scene::OnPointLightAdded>(*this);
		m_Registry.on_destroy<PointLightComponent>().connect<&Scene::OnPointLightRemoved>(*this);
		m_Registry.on_construct<SpotLightComponent>().connect<&Scene::OnSpotLightAdded>(*this);
		m_Registry.on_destroy<SpotLightComponent>().connect<&Scene::OnSpotLightRemoved>(*this);
	}
}
