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

		//TransformComponent
		mono_add_internal_call("Eagle.TransformComponent::GetTransform_Native", Eagle::Script::Eagle_TransformComponent_GetTransform);
		mono_add_internal_call("Eagle.TransformComponent::GetTranslation_Native", Eagle::Script::Eagle_TransformComponent_GetTranslation);
		mono_add_internal_call("Eagle.TransformComponent::GetRotation_Native", Eagle::Script::Eagle_TransformComponent_GetRotation);
		mono_add_internal_call("Eagle.TransformComponent::GetScale_Native", Eagle::Script::Eagle_TransformComponent_GetScale);
		mono_add_internal_call("Eagle.TransformComponent::SetTransform_Native", Eagle::Script::Eagle_TransformComponent_SetTransform);
		mono_add_internal_call("Eagle.TransformComponent::SetTranslation_Native", Eagle::Script::Eagle_TransformComponent_SetTranslation);
		mono_add_internal_call("Eagle.TransformComponent::SetRotation_Native", Eagle::Script::Eagle_TransformComponent_SetRotation);
		mono_add_internal_call("Eagle.TransformComponent::SetScale_Native", Eagle::Script::Eagle_TransformComponent_SetScale);
	}

}