#pragma once

#include "Eagle/Components/Components.h"
#include "Eagle/Physics/PhysicsEngine.h"
#include "Eagle/Input/Input.h"

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
	typedef struct _MonoReflectionType MonoReflectionType;
}

namespace Eagle::AINavigation
{
	struct AgentSettings;
	struct CrowdSettings;
}

namespace Eagle::Script
{
	//Entity
	GUID Eagle_Entity_GetParent(GUID entityID);
	void Eagle_Entity_SetParent(GUID entityID, GUID parentID);
	MonoArray* Eagle_Entity_GetChildren(GUID entityID);
	void Eagle_Entity_DestroyEntity(GUID entityID);
	void Eagle_Entity_AddComponent(GUID entityID, void* type);
	bool Eagle_Entity_HasComponent(GUID entityID, void* type);
	bool Eagle_Entity_IsValid(GUID entityID);
	MonoString* Eagle_Entity_GetEntityName(GUID entityID);
	void Eagle_Entity_GetForwardVector(GUID entityID, glm::vec3* result);
	void Eagle_Entity_GetRightVector(GUID entityID, glm::vec3* result);
	void Eagle_Entity_GetUpVector(GUID entityID, glm::vec3* result);
	GUID Eagle_Entity_GetChildrenByName(GUID entityID, MonoString* name);
	bool Eagle_Entity_IsMouseHovered(GUID entity);
	bool Eagle_Entity_IsMouseHoveredByCoord(GUID entity, const glm::vec2* pos);
	GUID Eagle_Entity_SpawnEntity(MonoString* monoName);
	GUID Eagle_Entity_SpawnEntityFromAsset(GUID assetID);

	void Eagle_Entity_GetWorldTransform(GUID entityID, Transform* outTransform);
	void Eagle_Entity_GetWorldLocation(GUID entityID, glm::vec3* outLocation);
	void Eagle_Entity_GetWorldRotation(GUID entityID, Rotator* outRotation);
	void Eagle_Entity_GetWorldScale(GUID entityID, glm::vec3* outScale);
	void Eagle_Entity_SetWorldTransform(GUID entityID, const Transform* inTransform);
	void Eagle_Entity_SetWorldLocation(GUID entityID, const glm::vec3* inLocation);
	void Eagle_Entity_SetWorldRotation(GUID entityID, const Rotator* inRotation);
	void Eagle_Entity_SetWorldScale(GUID entityID, const glm::vec3* inScale);

	void Eagle_Entity_GetRelativeTransform(GUID entityID, Transform* outTransform);
	void Eagle_Entity_GetRelativeLocation(GUID entityID, glm::vec3* outLocation);
	void Eagle_Entity_GetRelativeRotation(GUID entityID, Rotator* outRotation);
	void Eagle_Entity_GetRelativeScale(GUID entityID, glm::vec3* outScale);
	void Eagle_Entity_SetRelativeTransform(GUID entityID, const Transform* inTransform);
	void Eagle_Entity_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation);
	void Eagle_Entity_SetRelativeRotation(GUID entityID, const Rotator* inRotation);
	void Eagle_Entity_SetRelativeScale(GUID entityID, const glm::vec3* inScale);

	//Input
	bool Eagle_Input_IsMouseButtonPressed(Mouse button);
	bool Eagle_Input_IsKeyPressed(Key keyCode);
	void Eagle_Input_GetMousePosition(glm::vec2* outPosition);
	void Eagle_Input_GetMousePositionInViewport(glm::vec2* outPosition);
	void Eagle_Input_SetCursorMode(CursorMode mode);
	CursorMode Eagle_Input_GetCursorMode();
	void Eagle_Input_SetMousePosition(const glm::vec2* position);
	void Eagle_Input_SetMousePositionInViewport(const glm::vec2* position);

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
	void Eagle_SceneComponent_GetRightVector(GUID entityID, void* type, glm::vec3* outVector);
	void Eagle_SceneComponent_GetUpVector(GUID entityID, void* type, glm::vec3* outVector);

	//LightComponent
	void Eagle_LightComponent_GetLightColor(GUID entityID, void* type, glm::vec3* outLightColor);
	bool Eagle_LightComponent_GetAffectsWorld(GUID entityID, void* type);
	void Eagle_LightComponent_SetLightColor(GUID entityID, void* type, glm::vec3* inLightColor);
	void Eagle_LightComponent_SetAffectsWorld(GUID entityID, void* type, bool bAffectsWorld);
	float Eagle_LightComponent_GetIntensity(GUID entityID, void* type);
	void Eagle_LightComponent_SetIntensity(GUID entityID, void* type, float inIntensity);
	float Eagle_LightComponent_GetVolumetricFogIntensity(GUID entityID, void* type);
	void Eagle_LightComponent_SetVolumetricFogIntensity(GUID entityID, void* type, float inIntensity);
	bool Eagle_LightComponent_GetCastsShadows(GUID entityID, void* type);
	void Eagle_LightComponent_SetCastsShadows(GUID entityID, void* type, bool value);
	bool Eagle_LightComponent_GetIsVolumetricLight(GUID entityID, void* type);
	void Eagle_LightComponent_SetIsVolumetricLight(GUID entityID, void* type, bool value);

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

	// DirectionalLightComponent
	void Eagle_DirectionalLightComponent_GetAmbient(GUID entityID, glm::vec3* outAmbient);
	void Eagle_DirectionalLightComponent_SetAmbient(GUID entityID, glm::vec3* inAmbient);

	//StaticMeshComponent
	void Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid);
	GUID Eagle_StaticMeshComponent_GetMesh(GUID entityID);
	void Eagle_StaticMeshComponent_GetMaterial(GUID entityID, uint32_t index, GUID* outAssetID);
	void Eagle_StaticMeshComponent_SetMaterial(GUID entityID, uint32_t index, GUID assetID);
	uint32_t Eagle_StaticMeshComponent_GetMaterialsSlotsCount(GUID entityID);
	void Eagle_StaticMeshComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_StaticMeshComponent_DoesCastShadows(GUID entityID);
	void Eagle_StaticMeshComponent_SetReceivesDecals(GUID entityID, bool value);
	bool Eagle_StaticMeshComponent_DoesReceiveDecals(GUID entityID);

	// SkeletalMeshComponent
	void Eagle_SkeletalMeshComponent_SetMesh(GUID entityID, GUID guid);
	GUID Eagle_SkeletalMeshComponent_GetMesh(GUID entityID);
	void Eagle_SkeletalMeshComponent_GetMaterial(GUID entityID, uint32_t index, GUID* outAssetID);
	void Eagle_SkeletalMeshComponent_SetMaterial(GUID entityID, uint32_t index, GUID assetID);
	void Eagle_SkeletalMeshComponent_GetAnimation(GUID entityID, GUID* outAssetID);
	void Eagle_SkeletalMeshComponent_SetAnimation(GUID entityID, GUID assetID);
	void Eagle_SkeletalMeshComponent_GetAnimationGraph(GUID entityID, GUID* outAssetID);
	void Eagle_SkeletalMeshComponent_SetAnimationGraph(GUID entityID, GUID assetID);
	uint32_t Eagle_SkeletalMeshComponent_GetMaterialsSlotsCount(GUID entityID);
	void Eagle_SkeletalMeshComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_SkeletalMeshComponent_DoesCastShadows(GUID entityID);
	AnimationType Eagle_SkeletalMeshComponent_GetAnimType(GUID entityID);
	void Eagle_SkeletalMeshComponent_SetAnimType(GUID entityID, AnimationType value);
	void Eagle_SkeletalMeshComponent_SetReceivesDecals(GUID entityID, bool value);
	bool Eagle_SkeletalMeshComponent_DoesReceiveDecals(GUID entityID);
	bool Eagle_SkeletalMeshComponent_IsRootMotionLockFlagSet(GUID entityID, RootMotionLockFlag value);
	void Eagle_SkeletalMeshComponent_SetRootMotionLockFlagBool(GUID entityID, RootMotionLockFlag flag, bool value);
	void Eagle_SkeletalMeshComponent_SetRootMotionLockFlag(GUID entityID, RootMotionLockFlag value);
	RootMotionLockFlag Eagle_SkeletalMeshComponent_GetRootMotionLockFlags(GUID entityID);

	void Eagle_SkeletalMeshComponent_SetCurrentClipPlayTime(GUID entityID, float value);
	void Eagle_SkeletalMeshComponent_SetClipPlaybackSpeed(GUID entityID, float value);
	void Eagle_SkeletalMeshComponent_SetIsClipLooping(GUID entityID, bool value);
	float Eagle_SkeletalMeshComponent_GetCurrentClipPlayTime(GUID entityID);
	float Eagle_SkeletalMeshComponent_GetClipPlaybackSpeed(GUID entityID);
	bool Eagle_SkeletalMeshComponent_IsClipLooping(GUID entityID);

	void Eagle_SkeletalMeshComponent_SetAnimGraphVariableBool(GUID entityID, MonoString* monoName, bool value);
	void Eagle_SkeletalMeshComponent_SetAnimGraphVariableInt(GUID entityID, MonoString* monoName, int value);
	void Eagle_SkeletalMeshComponent_SetAnimGraphVariableFloat(GUID entityID, MonoString* monoName, float value);
	void Eagle_SkeletalMeshComponent_SetAnimGraphVariableAnim(GUID entityID, MonoString* monoName, GUID animID);
	void Eagle_SkeletalMeshComponent_SetAnimGraphVariableString(GUID entityID, MonoString* monoName, MonoString* monoValue);
	void Eagle_SkeletalMeshComponent_SetAnimGraphVariableVec4(GUID entityID, MonoString* monoName, const glm::vec4* value);
	bool Eagle_SkeletalMeshComponent_GetAnimGraphVariableBool(GUID entityID, MonoString* monoName);
	int Eagle_SkeletalMeshComponent_GetAnimGraphVariableInt(GUID entityID, MonoString* monoName);
	float Eagle_SkeletalMeshComponent_GetAnimGraphVariableFloat(GUID entityID, MonoString* monoName);
	GUID Eagle_SkeletalMeshComponent_GetAnimGraphVariableAnim(GUID entityID, MonoString* monoName);
	MonoString* Eagle_SkeletalMeshComponent_GetAnimGraphVariableString(GUID entityID, MonoString* monoName);
	void Eagle_SkeletalMeshComponent_GetAnimGraphVariableVec4(GUID entityID, MonoString* monoName, glm::vec4* outResult);

	void Eagle_SkeletalMeshComponent_SetRagdollEnabled(GUID entityID, bool bEnabled);
	bool Eagle_SkeletalMeshComponent_IsRagdollEnabled(GUID entityID);
	void Eagle_SkeletalMeshComponent_GetRagdollBoneWorldTransform(GUID entityID, MonoString* monoName, Transform* result);
	void Eagle_SkeletalMeshComponent_GetBoneWorldTransform(GUID entityID, MonoString* monoName, Transform* result);
	void Eagle_SkeletalMeshComponent_GetBoneWorldLocation(GUID entityID, MonoString* monoName, glm::vec3* result);
	void Eagle_SkeletalMeshComponent_GetBoneWorldRotation(GUID entityID, MonoString* monoName, Rotator* result);
	void Eagle_SkeletalMeshComponent_GetBoneWorldScale(GUID entityID, MonoString* monoName, glm::vec3* result);

	void Eagle_SkeletalMeshComponent_SetRagdollLinearVelocity(GUID entityID, const glm::vec3* velocity);
	void Eagle_SkeletalMeshComponent_SetRagdollAngularVelocity(GUID entityID, const glm::vec3* velocity);
	void Eagle_SkeletalMeshComponent_SetRagdollBoneLinearVelocity(GUID entityID, MonoString* boneName, const glm::vec3* velocity);
	void Eagle_SkeletalMeshComponent_SetRagdollBoneAngularVelocity(GUID entityID, MonoString* boneName, const glm::vec3* velocity);
	void Eagle_SkeletalMeshComponent_GetRagdollBoneLinearVelocity(GUID entityID, MonoString* boneName, glm::vec3* outVelocity);
	void Eagle_SkeletalMeshComponent_GetRagdollBoneAngularVelocity(GUID entityID, MonoString* boneName, glm::vec3* outVelocity);
	void Eagle_SkeletalMeshComponent_PutRagdollToSleep(GUID entityID);
	void Eagle_SkeletalMeshComponent_WakeUpRagdoll(GUID entityID);

	// Sound
	void Eagle_Sound_SetSettings(GUID id, const SoundSettings* settings);
	void Eagle_Sound_GetSettings(GUID id, SoundSettings* outSettings);
	void Eagle_Sound_Play(GUID id);
	void Eagle_Sound_Stop(GUID id);
	void Eagle_Sound_SetPaused(GUID id, bool bPaused);
	bool Eagle_Sound_IsPlaying(GUID id);
	void Eagle_Sound_SetPosition(GUID id, uint32_t ms);
	uint32_t Eagle_Sound_GetPosition(GUID id);
	void Eagle_Sound_SetFFTEnabled(GUID id, bool value);
	bool Eagle_Sound_IsFFTEnabled(GUID id);
	void Eagle_Sound_SetFFTSamples(GUID id, uint32_t value);
	uint32_t Eagle_Sound_GetFFTSamples(GUID id);
	void Eagle_Sound_SetFFTType(GUID id, FFTWindowType value);
	FFTWindowType Eagle_Sound_GetFFTType(GUID id);
	bool Eagle_Sound_GetSpectrumData(GUID id, MonoArray* data, int channelIndex);
	float Eagle_Sound_GetSampleRate(GUID id);
	int Eagle_Sound_GetChannelsCount(GUID id);

	//Sound2D
	GUID Eagle_Sound2D_Create(GUID assetID, const SoundSettings* settings);

	//Sound3D
	GUID Eagle_Sound3D_Create(GUID assetID, const glm::vec3* position, RollOffModel rolloff, const SoundSettings* settings);
	void Eagle_Sound3D_SetMinDistance(GUID id, float min);
	void Eagle_Sound3D_SetMaxDistance(GUID id, float max);
	void Eagle_Sound3D_SetMinMaxDistance(GUID id, float min, float max);
	void Eagle_Sound3D_SetWorldPosition(GUID id, const glm::vec3* position);
	void Eagle_Sound3D_SetVelocity(GUID id, const glm::vec3* velocity);
	void Eagle_Sound3D_SetRollOffModel(GUID id, RollOffModel rollOff);
	float Eagle_Sound3D_GetMinDistance(GUID id);
	float Eagle_Sound3D_GetMaxDistance(GUID id);
	void Eagle_Sound3D_GetWorldPosition(GUID id, glm::vec3* outPosition);
	void Eagle_Sound3D_GetVelocity(GUID id, glm::vec3* outVelocity);
	RollOffModel Eagle_Sound3D_GetRollOffModel(GUID id);

	//AudioComponent
	void Eagle_AudioComponent_SetMinDistance(GUID entityID, float minDistance);
	void Eagle_AudioComponent_SetMaxDistance(GUID entityID, float maxDistance);
	void Eagle_AudioComponent_SetMinMaxDistance(GUID entityID, float minDistance, float maxDistance);
	void Eagle_AudioComponent_SetRollOffModel(GUID entityID, RollOffModel rollOff);
	void Eagle_AudioComponent_SetVolume(GUID entityID, float volume);
	void Eagle_AudioComponent_SetPitch(GUID entityID, float pitch);
	void Eagle_AudioComponent_SetLoopCount(GUID entityID, int loopCount);
	void Eagle_AudioComponent_SetLooping(GUID entityID, bool bLooping);
	void Eagle_AudioComponent_SetMuted(GUID entityID, bool bMuted);
	void Eagle_AudioComponent_SetAudioAsset(GUID entityID, GUID assetID);
	void Eagle_AudioComponent_SetStreaming(GUID entityID, bool bStreaming);
	void Eagle_AudioComponent_Play(GUID entityID);
	void Eagle_AudioComponent_Stop(GUID entityID);
	void Eagle_AudioComponent_SetPaused(GUID entityID, bool bPaused);
	void Eagle_AudioComponent_SetDopplerEffectEnabled(GUID entityID, bool bEnable);
	float Eagle_AudioComponent_GetMinDistance(GUID entityID);
	float Eagle_AudioComponent_GetMaxDistance(GUID entityID);
	RollOffModel Eagle_AudioComponent_GetRollOffModel(GUID entityID);
	float Eagle_AudioComponent_GetVolume(GUID entityID);
	float Eagle_AudioComponent_GetPitch(GUID entityID);
	int Eagle_AudioComponent_GetLoopCount(GUID entityID);
	bool Eagle_AudioComponent_IsLooping(GUID entityID);
	bool Eagle_AudioComponent_IsMuted(GUID entityID);
	GUID Eagle_AudioComponent_GetAudioAsset(GUID entityID);
	bool Eagle_AudioComponent_IsStreaming(GUID entityID);
	bool Eagle_AudioComponent_IsPlaying(GUID entityID);
	bool Eagle_AudioComponent_IsDopplerEffectEnabled(GUID entityID);
	void Eagle_AudioComponent_SetFFTEnabled(GUID id, bool value);
	bool Eagle_AudioComponent_IsFFTEnabled(GUID id);
	void Eagle_AudioComponent_SetFFTSamples(GUID id, uint32_t value);
	uint32_t Eagle_AudioComponent_GetFFTSamples(GUID id);
	void Eagle_AudioComponent_SetFFTType(GUID id, FFTWindowType value);
	FFTWindowType Eagle_AudioComponent_GetFFTType(GUID id);
	bool Eagle_AudioComponent_GetSpectrumData(GUID entityID, MonoArray* data, int channelIndex);
	float Eagle_AudioComponent_GetSampleRate(GUID entityID);
	void Eagle_AudioComponent_SetPosition(GUID entityID, uint32_t ms);
	uint32_t Eagle_AudioComponent_GetPosition(GUID entityID);
	void Eagle_AudioComponent_SetIs3D(GUID entityID, bool value);
	bool Eagle_AudioComponent_Is3D(GUID entityID);
	void Eagle_AudioComponent_SetPan(GUID entityID, float pan);
	float Eagle_AudioComponent_GetPan(GUID entityID);
	int Eagle_AudioComponent_GetChannelsCount(GUID id);

	//RigidBodyComponent
	void Eagle_RigidBodyComponent_SetBodyType(GUID entityID, PhysicsBodyType type);
	PhysicsBodyType Eagle_RigidBodyComponent_GetBodyType(GUID entityID);
	void Eagle_RigidBodyComponent_SetCollisionDetectionType(GUID entityID, CollisionDetectionType type);
	CollisionDetectionType Eagle_RigidBodyComponent_GetCollisionDetectionType(GUID entityID);
	void Eagle_RigidBodyComponent_SetPositionSolverIterations(GUID entityID, uint32_t iterations);
	void Eagle_RigidBodyComponent_SetVelocitySolverIterations(GUID entityID, uint32_t iterations);
	uint32_t Eagle_RigidBodyComponent_GetPositionSolverIterations(GUID entityID);
	uint32_t Eagle_RigidBodyComponent_GetVelocitySolverIterations(GUID entityID);
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

	void Eagle_RigidBodyComponent_WakeUp(GUID entityID);
	void Eagle_RigidBodyComponent_PutToSleep(GUID entityID);
	void Eagle_RigidBodyComponent_AddForce(GUID entityID, const glm::vec3* force, ForceMode forceMode);
	void Eagle_RigidBodyComponent_AddTorque(GUID entityID, const glm::vec3* force, ForceMode forceMode);
	void Eagle_RigidBodyComponent_GetLinearVelocity(GUID entityID, glm::vec3* result);
	void Eagle_RigidBodyComponent_SetLinearVelocity(GUID entityID, const glm::vec3* velocity);
	void Eagle_RigidBodyComponent_GetAngularVelocity(GUID entityID, glm::vec3* result);
	void Eagle_RigidBodyComponent_SetAngularVelocity(GUID entityID, const glm::vec3* velocity);
	float Eagle_RigidBodyComponent_GetMaxLinearVelocity(GUID entityID);
	void Eagle_RigidBodyComponent_SetMaxLinearVelocity(GUID entityID, float maxVelocity);
	float Eagle_RigidBodyComponent_GetMaxAngularVelocity(GUID entityID);
	void Eagle_RigidBodyComponent_SetMaxAngularVelocity(GUID entityID, float maxVelocity);
	bool Eagle_RigidBodyComponent_IsDynamic(GUID entityID);
	bool Eagle_RigidBodyComponent_IsLockFlagSet(GUID entityID, ActorLockFlag flag);
	ActorLockFlag Eagle_RigidBodyComponent_GetLockFlags(GUID entityID);
	void Eagle_RigidBodyComponent_GetKinematicTarget(GUID entityID, Transform* outTransform);
	void Eagle_RigidBodyComponent_GetKinematicTargetLocation(GUID entityID, glm::vec3* outLocation);
	void Eagle_RigidBodyComponent_GetKinematicTargetRotation(GUID entityID, Rotator* outRotation);
	void Eagle_RigidBodyComponent_SetKinematicTarget(GUID entityID, const glm::vec3* location, const Rotator* rotation);
	void Eagle_RigidBodyComponent_SetKinematicTargetLocation(GUID entityID, const glm::vec3* location);
	void Eagle_RigidBodyComponent_SetKinematicTargetRotation(GUID entityID, const Rotator* rotation);
	void Eagle_RigidBodyComponent_SetLockFlag(GUID entityID, ActorLockFlag flag, bool value);

	//BaseColliderComponent
	void Eagle_BaseColliderComponent_SetCollisionGroup(GUID entityID, void* type, CollisionGroup groups);
	CollisionGroup Eagle_BaseColliderComponent_GetCollisionGroup(GUID entityID, void* type);
	void Eagle_BaseColliderComponent_SetInteractingCollisionGroup(GUID entityID, void* type, CollisionGroup groups);
	CollisionGroup Eagle_BaseColliderComponent_GetInteractingCollisionGroup(GUID entityID, void* type);
	void Eagle_BaseColliderComponent_SetIsTrigger(GUID entityID, void* type, bool bTrigger);
	bool Eagle_BaseColliderComponent_IsTrigger(GUID entityID, void* type);
	void Eagle_BaseColliderComponent_SetCollisionVisible(GUID entityID, void* type, bool bShow);
	bool Eagle_BaseColliderComponent_IsCollisionVisible(GUID entityID, void* type);
	GUID Eagle_BaseColliderComponent_GetPhysicsMaterial(GUID entityID, void* type);
	void Eagle_BaseColliderComponent_SetPhysicsMaterial(GUID entityID, void* type, GUID assetID);
	void Eagle_BaseColliderComponent_SetAffectsNavMeshBuild(GUID entityID, void* type, bool bAffects);
	bool Eagle_BaseColliderComponent_DoesAffectNavMeshBuild(GUID entityID, void* type);
	void Eagle_BaseColliderComponent_SetIsObstacle(GUID entityID, void* type, bool bObstacle);
	bool Eagle_BaseColliderComponent_IsObstacle(GUID entityID, void* type);

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
	void Eagle_MeshColliderComponent_SetIsTwoSided(GUID entityID, bool val);
	bool Eagle_MeshColliderComponent_IsTwoSided(GUID entityID);
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
	float Eagle_CameraComponent_GetCascadesSplitAlpha(GUID entityID);
	void Eagle_CameraComponent_SetCascadesSplitAlpha(GUID entityID, float value);
	float Eagle_CameraComponent_GetCascadesSmoothTransitionAlpha(GUID entityID);
	void Eagle_CameraComponent_SetCascadesSmoothTransitionAlpha(GUID entityID, float value);
	CameraProjectionMode Eagle_CameraComponent_GetCameraProjectionMode(GUID entityID);
	void Eagle_CameraComponent_SetCameraProjectionMode(GUID entityID, CameraProjectionMode value);
	float Eagle_CameraComponent_GetAspectRatio(GUID entityID);

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
	void Eagle_TextComponent_SetIsLit(GUID entityID, bool value);
	bool Eagle_TextComponent_GetIsLit(GUID entityID);
	void Eagle_TextComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_TextComponent_DoesCastShadows(GUID entityID);
	GUID Eagle_TextComponent_GetFont(GUID entityID);
	void Eagle_TextComponent_SetFont(GUID entityID, GUID assetID);
	void Eagle_TextComponent_GetMaterial(GUID entityID, GUID* outAssetID);
	void Eagle_TextComponent_SetMaterial(GUID entityID, GUID assetID);
	void Eagle_TextComponent_SetReceivesDecals(GUID entityID, bool value);
	bool Eagle_TextComponent_DoesReceiveDecals(GUID entityID);

	// Text2D Component
	MonoString* Eagle_Text2DComponent_GetText(GUID entityID);
	void Eagle_Text2DComponent_SetText(GUID entityID, MonoString* value);
	void Eagle_Text2DComponent_GetColor(GUID entityID, glm::vec3* outColor);
	void Eagle_Text2DComponent_SetColor(GUID entityID, const glm::vec3* color);
	void Eagle_Text2DComponent_GetPosition(GUID entityID, glm::vec2* outPos);
	void Eagle_Text2DComponent_SetPosition(GUID entityID, const glm::vec2* pos);
	void Eagle_Text2DComponent_GetScale(GUID entityID, glm::vec2* outScale);
	void Eagle_Text2DComponent_SetScale(GUID entityID, const glm::vec2* scale);
	float Eagle_Text2DComponent_GetRotation(GUID entityID);
	void Eagle_Text2DComponent_SetRotation(GUID entityID, float value);
	float Eagle_Text2DComponent_GetLineSpacing(GUID entityID);
	void Eagle_Text2DComponent_SetLineSpacing(GUID entityID, float value);
	float Eagle_Text2DComponent_GetKerning(GUID entityID);
	void Eagle_Text2DComponent_SetKerning(GUID entityID, float value);
	float Eagle_Text2DComponent_GetMaxWidth(GUID entityID);
	void Eagle_Text2DComponent_SetMaxWidth(GUID entityID, float value);
	void Eagle_Text2DComponent_SetOpacity(GUID entityID, float value);
	float Eagle_Text2DComponent_GetOpacity(GUID entityID);
	void Eagle_Text2DComponent_SetIsVisible(GUID entityID, bool value);
	bool Eagle_Text2DComponent_IsVisible(GUID entityID);
	GUID Eagle_Text2DComponent_GetFont(GUID entityID);
	void Eagle_Text2DComponent_SetFont(GUID entityID, GUID assetID);

	// Image2D Component
	void Eagle_Image2DComponent_SetTexture(GUID entityID, GUID textureID);
	GUID Eagle_Image2DComponent_GetTexture(GUID entityID);
	void Eagle_Image2DComponent_GetTint(GUID entityID, glm::vec3* outValue);
	void Eagle_Image2DComponent_SetTint(GUID entityID, const glm::vec3* value);
	void Eagle_Image2DComponent_GetPosition(GUID entityID, glm::vec2* outValue);
	void Eagle_Image2DComponent_SetPosition(GUID entityID, const glm::vec2* value);
	void Eagle_Image2DComponent_GetScale(GUID entityID, glm::vec2* outValue);
	void Eagle_Image2DComponent_SetScale(GUID entityID, const glm::vec2* value);
	float Eagle_Image2DComponent_GetRotation(GUID entityID);
	void Eagle_Image2DComponent_SetRotation(GUID entityID, float value);
	void Eagle_Image2DComponent_SetOpacity(GUID entityID, float value);
	float Eagle_Image2DComponent_GetOpacity(GUID entityID);
	void Eagle_Image2DComponent_SetIsVisible(GUID entityID, bool value);
	bool Eagle_Image2DComponent_IsVisible(GUID entityID);

	// Billboard Component
	void Eagle_BillboardComponent_SetTexture(GUID entityID, GUID textureID);
	GUID Eagle_BillboardComponent_GetTexture(GUID entityID);

	// Sprite component
	void Eagle_SpriteComponent_GetMaterial(GUID entityID, GUID* outAssetID);
	void Eagle_SpriteComponent_SetMaterial(GUID entityID, GUID assetID);
	void Eagle_SpriteComponent_GetAtlasSpriteCoords(GUID entityID, glm::vec2* outValue);
	void Eagle_SpriteComponent_SetAtlasSpriteCoords(GUID entityID, const glm::vec2* value);
	void Eagle_SpriteComponent_GetAtlasSpriteSize(GUID entityID, glm::vec2* outValue);
	void Eagle_SpriteComponent_SetAtlasSpriteSize(GUID entityID, const glm::vec2* value);
	void Eagle_SpriteComponent_GetAtlasSpriteSizeCoef(GUID entityID, glm::vec2* outValue);
	void Eagle_SpriteComponent_SetAtlasSpriteSizeCoef(GUID entityID, const glm::vec2* value);
	bool Eagle_SpriteComponent_GetIsAtlas(GUID entityID);
	void Eagle_SpriteComponent_SetIsAtlas(GUID entityID, bool value);
	void Eagle_SpriteComponent_SetCastsShadows(GUID entityID, bool value);
	bool Eagle_SpriteComponent_DoesCastShadows(GUID entityID);
	void Eagle_SpriteComponent_SetReceivesDecals(GUID entityID, bool value);
	bool Eagle_SpriteComponent_DoesReceiveDecals(GUID entityID);

	// Script Component
	void Eagle_ScriptComponent_SetScript(GUID entityID, void* type);
	MonoReflectionType* Eagle_ScriptComponent_GetScriptType(GUID entityID);
	MonoObject* Eagle_ScriptComponent_GetInstance(GUID entityID);

	// Particle System Component
	void Eagle_ParticleSystemComponent_Spawn(GUID entityID);
	void Eagle_ParticleSystemComponent_Destroy(GUID entityID);
	void Eagle_ParticleSystemComponent_SetAsset(GUID entityID, GUID assetGUID);
	GUID Eagle_ParticleSystemComponent_GetAsset(GUID entityID);
	void Eagle_ParticleSystemComponent_DuplicatePose(GUID entityID, uint32_t emitterIndex, GUID compEntityID);

	// Decal Component
	void Eagle_DecalComponent_SetMaterial(GUID entityID, GUID assetID);
	GUID Eagle_DecalComponent_GetMaterial(GUID entityID);
	void Eagle_DecalComponent_SetAdjustAspectRatioEnabled(GUID entityID, bool value);
	bool Eagle_DecalComponent_IsAdjustAspectRatioEnabled(GUID entityID);
	void Eagle_DecalComponent_SetSortPriority(GUID entityID, uint32_t value);
	uint32_t Eagle_DecalComponent_GetSortPriority(GUID entityID);

	// NavigationMeshComponent
	void Eagle_NavigationMeshComponent_Build(GUID entityID);
	void Eagle_NavigationMeshComponent_SetCrowdSettings(GUID entityID, const AINavigation::CrowdSettings* settings);
	void Eagle_NavigationMeshComponent_GetCrowdSettings(GUID entityID, AINavigation::CrowdSettings* settings);
	void Eagle_NavigationMeshComponent_SetSettings(GUID entityID, const AINavigation::MeshSettings* settings);
	void Eagle_NavigationMeshComponent_GetSettings(GUID entityID, AINavigation::MeshSettings* settings);
	void Eagle_NavigationMeshComponent_SetAutoRebuild(GUID entityID, bool value);
	bool Eagle_NavigationMeshComponent_GetAutoRebuild(GUID entityID);

	// NavigationCrowdAgentComponent
	void Eagle_NavigationCrowdAgentComponent_TeleportAgent(GUID entityID, const glm::vec3* location);
	void Eagle_NavigationCrowdAgentComponent_SetMoveTarget(GUID entityID, const glm::vec3* location);
	void Eagle_NavigationCrowdAgentComponent_ResetMoveTarget(GUID entityID);
	bool Eagle_NavigationCrowdAgentComponent_IsValid(GUID entityID);
	void Eagle_NavigationCrowdAgentComponent_SetSettings(GUID entityID, const AINavigation::AgentSettings* settings);
	void Eagle_NavigationCrowdAgentComponent_GetSettings(GUID entityID, AINavigation::AgentSettings* settings);
	bool Eagle_NavigationCrowdAgentComponent_GetLocation(GUID entityID, glm::vec3* location);
	bool Eagle_NavigationCrowdAgentComponent_GetVelocity(GUID entityID, glm::vec3* velocity);
	MoveRequestState Eagle_NavigationCrowdAgentComponent_GetTargetState(GUID entityID);

	// Renderer
	void Eagle_Renderer_SetFogSettings(const glm::vec3* color, float minDistance, float maxDistance, float density, FogEquation equation, bool bEnabled);
	void Eagle_Renderer_GetFogSettings(glm::vec3* outcolor, float* outMinDistance, float* outMaxDistance, float* outDensity, FogEquation* outEquation, bool* outbEnabled);
	void Eagle_Renderer_SetBloomSettings(GUID dirt, float threashold, float intensity, float dirtIntensity, float knee, bool bEnabled);
	void Eagle_Renderer_GetBloomSettings(GUID* outDirtTexture, float* outThreashold, float* outIntensity, float* outDirtIntensity, float* outKnee, bool* outbEnabled);
	void Eagle_Renderer_SetSSAOSettings(uint32_t samples, float radius, float bias);
	void Eagle_Renderer_GetSSAOSettings(uint32_t* outSamples, float* outRadius, float* outBias);
	void Eagle_Renderer_SetGTAOSettings(uint32_t samples, float radius);
	void Eagle_Renderer_GetGTAOSettings(uint32_t* outSamples, float* outRadius);
	void Eagle_Renderer_SetPhotoLinearTonemappingSettings(float sensitivity, float exposureTime, float fStop);
	void Eagle_Renderer_GetPhotoLinearTonemappingSettings(float* outSensitivity, float* outExposureTime, float* outfStop);
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
	void Eagle_Renderer_SetVSyncEnabled(bool value);
	bool Eagle_Renderer_GetVSyncEnabled();
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
	void Eagle_Renderer_SetVolumetricLightsSettings(const glm::vec3* albedo, float anisotropy, uint32_t samples, float maxScatteringDist, float fogSpeed, bool bFogEnable, bool bEnable);
	void Eagle_Renderer_GetVolumetricLightsSettings(glm::vec3* albedo, float* anisotropy, uint32_t* outSamples, float* outMaxScatteringDist, float* fogSpeed, bool* bFogEnable, bool* bEnable);
	MonoArray* Eagle_Renderer_GetShadowMapsSettings(uint32_t* outPointLightSize, uint32_t* outSpotLightSize);
	void Eagle_Renderer_GetDepthOfFieldSettings(glm::vec2* apertureShape, float* apertureSize, float* focalLength, float* COCScale, float* maxCOC);
	void Eagle_Renderer_GetMotionBlurSettings(bool* bEnabled, uint32_t* numSamples, float* strength);
	void Eagle_Renderer_GetAutoExposureSettings(float* minLogLum, float* maxLogLum, float* adaptationSpeed, float* adaptationKey, bool* bEnabled, bool* bHalfResolution);
	void Eagle_Renderer_GetScreenSpaceReflectionsSettings(float* roughnessThreshold, uint32_t* samplesPerQuad, uint32_t* maxIters, bool* bEnabled);
	void Eagle_Renderer_SetShadowMapsSettings(uint32_t pointLightSize, uint32_t spotLightSize, MonoArray* dirLightSizes);
	void Eagle_Renderer_SetDepthOfFieldSettings(const glm::vec2* apertureShape, float apertureSize, float focalLength, float COCScale, float maxCOC);
	void Eagle_Renderer_SetMotionBlurSettings(bool bEnabled, uint32_t numSamples, float strength);
	void Eagle_Renderer_SetAutoExposureSettings(float minLogLum, float maxLogLum, float adaptationSpeed, float adaptationKey, bool bEnabled, bool bHalfResolution);
	void Eagle_Renderer_SetScreenSpaceReflectionsSettings(float roughnessThreshold, uint32_t samplesPerQuad, uint32_t maxIters, bool bEnabled);
	void Eagle_Renderer_SetStutterlessShaders(bool value);
	bool Eagle_Renderer_GetStutterlessShaders();
	void Eagle_Renderer_SetTranslucentShadowsEnabled(bool value);
	bool Eagle_Renderer_GetTranslucentShadowsEnabled();
	void Eagle_Renderer_GetViewportSize(glm::vec2* outSize);
	void Eagle_Renderer_SetRenderSkyboxEnabled(bool value);
	bool Eagle_Renderer_IsRenderSkyboxEnabled();
	void Eagle_Renderer_SetSkyboxEnabled(bool value);
	bool Eagle_Renderer_IsSkyboxEnabled();
	void Eagle_Renderer_SetObjectPickingEnabled(bool value);
	bool Eagle_Renderer_IsObjectPickingEnabled();
	void Eagle_Renderer_Set2DObjectPickingEnabled(bool value);
	bool Eagle_Renderer_Is2DObjectPickingEnabled();
	void Eagle_Renderer_SetSortOpaqueParticlesEnabled(bool value);
	bool Eagle_Renderer_IsSortOpaqueParticlesEnabled();
	void Eagle_Renderer_SetDebugLinesDepthTestEnabled(bool value);
	bool Eagle_Renderer_IsDebugLinesDepthTestEnabled();

	void Eagle_Renderer_SetSkybox(GUID cubemapID);
	GUID Eagle_Renderer_GetSkybox();
	void Eagle_Renderer_SetCubemapIntensity(float intensity);
	float Eagle_Renderer_GetCubemapIntensity();

	void Eagle_Renderer_DrawLine(const glm::vec3* startColor, const glm::vec3* endColor, const glm::vec3* start, const glm::vec3* end);
	void Eagle_Renderer_DrawTriangle(const glm::vec3* v0Location, const glm::vec3* v0Color, const glm::vec3* v1Location, const glm::vec3* v1Color, const glm::vec3* v2Location, const glm::vec3* v2Color);
	void Eagle_Renderer_DrawArrow(const glm::vec3* start, const glm::vec3* end, const glm::vec3* up);
	void Eagle_Renderer_DrawAABB(const AABB* aabb, const Transform* transform);

	// Project
	MonoString* Eagle_Project_GetProjectPath();
	MonoString* Eagle_Project_GetBinariesPath();
	MonoString* Eagle_Project_GetConfigPath();
	MonoString* Eagle_Project_GetContentPath();
	MonoString* Eagle_Project_GetCachePath();
	MonoString* Eagle_Project_GetRendererCachePath();
	MonoString* Eagle_Project_GetSavedPath();

	// Scene
	void Eagle_Scene_OpenScene(GUID assetID);
	void Eagle_Scene_QuitGame();
	bool Eagle_Scene_Raycast(const glm::vec3* origin, const glm::vec3* dir, float maxDistance, PhysicsQueryType query, GUID* outHitEntity, glm::vec3* outPosition, glm::vec3* outNormal, float* outDistance);
	void Eagle_Scene_SetGravity(const glm::vec3* gravity);
	void Eagle_Scene_GetGravity(glm::vec3* gravity);
	MonoArray* Eagle_Scene_GetAllEntitiesWithComponent(void* type);

	// Navigation
	MonoArray* Eagle_Navigation_FindStraightPath(const glm::vec3* start, const glm::vec3* end, uint32_t maxPolys);
	MonoArray* Eagle_Navigation_FindSmoothPath(const glm::vec3* start, const glm::vec3* end, uint32_t maxPolys, uint32_t maxSmooth);
	bool Eagle_Navigation_FindDistanceToWall(const glm::vec3* pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance);
	bool Eagle_Navigation_FindRandomPoint(glm::vec3* outRandomPoint);
	bool Eagle_Navigation_FindRandomPointInCircle(const glm::vec3* pos, float radius, glm::vec3* outRandomPoint);
	bool Eagle_Navigation_IsValidPoint(const glm::vec3* pos);

	// CrowdNavigation
	void Eagle_CrowdNavigation_SetMoveTarget(const glm::vec3* pos);
	void Eagle_CrowdNavigation_ResetMoveTarget();

	// Log
	void Eagle_Log_Trace(MonoString* message);
	void Eagle_Log_Info(MonoString* message);
	void Eagle_Log_Warn(MonoString* message);
	void Eagle_Log_Error(MonoString* message);
	void Eagle_Log_Critical(MonoString* message);

	// Asset
	bool Eagle_Asset_Get(MonoString* path, AssetType* outType, GUID* outGUID);
	MonoString* Eagle_Asset_GetPath(GUID guid);
	AssetType Eagle_Asset_GetAssetType(GUID guid);

	// AssetTexture2D
	void Eagle_AssetTexture2D_SetAnisotropy(GUID id, float anisotropy);
	float Eagle_AssetTexture2D_GetAnisotropy(GUID id);
	void Eagle_AssetTexture2D_SetFilterMode(GUID id, FilterMode filterMode);
	FilterMode Eagle_AssetTexture2D_GetFilterMode(GUID id);
	void Eagle_AssetTexture2D_SetAddressMode(GUID id, AddressMode addressMode);
	AddressMode Eagle_AssetTexture2D_GetAddressMode(GUID id);
	void Eagle_AssetTexture2D_SetMipsCount(GUID id, uint32_t mipsCount);
	uint32_t Eagle_AssetTexture2D_GetMipsCount(GUID id);
	void Eagle_AssetTexture2D_SetFormat(GUID id, AssetTexture2DFormat value);
	void Eagle_AssetTexture2D_SetIsNormalMap(GUID id, bool value);
	void Eagle_AssetTexture2D_SetNeedsAlpha(GUID id, bool value);
	void Eagle_AssetTexture2D_SetIsCompressed(GUID id, bool value);
	AssetTexture2DFormat Eagle_AssetTexture2D_GetFormat(GUID id);
	bool Eagle_AssetTexture2D_IsNormalMap(GUID id);
	bool Eagle_AssetTexture2D_DoesNeedAlpha(GUID id);
	bool Eagle_AssetTexture2D_IsCompressed(GUID id);

	// AssetTextureCube
	void Eagle_AssetTextureCube_SetLayerSize(GUID id, uint32_t value);
	void Eagle_AssetTextureCube_SetPrefilterSize(GUID id, uint32_t value);
	bool Eagle_AssetTextureCube_SetFormat(GUID id, AssetTextureCubeFormat value);
	uint32_t Eagle_AssetTextureCube_GetLayerSize(GUID id);
	uint32_t Eagle_AssetTextureCube_GetPrefilterSize(GUID id);
	AssetTextureCubeFormat Eagle_AssetTextureCube_GetFormat(GUID id);

	// AssetMaterial
	GUID Eagle_AssetMaterial_Create();
	GUID Eagle_AssetMaterial_CreateFromAsset(GUID assetID);
	void Eagle_AssetMaterial_GetMaterial(GUID assetID,
		GUID* outAlbedoTexture, GUID* outMetalnessTexture, GUID* outNormalTexture, GUID* outRoughnessTexture, GUID* outAOTexture, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec3* albedo, float* metalness, float* roughness, float* ao, glm::vec3* emissive, float* opacity, float* opacityMask,
		bool* bUseAlbedoTexture, bool* bUseMetalnessTexture, bool* bUseRoughnessTexture, bool* bUseAOTexture, bool* bUseEmissiveTexture, bool* bUseOpacityTexture, bool* bUseOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode,
		Material::TextureChannel* outMetalnessTextureChannel, Material::TextureChannel* outRoughnessTextureChannel, Material::TextureChannel* outAOTextureChannel,
		Material::TextureChannel* outOpacityTextureChannel, Material::TextureChannel* outOpacityMaskTextureChannel);

	void Eagle_AssetMaterial_SetMaterial(GUID assetID,
		GUID albedoTexture, GUID metalnessTexture, GUID normalTexture, GUID roughnessTexture, GUID aoTexture, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec3* albedo, float metalness, float roughness, float ao, const glm::vec3* emissive, float opacity, float opacityMask,
		bool bUseAlbedoTexture, bool bUseMetalnessTexture, bool bUseRoughnessTexture, bool bUseAOTexture, bool bUseEmissiveTexture, bool bUseOpacityTexture, bool bUseOpacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode,
		Material::TextureChannel metalnessTextureChannel, Material::TextureChannel roughnessTextureChannel, Material::TextureChannel aoTextureChannel,
		Material::TextureChannel opacityTextureChannel, Material::TextureChannel opacityMaskTextureChannel);

	// AssetAudio
	void Eagle_AssetAudio_SetVolume(GUID id, float volume);
	float Eagle_AssetAudio_GetVolume(GUID id);
	void Eagle_AssetAudio_SetPitch(GUID id, float pitch);
	float Eagle_AssetAudio_GetPitch(GUID id);
	void Eagle_AssetAudio_SetPan(GUID id, float pan);
	float Eagle_AssetAudio_GetPan(GUID id);
	void Eagle_AssetAudio_SetSoundGroup(GUID id, GUID soundGroupID);
	GUID Eagle_AssetAudio_GetSoundGroup(GUID id);

	// AssetPhysicsMaterial
	void Eagle_AssetPhysicsMaterial_SetDynamicFriction(GUID assetID, float value);
	void Eagle_AssetPhysicsMaterial_SetBounciness(GUID assetID, float value);
	void Eagle_AssetPhysicsMaterial_SetStaticFriction(GUID assetID, float value);
	float Eagle_AssetPhysicsMaterial_GetStaticFriction(GUID assetID);
	float Eagle_AssetPhysicsMaterial_GetDynamicFriction(GUID assetID);
	float Eagle_AssetPhysicsMaterial_GetBounciness(GUID assetID);
	GUID Eagle_AssetPhysicsMaterial_Create(float staticFriction, float dynamicFriction, float bounciness);
	GUID Eagle_AssetPhysicsMaterial_CreateFromAsset(GUID assetID);

	// AssetSoundGroup
	void Eagle_AssetSoundGroup_Stop(GUID assetID);
	void Eagle_AssetSoundGroup_SetPaused(GUID assetID, bool value);
	void Eagle_AssetSoundGroup_SetVolume(GUID assetID, float value);
	void Eagle_AssetSoundGroup_SetMuted(GUID assetID, bool value);
	void Eagle_AssetSoundGroup_SetPitch(GUID assetID, float value);
	float Eagle_AssetSoundGroup_GetVolume(GUID assetID);
	float Eagle_AssetSoundGroup_GetPitch(GUID assetID);
	bool Eagle_AssetSoundGroup_IsPaused(GUID assetID);
	bool Eagle_AssetSoundGroup_IsMuted(GUID assetID);

	// AssetAnimation
	void Eagle_AssetAnimation_SetRootMotionEnabled(GUID id, bool value);
	float Eagle_AssetAnimation_GetDuration(GUID id);
	float Eagle_AssetAnimation_GetTicksPerSecond(GUID id);
	void Eagle_AssetAnimation_AddAnimationEvent(GUID id, MonoString* name, float time);
	bool Eagle_AssetAnimation_RemoveAnimationEvent(GUID id, MonoString* name);
	bool Eagle_AssetAnimation_HasAnimationEvent(GUID id, MonoString* name);

	// AssetParticleSystem
	uint32_t Eagle_AssetParticleSystem_GetEmittersCount(GUID assetID);
	void Eagle_AssetParticleSystem_RemoveEmitters(GUID assetID);
	void* Eagle_AssetParticleSystem_SetEmitters_Prepare(uint32_t count);
	void Eagle_AssetParticleSystem_SetEmitters_Finish(GUID assetID, void* data);
	GUID Eagle_AssetParticleSystem_Create();

	void Eagle_AssetParticleSystem_SetEmitter(void* data, uint32_t index,
		GUID texture, const glm::vec4* colorStart, const glm::vec4* colorEnd, const glm::vec3* velocityMin, const glm::vec3* velocityMax,
		const glm::vec3* velocityCoefStart, const glm::vec3* velocityCoefEnd, float rotationZStart, float rotationZEnd,
		const glm::vec2* sizeStart, const glm::vec2* sizeEnd, const glm::vec2* colliderSizeRatio, float lifetimeMin, float lifetimeMax,
		float bouncinessMin, float bouncinessMax, MonoString* name, const Transform* relativeTransform, const AABB* visibilityAABB,
		uint32_t loopCount, uint32_t numParticles, float numParticlesRatio, float radialAcceleration, float tangentialAcceleration,
		float normalVelocityFactor, ParticleEmitter::EmissionShapeType emissionShape, const glm::vec3* sphereRadius, const glm::vec3* boxMin, const glm::vec3* boxMax,
		const glm::vec3* ringRadius, const glm::vec3* ringThickness, GUID meshAsset, ParticleEmitter::CollisionModeType collisionMode, const glm::uvec2* animationImagesNum,
		float animationSpeed, bool bDestroyImmediately, bool bEmit, bool bExplode, bool bApplyGravity, bool bAlphaBlending,
		bool bAdditive, bool bBlendAnimation, bool bFaceDirection);

	MonoString* Eagle_AssetParticleSystem_GetEmitter(GUID assetID, uint32_t index,
		GUID* texture, glm::vec4* colorStart, glm::vec4* colorEnd, glm::vec3* velocityMin, glm::vec3* velocityMax,
		glm::vec3* velocityCoefStart, glm::vec3* velocityCoefEnd, float* rotationZStart, float* rotationZEnd,
		glm::vec2* sizeStart, glm::vec2* sizeEnd, glm::vec2* colliderSizeRatio, float* lifetimeMin, float* lifetimeMax,
		float* bouncinessMin, float* bouncinessMax, Transform* relativeTransform, AABB* visibilityAABB,
		uint32_t* loopCount, uint32_t* numParticles, float* numParticlesRatio, float* radialAcceleration, float* tangentialAcceleration,
		float* normalVelocityFactor, ParticleEmitter::EmissionShapeType* emissionShape, glm::vec3* sphereRadius, glm::vec3* boxMin, glm::vec3* boxMax,
		glm::vec3* ringRadius, glm::vec3* ringThickness, GUID* meshAsset, ParticleEmitter::CollisionModeType* collisionMode, glm::uvec2* animationImagesNum,
		float* animationSpeed, bool* bDestroyImmediately, bool* bEmit, bool* bExplode, bool* bApplyGravity, bool* bAlphaBlending,
		bool* bAdditive, bool* bBlendAnimation, bool* bFaceDirection);

	// Math
	glm::vec3 Eagle_Math_GetForwardVector(const Rotator* rotator);
	glm::vec3 Eagle_Math_GetUpVector(const Rotator* rotator);
	glm::vec3 Eagle_Math_GetRightVector(const Rotator* rotator);
	float Eagle_Math_CalculateDirection(const glm::vec3* velocity, const Rotator* rotator);

	// Quat
	glm::quat Eagle_Quat_Mul(const glm::quat& left, const glm::quat& right);
	glm::vec3 Eagle_Quat_EulerAngles(const glm::quat* q);
	glm::quat Eagle_Quat_FromEulerAngles(const glm::vec3* rads);
}
