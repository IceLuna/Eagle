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
	std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetAmbientFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetAmbientFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetSpecularFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetSpecularFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;

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
				m_SetLightColorFunctions[type] = [](Entity& entity, const glm::vec3* value) { ((LightComponent&)entity.GetComponent<Type>()).LightColor = *value; };\
				m_GetLightColorFunctions[type] = [](Entity& entity, glm::vec3* outValue) { *outValue = ((LightComponent&)entity.GetComponent<Type>()).LightColor; };\
				\
				m_SetAmbientFunctions[type] = [](Entity& entity, const glm::vec3* value) { ((LightComponent&)entity.GetComponent<Type>()).Ambient = *value; };\
				m_GetAmbientFunctions[type] = [](Entity& entity, glm::vec3* outValue) { *outValue = ((LightComponent&)entity.GetComponent<Type>()).Ambient; };\
				\
				m_SetSpecularFunctions[type] = [](Entity& entity, const glm::vec3* value) { ((LightComponent&)entity.GetComponent<Type>()).Specular = *value; };\
				m_GetSpecularFunctions[type] = [](Entity& entity, glm::vec3* outValue) { *outValue = ((LightComponent&)entity.GetComponent<Type>()).Specular; };\
				\
				m_SetAffectsWorldFunctions[type] = [](Entity& entity, bool value) { ((LightComponent&)entity.GetComponent<Type>()).bAffectsWorld = value; };\
				m_GetAffectsWorldFunctions[type] = [](Entity& entity) { return ((LightComponent&)entity.GetComponent<Type>()).bAffectsWorld; };\
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
		mono_add_internal_call("Eagle.LightComponent::GetAmbientColor_Native", Eagle::Script::Eagle_LightComponent_GetAmbientColor);
		mono_add_internal_call("Eagle.LightComponent::GetSpecularColor_Native", Eagle::Script::Eagle_LightComponent_GetSpecularColor);
		mono_add_internal_call("Eagle.LightComponent::GetAffectsWorld_Native", Eagle::Script::Eagle_LightComponent_GetAffectsWorld);
		mono_add_internal_call("Eagle.LightComponent::SetLightColor_Native", Eagle::Script::Eagle_LightComponent_SetLightColor);
		mono_add_internal_call("Eagle.LightComponent::SetAmbientColor_Native", Eagle::Script::Eagle_LightComponent_SetAmbientColor);
		mono_add_internal_call("Eagle.LightComponent::SetSpecularColor_Native", Eagle::Script::Eagle_LightComponent_SetSpecularColor);
		mono_add_internal_call("Eagle.LightComponent::SetAffectsWorld_Native", Eagle::Script::Eagle_LightComponent_SetAffectsWorld);
		
		//PointLight Component
		mono_add_internal_call("Eagle.PointLightComponent::GetIntensity_Native", Eagle::Script::Eagle_PointLightComponent_GetIntensity);
		mono_add_internal_call("Eagle.PointLightComponent::SetIntensity_Native", Eagle::Script::Eagle_PointLightComponent_SetIntensity);

		//DirectionalLight Component

		//SpotLight Component
		mono_add_internal_call("Eagle.SpotLightComponent::GetInnerCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetInnerCutoffAngle);
		mono_add_internal_call("Eagle.SpotLightComponent::GetOuterCutoffAngle_Native", Eagle::Script::Eagle_SpotLightComponent_GetOuterCutoffAngle);
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
	}
}
