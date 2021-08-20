#include "egpch.h"
#include "Scene.h"

#include "Entity.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Renderer/Renderer.h"
#include "Eagle/Renderer/Framebuffer.h"
#include "Eagle/Renderer/Renderer2D.h"

namespace Eagle
{
	static DirectionalLightComponent defaultDirectionalLight;

	template<typename T>
	static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const std::unordered_map<entt::entity, entt::entity>& createdEntities)
	{
		auto entities = srcRegistry.view<T>();
		for (auto srcEntity : entities)
		{	
			entt::entity destEntity = createdEntities.at(srcEntity);

			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			auto& destComponent = dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
		}
	}

	Scene::Scene()
	{
		defaultDirectionalLight.LightColor = glm::vec4{ glm::vec3(0.0f), 1.f };
		defaultDirectionalLight.Ambient = glm::vec3(0.0f);
		SetSceneGamma(m_SceneGamma);
		SetSceneExposure(m_SceneExposure);

		constexpr uint32_t maxEntities = 10000;
		m_Registry.reserve(maxEntities);
		m_Registry.reserve<NotificationComponent>(maxEntities);
		m_Registry.reserve<SceneComponent>(maxEntities);
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

	Scene::Scene(const Ref<Scene>& other) 
	: m_Cubemap(other->m_Cubemap)
	, bCanUpdateEditorCamera(other->bCanUpdateEditorCamera)
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
			Entity entity = CreateEntity(sceneName);
			createdEntities[entt] = entity.GetEnttID();
		}

		CopyComponent<NotificationComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<EntitySceneNameComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<TransformComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<OwnershipComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<NativeScriptComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<PointLightComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<DirectionalLightComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<SpotLightComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<SpriteComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<StaticMeshComponent>(m_Registry, other->m_Registry, createdEntities);
		CopyComponent<CameraComponent>(m_Registry, other->m_Registry, createdEntities);
	}

	Scene::~Scene()
	{
		ClearScene();
		EG_CORE_INFO("Scene has been destroyed!");
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		const std::string& sceneName = name.empty() ? "Unnamed Entity" : name;
		Entity entity = Entity(m_Registry.create(), this);
		entity.AddComponent<NotificationComponent>();
		entity.AddComponent<EntitySceneNameComponent>(sceneName);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<OwnershipComponent>();

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
			auto& children = ownershipComponent.Children;
			Entity myOwner = ownershipComponent.EntityOwner;
			entity.SetOwner(Entity::Null);

			while (children.size())
			{
				children[0].SetOwner(myOwner);
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
				pointLights.push_back(&view.get<PointLightComponent>(entity));
				++i;

				if (i == MAXPOINTLIGHTS)
					break;
			}
		}
		DirectionalLightComponent* directionalLight = &defaultDirectionalLight;
		{
			auto view = m_Registry.view<DirectionalLightComponent>();

			for (auto entity : view)
			{
				directionalLight = &view.get<DirectionalLightComponent>(entity);
				break;
			}
		}

		std::vector<SpotLightComponent*> spotLights;
		spotLights.reserve(MAXSPOTLIGHTS);
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
		//Remove entities when a new frame begins
		for (auto& entity : m_EntitiesToDestroy)
		{
			auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();
			auto& children = ownershipComponent.Children;
			Entity myOwner = ownershipComponent.EntityOwner;
			entity.SetOwner(Entity::Null);

			while (children.size())
			{
				children[0].SetOwner(myOwner);
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

			DirectionalLightComponent* directionalLight = &defaultDirectionalLight;
			{
				auto view = m_Registry.view<DirectionalLightComponent>();

				for (auto entity : view)
				{
					directionalLight = &view.get<DirectionalLightComponent>(entity);
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

	int Scene::GetEntityIDAtCoords(int x, int y)
	{
		auto& framebuffer = Renderer::GetGFramebuffer();
		framebuffer->Bind();
		int result = framebuffer->ReadPixel(4, x, y); //4 - RED_INTEGER
		framebuffer->Unbind();
		return result;
	}

	uint32_t Scene::GetMainColorAttachment(uint32_t index)
	{
		return Renderer::GetFinalFramebuffer()->GetColorAttachment(index);
	}

	uint32_t Scene::GetGBufferColorAttachment(uint32_t index)
	{
		return Renderer::GetGFramebuffer()->GetColorAttachment(index);
	}
}
