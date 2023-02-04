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
	void Eagle_Entity_GetForwardVector(GUID entityID, glm::vec3* result);
	void Eagle_Entity_GetRightVector(GUID entityID, glm::vec3* result);
	void Eagle_Entity_GetUpVector(GUID entityID, glm::vec3* result);

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
	void Eagle_TransformComponent_GetWorldRotation(GUID entityID, Rotator* outRotation);
	void Eagle_TransformComponent_GetWorldScale(GUID entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetWorldTransform(GUID entityID, const Transform* inTransform);
	void Eagle_TransformComponent_SetWorldLocation(GUID entityID, const glm::vec3* inLocation);
	void Eagle_TransformComponent_SetWorldRotation(GUID entityID, const Rotator* inRotation);
	void Eagle_TransformComponent_SetWorldScale(GUID entityID, const glm::vec3* inScale);

	void Eagle_TransformComponent_GetRelativeTransform(GUID entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetRelativeLocation(GUID entityID, glm::vec3* outLocation);
	void Eagle_TransformComponent_GetRelativeRotation(GUID entityID, Rotator* outRotation);
	void Eagle_TransformComponent_GetRelativeScale(GUID entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetRelativeTransform(GUID entityID, const Transform* inTransform);
	void Eagle_TransformComponent_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation);
	void Eagle_TransformComponent_SetRelativeRotation(GUID entityID, const Rotator* inRotation);
	void Eagle_TransformComponent_SetRelativeScale(GUID entityID, const glm::vec3* inScale);

	//SceneComponent
	void Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform);
	void Eagle_SceneComponent_GetWorldLocation(GUID entityID, void* type, glm::vec3* outLocation);
	void Eagle_SceneComponent_GetWorldRotation(GUID entityID, void* type, Rotator* outRotation);
	void Eagle_SceneComponent_GetWorldScale(GUID entityID, void* type, glm::vec3* outScale);
	void Eagle_SceneComponent_SetWorldTransform(GUID entityID, void* type, const Transform* inTransform);
	void Eagle_SceneComponent_SetWorldLocation(GUID entityID, void* type, const glm::vec3* inLocation);
	void Eagle_SceneComponent_SetWorldRotation(GUID entityID, void* type, const Rotator* inRotation);
	void Eagle_SceneComponent_SetWorldScale(GUID entityID, void* type, const glm::vec3* inScale);

	void Eagle_SceneComponent_GetRelativeTransform(GUID entityID, void* type, Transform* outTransform);
	void Eagle_SceneComponent_GetRelativeLocation(GUID entityID, void* type, glm::vec3* outLocation);
	void Eagle_SceneComponent_GetRelativeRotation(GUID entityID, void* type, Rotator* outRotation);
	void Eagle_SceneComponent_GetRelativeScale(GUID entityID, void* type, glm::vec3* outScale);
	void Eagle_SceneComponent_SetRelativeTransform(GUID entityID, void* type, const Transform* inTransform);
	void Eagle_SceneComponent_SetRelativeLocation(GUID entityID, void* type, const glm::vec3* inLocation);
	void Eagle_SceneComponent_SetRelativeRotation(GUID entityID, void* type, const Rotator* inRotation);
	void Eagle_SceneComponent_SetRelativeScale(GUID entityID, void* type, const glm::vec3* inScale);

	void Eagle_SceneComponent_GetForwardVector(GUID entityID, void* type, glm::vec3* outVector);

	//LightComponent
	void Eagle_LightComponent_GetLightColor(GUID entityID, void* type, glm::vec3* outLightColor);
	bool Eagle_LightComponent_GetAffectsWorld(GUID entityID, void* type);
	void Eagle_LightComponent_SetLightColor(GUID entityID, void* type, glm::vec3* inLightColor);
	void Eagle_LightComponent_SetAffectsWorld(GUID entityID, void* type, bool bAffectsWorld);

	//PointLight Component
	void Eagle_PointLightComponent_GetIntensity(GUID entityID, float* outIntensity);
	void Eagle_PointLightComponent_SetIntensity(GUID entityID, float inIntensity);

	//DirectionalLight Component
	void Eagle_DirectionalLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor);
	void Eagle_DirectionalLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor);

	//SpotLight Component
	void Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID, float* outInnerCutoffAngle);
	void Eagle_SpotLightComponent_GetOuterCutoffAngle(GUID entityID, float* outOuterCutoffAngle);
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
	void Eagle_StaticMesh_SetAlbedoTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetMetallnessTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetNormalTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetRoughnessTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetAOTexture(GUID parentID, GUID meshID, GUID textureID);
	void Eagle_StaticMesh_SetScalarMaterialParams(GUID parentID, GUID meshID, const glm::vec4* tintColor, float tilingFactor);
	void Eagle_StaticMesh_GetMaterial(GUID parentID, GUID meshID, GUID* albedo, GUID* metallness, GUID* normal, GUID* roughness, GUID* ao, glm::vec4* tint, float* tilingFactor);

	//StaticMeshComponent
	void Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid);
	GUID Eagle_StaticMeshComponent_GetMesh(GUID entityID);

	//Sound2D
	void Eagle_Sound2D_Play(MonoString* path, float volume, int loopCount);

	//Sound3D
	void Eagle_Sound3D_Play(MonoString* path, const glm::vec3* position, float volume, int loopCount);

	//AudioComponent
	void Eagle_AudioComponent_SetMinDistance(GUID entityID, float minDistance);
	void Eagle_AudioComponent_SetMaxDistance(GUID entityID, float maxDistance);
	void Eagle_AudioComponent_SetMinMaxDistance(GUID entityID, float minDistance, float maxDistance);
	void Eagle_AudioComponent_SetRollOffModel(GUID entityID, RollOffModel rollOff);
	void Eagle_AudioComponent_SetVolume(GUID entityID, float volume);
	void Eagle_AudioComponent_SetLoopCount(GUID entityID, int loopCount);
	void Eagle_AudioComponent_SetLooping(GUID entityID, bool bLooping);
	void Eagle_AudioComponent_SetMuted(GUID entityID, bool bMuted);
	void Eagle_AudioComponent_SetSound(GUID entityID, MonoString* filepath);
	void Eagle_AudioComponent_SetStreaming(GUID entityID, bool bStreaming);
	void Eagle_AudioComponent_Play(GUID entityID);
	void Eagle_AudioComponent_Stop(GUID entityID);
	void Eagle_AudioComponent_SetPaused(GUID entityID, bool bPaused);
	float Eagle_AudioComponent_GetMinDistance(GUID entityID);
	float Eagle_AudioComponent_GetMaxDistance(GUID entityID);
	RollOffModel Eagle_AudioComponent_GetRollOffModel(GUID entityID);
	float Eagle_AudioComponent_GetVolume(GUID entityID);
	int Eagle_AudioComponent_GetLoopCount(GUID entityID);
	bool Eagle_AudioComponent_IsLooping(GUID entityID);
	bool Eagle_AudioComponent_IsMuted(GUID entityID);
	bool Eagle_AudioComponent_IsStreaming(GUID entityID);
	bool Eagle_AudioComponent_IsPlaying(GUID entityID);
	
	//RigidBodyComponent
	void Eagle_RigidBodyComponent_SetMass(GUID entityID, float mass);
	float Eagle_RigidBodyComponent_GetMass(GUID entityID);
	void Eagle_RigidBodyComponent_SetLinearDamping(GUID entityID, float linearDamping);
	float Eagle_RigidBodyComponent_GetLinearDamping(GUID entityID);
	void Eagle_RigidBodyComponent_SetAngularDamping(GUID entityID, float angularDamping);
	float Eagle_RigidBodyComponent_GetAngularDamping(GUID entityID);
	void Eagle_RigidBodyComponent_SetEnableGravity(GUID entityID, bool bEnable);
	bool Eagle_RigidBodyComponent_IsGravityEnabled(GUID entityID);
	void Eagle_RigidBodyComponent_SetIsKinematic(GUID entityID, bool bKinematic);
	bool Eagle_RigidBodyComponent_IsKinematic(GUID entityID);
	void Eagle_RigidBodyComponent_SetLockPosition(GUID entityID, bool bLockX, bool bLockY, bool bLockZ);
	void Eagle_RigidBodyComponent_SetLockPositionX(GUID entityID, bool bLock);
	void Eagle_RigidBodyComponent_SetLockPositionY(GUID entityID, bool bLock);
	void Eagle_RigidBodyComponent_SetLockPositionZ(GUID entityID, bool bLock);
	void Eagle_RigidBodyComponent_SetLockRotation(GUID entityID, bool bLockX, bool bLockY, bool bLockZ);
	void Eagle_RigidBodyComponent_SetLockRotationX(GUID entityID, bool bLock);
	void Eagle_RigidBodyComponent_SetLockRotationY(GUID entityID, bool bLock);
	void Eagle_RigidBodyComponent_SetLockRotationZ(GUID entityID, bool bLock);
	bool Eagle_RigidBodyComponent_IsPositionXLocked(GUID entityID);
	bool Eagle_RigidBodyComponent_IsPositionYLocked(GUID entityID);
	bool Eagle_RigidBodyComponent_IsPositionZLocked(GUID entityID);
	bool Eagle_RigidBodyComponent_IsRotationXLocked(GUID entityID);
	bool Eagle_RigidBodyComponent_IsRotationYLocked(GUID entityID);
	bool Eagle_RigidBodyComponent_IsRotationZLocked(GUID entityID);

	//BaseColliderComponent
	void  Eagle_BaseColliderComponent_SetIsTrigger(GUID entityID, void* type, bool bTrigger);
	bool  Eagle_BaseColliderComponent_IsTrigger(GUID entityID, void* type);
	void  Eagle_BaseColliderComponent_SetStaticFriction(GUID entityID, void* type, float staticFriction);
	void  Eagle_BaseColliderComponent_SetDynamicFriction(GUID entityID, void* type, float dynamicFriction);
	void  Eagle_BaseColliderComponent_SetBounciness(GUID entityID, void* type, float bounciness);
	float Eagle_BaseColliderComponent_GetStaticFriction(GUID entityID, void* type);
	float Eagle_BaseColliderComponent_GetDynamicFriction(GUID entityID, void* type);
	float Eagle_BaseColliderComponent_GetBounciness(GUID entityID, void* type);

	//BoxColliderComponent
	void Eagle_BoxColliderComponent_SetSize(GUID entityID, const glm::vec3* size);
	void Eagle_BoxColliderComponent_GetSize(GUID entityID, glm::vec3* outSize);

	//SphereColliderComponent
	void  Eagle_SphereColliderComponent_SetRadius(GUID entityID, float val);
	float Eagle_SphereColliderComponent_GetRadius(GUID entityID);

	//CapsuleColliderComponent
	void  Eagle_CapsuleColliderComponent_SetRadius(GUID entityID, float val);
	float Eagle_CapsuleColliderComponent_GetRadius(GUID entityID);
	void  Eagle_CapsuleColliderComponent_SetHeight(GUID entityID, float val);
	float Eagle_CapsuleColliderComponent_GetHeight(GUID entityID);

	//MeshColliderComponent
	void Eagle_MeshColliderComponent_SetIsConvex(GUID entityID, bool val);
	bool Eagle_MeshColliderComponent_IsConvex(GUID entityID);
	void Eagle_MeshColliderComponent_SetCollisionMesh(GUID entityID, GUID meshGUID);
	GUID Eagle_MeshColliderComponent_GetCollisionMesh(GUID entityID);
}
