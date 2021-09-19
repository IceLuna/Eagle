#include "egpch.h"
#include "ScriptEngineRegistry.h"
#include "ScriptEngine.h"
#include "ScriptWrappers.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/Entity.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

namespace Eagle
{
	std::unordered_map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	extern MonoImage* s_CoreAssemblyImage;

#define REGISTER_COMPONENT_TYPE(Type)\
	{\
		MonoType* type = mono_reflection_type_from_name("Eagle." #Type, s_CoreAssemblyImage);\
		if (type)\
		{\
			m_HasComponentFunctions[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };\
			m_AddComponentFunctions[type] = [](Entity& entity) { entity.AddComponent<Type>(); };\
		}\
		else\
			EG_CORE_ERROR("No C# Component found for " #Type "!");\
	}

	static void InitComponentTypes()
	{
		REGISTER_COMPONENT_TYPE(TransformComponent);
		REGISTER_COMPONENT_TYPE(PointLightComponent);
		REGISTER_COMPONENT_TYPE(DirectionalLightComponent);
		REGISTER_COMPONENT_TYPE(SpotLightComponent);
	}

	void ScriptEngineRegistry::RegisterAll()
	{
		InitComponentTypes();

		//Entity
		mono_add_internal_call("Eagle.Entity::GetParent_Native", Eagle::Script::Eagle_Entity_GetParent);
		mono_add_internal_call("Eagle.Entity::SetParent_Native", Eagle::Script::Eagle_Entity_SetParent);
		mono_add_internal_call("Eagle.Entity::GetChildren_Native", Eagle::Script::Eagle_Entity_GetChildren);
		mono_add_internal_call("Eagle.Entity::CreateEntity_Native", Eagle::Script::Eagle_Entity_CreateEntity);
		mono_add_internal_call("Eagle.Entity::DestroyEntity_Native", Eagle::Script::Eagle_Entity_DestroyEntity);
		mono_add_internal_call("Eagle.Entity::AddComponent_Native", Eagle::Script::Eagle_Entity_AddComponent);
		mono_add_internal_call("Eagle.Entity::HasComponent_Native", Eagle::Script::Eagle_Entity_HasComponent);
		mono_add_internal_call("Eagle.Entity::GetEntityName_Native", Eagle::Script::Eagle_Entity_GetEntityName);

		//Input
		mono_add_internal_call("Eagle.Input::IsMouseButtonPressed_Native", Eagle::Script::Eagle_Input_IsMouseButtonPressed);
		mono_add_internal_call("Eagle.Input::IsKeyPressed_Native", Eagle::Script::Eagle_Input_IsKeyPressed);
		mono_add_internal_call("Eagle.Input::GetMousePosition_Native", Eagle::Script::Eagle_Input_GetMousePosition);
		mono_add_internal_call("Eagle.Input::SetCursorMode_Native", Eagle::Script::Eagle_Input_SetCursorMode);
		mono_add_internal_call("Eagle.Input::GetCursorMode_Native", Eagle::Script::Eagle_Input_GetCursorMode);

		//TransformComponent
		mono_add_internal_call("Eagle.TransformComponent::GetWorldTransform_Native", Eagle::Script::Eagle_TransformComponent_GetWorldTransform);
		mono_add_internal_call("Eagle.TransformComponent::GetWorldLocation_Native", Eagle::Script::Eagle_TransformComponent_GetWorldLocation);
		mono_add_internal_call("Eagle.TransformComponent::GetWorldRotation_Native", Eagle::Script::Eagle_TransformComponent_GetWorldRotation);
		mono_add_internal_call("Eagle.TransformComponent::GetWorldScale_Native", Eagle::Script::Eagle_TransformComponent_GetWorldScale);
		mono_add_internal_call("Eagle.TransformComponent::SetWorldTransform_Native", Eagle::Script::Eagle_TransformComponent_SetWorldTransform);
		mono_add_internal_call("Eagle.TransformComponent::SetWorldLocation_Native", Eagle::Script::Eagle_TransformComponent_SetWorldLocation);
		mono_add_internal_call("Eagle.TransformComponent::SetWorldRotation_Native", Eagle::Script::Eagle_TransformComponent_SetWorldRotation);
		mono_add_internal_call("Eagle.TransformComponent::SetWorldScale_Native", Eagle::Script::Eagle_TransformComponent_SetWorldScale);

		mono_add_internal_call("Eagle.TransformComponent::GetRelativeTransform_Native", Eagle::Script::Eagle_TransformComponent_GetRelativeTransform);
		mono_add_internal_call("Eagle.TransformComponent::GetRelativeLocation_Native", Eagle::Script::Eagle_TransformComponent_GetRelativeLocation);
		mono_add_internal_call("Eagle.TransformComponent::GetRelativeRotation_Native", Eagle::Script::Eagle_TransformComponent_GetRelativeRotation);
		mono_add_internal_call("Eagle.TransformComponent::GetRelativeScale_Native", Eagle::Script::Eagle_TransformComponent_GetRelativeScale);
		mono_add_internal_call("Eagle.TransformComponent::SetRelativeTransform_Native", Eagle::Script::Eagle_TransformComponent_SetRelativeTransform);
		mono_add_internal_call("Eagle.TransformComponent::SetRelativeLocation_Native", Eagle::Script::Eagle_TransformComponent_SetRelativeLocation);
		mono_add_internal_call("Eagle.TransformComponent::SetRelativeRotation_Native", Eagle::Script::Eagle_TransformComponent_SetRelativeRotation);
		mono_add_internal_call("Eagle.TransformComponent::SetRelativeScale_Native", Eagle::Script::Eagle_TransformComponent_SetRelativeScale);

		//PointLight Component
		mono_add_internal_call("Eagle.PointLightComponent::GetLightColor_Native", Eagle::Script::Eagle_PointLightComponent_GetLightColor);
		mono_add_internal_call("Eagle.PointLightComponent::GetAmbientColor_Native", Eagle::Script::Eagle_PointLightComponent_GetAmbientColor);
		mono_add_internal_call("Eagle.PointLightComponent::GetSpecularColor_Native", Eagle::Script::Eagle_PointLightComponent_GetSpecularColor);
		mono_add_internal_call("Eagle.PointLightComponent::GetDistance_Native", Eagle::Script::Eagle_PointLightComponent_GetDistance);
		mono_add_internal_call("Eagle.PointLightComponent::SetLightColor_Native", Eagle::Script::Eagle_PointLightComponent_SetLightColor);
		mono_add_internal_call("Eagle.PointLightComponent::SetAmbientColor_Native", Eagle::Script::Eagle_PointLightComponent_SetAmbientColor);
		mono_add_internal_call("Eagle.PointLightComponent::SetSpecularColor_Native", Eagle::Script::Eagle_PointLightComponent_SetSpecularColor);
		mono_add_internal_call("Eagle.PointLightComponent::SetDistance_Native", Eagle::Script::Eagle_PointLightComponent_SetDistance);

		//DirectionalLight Component
		mono_add_internal_call("Eagle.DirectionalLightComponent::GetLightColor_Native", Eagle::Script::Eagle_DirectionalLightComponent_GetLightColor);
		mono_add_internal_call("Eagle.DirectionalLightComponent::GetAmbientColor_Native", Eagle::Script::Eagle_DirectionalLightComponent_GetAmbientColor);
		mono_add_internal_call("Eagle.DirectionalLightComponent::GetSpecularColor_Native", Eagle::Script::Eagle_DirectionalLightComponent_GetSpecularColor);
		mono_add_internal_call("Eagle.DirectionalLightComponent::SetLightColor_Native", Eagle::Script::Eagle_DirectionalLightComponent_SetLightColor);
		mono_add_internal_call("Eagle.DirectionalLightComponent::SetAmbientColor_Native", Eagle::Script::Eagle_DirectionalLightComponent_SetAmbientColor);
		mono_add_internal_call("Eagle.DirectionalLightComponent::SetSpecularColor_Native", Eagle::Script::Eagle_DirectionalLightComponent_SetSpecularColor);

		//SpotLight Component
		mono_add_internal_call("Eagle.SpotLightComponent::GetLightColor_Native", Eagle::Script::Eagle_SpotLightComponent_GetLightColor);
		mono_add_internal_call("Eagle.SpotLightComponent::GetAmbientColor_Native", Eagle::Script::Eagle_SpotLightComponent_GetAmbientColor);
		mono_add_internal_call("Eagle.SpotLightComponent::GetSpecularColor_Native", Eagle::Script::Eagle_SpotLightComponent_GetSpecularColor);
		mono_add_internal_call("Eagle.SpotLightComponent::GetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::GetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetOuterCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetLightColor_Native", Eagle::Script::Eagle_SpotLightComponent_SetLightColor);
		mono_add_internal_call("Eagle.SpotLightComponent::SetAmbientColor_Native", Eagle::Script::Eagle_SpotLightComponent_SetAmbientColor);
		mono_add_internal_call("Eagle.SpotLightComponent::SetSpecularColor_Native", Eagle::Script::Eagle_SpotLightComponent_SetSpecularColor);
		mono_add_internal_call("Eagle.SpotLightComponent::SetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_SetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_SetOuterCutoffAngle);
	}

}