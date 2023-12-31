#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Debug/CPUTimings.h"

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
				line.Start = center + radius * glm::vec3(cosAngle1, sinAngle1, 0.f);
				line.End = center + radius * glm::vec3(cosAngle2, sinAngle2, 0.f);
				line.Color = color;

				auto& line2 = buffer.emplace_back();
				line2.Start = center + radius * glm::vec3(0.f, cosAngle1, sinAngle1);
				line2.End = center + radius * glm::vec3(0.f, cosAngle2, sinAngle2);
				line2.Color = color;

				auto& line3 = buffer.emplace_back();
				line3.Start = center + radius * glm::vec3(cos45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line3.End = center + radius * glm::vec3(cos45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line3.Color = color;

				auto& line4 = buffer.emplace_back();
				line4.Start = center + radius * glm::vec3(cosMinus45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line4.End = center + radius * glm::vec3(cosMinus45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line4.Color = color;
			}
		}
	}

	Ref<Scene> Scene::s_CurrentScene;

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

	Scene::Scene(const std::string& debugName, const Ref<SceneRenderer>& sceneRenderer, bool bRuntime)
		: m_DebugName(debugName)
	{
		if (sceneRenderer)
			m_SceneRenderer = sceneRenderer;
		else
			m_SceneRenderer = MakeRef<SceneRenderer>(glm::uvec2{ m_ViewportWidth, m_ViewportHeight });
		ConnectSignals();

		m_RuntimePhysicsScene = MakeRef<PhysicsScene>(PhysicsSettings());
		if (bRuntime)
		{
			m_PhysicsScene = m_RuntimePhysicsScene;
		}
		else
		{
			PhysicsSettings editorSettings;
			editorSettings.FixedTimeStep = 1 / 30.f;
			editorSettings.SolverIterations = 1;
			editorSettings.SolverVelocityIterations = 1;
			editorSettings.Gravity = glm::vec3{ 0.f };
			editorSettings.DebugOnPlay = false;
			editorSettings.EditorScene = true;
			m_PhysicsScene = MakeRef<PhysicsScene>(editorSettings);
		}
	}

	Scene::Scene(const Ref<Scene>& other, const std::string& debugName)
	: bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
	, m_PhysicsScene(other->m_RuntimePhysicsScene)
	, m_EditorCamera(other->m_EditorCamera)
	, m_EntitiesToDestroy(other->m_EntitiesToDestroy)
	, m_ViewportWidth(other->m_ViewportWidth)
	, m_ViewportHeight(other->m_ViewportHeight)
	, m_DebugName(debugName)
	{
		// Reuse renderer so that we don't allocate additional GPU resources
		m_SceneRenderer = other->m_SceneRenderer;

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
		EntityCopyComponent<BillboardComponent>(source, result);
		EntityCopyComponent<CameraComponent>(source, result);
		EntityCopyComponent<RigidBodyComponent>(source, result);
		EntityCopyComponent<BoxColliderComponent>(source, result);
		EntityCopyComponent<SphereColliderComponent>(source, result);
		EntityCopyComponent<CapsuleColliderComponent>(source, result);
		EntityCopyComponent<MeshColliderComponent>(source, result);
		EntityCopyComponent<AudioComponent>(source, result);
		EntityCopyComponent<ReverbComponent>(source, result);
		EntityCopyComponent<TextComponent>(source, result);
		EntityCopyComponent<Text2DComponent>(source, result);
		EntityCopyComponent<Image2DComponent>(source, result);

		return result;
	}

	void Scene::DestroyEntity(Entity entity)
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

		m_EntitiesToDestroy.push_back(entity);

		// EG_EDITOR_TRACE("Destroyed Entity: {}", entity.GetComponent<EntitySceneNameComponent>().Name);
	}

	void Scene::OnUpdate(Timestep ts, bool bRender)
	{
		if (bIsPlaying)
			OnUpdateRuntime(ts, bRender);
		else
			OnUpdateEditor(ts, bRender);
	}

	void Scene::OpenScene(const Path& path, bool bReuseCurrentSceneRenderer, bool bRuntime)
	{
		auto func = [path, bReuseCurrentSceneRenderer, bRuntime]()
		{
			ComponentsNotificationSystem::ResetSystem();
			ScriptEngine::Reset();
			RenderManager::Wait();
			Ref<Scene> scene = MakeRef<Scene>(path.u8string(), (bReuseCurrentSceneRenderer && s_CurrentScene) ? s_CurrentScene->GetSceneRenderer() : nullptr, bRuntime);
			if (std::filesystem::exists(path))
			{
				SceneSerializer serializer(scene);
				serializer.Deserialize(path);
			}
			OnSceneOpened(scene);
		};

		Application::Get().CallNextFrame(func);
	}

	void Scene::AddOnSceneOpenedCallback(GUID id, const std::function<void(const Ref<Scene>&)>& func)
	{
		s_OnSceneOpenedCallbacks[id] = func;
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

	void Scene::OnUpdateEditor(Timestep ts, bool bRender)
	{
		DestroyPendingEntities();

		m_EditorCamera.OnUpdate(ts, bCanUpdateEditorCamera);
		m_PhysicsScene->Simulate(ts, false);
		
		if (bRender) [[likely]]
			RenderScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts, bool bRender)
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

		m_PhysicsScene->Simulate(ts, true);

		if (bRender) [[likely]]
			RenderScene();
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
				if (component.DoesAffectWorld())
				{
					m_PointLights.push_back(&component);
					if (component.VisualizeRadiusEnabled())
						m_PointLightsDebugRadii.emplace(&component);
				}
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
				if (component.DoesAffectWorld())
				{
					m_SpotLights.push_back(&component);
					if (component.VisualizeDistanceEnabled())
						m_SpotLightsDebugRadii.emplace(&component);
				}
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
			if (!m_RuntimeCameraHolder)
			{
				//If user provided primary-camera doesn't exist, provide one and set its transform to match editor camera's transform
				m_RuntimeCameraHolder = new Entity(CreateEntity("Runtime Camera"));
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

	void Scene::RenderScene()
	{
		EG_CPU_TIMING_SCOPED("Scene. Render Scene");

		GatherLightsInfo();

		// If meshes are dirty, there's not point in updating specific transforms
		// Since meshes are going to be fully updated anyway
		if (m_DirtyFlags.bMeshTransformsDirty && !m_DirtyFlags.bMeshesDirty)
		{
			m_SceneRenderer->UpdateMeshesTransforms(m_DirtyTransformMeshes);
		}
		m_DirtyTransformMeshes.clear();

		// Same for sprites
		if (m_DirtyFlags.bSpriteTransformsDirty && !m_DirtyFlags.bSpritesDirty)
		{
			m_SceneRenderer->UpdateSpritesTransforms(m_DirtyTransformSprites);
		}
		m_DirtyTransformSprites.clear();

		// Same for texts
		if (m_DirtyFlags.bTextTransformsDirty && !m_DirtyFlags.bTextDirty)
		{
			m_SceneRenderer->UpdateTextsTransforms(m_DirtyTransformTexts);
		}
		m_DirtyTransformTexts.clear();

		if (m_DirtyFlags.bMeshesDirty)
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
						innerCircleLine.Start = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
						innerCircleLine.End = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));

						auto& toInnerLine = m_DebugSpotLines.emplace_back();
						toInnerLine.Start = location;
						toInnerLine.End = innerCircleLine.Start;

						auto& outerCircleLine = m_DebugSpotLines.emplace_back();
						outerCircleLine.Start = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
						outerCircleLine.End = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));
						outerCircleLine.Color = glm::vec3(0.75, 0.75f, 0.f);

						auto& toOuterLine = m_DebugSpotLines.emplace_back();
						toOuterLine.Start = location;
						toOuterLine.End = outerCircleLine.Start;
						toOuterLine.Color = glm::vec3(0.75, 0.75f, 0.f);
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

			for (auto entity : dirLightsView)
			{
				auto& dir = dirLightsView.get<DirectionalLightComponent>(entity);
				if (dir.bVisualizeDirection)
				{
					const auto& location = dir.GetWorldTransform().Location;
					const auto forward = dir.GetForwardVector();
					const auto up = dir.GetUpVector();

					const auto endLocation = location + forward * 0.2f;

					// Drawing an arrow
					RendererLine line;
					line.Start = location;
					line.End = endLocation;
					m_DebugLinesToDraw.push_back(line);

					line.Start = location + forward * 0.15f + up * 0.05f;
					m_DebugLinesToDraw.push_back(line);

					line.Start = location + forward * 0.15f + up * -0.05f;
					m_DebugLinesToDraw.push_back(line);
				}
			}

			// Debug collisions
			if (debugCollisionsLinesSize)
			{
				const physx::PxDebugLine* physicsLines = rb.getLines();

				for (uint32_t i = 0; i < debugCollisionsLinesSize; ++i)
				{
					auto& line = physicsLines[i];
					// color, start, end
					const RendererLine rendererLine{ { 0.f, 1.f, 0.f }, *(glm::vec3*)(&line.pos0), *(glm::vec3*)(&line.pos1) };
					m_DebugLinesToDraw.push_back(rendererLine);
				}
			}

			// Append user provided lines
			m_DebugLinesToDraw.insert(m_DebugLinesToDraw.end(), m_UserDebugLines.begin(), m_UserDebugLines.end());
			m_UserDebugLines.clear(); // User provided lines need to provided each frame. So clear it.
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

		const Camera* camera = bIsPlaying ? (Camera*)&m_RuntimeCamera->Camera : (Camera*)&m_EditorCamera;
		m_SceneRenderer->SetPointLights(m_PointLights, m_DirtyFlags.bPointLightsDirty);
		m_SceneRenderer->SetSpotLights(m_SpotLights, m_DirtyFlags.bSpotLightsDirty);
		m_SceneRenderer->SetDirectionalLight(m_DirectionalLight);
		m_SceneRenderer->SetMeshes(m_Meshes, m_DirtyFlags.bMeshesDirty);
		m_SceneRenderer->SetSprites(m_Sprites, m_DirtyFlags.bSpritesDirty);
		m_SceneRenderer->SetDebugLines(m_DebugLinesToDraw);
		m_SceneRenderer->SetBillboards(m_Billboards);
		m_SceneRenderer->SetTexts(m_Texts, m_DirtyFlags.bTextDirty);
		m_SceneRenderer->SetTexts2D(m_Texts2D, m_DirtyFlags.bText2DDirty);
		m_SceneRenderer->SetImages2D(m_Images2D, m_DirtyFlags.bImage2DDirty);
		m_SceneRenderer->SetIsRuntime(bIsPlaying);

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
		m_SceneRenderer->Render(camera, viewMatrix, viewPos);

		m_DirtyFlags.SetEverythingDirty(false);
	}

	void Scene::OnRuntimeStart()
	{
		EG_EDITOR_TRACE("Runtime started");

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
		EG_EDITOR_TRACE("Runtime stopped");

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

			// Destroy script instances
			for (auto entity : view)
			{
				Entity e = { entity, this };
				if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
					ScriptEngine::RemoveEntityScript(e);
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
		if (m_ViewportWidth == width && m_ViewportHeight == height)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_EditorCamera.SetViewportSize(width, height);
		m_SceneRenderer->SetViewportSize({ width, height });
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

	SceneSoundData Scene::SpawnSound2D(const Path& path, const SoundSettings& settings)
	{
		SceneSoundData result;
		result.Sound = Sound2D::Create(path, settings);
		m_SpawnedSounds[result.ID] = result.Sound;
		return result;
	}

	SceneSoundData Scene::SpawnSound3D(const Path& path, const glm::vec3& position, RollOffModel rollOff, const SoundSettings& settings)
	{
		SceneSoundData result;
		result.Sound = Sound3D::Create(path, position, rollOff, settings);
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

	void Scene::OnStaticMeshComponentRemoved(entt::registry& r, entt::entity e)
	{
		Entity entity(e, this);
		auto& sm = entity.GetComponent<StaticMeshComponent>().GetStaticMesh();
		if (sm && sm->IsValid())
		{
			m_DirtyFlags.bMeshesDirty = true;
			m_DirtyFlags.bMeshTransformsDirty = true;
		}
	}

	void Scene::OnSpriteComponentAddedRemoved(entt::registry& r, entt::entity e)
	{
		m_DirtyFlags.bSpritesDirty = true;
		m_DirtyFlags.bSpriteTransformsDirty = true;
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

	void Scene::ConnectSignals()
	{
		m_Registry.on_destroy<StaticMeshComponent>().connect<&Scene::OnStaticMeshComponentRemoved>(*this);
		m_Registry.on_construct<SpriteComponent>().connect<&Scene::OnSpriteComponentAddedRemoved>(*this);
		m_Registry.on_destroy<SpriteComponent>().connect<&Scene::OnSpriteComponentAddedRemoved>(*this);
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
	}
}
