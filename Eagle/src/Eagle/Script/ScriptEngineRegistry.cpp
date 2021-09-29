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

	//SceneComponents
	std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;

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

#define REGISTER_SCENECOMPONENT_TYPE(Type)\
	{\
		static_assert(std::is_base_of<SceneComponent, Type>::value, "Not Scene Component!");\
		MonoType* type = mono_reflection_type_from_name("Eagle." #Type, s_CoreAssemblyImage);\
		if (type)\
		{\
			m_HasComponentFunctions[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };\
			m_AddComponentFunctions[type] = [](Entity& entity) { entity.AddComponent<Type>(); };\
			\
			m_SetWorldTransformFunctions[type] = [](Entity& entity, const Transform* transform) { entity.GetComponent<Type>().SetWorldTransform(*transform); };\
			m_SetRelativeTransformFunctions[type] = [](Entity& entity, const Transform* transform) { entity.GetComponent<Type>().SetRelativeTransform(*transform); };\
			\
			m_GetWorldTransformFunctions[type] = [](Entity& entity, Transform* transform) { *transform = entity.GetComponent<Type>().GetWorldTransform(); };\
			m_GetRelativeTransformFunctions[type] = [](Entity& entity, Transform* transform) { *transform = entity.GetComponent<Type>().GetRelativeTransform(); };\
		}\
		else\
			EG_CORE_ERROR("No C# Component found for " #Type "!");\
	}


	static void InitComponentTypes()
	{
		REGISTER_COMPONENT_TYPE(TransformComponent);
		REGISTER_SCENECOMPONENT_TYPE(SceneComponent);
		REGISTER_SCENECOMPONENT_TYPE(PointLightComponent);
		REGISTER_SCENECOMPONENT_TYPE(DirectionalLightComponent);
		REGISTER_SCENECOMPONENT_TYPE(SpotLightComponent);
		REGISTER_SCENECOMPONENT_TYPE(StaticMeshComponent);
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

		//Entity-Physics
		mono_add_internal_call("Eagle.Entity::WakeUp_Native", Eagle::Script::Eagle_Entity_WakeUp);
		mono_add_internal_call("Eagle.Entity::PutToSleep_Native", Eagle::Script::Eagle_Entity_PutToSleep);
		mono_add_internal_call("Eagle.Entity::GetMass_Native", Eagle::Script::Eagle_Entity_GetMass);
		mono_add_internal_call("Eagle.Entity::SetMass_Native", Eagle::Script::Eagle_Entity_SetMass);
		mono_add_internal_call("Eagle.Entity::AddForce_Native", Eagle::Script::Eagle_Entity_AddForce);
		mono_add_internal_call("Eagle.Entity::AddTorque_Native", Eagle::Script::Eagle_Entity_AddTorque);
		mono_add_internal_call("Eagle.Entity::GetLinearVelocity_Native", Eagle::Script::Eagle_Entity_GetLinearVelocity);
		mono_add_internal_call("Eagle.Entity::SetLinearVelocity_Native", Eagle::Script::Eagle_Entity_SetLinearVelocity);
		mono_add_internal_call("Eagle.Entity::GetAngularVelocity_Native", Eagle::Script::Eagle_Entity_GetAngularVelocity);
		mono_add_internal_call("Eagle.Entity::SetAngularVelocity_Native", Eagle::Script::Eagle_Entity_SetAngularVelocity);
		mono_add_internal_call("Eagle.Entity::GetMaxLinearVelocity_Native", Eagle::Script::Eagle_Entity_GetMaxLinearVelocity);
		mono_add_internal_call("Eagle.Entity::SetMaxLinearVelocity_Native", Eagle::Script::Eagle_Entity_SetMaxLinearVelocity);
		mono_add_internal_call("Eagle.Entity::GetMaxAngularVelocity_Native", Eagle::Script::Eagle_Entity_GetMaxAngularVelocity);
		mono_add_internal_call("Eagle.Entity::SetMaxAngularVelocity_Native", Eagle::Script::Eagle_Entity_SetMaxAngularVelocity);
		mono_add_internal_call("Eagle.Entity::SetLinearDamping_Native", Eagle::Script::Eagle_Entity_SetLinearDamping);
		mono_add_internal_call("Eagle.Entity::SetAngularDamping_Native", Eagle::Script::Eagle_Entity_SetAngularDamping);
		mono_add_internal_call("Eagle.Entity::IsDynamic_Native", Eagle::Script::Eagle_Entity_IsDynamic);
		mono_add_internal_call("Eagle.Entity::IsKinematic_Native", Eagle::Script::Eagle_Entity_IsKinematic);
		mono_add_internal_call("Eagle.Entity::IsGravityEnabled_Native", Eagle::Script::Eagle_Entity_IsGravityEnabled);
		mono_add_internal_call("Eagle.Entity::IsLockFlagSet_Native", Eagle::Script::Eagle_Entity_IsLockFlagSet);
		mono_add_internal_call("Eagle.Entity::GetLockFlags_Native", Eagle::Script::Eagle_Entity_GetLockFlags);
		mono_add_internal_call("Eagle.Entity::SetKinematic_Native", Eagle::Script::Eagle_Entity_SetKinematic);
		mono_add_internal_call("Eagle.Entity::SetGravityEnabled_Native", Eagle::Script::Eagle_Entity_SetGravityEnabled);
		mono_add_internal_call("Eagle.Entity::SetLockFlag_Native", Eagle::Script::Eagle_Entity_SetLockFlag);

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

		//Scene Component
		mono_add_internal_call("Eagle.SceneComponent::GetWorldTransform_Native", Eagle::Script::Eagle_SceneComponent_GetWorldTransform);
		mono_add_internal_call("Eagle.SceneComponent::GetWorldLocation_Native", Eagle::Script::Eagle_SceneComponent_GetWorldLocation);
		mono_add_internal_call("Eagle.SceneComponent::GetWorldRotation_Native", Eagle::Script::Eagle_SceneComponent_GetWorldRotation);
		mono_add_internal_call("Eagle.SceneComponent::GetWorldScale_Native", Eagle::Script::Eagle_SceneComponent_GetWorldScale);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldTransform_Native", Eagle::Script::Eagle_SceneComponent_SetWorldTransform);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldLocation_Native", Eagle::Script::Eagle_SceneComponent_SetWorldLocation);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldRotation_Native", Eagle::Script::Eagle_SceneComponent_SetWorldRotation);
		mono_add_internal_call("Eagle.SceneComponent::SetWorldScale_Native", Eagle::Script::Eagle_SceneComponent_SetWorldScale);

		mono_add_internal_call("Eagle.SceneComponent::GetRelativeTransform_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeTransform);
		mono_add_internal_call("Eagle.SceneComponent::GetRelativeLocation_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeLocation);
		mono_add_internal_call("Eagle.SceneComponent::GetRelativeRotation_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeRotation);
		mono_add_internal_call("Eagle.SceneComponent::GetRelativeScale_Native", Eagle::Script::Eagle_SceneComponent_GetRelativeScale);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeTransform_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeTransform);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeLocation_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeLocation);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeRotation_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeRotation);
		mono_add_internal_call("Eagle.SceneComponent::SetRelativeScale_Native", Eagle::Script::Eagle_SceneComponent_SetRelativeScale);

		//PointLight Component
		mono_add_internal_call("Eagle.PointLightComponent::GetLightColor_Native", Eagle::Script::Eagle_PointLightComponent_GetLightColor);
		mono_add_internal_call("Eagle.PointLightComponent::GetAmbientColor_Native", Eagle::Script::Eagle_PointLightComponent_GetAmbientColor);
		mono_add_internal_call("Eagle.PointLightComponent::GetSpecularColor_Native", Eagle::Script::Eagle_PointLightComponent_GetSpecularColor);
		mono_add_internal_call("Eagle.PointLightComponent::GetIntensity_Native", Eagle::Script::Eagle_PointLightComponent_GetIntensity);
		mono_add_internal_call("Eagle.PointLightComponent::SetLightColor_Native", Eagle::Script::Eagle_PointLightComponent_SetLightColor);
		mono_add_internal_call("Eagle.PointLightComponent::SetAmbientColor_Native", Eagle::Script::Eagle_PointLightComponent_SetAmbientColor);
		mono_add_internal_call("Eagle.PointLightComponent::SetSpecularColor_Native", Eagle::Script::Eagle_PointLightComponent_SetSpecularColor);
		mono_add_internal_call("Eagle.PointLightComponent::SetIntensity_Native", Eagle::Script::Eagle_PointLightComponent_SetIntensity);

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
		mono_add_internal_call("Eagle.SpotLightComponent::SetIntensity_Native", Eagle::Script::Eagle_SpotLightComponent_SetIntensity);
		mono_add_internal_call("Eagle.SpotLightComponent::GetIntensity_Native", Eagle::Script::Eagle_SpotLightComponent_GetIntensity);
	
		//Texture
		mono_add_internal_call("Eagle.Texture::IsValid_Native", Eagle::Script::Eagle_Texture_IsValid);
		
		//Texture2D
		mono_add_internal_call("Eagle.Texture2D::Create_Native", Eagle::Script::Eagle_Texture2D_Create);

		//Static Mesh
		mono_add_internal_call("Eagle.StaticMesh::Create_Native", Eagle::Script::Eagle_StaticMesh_Create);
		mono_add_internal_call("Eagle.StaticMesh::IsValid_Native", Eagle::Script::Eagle_StaticMesh_IsValid);
		mono_add_internal_call("Eagle.StaticMesh::SetDiffuseTexture_Native", Eagle::Script::Eagle_StaticMesh_SetDiffuseTexture);
		mono_add_internal_call("Eagle.StaticMesh::SetSpecularTexture_Native", Eagle::Script::Eagle_StaticMesh_SetSpecularTexture);
		mono_add_internal_call("Eagle.StaticMesh::SetNormalTexture_Native", Eagle::Script::Eagle_StaticMesh_SetNormalTexture);
		mono_add_internal_call("Eagle.StaticMesh::SetScalarMaterialParams_Native", Eagle::Script::Eagle_StaticMesh_SetScalarMaterialParams);
		mono_add_internal_call("Eagle.StaticMesh::GetMaterial_Native", Eagle::Script::Eagle_StaticMesh_GetMaterial);

		//StaticMeshComponent
		mono_add_internal_call("Eagle.StaticMeshComponent::SetMesh_Native", Eagle::Script::Eagle_StaticMeshComponent_SetMesh);
		mono_add_internal_call("Eagle.StaticMeshComponent::GetMesh_Native", Eagle::Script::Eagle_StaticMeshComponent_GetMesh);
	}

}