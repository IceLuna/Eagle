#pragma once

#include "Eagle/Components/Components.h"
#include "Eagle/Input/Input.h"

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
}

namespace Eagle::Script
{
	//Entity
	EG_GUID_TYPE Eagle_Entity_GetParent(EG_GUID_TYPE entityID);
	void Eagle_Entity_SetParent(EG_GUID_TYPE entityID, EG_GUID_TYPE parentID);
	MonoArray* Eagle_Entity_GetChildren(EG_GUID_TYPE entityID);
	EG_GUID_TYPE Eagle_Entity_CreateEntity();
	void Eagle_Entity_DestroyEntity(EG_GUID_TYPE entityID);
	void Eagle_Entity_AddComponent(EG_GUID_TYPE entityID, void* type);
	bool Eagle_Entity_HasComponent(EG_GUID_TYPE entityID, void* type);
	void Eagle_Entity_GetEntityName(EG_GUID_TYPE entityID, MonoString* string);

	//Input
	bool Eagle_Input_IsMouseButtonPressed(Mouse button);
	bool Eagle_Input_IsKeyPressed(Key keyCode);
	void Eagle_Input_GetMousePosition(glm::vec2* outPosition);
	void Eagle_Input_SetCursorMode(CursorMode mode);
	CursorMode Eagle_Input_GetCursorMode();

	//Transform Component
	void Eagle_TransformComponent_GetWorldTransform(EG_GUID_TYPE entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetWorldLocation(EG_GUID_TYPE entityID, glm::vec3* outLocation);
	void Eagle_TransformComponent_GetWorldRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation);
	void Eagle_TransformComponent_GetWorldScale(EG_GUID_TYPE entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetWorldTransform(EG_GUID_TYPE entityID, Transform* inTransform);
	void Eagle_TransformComponent_SetWorldLocation(EG_GUID_TYPE entityID, glm::vec3* inLocation);
	void Eagle_TransformComponent_SetWorldRotation(EG_GUID_TYPE entityID, glm::vec3* inRotation);
	void Eagle_TransformComponent_SetWorldScale(EG_GUID_TYPE entityID, glm::vec3* inScale);

	void Eagle_TransformComponent_GetRelativeTransform(EG_GUID_TYPE entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetRelativeLocation(EG_GUID_TYPE entityID, glm::vec3* outLocation);
	void Eagle_TransformComponent_GetRelativeRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation);
	void Eagle_TransformComponent_GetRelativeScale(EG_GUID_TYPE entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetRelativeTransform(EG_GUID_TYPE entityID, Transform* inTransform);
	void Eagle_TransformComponent_SetRelativeLocation(EG_GUID_TYPE entityID, glm::vec3* inLocation);
	void Eagle_TransformComponent_SetRelativeRotation(EG_GUID_TYPE entityID, glm::vec3* inRotation);
	void Eagle_TransformComponent_SetRelativeScale(EG_GUID_TYPE entityID, glm::vec3* inScale);

	//PointLight Component
	void Eagle_PointLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor);
	void Eagle_PointLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor);
	void Eagle_PointLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor);
	void Eagle_PointLightComponent_GetDistance(EG_GUID_TYPE entityID, float* outDistance);
	void Eagle_PointLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor);
	void Eagle_PointLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor);
	void Eagle_PointLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor);
	void Eagle_PointLightComponent_SetDistance(EG_GUID_TYPE entityID, float inDistance);

	//DirectionalLight Component
	void Eagle_DirectionalLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor);
	void Eagle_DirectionalLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor);
	void Eagle_DirectionalLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor);
	void Eagle_DirectionalLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor);
	void Eagle_DirectionalLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor);
	void Eagle_DirectionalLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor);

	//SpotLight Component
	void Eagle_SpotLightComponent_GetLightColor(EG_GUID_TYPE entityID, glm::vec3* outLightColor);
	void Eagle_SpotLightComponent_GetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* outAmbientColor);
	void Eagle_SpotLightComponent_GetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* outSpecularColor);
	void Eagle_SpotLightComponent_GetInnerCutoffAngle(EG_GUID_TYPE entityID, float* outInnerCutoffAngle);
	void Eagle_SpotLightComponent_GetOuterCutoffAngle(EG_GUID_TYPE entityID, float* outOuterCutoffAngle);
	void Eagle_SpotLightComponent_SetLightColor(EG_GUID_TYPE entityID, glm::vec3* inLightColor);
	void Eagle_SpotLightComponent_SetAmbientColor(EG_GUID_TYPE entityID, glm::vec3* inAmbientColor);
	void Eagle_SpotLightComponent_SetSpecularColor(EG_GUID_TYPE entityID, glm::vec3* inSpecularColor);
	void Eagle_SpotLightComponent_SetInnerCutoffAngle(EG_GUID_TYPE entityID, float inInnerCutoffAngle);
	void Eagle_SpotLightComponent_SetOuterCutoffAngle(EG_GUID_TYPE entityID, float inOuterCutoffAngle);
}
