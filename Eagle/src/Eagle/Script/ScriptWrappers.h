#pragma once

#include "Eagle/Components/Components.h"

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
}

namespace Eagle::Script
{
	//Entity
	GUID_TYPE Eagle_Entity_GetParent(GUID_TYPE entityID);
	void Eagle_Entity_SetParent(GUID_TYPE entityID, GUID_TYPE parentID);
	MonoArray* Eagle_Entity_GetChildren(GUID_TYPE entityID);
	GUID_TYPE Eagle_Entity_CreateEntity();
	void Eagle_Entity_DestroyEntity(GUID_TYPE entityID);
	void Eagle_Entity_AddComponent(GUID_TYPE entityID, void* type);
	bool Eagle_Entity_HasComponent(GUID_TYPE entityID, void* type);

	//Transform Component
	void Eagle_TransformComponent_GetTransform(GUID_TYPE entityID, Transform* outTransform);
	void Eagle_TransformComponent_GetTranslation(GUID_TYPE entityID, glm::vec3* outTranslation);
	void Eagle_TransformComponent_GetRotation(GUID_TYPE entityID, glm::vec3* outRotation);
	void Eagle_TransformComponent_GetScale(GUID_TYPE entityID, glm::vec3* outScale);
	void Eagle_TransformComponent_SetTransform(GUID_TYPE entityID, Transform* inTransform);
	void Eagle_TransformComponent_SetTranslation(GUID_TYPE entityID, glm::vec3* inTranslation);
	void Eagle_TransformComponent_SetRotation(GUID_TYPE entityID, glm::vec3* inRotation);
	void Eagle_TransformComponent_SetScale(GUID_TYPE entityID, glm::vec3* inScale);
}
