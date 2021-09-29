#pragma once

#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsUtils.h"
#include "Eagle/Input/Input.h"

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
}

namespace Eagle::Script
{
	//Entity
	GUID Eagle_Entity_GetParent(GUID entityID);
	void Eagle_Entity_SetParent(GUID entityID, GUID parentID);
	MonoArray* Eagle_Entity_GetChildren(GUID entityID);
	GUID Eagle_Entity_CreateEntity();
	void Eagle_Entity_DestroyEntity(GUID entityID);
	void Eagle_Entity_AddComponent(GUID entityID, void* type);
	bool Eagle_Entity_HasComponent(GUID entityID, void* type);
	MonoString* Eagle_Entity_GetEntityName(GUID entityID);

	//Entity-Physics
	void Eagle_Entity_WakeUp(GUID entityID);
	void Eagle_Entity_PutToSleep(GUID entityID);
	float Eagle_Entity_GetMass(GUID entityID);
	void Eagle_Entity_SetMass(GUID entityID, float mass);
	void Eagle_Entity_AddForce(GUID entityID, const glm::vec3* force, ForceMode forceMode);
	void Eagle_Entity_AddTorque(GUID entityID, const glm::vec3* force, ForceMode forceMode);
	void Eagle_Entity_GetLinearVelocity(GUID entityID, glm::vec3* result);
	void Eagle_Entity_SetLinearVelocity(GUID entityID, const glm::vec3* velocity);
	void Eagle_Entity_GetAngularVelocity(GUID entityID, glm::vec3* result);
	void Eagle_Entity_SetAngularVelocity(GUID entityID, const glm::vec3* velocity);
	float Eagle_Entity_GetMaxLinearVelocity(GUID entityID);
	void Eagle_Entity_SetMaxLinearVelocity(GUID entityID, float maxVelocity);
	float Eagle_Entity_GetMaxAngularVelocity(GUID entityID);
	void Eagle_Entity_SetMaxAngularVelocity(GUID entityID, float maxVelocity);
	void Eagle_Entity_SetLinearDamping(GUID entityID, float damping);
	void Eagle_Entity_SetAngularDamping(GUID entityID, float damping);
	bool Eagle_Entity_IsDynamic(GUID entityID);
	bool Eagle_Entity_IsKinematic(GUID entityID);
	bool Eagle_Entity_IsGravityEnabled(GUID entityID);
	bool Eagle_Entity_IsLockFlagSet(GUID entityID, ActorLockFlag flag);
	ActorLockFlag Eagle_Entity_GetLockFlags(GUID entityID);
	void Eagle_Entity_SetKinematic(GUID entityID, bool value);
	void Eagle_Entity_SetGravityEnabled(GUID entityID, bool value);
	void Eagle_Entity_SetLockFlag(GUID entityID, ActorLockFlag flag, bool value);

	//Input
	bool Eagle_Input_IsMouseButtonPressed(Mouse button);
	bool Eagle_Input_IsKeyPressed(Key keyCode);
	void Eagle_Input_GetMousePosition(glm::vec2* outPosition);
	void Eagle_Input_SetCursorMode(CursorMode mode);
	CursorMode Eagle_Input_GetCursorMode();

	//Transform Component
	void Eagle_TransformComponent_GetWorldTransform(GUID entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetWorldLocation(GUID entityID, glm::vec3* outLocation);
	void Eagle_TransformComponent_GetWorldRotation(GUID entityID, glm::vec3* outRotation);
	void Eagle_TransformComponent_GetWorldScale(GUID entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetWorldTransform(GUID entityID, const Transform* inTransform);
	void Eagle_TransformComponent_SetWorldLocation(GUID entityID, const glm::vec3* inLocation);
	void Eagle_TransformComponent_SetWorldRotation(GUID entityID, const glm::vec3* inRotation);
	void Eagle_TransformComponent_SetWorldScale(GUID entityID, const glm::vec3* inScale);

	void Eagle_TransformComponent_GetRelativeTransform(GUID entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetRelativeLocation(GUID entityID, glm::vec3* outLocation);
	void Eagle_TransformComponent_GetRelativeRotation(GUID entityID, glm::vec3* outRotation);
	void Eagle_TransformComponent_GetRelativeScale(GUID entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetRelativeTransform(GUID entityID, const Transform* inTransform);
	void Eagle_TransformComponent_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation);
	void Eagle_TransformComponent_SetRelativeRotation(GUID entityID, const glm::vec3* inRotation);
	void Eagle_TransformComponent_SetRelativeScale(GUID entityID, const glm::vec3* inScale);

	//SceneComponent
	void Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform);
	void Eagle_SceneComponent_GetWorldLocation(GUID entityID, void* type, glm::vec3* outLocation);
	void Eagle_SceneComponent_GetWorldRotation(GUID entityID, void* type, glm::vec3* outRotation);
	void Eagle_SceneComponent_GetWorldScale(GUID entityID, void* type, glm::vec3* outScale);
	void Eagle_SceneComponent_SetWorldTransform(GUID entityID, void* type, const Transform* inTransform);
	void Eagle_SceneComponent_SetWorldLocation(GUID entityID, void* type, const glm::vec3* inLocation);
	void Eagle_SceneComponent_SetWorldRotation(GUID entityID, void* type, const glm::vec3* inRotation);
	void Eagle_SceneComponent_SetWorldScale(GUID entityID, void* type, const glm::vec3* inScale);

	void Eagle_SceneComponent_GetRelativeTransform(GUID entityID, void* type, Transform* outTransform);
	void Eagle_SceneComponent_GetRelativeLocation(GUID entityID, void* type, glm::vec3* outLocation);
	void Eagle_SceneComponent_GetRelativeRotation(GUID entityID, void* type, glm::vec3* outRotation);
	void Eagle_SceneComponent_GetRelativeScale(GUID entityID, void* type, glm::vec3* outScale);
	void Eagle_SceneComponent_SetRelativeTransform(GUID entityID, void* type, const Transform* inTransform);
	void Eagle_SceneComponent_SetRelativeLocation(GUID entityID, void* type, const glm::vec3* inLocation);
	void Eagle_SceneComponent_SetRelativeRotation(GUID entityID, void* type, const glm::vec3* inRotation);
	void Eagle_SceneComponent_SetRelativeScale(GUID entityID, void* type, const glm::vec3* inScale);

	//PointLight Component
	void Eagle_PointLightComponent_GetLightColor(GUID entityID, glm::vec3* outLightColor);
	void Eagle_PointLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor);
	void Eagle_PointLightComponent_GetSpecularColor(GUID entityID, glm::vec3* outSpecularColor);
	void Eagle_PointLightComponent_GetIntensity(GUID entityID, float* outIntensity);
	void Eagle_PointLightComponent_SetLightColor(GUID entityID, glm::vec3* inLightColor);
	void Eagle_PointLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor);
	void Eagle_PointLightComponent_SetSpecularColor(GUID entityID, glm::vec3* inSpecularColor);
	void Eagle_PointLightComponent_SetIntensity(GUID entityID, float inIntensity);

	//DirectionalLight Component
	void Eagle_DirectionalLightComponent_GetLightColor(GUID entityID, glm::vec3* outLightColor);
	void Eagle_DirectionalLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor);
	void Eagle_DirectionalLightComponent_GetSpecularColor(GUID entityID, glm::vec3* outSpecularColor);
	void Eagle_DirectionalLightComponent_SetLightColor(GUID entityID, glm::vec3* inLightColor);
	void Eagle_DirectionalLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor);
	void Eagle_DirectionalLightComponent_SetSpecularColor(GUID entityID, glm::vec3* inSpecularColor);

	//SpotLight Component
	void Eagle_SpotLightComponent_GetLightColor(GUID entityID, glm::vec3* outLightColor);
	void Eagle_SpotLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor);
	void Eagle_SpotLightComponent_GetSpecularColor(GUID entityID, glm::vec3* outSpecularColor);
	void Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID, float* outInnerCutoffAngle);
	void Eagle_SpotLightComponent_GetOuterCutoffAngle(GUID entityID, float* outOuterCutoffAngle);
	void Eagle_SpotLightComponent_SetLightColor(GUID entityID, glm::vec3* inLightColor);
	void Eagle_SpotLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor);
	void Eagle_SpotLightComponent_SetSpecularColor(GUID entityID, glm::vec3* inSpecularColor);
	void Eagle_SpotLightComponent_SetInnerCutoffAngle(GUID entityID, float inInnerCutoffAngle);
	void Eagle_SpotLightComponent_SetOuterCutoffAngle(GUID entityID, float inOuterCutoffAngle);

	void Eagle_SpotLightComponent_SetIntensity(GUID entityID, float intensity);
	void Eagle_SpotLightComponent_GetIntensity(GUID entityID, float* outIntensity);

	//Texture
	bool Eagle_Texture_IsValid(GUID guid);

	//Texture2D
	GUID Eagle_Texture2D_Create(MonoString* texturePath);

	//Static Mesh
	GUID Eagle_StaticMesh_Create(MonoString* meshPath);
	bool Eagle_StaticMesh_IsValid(GUID GUID);
	void Eagle_StaticMesh_SetDiffuseTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetSpecularTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetNormalTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetScalarMaterialParams(GUID parentID, GUID meshID, const glm::vec4* tintColor, float tilingFactor, float shininess);
	void Eagle_StaticMesh_GetMaterial(GUID parentID, GUID meshID, GUID* diffuse, GUID* specular, GUID* normal, glm::vec4* tint, float* tilingFactor, float* shininess);

	//StaticMeshComponent
	void Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid);
	GUID Eagle_StaticMeshComponent_GetMesh(GUID entityID);
}
