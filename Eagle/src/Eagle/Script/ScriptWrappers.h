#pragma once

#include "Eagle/Components/Components.h"

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

	//Transform Component
	void Eagle_TransformComponent_GetTransform(EG_GUID_TYPE entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetTranslation(EG_GUID_TYPE entityID, glm::vec3* outTranslation);
	void Eagle_TransformComponent_GetRotation(EG_GUID_TYPE entityID, glm::vec3* outRotation);
	void Eagle_TransformComponent_GetScale(EG_GUID_TYPE entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetTransform(EG_GUID_TYPE entityID, Transform* inTransform);
	void Eagle_TransformComponent_SetTranslation(EG_GUID_TYPE entityID, glm::vec3* inTranslation);
	void Eagle_TransformComponent_SetRotation(EG_GUID_TYPE entityID, glm::vec3* inRotation);
	void Eagle_TransformComponent_SetScale(EG_GUID_TYPE entityID, glm::vec3* inScale);
}
