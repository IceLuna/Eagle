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
	//--------------Entity--------------
	EG_GUID_TYPE Eagle_Entity_GetParent(EG_GUID_TYPE entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (!entity)
			return 0;

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

	void Eagle_Entity_GetEntityName(EG_GUID_TYPE entityID, MonoString* string)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			string = mono_string_new(mono_domain_get(), entity.GetComponent<EntitySceneNameComponent>().Name.c_str());
	}

	//--------------Transform Component--------------
	void Eagle_TransformComponent_GetWorldTransform(EG_GUID_TYPE entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outTransform = entity.GetWorldTransform();
	}

	void Eagle_TransformComponent_GetWorldLocation(EG_GUID_TYPE entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLocation = entity.GetWorldLocation();
	}

	void Eagle_TransformComponent_GetWorldRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outRotation = entity.GetWorldRotation();
	}

	void Eagle_TransformComponent_GetWorldScale(EG_GUID_TYPE entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outScale = entity.GetWorldScale();
	}

	void Eagle_TransformComponent_SetWorldTransform(EG_GUID_TYPE entityID, Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldTransform(*inTransform);
	}

	void Eagle_TransformComponent_SetWorldLocation(EG_GUID_TYPE entityID, glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldLocation(*inLocation);
	}

	void Eagle_TransformComponent_SetWorldRotation(EG_GUID_TYPE entityID, glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldRotation(*inRotation);
	}

	void Eagle_TransformComponent_SetWorldScale(EG_GUID_TYPE entityID, glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldScale(*inScale);
	}
	
	void Eagle_TransformComponent_GetRelativeTransform(EG_GUID_TYPE entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outTransform = entity.GetRelativeTransform();
	}

	void Eagle_TransformComponent_GetRelativeLocation(EG_GUID_TYPE entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLocation = entity.GetRelativeLocation();
	}

	void Eagle_TransformComponent_GetRelativeRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outRotation = entity.GetRelativeRotation();
	}

	void Eagle_TransformComponent_GetRelativeScale(EG_GUID_TYPE entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outScale = entity.GetRelativeScale();
	}

	void Eagle_TransformComponent_SetRelativeTransform(EG_GUID_TYPE entityID, Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeTransform(*inTransform);
	}

	void Eagle_TransformComponent_SetRelativeLocation(EG_GUID_TYPE entityID, glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeLocation(*inLocation);
	}

	void Eagle_TransformComponent_SetRelativeRotation(EG_GUID_TYPE entityID, glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeRotation(*inRotation);
	}

	void Eagle_TransformComponent_SetRelativeScale(EG_GUID_TYPE entityID, glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeScale(*inScale);
	}

	//--------------PointLight Component--------------
	void Script::Eagle_PointLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<PointLightComponent>().LightColor;
	}

	void Script::Eagle_PointLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<PointLightComponent>().Ambient;
	}

	void Script::Eagle_PointLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<PointLightComponent>().Specular;
	}

	void Script::Eagle_PointLightComponent_GetDistance(EG_GUID_TYPE entityID, float* outDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outDistance = entity.GetComponent<PointLightComponent>().Distance;
	}

	void Script::Eagle_PointLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().LightColor = *inLightColor;
	}

	void Script::Eagle_PointLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Ambient = *inAmbientColor;
	}

	void Script::Eagle_PointLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Specular = *inSpecularColor;
	}

	void Script::Eagle_PointLightComponent_SetDistance(EG_GUID_TYPE entityID, float inDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Distance = inDistance;
	}

	//--------------DirectionalLight Component--------------
	void Script::Eagle_DirectionalLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<DirectionalLightComponent>().LightColor;
	}

	void Script::Eagle_DirectionalLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<DirectionalLightComponent>().Ambient;
	}

	void Script::Eagle_DirectionalLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<DirectionalLightComponent>().Specular;
	}

	void Script::Eagle_DirectionalLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().LightColor = *inLightColor;
	}

	void Script::Eagle_DirectionalLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Ambient = *inAmbientColor;
	}

	void Script::Eagle_DirectionalLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Specular = *inSpecularColor;
	}

	//--------------SpotLight Component--------------
	void Script::Eagle_SpotLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<SpotLightComponent>().LightColor;
	}

	void Script::Eagle_SpotLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<SpotLightComponent>().Ambient;
	}

	void Script::Eagle_SpotLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<SpotLightComponent>().Specular;
	}

	void Script::Eagle_SpotLightComponent_GetInnerCutoffAngle(EG_GUID_TYPE entityID, float* outInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outInnerCutoffAngle = entity.GetComponent<SpotLightComponent>().InnerCutOffAngle;
	}

	void Script::Eagle_SpotLightComponent_GetOuterCutoffAngle(EG_GUID_TYPE entityID, float* outOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outOuterCutoffAngle = entity.GetComponent<SpotLightComponent>().OuterCutOffAngle;
	}

	void Script::Eagle_SpotLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().LightColor = *inLightColor;
	}

	void Script::Eagle_SpotLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().Ambient = *inAmbientColor;
	}

	void Script::Eagle_SpotLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().Specular = *inSpecularColor;
	}

	void Script::Eagle_SpotLightComponent_SetInnerCutoffAngle(EG_GUID_TYPE entityID, float inInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().InnerCutOffAngle = inInnerCutoffAngle;
	}

	void Script::Eagle_SpotLightComponent_SetOuterCutoffAngle(EG_GUID_TYPE entityID, float inOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().OuterCutOffAngle = inOuterCutoffAngle;
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