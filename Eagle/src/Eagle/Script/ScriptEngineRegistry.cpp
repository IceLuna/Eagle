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
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetForwardVectorFunctions;

	//Light Component
	std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetLightColorFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetLightColorFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;
	std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetIntensityFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetIntensityFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetCastsShadowsFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetCastsShadowsFunctions;

	//BaseColliderComponent
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsTriggerFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsTriggerFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetStaticFrictionFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetDynamicFrictionFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetBouncinessFrictionFunctions;
	std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetStaticFrictionFunctions;
	std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetDynamicFrictionFunctions;
	std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetBouncinessFunctions;

	extern MonoImage* s_CoreAssemblyImage;

#define REGISTER_COMPONENT_TYPE(Type)\
	{\
		MonoType* type = mono_reflection_type_from_name("Eagle." #Type, s_CoreAssemblyImage);\
		if (type)\
		{\
			m_HasComponentFunctions[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };\
			m_AddComponentFunctions[type] = [](Entity& entity) { entity.AddComponent<Type>(); };\
			\
			if (std::is_base_of<SceneComponent, Type>::value)\
			{\
				m_SetWorldTransformFunctions[type] = [](Entity& entity, const Transform* transform) { ((SceneComponent&)entity.GetComponent<Type>()).SetWorldTransform(*transform); };\
				m_SetRelativeTransformFunctions[type] = [](Entity& entity, const Transform* transform) { ((SceneComponent&)entity.GetComponent<Type>()).SetRelativeTransform(*transform); };\
				\
				m_GetWorldTransformFunctions[type] = [](Entity& entity, Transform* transform) { *transform = ((SceneComponent&)entity.GetComponent<Type>()).GetWorldTransform(); };\
				m_GetRelativeTransformFunctions[type] = [](Entity& entity, Transform* transform) { *transform = ((SceneComponent&)entity.GetComponent<Type>()).GetRelativeTransform(); };\
				m_GetForwardVectorFunctions[type] = [](Entity& entity, glm::vec3* outVector) { *outVector = ((SceneComponent&)entity.GetComponent<Type>()).GetForwardVector(); };\
			}\
			\
			if (std::is_base_of<LightComponent, Type>::value)\
			{\
				m_SetLightColorFunctions[type] = [](Entity& entity, const glm::vec3* value) { ((LightComponent&)entity.GetComponent<Type>()).SetLightColor(*value); };\
				m_GetLightColorFunctions[type] = [](Entity& entity, glm::vec3* outValue) { *outValue = ((LightComponent&)entity.GetComponent<Type>()).GetLightColor(); };\
				\
				m_SetAffectsWorldFunctions[type] = [](Entity& entity, bool value) { ((LightComponent&)entity.GetComponent<Type>()).SetAffectsWorld(value); };\
				m_GetAffectsWorldFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).DoesAffectWorld(); };\
				\
				m_GetIntensityFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).GetIntensity(); };\
				m_SetIntensityFunctions[type] = [](Entity& entity, float value) { return ((LightComponent&)entity.GetComponent<Type>()).SetIntensity(value); };\
				\
				m_SetCastsShadowsFunctions[type] = [](Entity& entity, bool value) { ((LightComponent&)entity.GetComponent<Type>()).SetCastsShadows(value); };\
				m_GetCastsShadowsFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).DoesCastShadows(); };\
				\
			}\
			if (std::is_base_of<BaseColliderComponent, Type>::value)\
			{\
				m_SetIsTriggerFunctions[type] = [](Entity& entity, bool bTrigger) { ((BaseColliderComponent&)entity.GetComponent<Type>()).SetIsTrigger(bTrigger); };\
				m_IsTriggerFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).IsTrigger(); };\
				m_SetStaticFrictionFunctions[type] = [](Entity& entity, float value) { auto& comp = ((BaseColliderComponent&)entity.GetComponent<Type>()); auto mat = MakeRef<PhysicsMaterial>(comp.GetPhysicsMaterial()); mat->StaticFriction = value; comp.SetPhysicsMaterial(mat); };\
				m_SetDynamicFrictionFunctions[type] = [](Entity& entity, float value) { auto& comp = ((BaseColliderComponent&)entity.GetComponent<Type>()); auto mat = MakeRef<PhysicsMaterial>(comp.GetPhysicsMaterial()); mat->DynamicFriction = value; comp.SetPhysicsMaterial(mat); };\
				m_SetBouncinessFrictionFunctions[type] = [](Entity& entity, float value) { auto& comp = ((BaseColliderComponent&)entity.GetComponent<Type>()); auto mat = MakeRef<PhysicsMaterial>(comp.GetPhysicsMaterial()); mat->Bounciness = value; comp.SetPhysicsMaterial(mat); };\
				m_GetStaticFrictionFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).GetPhysicsMaterial()->StaticFriction; };\
				m_GetDynamicFrictionFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).GetPhysicsMaterial()->DynamicFriction; };\
				m_GetBouncinessFunctions[type] = [](Entity& entity) { return ((BaseColliderComponent&)entity.GetComponent<Type>()).GetPhysicsMaterial()->Bounciness; };\
			}\
		}\
		else\
			EG_CORE_ERROR("No C# Component found for " #Type "!");\
	}

	static void InitComponentTypes()
	{
		REGISTER_COMPONENT_TYPE(TransformComponent);
		REGISTER_COMPONENT_TYPE(SceneComponent);
		REGISTER_COMPONENT_TYPE(PointLightComponent);
		REGISTER_COMPONENT_TYPE(DirectionalLightComponent);
		REGISTER_COMPONENT_TYPE(SpotLightComponent);
		REGISTER_COMPONENT_TYPE(StaticMeshComponent);
		REGISTER_COMPONENT_TYPE(AudioComponent);
		REGISTER_COMPONENT_TYPE(RigidBodyComponent);
		REGISTER_COMPONENT_TYPE(BoxColliderComponent);
		REGISTER_COMPONENT_TYPE(SphereColliderComponent);
		REGISTER_COMPONENT_TYPE(CapsuleColliderComponent);
		REGISTER_COMPONENT_TYPE(MeshColliderComponent);
		REGISTER_COMPONENT_TYPE(CameraComponent);
		REGISTER_COMPONENT_TYPE(ReverbComponent);
		REGISTER_COMPONENT_TYPE(TextComponent);
		REGISTER_COMPONENT_TYPE(BillboardComponent);
		REGISTER_COMPONENT_TYPE(SpriteComponent);
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
		mono_add_internal_call("Eagle.Entity::GetForwardVector_Native", Eagle::Script::Eagle_Entity_GetForwardVector);
		mono_add_internal_call("Eagle.Entity::GetRightVector_Native", Eagle::Script::Eagle_Entity_GetRightVector);
		mono_add_internal_call("Eagle.Entity::GetUpVector_Native", Eagle::Script::Eagle_Entity_GetUpVector);

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

		// Renderer
		mono_add_internal_call("Eagle.Renderer::SetFogSettings_Native", Eagle::Script::Eagle_Renderer_SetFogSettings);
		mono_add_internal_call("Eagle.Renderer::GetFogSettings_Native", Eagle::Script::Eagle_Renderer_GetFogSettings);
		mono_add_internal_call("Eagle.Renderer::SetBloomSettings_Native", Eagle::Script::Eagle_Renderer_SetBloomSettings);
		mono_add_internal_call("Eagle.Renderer::GetBloomSettings_Native", Eagle::Script::Eagle_Renderer_GetBloomSettings);
		mono_add_internal_call("Eagle.Renderer::SetSSAOSettings_Native", Eagle::Script::Eagle_Renderer_SetSSAOSettings);
		mono_add_internal_call("Eagle.Renderer::GetSSAOSettings_Native", Eagle::Script::Eagle_Renderer_GetSSAOSettings);
		mono_add_internal_call("Eagle.Renderer::SetGTAOSettings_Native", Eagle::Script::Eagle_Renderer_SetGTAOSettings);
		mono_add_internal_call("Eagle.Renderer::GetGTAOSettings_Native", Eagle::Script::Eagle_Renderer_GetGTAOSettings);
		mono_add_internal_call("Eagle.Renderer::SetPhotoLinearTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_SetPhotoLinearTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetPhotoLinearTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_GetPhotoLinearTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::SetFilmicTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_SetFilmicTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetFilmicTonemappingSettings_Native", Eagle::Script::Eagle_Renderer_GetFilmicTonemappingSettings);
		mono_add_internal_call("Eagle.Renderer::GetGamma_Native", Eagle::Script::Eagle_Renderer_GetGamma);
		mono_add_internal_call("Eagle.Renderer::SetGamma_Native", Eagle::Script::Eagle_Renderer_SetGamma);
		mono_add_internal_call("Eagle.Renderer::GetExposure_Native", Eagle::Script::Eagle_Renderer_GetExposure);
		mono_add_internal_call("Eagle.Renderer::SetExposure_Native", Eagle::Script::Eagle_Renderer_SetExposure);
		mono_add_internal_call("Eagle.Renderer::GetLineWidth_Native", Eagle::Script::Eagle_Renderer_GetLineWidth);
		mono_add_internal_call("Eagle.Renderer::SetLineWidth_Native", Eagle::Script::Eagle_Renderer_SetLineWidth);
		mono_add_internal_call("Eagle.Renderer::SetTonemappingMethod_Native", Eagle::Script::Eagle_Renderer_SetTonemappingMethod);
		mono_add_internal_call("Eagle.Renderer::GetTonemappingMethod_Native", Eagle::Script::Eagle_Renderer_GetTonemappingMethod);
		mono_add_internal_call("Eagle.Renderer::SetAAMethod_Native", Eagle::Script::Eagle_Renderer_SetAAMethod);
		mono_add_internal_call("Eagle.Renderer::GetAAMethod_Native", Eagle::Script::Eagle_Renderer_GetAAMethod);
		mono_add_internal_call("Eagle.Renderer::GetAO_Native", Eagle::Script::Eagle_Renderer_GetAO);
		mono_add_internal_call("Eagle.Renderer::SetAO_Native", Eagle::Script::Eagle_Renderer_SetAO);
		mono_add_internal_call("Eagle.Renderer::SetSoftShadowsEnabled_Native", Eagle::Script::Eagle_Renderer_SetSoftShadowsEnabled);
		mono_add_internal_call("Eagle.Renderer::GetSoftShadowsEnabled_Native", Eagle::Script::Eagle_Renderer_GetSoftShadowsEnabled);
		mono_add_internal_call("Eagle.Renderer::SetCSMSmoothTransitionEnabled_Native", Eagle::Script::Eagle_Renderer_SetCSMSmoothTransitionEnabled);
		mono_add_internal_call("Eagle.Renderer::GetCSMSmoothTransitionEnabled_Native", Eagle::Script::Eagle_Renderer_GetCSMSmoothTransitionEnabled);
		mono_add_internal_call("Eagle.Renderer::SetVisualizeCascades_Native", Eagle::Script::Eagle_Renderer_SetVisualizeCascades);
		mono_add_internal_call("Eagle.Renderer::GetVisualizeCascades_Native", Eagle::Script::Eagle_Renderer_GetVisualizeCascades);
		mono_add_internal_call("Eagle.Renderer::SetTransparencyLayers_Native", Eagle::Script::Eagle_Renderer_SetTransparencyLayers);
		mono_add_internal_call("Eagle.Renderer::GetTransparencyLayers_Native", Eagle::Script::Eagle_Renderer_GetTransparencyLayers);
		mono_add_internal_call("Eagle.Renderer::GetSkySettings_Native", Eagle::Script::Eagle_Renderer_GetSkySettings);
		mono_add_internal_call("Eagle.Renderer::SetSkySettings_Native", Eagle::Script::Eagle_Renderer_SetSkySettings);
		mono_add_internal_call("Eagle.Renderer::SetUseSkyAsBackground_Native", Eagle::Script::Eagle_Renderer_SetUseSkyAsBackground);
		mono_add_internal_call("Eagle.Renderer::GetUseSkyAsBackground_Native", Eagle::Script::Eagle_Renderer_GetUseSkyAsBackground);

		// Log
		mono_add_internal_call("Eagle.Log::Trace", Eagle::Script::Eagle_Log_Trace);
		mono_add_internal_call("Eagle.Log::Info", Eagle::Script::Eagle_Log_Info);
		mono_add_internal_call("Eagle.Log::Warn", Eagle::Script::Eagle_Log_Warn);
		mono_add_internal_call("Eagle.Log::Error", Eagle::Script::Eagle_Log_Error);
		mono_add_internal_call("Eagle.Log::Critical", Eagle::Script::Eagle_Log_Critical);

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

		mono_add_internal_call("Eagle.SceneComponent::GetForwardVector_Native", Eagle::Script::Eagle_SceneComponent_GetForwardVector);

		//Light Component
		mono_add_internal_call("Eagle.LightComponent::GetLightColor_Native", Eagle::Script::Eagle_LightComponent_GetLightColor);
		mono_add_internal_call("Eagle.LightComponent::GetIntensity_Native", Eagle::Script::Eagle_LightComponent_GetIntensity);
		mono_add_internal_call("Eagle.LightComponent::GetAffectsWorld_Native", Eagle::Script::Eagle_LightComponent_GetAffectsWorld);
		mono_add_internal_call("Eagle.LightComponent::GetCastsShadows_Native", Eagle::Script::Eagle_LightComponent_GetCastsShadows);
		mono_add_internal_call("Eagle.LightComponent::SetLightColor_Native", Eagle::Script::Eagle_LightComponent_SetLightColor);
		mono_add_internal_call("Eagle.LightComponent::SetIntensity_Native", Eagle::Script::Eagle_LightComponent_SetIntensity);
		mono_add_internal_call("Eagle.LightComponent::SetAffectsWorld_Native", Eagle::Script::Eagle_LightComponent_SetAffectsWorld);
		mono_add_internal_call("Eagle.LightComponent::SetCastsShadows_Native", Eagle::Script::Eagle_LightComponent_SetCastsShadows);
		
		//PointLight Component
		mono_add_internal_call("Eagle.PointLightComponent::GetRadius_Native", Eagle::Script::Eagle_PointLightComponent_GetRadius);
		mono_add_internal_call("Eagle.PointLightComponent::SetRadius_Native", Eagle::Script::Eagle_PointLightComponent_SetRadius);

		//SpotLight Component
		mono_add_internal_call("Eagle.SpotLightComponent::GetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::GetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetOuterCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_SetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_SetOuterCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::SetDistance_Native", Eagle::Script::Eagle_SpotLightComponent_SetDistance);
		mono_add_internal_call("Eagle.SpotLightComponent::GetDistance_Native", Eagle::Script::Eagle_SpotLightComponent_GetDistance);
	
		//Texture
		mono_add_internal_call("Eagle.Texture::IsValid_Native", Eagle::Script::Eagle_Texture_IsValid);
		
		//Texture2D
		mono_add_internal_call("Eagle.Texture2D::Create_Native", Eagle::Script::Eagle_Texture2D_Create);

		//Static Mesh
		mono_add_internal_call("Eagle.StaticMesh::Create_Native", Eagle::Script::Eagle_StaticMesh_Create);
		mono_add_internal_call("Eagle.StaticMesh::IsValid_Native", Eagle::Script::Eagle_StaticMesh_IsValid);

		//StaticMeshComponent
		mono_add_internal_call("Eagle.StaticMeshComponent::SetMesh_Native", Eagle::Script::Eagle_StaticMeshComponent_SetMesh);
		mono_add_internal_call("Eagle.StaticMeshComponent::GetMesh_Native", Eagle::Script::Eagle_StaticMeshComponent_GetMesh);
		mono_add_internal_call("Eagle.StaticMeshComponent::GetMaterial_Native", Eagle::Script::Eagle_StaticMeshComponent_GetMaterial);
		mono_add_internal_call("Eagle.StaticMeshComponent::SetMaterial_Native", Eagle::Script::Eagle_StaticMeshComponent_SetMaterial);
		mono_add_internal_call("Eagle.StaticMeshComponent::SetCastsShadows_Native", Eagle::Script::Eagle_StaticMeshComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.StaticMeshComponent::DoesCastShadows_Native", Eagle::Script::Eagle_StaticMeshComponent_DoesCastShadows);

		//Sound
		mono_add_internal_call("Eagle.Sound2D::Play_Native", Eagle::Script::Eagle_Sound2D_Play);
		mono_add_internal_call("Eagle.Sound3D::Play_Native", Eagle::Script::Eagle_Sound3D_Play);

		//AudioComponent
		mono_add_internal_call("Eagle.AudioComponent::SetMinDistance_Native", Eagle::Script::Eagle_AudioComponent_SetMinDistance);
		mono_add_internal_call("Eagle.AudioComponent::SetMaxDistance_Native", Eagle::Script::Eagle_AudioComponent_SetMaxDistance);
		mono_add_internal_call("Eagle.AudioComponent::SetMinMaxDistance_Native", Eagle::Script::Eagle_AudioComponent_SetMinMaxDistance);
		mono_add_internal_call("Eagle.AudioComponent::SetRollOffModel_Native", Eagle::Script::Eagle_AudioComponent_SetRollOffModel);
		mono_add_internal_call("Eagle.AudioComponent::SetVolume_Native", Eagle::Script::Eagle_AudioComponent_SetVolume);
		mono_add_internal_call("Eagle.AudioComponent::SetLoopCount_Native", Eagle::Script::Eagle_AudioComponent_SetLoopCount);
		mono_add_internal_call("Eagle.AudioComponent::SetLooping_Native", Eagle::Script::Eagle_AudioComponent_SetLooping);
		mono_add_internal_call("Eagle.AudioComponent::SetMuted_Native", Eagle::Script::Eagle_AudioComponent_SetMuted);
		mono_add_internal_call("Eagle.AudioComponent::SetSound_Native", Eagle::Script::Eagle_AudioComponent_SetSound);
		mono_add_internal_call("Eagle.AudioComponent::SetStreaming_Native", Eagle::Script::Eagle_AudioComponent_SetStreaming);
		mono_add_internal_call("Eagle.AudioComponent::Play_Native", Eagle::Script::Eagle_AudioComponent_Play);
		mono_add_internal_call("Eagle.AudioComponent::Stop_Native", Eagle::Script::Eagle_AudioComponent_Stop);
		mono_add_internal_call("Eagle.AudioComponent::SetPaused_Native", Eagle::Script::Eagle_AudioComponent_SetPaused);
		mono_add_internal_call("Eagle.AudioComponent::GetMinDistance_Native", Eagle::Script::Eagle_AudioComponent_GetMinDistance);
		mono_add_internal_call("Eagle.AudioComponent::GetMaxDistance_Native", Eagle::Script::Eagle_AudioComponent_GetMaxDistance);
		mono_add_internal_call("Eagle.AudioComponent::GetRollOffModel_Native", Eagle::Script::Eagle_AudioComponent_GetRollOffModel);
		mono_add_internal_call("Eagle.AudioComponent::GetVolume_Native", Eagle::Script::Eagle_AudioComponent_GetVolume);
		mono_add_internal_call("Eagle.AudioComponent::GetLoopCount_Native", Eagle::Script::Eagle_AudioComponent_GetLoopCount);
		mono_add_internal_call("Eagle.AudioComponent::IsLooping_Native", Eagle::Script::Eagle_AudioComponent_IsLooping);
		mono_add_internal_call("Eagle.AudioComponent::IsMuted_Native", Eagle::Script::Eagle_AudioComponent_IsMuted);
		mono_add_internal_call("Eagle.AudioComponent::IsStreaming_Native", Eagle::Script::Eagle_AudioComponent_IsStreaming);
		mono_add_internal_call("Eagle.AudioComponent::IsPlaying_Native", Eagle::Script::Eagle_AudioComponent_IsPlaying);

		//RigidBodyComponent
		mono_add_internal_call("Eagle.RigidBodyComponent::SetMass_Native", Eagle::Script::Eagle_RigidBodyComponent_SetMass);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetMass_Native", Eagle::Script::Eagle_RigidBodyComponent_GetMass);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLinearDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLinearDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetLinearDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_GetLinearDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetAngularDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_SetAngularDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::GetAngularDamping_Native", Eagle::Script::Eagle_RigidBodyComponent_GetAngularDamping);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetEnableGravity_Native", Eagle::Script::Eagle_RigidBodyComponent_SetEnableGravity);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsGravityEnabled_Native", Eagle::Script::Eagle_RigidBodyComponent_IsGravityEnabled);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetIsKinematic_Native", Eagle::Script::Eagle_RigidBodyComponent_SetIsKinematic);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsKinematic_Native", Eagle::Script::Eagle_RigidBodyComponent_IsKinematic);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockPosition_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockPosition);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockPositionX_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockPositionX);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockPositionY_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockPositionY);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockPositionZ_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockPositionZ);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockRotation_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockRotation);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockRotationX_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockRotationX);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockRotationY_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockRotationY);
		mono_add_internal_call("Eagle.RigidBodyComponent::SetLockRotationZ_Native", Eagle::Script::Eagle_RigidBodyComponent_SetLockRotationZ);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsPositionXLocked_Native", Eagle::Script::Eagle_RigidBodyComponent_IsPositionXLocked);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsPositionYLocked_Native", Eagle::Script::Eagle_RigidBodyComponent_IsPositionYLocked);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsPositionZLocked_Native", Eagle::Script::Eagle_RigidBodyComponent_IsPositionZLocked);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsRotationXLocked_Native", Eagle::Script::Eagle_RigidBodyComponent_IsRotationXLocked);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsRotationYLocked_Native", Eagle::Script::Eagle_RigidBodyComponent_IsRotationYLocked);
		mono_add_internal_call("Eagle.RigidBodyComponent::IsRotationZLocked_Native", Eagle::Script::Eagle_RigidBodyComponent_IsRotationZLocked);

		//BaseColliderComponent
		mono_add_internal_call("Eagle.BaseColliderComponent::SetIsTrigger_Native", Eagle::Script::Eagle_BaseColliderComponent_SetIsTrigger);
		mono_add_internal_call("Eagle.BaseColliderComponent::IsTrigger_Native", Eagle::Script::Eagle_BaseColliderComponent_IsTrigger);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetStaticFriction_Native", Eagle::Script::Eagle_BaseColliderComponent_SetStaticFriction);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetDynamicFriction_Native", Eagle::Script::Eagle_BaseColliderComponent_SetDynamicFriction);
		mono_add_internal_call("Eagle.BaseColliderComponent::SetBounciness_Native", Eagle::Script::Eagle_BaseColliderComponent_SetBounciness);
		mono_add_internal_call("Eagle.BaseColliderComponent::GetStaticFriction_Native", Eagle::Script::Eagle_BaseColliderComponent_GetStaticFriction);
		mono_add_internal_call("Eagle.BaseColliderComponent::GetDynamicFriction_Native", Eagle::Script::Eagle_BaseColliderComponent_GetDynamicFriction);
		mono_add_internal_call("Eagle.BaseColliderComponent::GetBounciness_Native", Eagle::Script::Eagle_BaseColliderComponent_GetBounciness);

		//BoxColliderComponent
		mono_add_internal_call("Eagle.BoxColliderComponent::SetSize_Native", Eagle::Script::Eagle_BoxColliderComponent_SetSize);
		mono_add_internal_call("Eagle.BoxColliderComponent::GetSize_Native", Eagle::Script::Eagle_BoxColliderComponent_GetSize);

		//SphereColliderComponent
		mono_add_internal_call("Eagle.SphereColliderComponent::SetRadius_Native", Eagle::Script::Eagle_SphereColliderComponent_SetRadius);
		mono_add_internal_call("Eagle.SphereColliderComponent::GetRadius_Native", Eagle::Script::Eagle_SphereColliderComponent_GetRadius);

		//CapsuleColliderComponent
		mono_add_internal_call("Eagle.CapsuleColliderComponent::SetRadius_Native", Eagle::Script::Eagle_CapsuleColliderComponent_SetRadius);
		mono_add_internal_call("Eagle.CapsuleColliderComponent::GetRadius_Native", Eagle::Script::Eagle_CapsuleColliderComponent_GetRadius);
		mono_add_internal_call("Eagle.CapsuleColliderComponent::SetHeight_Native", Eagle::Script::Eagle_CapsuleColliderComponent_SetHeight);
		mono_add_internal_call("Eagle.CapsuleColliderComponent::GetHeight_Native", Eagle::Script::Eagle_CapsuleColliderComponent_GetHeight);

		//MeshColliderComponent
		mono_add_internal_call("Eagle.MeshColliderComponent::SetIsConvex_Native", Eagle::Script::Eagle_MeshColliderComponent_SetIsConvex);
		mono_add_internal_call("Eagle.MeshColliderComponent::IsConvex_Native", Eagle::Script::Eagle_MeshColliderComponent_IsConvex);
		mono_add_internal_call("Eagle.MeshColliderComponent::SetCollisionMesh_Native", Eagle::Script::Eagle_MeshColliderComponent_SetCollisionMesh);
		mono_add_internal_call("Eagle.MeshColliderComponent::GetCollisionMesh_Native", Eagle::Script::Eagle_MeshColliderComponent_GetCollisionMesh);

		// Camera Component
		mono_add_internal_call("Eagle.CameraComponent::SetIsPrimary_Native", Eagle::Script::Eagle_CameraComponent_SetIsPrimary);
		mono_add_internal_call("Eagle.CameraComponent::GetIsPrimary_Native", Eagle::Script::Eagle_CameraComponent_GetIsPrimary);
		mono_add_internal_call("Eagle.CameraComponent::SetPerspectiveVerticalFOV_Native", Eagle::Script::Eagle_CameraComponent_SetPerspectiveVerticalFOV);
		mono_add_internal_call("Eagle.CameraComponent::GetPerspectiveVerticalFOV_Native", Eagle::Script::Eagle_CameraComponent_GetPerspectiveVerticalFOV);
		mono_add_internal_call("Eagle.CameraComponent::SetPerspectiveNearClip_Native", Eagle::Script::Eagle_CameraComponent_SetPerspectiveNearClip);
		mono_add_internal_call("Eagle.CameraComponent::GetPerspectiveNearClip_Native", Eagle::Script::Eagle_CameraComponent_GetPerspectiveNearClip);
		mono_add_internal_call("Eagle.CameraComponent::SetPerspectiveFarClip_Native", Eagle::Script::Eagle_CameraComponent_SetPerspectiveFarClip);
		mono_add_internal_call("Eagle.CameraComponent::GetPerspectiveFarClip_Native", Eagle::Script::Eagle_CameraComponent_GetPerspectiveFarClip);
		mono_add_internal_call("Eagle.CameraComponent::SetShadowFarClip_Native", Eagle::Script::Eagle_CameraComponent_SetShadowFarClip);
		mono_add_internal_call("Eagle.CameraComponent::GetShadowFarClip_Native", Eagle::Script::Eagle_CameraComponent_GetShadowFarClip);
		mono_add_internal_call("Eagle.CameraComponent::GetCameraProjectionMode_Native", Eagle::Script::Eagle_CameraComponent_GetCameraProjectionMode);
		mono_add_internal_call("Eagle.CameraComponent::SetCameraProjectionMode_Native", Eagle::Script::Eagle_CameraComponent_SetCameraProjectionMode);

		// Reverb component
		mono_add_internal_call("Eagle.ReverbComponent::IsActive_Native", Eagle::Script::Eagle_ReverbComponent_IsActive);
		mono_add_internal_call("Eagle.ReverbComponent::SetIsActive_Native", Eagle::Script::Eagle_ReverbComponent_SetIsActive);
		mono_add_internal_call("Eagle.ReverbComponent::GetReverbPreset_Native", Eagle::Script::Eagle_ReverbComponent_GetReverbPreset);
		mono_add_internal_call("Eagle.ReverbComponent::SetReverbPreset_Native", Eagle::Script::Eagle_ReverbComponent_SetReverbPreset);
		mono_add_internal_call("Eagle.ReverbComponent::GetMinDistance_Native", Eagle::Script::Eagle_ReverbComponent_GetMinDistance);
		mono_add_internal_call("Eagle.ReverbComponent::SetMinDistance_Native", Eagle::Script::Eagle_ReverbComponent_SetMinDistance);
		mono_add_internal_call("Eagle.ReverbComponent::GetMaxDistance_Native", Eagle::Script::Eagle_ReverbComponent_GetMaxDistance);
		mono_add_internal_call("Eagle.ReverbComponent::SetMaxDistance_Native", Eagle::Script::Eagle_ReverbComponent_SetMaxDistance);

		// Text Component
		mono_add_internal_call("Eagle.TextComponent::GetText_Native", Eagle::Script::Eagle_TextComponent_GetText);
		mono_add_internal_call("Eagle.TextComponent::SetText_Native", Eagle::Script::Eagle_TextComponent_SetText);
		mono_add_internal_call("Eagle.TextComponent::GetColor_Native", Eagle::Script::Eagle_TextComponent_GetColor);
		mono_add_internal_call("Eagle.TextComponent::SetColor_Native", Eagle::Script::Eagle_TextComponent_SetColor);
		mono_add_internal_call("Eagle.TextComponent::GetLineSpacing_Native", Eagle::Script::Eagle_TextComponent_GetLineSpacing);
		mono_add_internal_call("Eagle.TextComponent::SetLineSpacing_Native", Eagle::Script::Eagle_TextComponent_SetLineSpacing);
		mono_add_internal_call("Eagle.TextComponent::GetKerning_Native", Eagle::Script::Eagle_TextComponent_GetKerning);
		mono_add_internal_call("Eagle.TextComponent::SetKerning_Native", Eagle::Script::Eagle_TextComponent_SetKerning);
		mono_add_internal_call("Eagle.TextComponent::GetMaxWidth_Native", Eagle::Script::Eagle_TextComponent_GetMaxWidth);
		mono_add_internal_call("Eagle.TextComponent::SetMaxWidth_Native", Eagle::Script::Eagle_TextComponent_SetMaxWidth);
		mono_add_internal_call("Eagle.TextComponent::GetAlbedo_Native", Eagle::Script::Eagle_TextComponent_GetAlbedo);
		mono_add_internal_call("Eagle.TextComponent::SetAlbedo_Native", Eagle::Script::Eagle_TextComponent_SetAlbedo);
		mono_add_internal_call("Eagle.TextComponent::GetEmissive_Native", Eagle::Script::Eagle_TextComponent_GetEmissive);
		mono_add_internal_call("Eagle.TextComponent::SetEmissive_Native", Eagle::Script::Eagle_TextComponent_SetEmissive);
		mono_add_internal_call("Eagle.TextComponent::GetMetallness_Native", Eagle::Script::Eagle_TextComponent_GetMetallness);
		mono_add_internal_call("Eagle.TextComponent::SetMetallness_Native", Eagle::Script::Eagle_TextComponent_SetMetallness);
		mono_add_internal_call("Eagle.TextComponent::GetRoughness_Native", Eagle::Script::Eagle_TextComponent_GetRoughness);
		mono_add_internal_call("Eagle.TextComponent::SetRoughness_Native", Eagle::Script::Eagle_TextComponent_SetRoughness);
		mono_add_internal_call("Eagle.TextComponent::GetAO_Native", Eagle::Script::Eagle_TextComponent_GetAO);
		mono_add_internal_call("Eagle.TextComponent::SetAO_Native", Eagle::Script::Eagle_TextComponent_SetAO);
		mono_add_internal_call("Eagle.TextComponent::GetIsLit_Native", Eagle::Script::Eagle_TextComponent_GetIsLit);
		mono_add_internal_call("Eagle.TextComponent::SetIsLit_Native", Eagle::Script::Eagle_TextComponent_SetIsLit);
		mono_add_internal_call("Eagle.TextComponent::SetCastsShadows_Native", Eagle::Script::Eagle_TextComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.TextComponent::DoesCastShadows_Native", Eagle::Script::Eagle_TextComponent_DoesCastShadows);

		// Billboard Component
		mono_add_internal_call("Eagle.BillboardComponent::SetTexture_Native", Eagle::Script::Eagle_BillboardComponent_SetTexture);
		mono_add_internal_call("Eagle.BillboardComponent::GetTexture_Native", Eagle::Script::Eagle_BillboardComponent_GetTexture);

		// Sprite Component
		mono_add_internal_call("Eagle.SpriteComponent::GetMaterial_Native", Eagle::Script::Eagle_SpriteComponent_GetMaterial);
		mono_add_internal_call("Eagle.SpriteComponent::SetMaterial_Native", Eagle::Script::Eagle_SpriteComponent_SetMaterial);
		mono_add_internal_call("Eagle.SpriteComponent::SetSubtexture_Native", Eagle::Script::Eagle_SpriteComponent_SetSubtexture);
		mono_add_internal_call("Eagle.SpriteComponent::GetSubtexture_Native", Eagle::Script::Eagle_SpriteComponent_GetSubtexture);
		mono_add_internal_call("Eagle.SpriteComponent::GetSubtextureCoords_Native", Eagle::Script::Eagle_SpriteComponent_GetSubtextureCoords);
		mono_add_internal_call("Eagle.SpriteComponent::SetSubtextureCoords_Native", Eagle::Script::Eagle_SpriteComponent_SetSubtextureCoords);
		mono_add_internal_call("Eagle.SpriteComponent::GetSpriteSize_Native", Eagle::Script::Eagle_SpriteComponent_GetSpriteSize);
		mono_add_internal_call("Eagle.SpriteComponent::SetSpriteSize_Native", Eagle::Script::Eagle_SpriteComponent_SetSpriteSize);
		mono_add_internal_call("Eagle.SpriteComponent::GetSpriteSizeCoef_Native", Eagle::Script::Eagle_SpriteComponent_GetSpriteSizeCoef);
		mono_add_internal_call("Eagle.SpriteComponent::SetSpriteSizeCoef_Native", Eagle::Script::Eagle_SpriteComponent_SetSpriteSizeCoef);
		mono_add_internal_call("Eagle.SpriteComponent::GetIsSubtexture_Native", Eagle::Script::Eagle_SpriteComponent_GetIsSubtexture);
		mono_add_internal_call("Eagle.SpriteComponent::SetIsSubtexture_Native", Eagle::Script::Eagle_SpriteComponent_SetIsSubtexture);
		mono_add_internal_call("Eagle.SpriteComponent::SetCastsShadows_Native", Eagle::Script::Eagle_SpriteComponent_SetCastsShadows);
		mono_add_internal_call("Eagle.SpriteComponent::DoesCastShadows_Native", Eagle::Script::Eagle_SpriteComponent_DoesCastShadows);
	}
}
