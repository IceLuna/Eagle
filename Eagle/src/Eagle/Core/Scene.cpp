#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/Renderer.h"
#include "Eagle/Renderer/Renderer2D.h"

namespace Eagle
{
	static void OnCreate(entt::registry& reg, entt::entity e)
	{
		//EG_CORE_CRITICAL("Created!");
	}
	Scene::Scene()
	{
	#if ENTT_EXAMPLE_CODE
		entt::entity entity = m_Registry.create();
		m_Registry.emplace<TransformComponent>(entity, glm::mat4(1.0f));

		m_Registry.on_construct<TransformComponent>().connect<&OnTransformConstruct>();


		if (m_Registry.has<TransformComponent>(entity))
			TransformComponent& transform = m_Registry.get<TransformComponent>(entity);


		auto view = m_Registry.view<TransformComponent>();
		for (auto entity : view)
		{
			TransformComponent& transform = view.get<TransformComponent>(entity);
		}

		auto group = m_Registry.group<TransformComponent>(entt::get<MeshComponent>);
		for (auto entity : group)
		{
			auto& [transform, mesh] = group.get<TransformComponent, MeshComponent>(entity);
		}
	#endif
		m_Registry.on_construct<SpriteComponent>().connect<&OnCreate>();
	}

	Scene::~Scene()
	{
		ClearScene();
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		const std::string sceneName = name.empty() ? "Unnamed Entity" : name;
		Entity entity = Entity(m_Registry.create(), this);
		entity.AddComponent<EntitySceneNameComponent>(sceneName);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<OwnershipComponent>();
		entity.AddComponent<NotificationComponent>();

		return entity;
	}

	void Scene::DestroyEntity(Entity& entity)
	{
		if (m_Registry.has<NativeScriptComponent>(entity.GetEnttID()))
		{
			auto& nsc = m_Registry.get<NativeScriptComponent>(entity.GetEnttID());
			if (nsc.Instance)
				nsc.Instance->OnDestroy();
		}

		m_EntitiesToDestroy.push_back(entity);
	}

	void Scene::OnUpdateEditor(Timestep ts)
	{
		//Remove entities when a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
		{
			auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
			auto& owner = ownershipComponent.Owner;
			auto& children = ownershipComponent.Children;
			Entity myOwner = entity.GetOwner();
			entity.SetOwner(Entity::Null);

			while (children.size())
			{
				children[0].SetOwner(myOwner);
			}
			m_DestroyedEntities.push_back(entity);
			m_Registry.destroy(entity.GetEnttID());
		}

		m_EntitiesToDestroy.clear();

		m_EditorCamera.OnUpdate(ts);

		std::vector<PointLightComponent*> pointLights;
		pointLights.reserve(MAXPOINTLIGHTS);
		{
			int i = 0;
			auto view = m_Registry.view<PointLightComponent>();

			for (auto entity : view)
			{
				pointLights.push_back(&view.get<PointLightComponent>(entity));
				++i;

				if (i == MAXPOINTLIGHTS)
					break;
			}
		}

		DirectionalLightComponent directionalLight;
		directionalLight.LightColor = glm::vec4{ glm::vec3(0.f), 1.f };
		{
			auto view = m_Registry.view<DirectionalLightComponent>();

			for (auto entity : view)
			{
				directionalLight = view.get<DirectionalLightComponent>(entity);
				break;
			}
		}

		std::vector<SpotLightComponent*> spotLights;
		pointLights.reserve(MAXSPOTLIGHTS);
		{
			int i = 0;
			auto view = m_Registry.view<SpotLightComponent>();

			for (auto entity : view)
			{
				spotLights.push_back(&view.get<SpotLightComponent>(entity));
				++i;

				if (i == MAXSPOTLIGHTS)
					break;
			}
		}

		//Rendering 2D Sprites
		Renderer2D::BeginScene(m_EditorCamera, pointLights, directionalLight, spotLights);
		{
			auto view = m_Registry.view<SpriteComponent>();

			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);

				Renderer2D::DrawQuad(sprite.GetWorldTransform(), sprite.Material, (int)entity);
			}
		}
		Renderer2D::EndScene();

		//Rendering Static Meshes
		Renderer::BeginScene(m_EditorCamera, pointLights, directionalLight, spotLights);
		{
			auto view = m_Registry.view<StaticMeshComponent>();

			for (auto entity : view)
			{
				auto& smComponent = view.get<StaticMeshComponent>(entity);

				Renderer::Draw(smComponent, (int)entity);
			}
		}
		Renderer::EndScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{	
		//Remove entities when a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
		{
			auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
			auto& owner = ownershipComponent.Owner;
			auto& children = ownershipComponent.Children;
			Entity myOwner = entity.GetOwner();
			entity.SetOwner(Entity::Null);

			while (children.size())
			{
				children[0].SetOwner(myOwner);
			}
			m_DestroyedEntities.push_back(entity);
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

		CameraComponent* mainCamera = nullptr;
		//Getting Primary Camera
		{
			auto view = m_Registry.view<CameraComponent>();
			for (auto entity : view)
			{
				auto& cameraComponent = view.get<CameraComponent>(entity);

				if (cameraComponent.Primary)
				{
					mainCamera = &cameraComponent;
					break;
				}
			}
		}

		if (mainCamera)
		{
			if (!mainCamera->FixedAspectRatio)
			{
				if (mainCamera->Camera.GetViewportWidth() != m_ViewportWidth || mainCamera->Camera.GetViewportHeight() != m_ViewportHeight)
					mainCamera->Camera.SetViewportSize(m_ViewportHeight, m_ViewportHeight);
			}

			std::vector<PointLightComponent*> pointLights;
			pointLights.reserve(MAXPOINTLIGHTS);
			{
				int i = 0;
				auto view = m_Registry.view<PointLightComponent>();

				for (auto entity : view)
				{
					pointLights.push_back(&view.get<PointLightComponent>(entity));
					++i;

					if (i == MAXPOINTLIGHTS)
						break;
				}
			}

			DirectionalLightComponent directionalLight;
			directionalLight.LightColor = glm::vec4{ glm::vec3(0.f), 1.f };
			{
				auto view = m_Registry.view<DirectionalLightComponent>();

				for (auto entity : view)
				{
					directionalLight = view.get<DirectionalLightComponent>(entity);
					break;
				}
			}

			std::vector<SpotLightComponent*> spotLights;
			pointLights.reserve(MAXSPOTLIGHTS);
			{
				int i = 0;
				auto view = m_Registry.view<SpotLightComponent>();

				for (auto entity : view)
				{
					spotLights.push_back(&view.get<SpotLightComponent>(entity));
					++i;

					if (i == MAXSPOTLIGHTS)
						break;
				}
			}

			//Rendering 2D Sprites
			Renderer2D::BeginScene(*mainCamera, pointLights, directionalLight, spotLights);
			{
				auto view = m_Registry.view<SpriteComponent>();

				for (auto entity : view)
				{
					auto& sprite = view.get<SpriteComponent>(entity);

					Renderer2D::DrawQuad(sprite.GetWorldTransform(), sprite.Material, (int)entity);
				}
			}
			Renderer2D::EndScene();

			//Rendering Static Meshes
			Renderer::BeginScene(m_EditorCamera, pointLights, directionalLight, spotLights);
			{
				auto view = m_Registry.view<StaticMeshComponent>();

				for (auto entity : view)
				{
					auto& smComponent = view.get<StaticMeshComponent>(entity);

					Renderer::Draw(smComponent, (int)entity);
				}
			}
			Renderer::EndScene();
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
		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	void Scene::ClearScene()
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

	bool Scene::WasEntityDestroyed(Entity entity)
	{
		auto it = std::find(m_DestroyedEntities.begin(), m_DestroyedEntities.end(), entity);

		return (it != m_DestroyedEntities.end());
	}
}