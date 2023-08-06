#pragma once

#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsUtils.h"
#include "Eagle/Input/Input.h"

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
	typedef struct _MonoReflectionType MonoReflectionType;
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
	float Eagle_LightComponent_GetIntensity(GUID entityID, void* type);
	void Eagle_LightComponent_SetIntensity(GUID entityID, void* type, float inIntensity);
	bool Eagle_LightComponent_GetCastsShadows(GUID entityID, void* type);
	void Eagle_LightComponent_SetCastsShadows(GUID entityID, void* type, bool value);

	//PointLight Component
	float Eagle_PointLightComponent_GetRadius(GUID entityID);
	void Eagle_PointLightComponent_SetRadius(GUID entityID, float inRadius);

	//SpotLight Component
	float Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID);
	float Eagle_SpotLightComponent_GetOuterCutoffAngle(GUID entityID);
	float Eagle_SpotLightComponent_GetDistance(GUID entityID);
	void Eagle_SpotLightComponent_SetInnerCutoffAngle(GUID entityID, float inInnerCutoffAngle);
	void Eagle_SpotLightComponent_SetOuterCutoffAngle(GUID entityID, float inOuterCutoffAngle);
	void Eagle_SpotLightComponent_SetDistance(GUID entityID, float inDistance);

	//Texture
	bool Eagle_Texture_IsValid(GUID guid);

	//Texture2D
	GUID Eagle_Texture2D_Create(MonoString* texturePath);

	//Static Mesh
	GUID Eagle_StaticMesh_Create(MonoString* meshPath);
	bool Eagle_StaticMesh_IsValid(GUID GUID);

	//StaticMeshComponent
	void Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid);
	GUID Eagle_StaticMeshComponent_GetMesh(GUID entityID);
	void Eagle_StaticMeshComponent_GetMaterial(GUID entityID, GUID* outAlbedo, GUID* outMetallness, GUID* outNormal, GUID* outRoughness, GUID* outAO, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode);
	void Eagle_StaticMeshComponent_SetMaterial(GUID entityID, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode);
	void Eagle_StaticMeshComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_StaticMeshComponent_DoesCastShadows(GUID entityID);

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

	// Camera Component
	void Eagle_CameraComponent_SetIsPrimary(GUID entityID, bool val);
	bool Eagle_CameraComponent_GetIsPrimary(GUID entityID);
	float Eagle_CameraComponent_GetPerspectiveVerticalFOV(GUID entityID);
	void Eagle_CameraComponent_SetPerspectiveVerticalFOV(GUID entityID, float value);
	float Eagle_CameraComponent_GetPerspectiveNearClip(GUID entityID);
	void Eagle_CameraComponent_SetPerspectiveNearClip(GUID entityID, float value);
	float Eagle_CameraComponent_GetPerspectiveFarClip(GUID entityID);
	void Eagle_CameraComponent_SetPerspectiveFarClip(GUID entityID, float value);
	float Eagle_CameraComponent_GetShadowFarClip(GUID entityID);
	void Eagle_CameraComponent_SetShadowFarClip(GUID entityID, float value);
	CameraProjectionMode Eagle_CameraComponent_GetCameraProjectionMode(GUID entityID);
	void Eagle_CameraComponent_SetCameraProjectionMode(GUID entityID, CameraProjectionMode value);

	// Reverb Component
	bool Eagle_ReverbComponent_IsActive(GUID entityID);
	void Eagle_ReverbComponent_SetIsActive(GUID entityID, bool value);
	ReverbPreset Eagle_ReverbComponent_GetReverbPreset(GUID entityID);
	void Eagle_ReverbComponent_SetReverbPreset(GUID entityID, ReverbPreset value);
	float Eagle_ReverbComponent_GetMinDistance(GUID entityID);
	void Eagle_ReverbComponent_SetMinDistance(GUID entityID, float value);
	float Eagle_ReverbComponent_GetMaxDistance(GUID entityID);
	void Eagle_ReverbComponent_SetMaxDistance(GUID entityID, float value);

	// Text Component
	MonoString* Eagle_TextComponent_GetText(GUID entityID);
	void Eagle_TextComponent_SetText(GUID entityID, MonoString* value);
	void Eagle_TextComponent_GetColor(GUID entityID, glm::vec3* outValue);
	void Eagle_TextComponent_SetColor(GUID entityID, const glm::vec3* value);
	float Eagle_TextComponent_GetLineSpacing(GUID entityID);
	void Eagle_TextComponent_SetLineSpacing(GUID entityID, float value);
	float Eagle_TextComponent_GetKerning(GUID entityID);
	void Eagle_TextComponent_SetKerning(GUID entityID, float value);
	float Eagle_TextComponent_GetMaxWidth(GUID entityID);
	void Eagle_TextComponent_SetMaxWidth(GUID entityID, float value);
	void Eagle_TextComponent_GetAlbedo(GUID entityID, glm::vec3* outValue);
	void Eagle_TextComponent_SetAlbedo(GUID entityID, const glm::vec3* value);
	void Eagle_TextComponent_GetEmissive(GUID entityID, glm::vec3* outValue);
	void Eagle_TextComponent_SetEmissive(GUID entityID, const glm::vec3* value);
	void Eagle_TextComponent_SetMetallness(GUID entityID, float value);
	float Eagle_TextComponent_GetMetallness(GUID entityID);
	void Eagle_TextComponent_SetRoughness(GUID entityID, float value);
	float Eagle_TextComponent_GetRoughness(GUID entityID);
	void Eagle_TextComponent_SetAO(GUID entityID, float value);
	float Eagle_TextComponent_GetAO(GUID entityID);
	void Eagle_TextComponent_SetIsLit(GUID entityID, bool value);
	bool Eagle_TextComponent_GetIsLit(GUID entityID);
	void Eagle_TextComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_TextComponent_DoesCastShadows(GUID entityID);
	void Eagle_TextComponent_SetOpacity(GUID entityID, float value);
	float Eagle_TextComponent_GetOpacity(GUID entityID);
	void Eagle_TextComponent_SetOpacityMask(GUID entityID, float value);
	float Eagle_TextComponent_GetOpacityMask(GUID entityID);

	// Billboard Component
	void Eagle_BillboardComponent_SetTexture(GUID entityID, GUID textureID);
	GUID Eagle_BillboardComponent_GetTexture(GUID entityID);

	// Sprite component
	void Eagle_SpriteComponent_GetMaterial(GUID entityID, GUID* outAlbedo, GUID* outMetallness, GUID* outNormal, GUID* outRoughness, GUID* outAO, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode);
	void Eagle_SpriteComponent_SetMaterial(GUID entityID, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode);
	void Eagle_SpriteComponent_SetSubtexture(GUID entityID, GUID subtexture);
	GUID Eagle_SpriteComponent_GetSubtexture(GUID entityID);
	void Eagle_SpriteComponent_GetSubtextureCoords(GUID entityID, glm::vec2* outValue);
	void Eagle_SpriteComponent_SetSubtextureCoords(GUID entityID, const glm::vec2* value);
	void Eagle_SpriteComponent_GetSpriteSize(GUID entityID, glm::vec2* outValue);
	void Eagle_SpriteComponent_SetSpriteSize(GUID entityID, const glm::vec2* value);
	void Eagle_SpriteComponent_GetSpriteSizeCoef(GUID entityID, glm::vec2* outValue);
	void Eagle_SpriteComponent_SetSpriteSizeCoef(GUID entityID, const glm::vec2* value);
	bool Eagle_SpriteComponent_GetIsSubtexture(GUID entityID);
	void Eagle_SpriteComponent_SetIsSubtexture(GUID entityID, bool value);
	void Eagle_SpriteComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_SpriteComponent_DoesCastShadows(GUID entityID);

	// Script Component
	MonoReflectionType* Eagle_ScriptComponent_GetScriptType(GUID entityID);

	// Renderer
	void Eagle_Renderer_SetFogSettings(const glm::vec3* color, float minDistance, float maxDistance, float density, FogEquation equation, bool bEnabled);
	void Eagle_Renderer_GetFogSettings(glm::vec3* outcolor, float* outMinDistance, float* outMaxDistance, float* outDensity, FogEquation* outEquation, bool* outbEnabled);
	void Eagle_Renderer_SetBloomSettings(GUID dirt, float threashold, float intensity, float dirtIntensity, float knee, bool bEnabled);
	void Eagle_Renderer_GetBloomSettings(GUID* outDirtTexture, float* outThreashold, float* outIntensity, float* outDirtIntensity, float* outKnee, bool* outbEnabled);
	void Eagle_Renderer_SetSSAOSettings(uint32_t samples, float radius, float bias);
	void Eagle_Renderer_GetSSAOSettings(uint32_t* outSamples, float* outRadius, float* outBias);
	void Eagle_Renderer_SetGTAOSettings(uint32_t samples, float radius);
	void Eagle_Renderer_GetGTAOSettings(uint32_t* outSamples, float* outRadius);
	void Eagle_Renderer_SetPhotoLinearTonemappingSettings(float sensetivity, float exposureTime, float fStop);
	void Eagle_Renderer_GetPhotoLinearTonemappingSettings(float* outSensetivity, float* outExposureTime, float* outfStop);
	void Eagle_Renderer_SetFilmicTonemappingSettings(float whitePoint);
	void Eagle_Renderer_GetFilmicTonemappingSettings(float* outWhitePoint);
	float Eagle_Renderer_GetGamma();
	void Eagle_Renderer_SetGamma(float value);
	float Eagle_Renderer_GetExposure();
	void Eagle_Renderer_SetExposure(float value);
	float Eagle_Renderer_GetLineWidth();
	void Eagle_Renderer_SetLineWidth(float value);
	TonemappingMethod Eagle_Renderer_GetTonemappingMethod();
	void Eagle_Renderer_SetTonemappingMethod(TonemappingMethod value);
	AmbientOcclusion Eagle_Renderer_GetAO();
	void Eagle_Renderer_SetAO(AmbientOcclusion value);
	void Eagle_Renderer_SetSoftShadowsEnabled(bool value);
	bool Eagle_Renderer_GetSoftShadowsEnabled();
	void Eagle_Renderer_SetCSMSmoothTransitionEnabled(bool value);
	bool Eagle_Renderer_GetCSMSmoothTransitionEnabled();
	void Eagle_Renderer_SetVisualizeCascades(bool value);
	bool Eagle_Renderer_GetVisualizeCascades();
	void Eagle_Renderer_SetTransparencyLayers(uint32_t value);
	uint32_t Eagle_Renderer_GetTransparencyLayers();
	AAMethod Eagle_Renderer_GetAAMethod();
	void Eagle_Renderer_SetAAMethod(AAMethod value);
	void Eagle_Renderer_GetSkySettings(glm::vec3* sunPos, glm::vec3* cloudsColor, float* skyIntensity, float* cloudsIntensity, float* scattering, float* cirrus, float* cumulus, uint32_t* cumulusLayers, bool* bCirrus, bool* bCumulus);
	void Eagle_Renderer_SetSkySettings(const glm::vec3* sunPos, const glm::vec3* cloudsColor, float skyIntensity, float cloudsIntensity, float scattering, float cirrus, float cumulus, uint32_t cumulusLayers, bool bEnableCirrusClouds, bool bEnableCumulusClouds);
	void Eagle_Renderer_SetUseSkyAsBackground(bool value);
	bool Eagle_Renderer_GetUseSkyAsBackground();

	// Log
	void Eagle_Log_Trace(MonoString* message);
	void Eagle_Log_Info(MonoString* message);
	void Eagle_Log_Warn(MonoString* message);
	void Eagle_Log_Error(MonoString* message);
	void Eagle_Log_Critical(MonoString* message);
}
