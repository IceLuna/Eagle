#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include <mono/jit/jit.h>

namespace Eagle 
{
	extern std::unordered_map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;
}

namespace Eagle::Script
{
	//Entity

	EG_GUID_TYPE Eagle_Entity_GetParent(EG_GUID_TYPE entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (!entity)
			return 0;

		if (Entity& parent = entity.GetOwner())
			return parent.GetGUID();

		return 0;
	}

	void Eagle_Entity_SetParent(EG_GUID_TYPE entityID, EG_GUID_TYPE parentID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			Entity parent = scene->GetEntityByGUID(GUID(parentID));
			entity.SetOwner(parent);
		}
	}

	MonoArray* Eagle_Entity_GetChildren(EG_GUID_TYPE entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			const std::vector<Entity>& children = entity.GetChildren();
			MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetCoreClass("Eagle", "Entity"), children.size());

			uint32_t index = 0;
			for (auto& child : children)
			{
				GUID guid = child.GetGUID();
				void* data[] = 
				{
					&guid
				};
				MonoObject* obj = ScriptEngine::Construct("Eagle.Entity:.ctor(ulong)", true, data);
				mono_array_set(result, MonoObject*, index++, obj);
			}
			return result;
		}
		return nullptr;
	}

	EG_GUID_TYPE Eagle_Entity_CreateEntity()
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->CreateEntity();
		return entity.GetGUID();
	}

	void Eagle_Entity_DestroyEntity(EG_GUID_TYPE entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			scene->DestroyEntity(entity);
		}
	}

	void Eagle_Entity_AddComponent(EG_GUID_TYPE entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));

		if (entity)
		{
			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			m_AddComponentFunctions[monoType](entity);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. Entity is null");
	}

	bool Eagle_Entity_HasComponent(EG_GUID_TYPE entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		
		if (entity)
		{
			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			return m_HasComponentFunctions[monoType](entity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'HasComponent'. Entity is null");
			return false;
		}

	}

	//Transform Component
	void Eagle_TransformComponent_GetTransform(EG_GUID_TYPE entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			*outTransform = entity.GetWorldTransform();
		}
	}

	void Eagle_TransformComponent_GetTranslation(EG_GUID_TYPE entityID, glm::vec3* outTranslation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			*outTranslation = entity.GetWorldTransform().Translation;
		}
	}

	void Eagle_TransformComponent_GetRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			*outRotation = entity.GetWorldTransform().Rotation;
		}
	}

	void Eagle_TransformComponent_GetScale(EG_GUID_TYPE entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			*outScale = entity.GetWorldTransform().Scale3D;
		}
	}

	void Eagle_TransformComponent_SetTransform(EG_GUID_TYPE entityID, Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			entity.SetWorldTransform(*inTransform);
		}
	}

	void Eagle_TransformComponent_SetTranslation(EG_GUID_TYPE entityID, glm::vec3* inTranslation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			entity.SetWorldLocation(*inTranslation);
		}
	}

	void Eagle_TransformComponent_SetRotation(EG_GUID_TYPE entityID, glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			entity.SetWorldRotation(*inRotation);
		}
	}

	void Eagle_TransformComponent_SetScale(EG_GUID_TYPE entityID, glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
		{
			entity.SetWorldScale(*inScale);
		}
	}
}