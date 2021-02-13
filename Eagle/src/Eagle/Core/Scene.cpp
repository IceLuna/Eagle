#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/Renderer2D.h"

namespace Eagle
{
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
		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (m_Registry.has<NativeScriptComponent>(entity))
		{
			auto& nsc = m_Registry.get<NativeScriptComponent>(entity);
			if (nsc.Instance)
				nsc.Instance->OnDestroy();
		}

		m_EntitiesToDestroy.push_back(entity);
	}

	void Scene::OnUpdateEditor(Timestep ts)
	{
		//Remove entities a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
			m_Registry.destroy(entity);

		m_EntitiesToDestroy.clear();

		m_EditorCamera.OnUpdate(ts);

		//Rendering 2D Sprites
		Renderer2D::BeginScene(m_EditorCamera);
		{
			auto view = m_Registry.view<SpriteComponent>();

			for (auto entity : view)
			{
				auto& sprite = view.get<SpriteComponent>(entity);

				Renderer2D::DrawQuad(sprite.Transform, sprite.Color);
			}
		}
		Renderer2D::EndScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{	
		//Remove entities a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
			m_Registry.destroy(entity);

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
					nsc.Instance->m_Entity = Entity{entity, this};
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

			Renderer2D::BeginScene(*mainCamera);
			//Rendering 2D Sprites
			{
				auto view = m_Registry.view<SpriteComponent>();

				for (auto entity : view)
				{
					auto& sprite = view.get<SpriteComponent>(entity);

					Renderer2D::DrawQuad(sprite.Transform, sprite.Color);
				}
			}
			
			Renderer2D::EndScene();
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
}