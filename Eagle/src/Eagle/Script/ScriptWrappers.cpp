#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include <mono/jit/jit.h>

namespace Eagle 
{
	extern std::unordered_map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;
}

namespace Eagle::Script
{
	//--------------Entity--------------
	EG_GUID_TYPE Eagle_Entity_GetParent(EG_GUID_TYPE entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get parent. Entity is null");
			return 0;
		}

		if (Entity& parent = entity.GetParent())
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
			entity.SetParent(parent);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set parent. Entity is null");
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
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get children. Entity is null");
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
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't destroy entity. Entity is null");
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

	void Eagle_Entity_GetEntityName(EG_GUID_TYPE entityID, MonoString* string)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			string = mono_string_new(mono_domain_get(), entity.GetComponent<EntitySceneNameComponent>().Name.c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Entity name. Entity is null");
	}

	//--------------Transform Component--------------
	void Eagle_TransformComponent_GetWorldTransform(EG_GUID_TYPE entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outTransform = entity.GetWorldTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world transform. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldLocation(EG_GUID_TYPE entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLocation = entity.GetWorldLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world location. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outRotation = entity.GetWorldRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world rotation. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldScale(EG_GUID_TYPE entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outScale = entity.GetWorldScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world scale. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldTransform(EG_GUID_TYPE entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world transform. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldLocation(EG_GUID_TYPE entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world location. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldRotation(EG_GUID_TYPE entityID, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world rotation. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldScale(EG_GUID_TYPE entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world scale. Entity is null");
	}
	
	void Eagle_TransformComponent_GetRelativeTransform(EG_GUID_TYPE entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outTransform = entity.GetRelativeTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative transform. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeLocation(EG_GUID_TYPE entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLocation = entity.GetRelativeLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative location. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outRotation = entity.GetRelativeRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative rotation. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeScale(EG_GUID_TYPE entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outScale = entity.GetRelativeScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative scale. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeTransform(EG_GUID_TYPE entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative transform. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeLocation(EG_GUID_TYPE entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative location. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeRotation(EG_GUID_TYPE entityID, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative rotation. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeScale(EG_GUID_TYPE entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative scale. Entity is null");
	}

	//Scene Component
	void Eagle_SceneComponent_GetWorldTransform(EG_GUID_TYPE entityID, void* type, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetWorldTransformFunctions[monoType](entity, outTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world transform. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetWorldLocation(EG_GUID_TYPE entityID, void* type, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			*outLocation = tempTransform.Location;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world location. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetWorldRotation(EG_GUID_TYPE entityID, void* type, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			*outRotation = tempTransform.Rotation;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world rotation. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetWorldScale(EG_GUID_TYPE entityID, void* type, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			*outScale = tempTransform.Scale3D;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world scale. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetWorldTransform(EG_GUID_TYPE entityID, void* type, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetWorldTransformFunctions[monoType](entity, inTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world transform. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetWorldLocation(EG_GUID_TYPE entityID, void* type, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Location = *inLocation;
			m_SetWorldTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world location. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetWorldRotation(EG_GUID_TYPE entityID, void* type, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Rotation = *inRotation;
			m_SetWorldTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world rotation. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetWorldScale(EG_GUID_TYPE entityID, void* type, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Scale3D = *inScale;
			m_SetWorldTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world scale. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetRelativeTransform(EG_GUID_TYPE entityID, void* type, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetRelativeTransformFunctions[monoType](entity, outTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative transform. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetRelativeLocation(EG_GUID_TYPE entityID, void* type, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			*outLocation = tempTransform.Location;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative location. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetRelativeRotation(EG_GUID_TYPE entityID, void* type, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			*outRotation = tempTransform.Rotation;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative rotation. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_GetRelativeScale(EG_GUID_TYPE entityID, void* type, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			*outScale = tempTransform.Scale3D;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative scale. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetRelativeTransform(EG_GUID_TYPE entityID, void* type, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetRelativeTransformFunctions[monoType](entity, inTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative transform. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetRelativeLocation(EG_GUID_TYPE entityID, void* type, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Location = *inLocation;
			m_SetRelativeTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative location. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetRelativeRotation(EG_GUID_TYPE entityID, void* type, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Rotation = *inRotation;
			m_SetRelativeTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative rotation. Entity is null", typeName);
		}
	}

	void Eagle_SceneComponent_SetRelativeScale(EG_GUID_TYPE entityID, void* type, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Scale3D = *inScale;
			m_SetRelativeTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative scale. Entity is null", typeName);
		}
	}

	//--------------PointLight Component--------------
	void Script::Eagle_PointLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<PointLightComponent>().LightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<PointLightComponent>().Ambient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light ambient color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<PointLightComponent>().Specular;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light specular color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_GetDistance(EG_GUID_TYPE entityID, float* outDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outDistance = entity.GetComponent<PointLightComponent>().Distance;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light distance. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().LightColor = *inLightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Ambient = *inAmbientColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light ambient color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Specular = *inSpecularColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light specular color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetDistance(EG_GUID_TYPE entityID, float inDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Distance = inDistance;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light distance. Entity is null");
	}

	//--------------DirectionalLight Component--------------
	void Script::Eagle_DirectionalLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<DirectionalLightComponent>().LightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get directional light color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<DirectionalLightComponent>().Ambient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get directional light ambient color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<DirectionalLightComponent>().Specular;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get directional light specular color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().LightColor = *inLightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set directional light color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Ambient = *inAmbientColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set directional light ambient color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Specular = *inSpecularColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set directional light specular color. Entity is null");
	}

	//--------------SpotLight Component--------------
	void Script::Eagle_SpotLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<SpotLightComponent>().LightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<SpotLightComponent>().Ambient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light ambient color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<SpotLightComponent>().Specular;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light specular color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetInnerCutoffAngle(EG_GUID_TYPE entityID, float* outInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outInnerCutoffAngle = entity.GetComponent<SpotLightComponent>().InnerCutOffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetOuterCutoffAngle(EG_GUID_TYPE entityID, float* outOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outOuterCutoffAngle = entity.GetComponent<SpotLightComponent>().OuterCutOffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light outer cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().LightColor = *inLightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().Ambient = *inAmbientColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light ambient color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().Specular = *inSpecularColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light specular color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetInnerCutoffAngle(EG_GUID_TYPE entityID, float inInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().InnerCutOffAngle = inInnerCutoffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetOuterCutoffAngle(EG_GUID_TYPE entityID, float inOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().OuterCutOffAngle = inOuterCutoffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light outer cut off angle. Entity is null");
	}

	//Texture2D
	bool Eagle_Texture2D_Create(MonoString* texturePath)
	{
		Ref<Texture> texture;
		std::filesystem::path path = mono_string_to_utf8(texturePath);
		if (TextureLibrary::Get(path, &texture) == false)
		{
			texture = Texture2D::Create(path);
			return texture != nullptr;
		}
		return true;
	}

	//Static Mesh
	bool Eagle_StaticMesh_Create(MonoString* meshPath)
	{
		Ref<StaticMesh> staticMesh;
		std::filesystem::path path = mono_string_to_utf8(meshPath);

		if (StaticMeshLibrary::Get(path, &staticMesh) == false)
			staticMesh = StaticMesh::Create(path, false, true, false);

		if (staticMesh)
			return staticMesh->IsValid();
		else 
			return false;
	}

	void Eagle_StaticMesh_SetDiffuseTexture(MonoString* meshPath, MonoString* texturePath)
	{
		Ref<StaticMesh> staticMesh;
		std::filesystem::path path = mono_string_to_utf8(meshPath);

		if (StaticMeshLibrary::Get(path, &staticMesh))
		{
			Ref<Texture> texture;
			std::filesystem::path texturepath = mono_string_to_utf8(texturePath);

			if (TextureLibrary::Get(path, &texture) == false)
				texture = Texture2D::Create(path);

			staticMesh->Material->DiffuseTexture = texture;
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set diffuse texture. StaticMesh is null");
	}

	void Eagle_StaticMesh_SetSpecularTexture(MonoString* meshPath, MonoString* texturePath)
	{
		Ref<StaticMesh> staticMesh;
		std::filesystem::path path = mono_string_to_utf8(meshPath);

		if (StaticMeshLibrary::Get(path, &staticMesh))
		{
			Ref<Texture> texture;
			std::filesystem::path texturepath = mono_string_to_utf8(texturePath);

			if (TextureLibrary::Get(path, &texture) == false)
				texture = Texture2D::Create(path);

			staticMesh->Material->SpecularTexture = texture;
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set specular texture. StaticMesh is null");
	}

	void Eagle_StaticMesh_SetNormalTexture(MonoString* meshPath, MonoString* texturePath)
	{
		Ref<StaticMesh> staticMesh;
		std::filesystem::path path = mono_string_to_utf8(meshPath);

		if (StaticMeshLibrary::Get(path, &staticMesh))
		{
			Ref<Texture> texture;
			std::filesystem::path texturepath = mono_string_to_utf8(texturePath);

			if (TextureLibrary::Get(path, &texture) == false)
				texture = Texture2D::Create(path);

			staticMesh->Material->NormalTexture = texture;
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set normal texture. StaticMesh is null");
	}

	void Eagle_StaticMesh_SetScalarMaterialParams(MonoString* meshPath, const glm::vec4* tintColor, float tilingFactor, float shininess)
	{
		Ref<StaticMesh> staticMesh;
		std::filesystem::path path = mono_string_to_utf8(meshPath);

		if (StaticMeshLibrary::Get(path, &staticMesh))
		{
			staticMesh->Material->TintColor = *tintColor;
			staticMesh->Material->TilingFactor = tilingFactor;
			staticMesh->Material->Shininess = shininess;
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set material scalar params. StaticMesh is null");
	}

	//StaticMesh Component
	void Eagle_StaticMeshComponent_SetMesh(EG_GUID_TYPE entityID, MonoString* meshPath)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Static Mesh. Entity is null");

		Ref<StaticMesh> staticMesh;
		std::filesystem::path path = mono_string_to_utf8(meshPath);

		if (StaticMeshLibrary::Get(path, &staticMesh) == false)
			staticMesh = StaticMesh::Create(path, false, true, false);

		entity.GetComponent<StaticMeshComponent>().StaticMesh = staticMesh;
	}


	//--------------Input--------------
	bool Script::Eagle_Input_IsMouseButtonPressed(Mouse button)
	{
		return Input::IsMouseButtonPressed(button);
	}

	bool Script::Eagle_Input_IsKeyPressed(Key keyCode)
	{
		return Input::IsKeyPressed(keyCode);
	}

	void Script::Eagle_Input_GetMousePosition(glm::vec2* outPosition)
	{
		auto [x, y] = Input::GetMousePosition();
		*outPosition = {x, y};
	}

	void Script::Eagle_Input_SetCursorMode(CursorMode mode)
	{
		Input::SetShowCursor(mode == CursorMode::Normal);
	}

	CursorMode Script::Eagle_Input_GetCursorMode()
	{
		return Input::IsCursorVisible() ? CursorMode::Normal : CursorMode::Hidden;
	}
}