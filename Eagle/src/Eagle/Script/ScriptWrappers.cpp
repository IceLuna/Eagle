#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngineRegistry.h"
#include "ScriptUserComponents.h"

#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsRagdollActor.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include <mono/jit/jit.h>

// Note: keep last
#include "ScriptWrappersInternal.h"

namespace Eagle
{
	//--------------Entity--------------
	MonoObject* Script::Eagle_Entity_GetParent(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get parent. Entity is null");
			return nullptr;
		}

		if (Entity parent = entity.GetParent())
			return ScriptEngine::GetEntityMonoObject(parent);

		return nullptr;
	}

	void Script::Eagle_Entity_SetParent(GUID entityID, GUID parentID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			Entity parent = scene->GetEntityByGUID(GUID(parentID));
			entity.SetParent(parent);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set parent. Entity is null");
	}

	MonoArray* Script::Eagle_Entity_GetChildren(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const std::vector<Entity>& children = entity.GetChildren();
			if (children.empty())
				return mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), 0);

			MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), children.size());

			size_t index = 0;
			for (auto& child : children)
			{
				GUID guid = child.GetGUID();
				void* data[] = 
				{
					&guid
				};
				MonoObject* obj = ScriptEngine::Construct("Eagle.Entity:.ctor(Eagle.GUID)", true, data);
				mono_array_set(result, MonoObject*, index++, obj);
			}
			return result;
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get children. Entity is null");
		return mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), 0);
	}

	void Script::Eagle_Entity_DestroyEntity(GUID entityID, bool bDestroyChildren)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			scene->DestroyEntity(entity, bDestroyChildren);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't destroy entity. Entity is null");
	}

	void Script::Eagle_Entity_AddComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. Entity is null");
			return;
		}

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		const ComponentBindings* bindings = ScriptEngineRegistry::FindComponentBindings(monoType);
		if (!bindings)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. '{}' is not an engine component", mono_type_get_name(monoType));
			return;
		}

		if (bindings->Has(entity))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. This component already exists!");
			return;
		}

		bindings->Add(entity);
	}

	void Script::Eagle_Entity_RemoveComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component from an Entity. Entity is null");
			return;
		}

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		const ComponentBindings* bindings = ScriptEngineRegistry::FindComponentBindings(monoType);
		if (!bindings)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component from an Entity. '{}' is not an engine component", mono_type_get_name(monoType));
			return;
		}

		if (!bindings->Has(entity))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component from an Entity. This component doesn't exist!");
			return;
		}

		bindings->Remove(entity);
	}

	// ---------- User C# components ----------
	// Managed instances are stored in `ScriptUserComponents`
	// C# decides whether a type is a user component and constructs it; C++ only owns the GC handles.
	bool Script::Eagle_Entity_AddUserComponent(GUID entityID, MonoObject* component)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. Entity is null");
			return false;
		}
		if (!component)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. Component is null");
			return false;
		}

		MonoClass* klass = mono_object_get_class(component);
		auto& storage = entity.HasComponent<ScriptUserComponents>() ? entity.GetComponent<ScriptUserComponents>() : entity.AddComponent<ScriptUserComponents>();
		auto [it, bInserted] = storage.Instances.try_emplace(klass);
		if (!bInserted)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component '{}' to Entity '{}'. This component already exists!", mono_class_get_name(klass), entity.GetName());
			return false;
		}

		it->second = MonoInstance::CreateFromObject(component);
		return true;
	}

	void Script::Eagle_Entity_RemoveUserComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component from an Entity. Entity is null");
			return;
		}

		MonoClass* klass = Utils::GetClassFromReflectionType(type);
		ScriptUserComponents* storage = Utils::GetUserComponents(entity);
		if (!klass || !storage || storage->Instances.erase(klass) == 0)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component from an Entity. This component doesn't exist!");
			return;
		}

		if (storage->Instances.empty())
			entity.RemoveComponent<ScriptUserComponents>();
	}

	bool Script::Eagle_Entity_HasUserComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'HasComponent'. Entity is null");
			return false;
		}

		MonoClass* klass = Utils::GetClassFromReflectionType(type);
		const ScriptUserComponents* storage = Utils::GetUserComponents(entity);
		return klass && storage && storage->Instances.find(klass) != storage->Instances.end();
	}

	MonoObject* Script::Eagle_Entity_GetUserComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetComponent'. Entity is null");
			return nullptr;
		}

		MonoClass* klass = Utils::GetClassFromReflectionType(type);
		const ScriptUserComponents* storage = Utils::GetUserComponents(entity);
		if (!klass || !storage)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get component from an Entity. This component doesn't exist!");
			return nullptr;
		}

		auto it = storage->Instances.find(klass);
		return it != storage->Instances.end() ? it->second->GetInstance() : nullptr;
	}

	bool Script::Eagle_Entity_HasComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		
		if (entity)
		{
			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			const ComponentBindings* bindings = ScriptEngineRegistry::FindComponentBindings(monoType);
			if (!bindings)
			{
				EG_CORE_ERROR("[ScriptEngine] Failed to call 'HasComponent'. It's not a component");
				return false;
			}
			return bindings->Has(entity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'HasComponent'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_Entity_IsValid(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		return entity.IsValid();
	}

	bool Script::Eagle_Entity_IsPendingDestroy(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		return entity && scene->IsPendingDestroy(entity);
	}

	MonoString* Script::Eagle_Entity_GetEntityName(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<EntitySceneNameComponent>().GetName().c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Entity name. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_Entity_GetForwardVector(GUID entityID, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*result = entity.GetForwardVector();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get forward vector of Entity. Entity is null");
	}

	void Script::Eagle_Entity_GetRightVector(GUID entityID, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*result = entity.GetRightVector();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get right vector of Entity. Entity is null");
	}

	void Script::Eagle_Entity_GetUpVector(GUID entityID, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*result = entity.GetUpVector();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get up vector of Entity. Entity is null");
	}

	MonoObject* Script::Eagle_Entity_GetChildrenByName(GUID entityID, MonoString* monoName)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call GetChildrenByName. Entity is null");
			return {};
		}

		const auto& children = entity.GetChildren();
		if (children.empty())
			return nullptr;

		const std::string name = MonoStringHandler(monoName).c_str();

		for (auto& child : children)
			if (child.GetName() == name)
				return ScriptEngine::GetEntityMonoObject(child);

		return nullptr;
	}

	bool Script::Eagle_Entity_IsMouseHovered(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		glm::vec2 mousePos = ImGuiLayer::GetMousePos() - scene->ViewportBounds[0];
		return Eagle_Entity_IsMouseHoveredByCoord(entityID, &mousePos);
	}

	bool Script::Eagle_Entity_IsMouseHoveredByCoord(GUID entityID, const glm::vec2* pos)
	{
		auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		if (!sceneRenderer->GetOptions().bEnableObjectPicking)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call IsMouseHovered. Object picking is disabled in the project settings");
			return false;
		}

		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call IsMouseHovered. Entity is null");
			return false;
		}

		// TODO: Check if works
		const glm::vec2 viewportSize = scene->ViewportBounds[1] - scene->ViewportBounds[0];
		const int mouseX = int(pos->x);
		const int mouseY = int(pos->y);

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
		{
			// Returns false if this coord hasn't been read back yet (only the first frames of polling a new non-mouse coord)
			int32_t data = -1;
			if (!sceneRenderer->GetObjectIDAt(glm::ivec2(mouseX, mouseY), data))
				return false;

			if (data >= 0)
			{
				const entt::entity enttID = (entt::entity)data;
				return entity.GetEnttID() == enttID;
			}
		}

		return false;
	}

	//-------------- Entity Transforms --------------
	void Script::Eagle_Entity_GetWorldTransform(GUID entityID, Transform* outTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetWorldTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world transform. Entity is null");
	}

	void Script::Eagle_Entity_GetWorldLocation(GUID entityID, glm::vec3* outLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetWorldLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world location. Entity is null");
	}

	void Script::Eagle_Entity_GetWorldRotation(GUID entityID, Rotator* outRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetWorldRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world rotation. Entity is null");
	}

	void Script::Eagle_Entity_GetWorldScale(GUID entityID, glm::vec3* outScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetWorldScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world scale. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldTransform(GUID entityID, const Transform* inTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world transform. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldLocation(GUID entityID, const glm::vec3* inLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world location. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldRotation(GUID entityID, const Rotator* inRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world rotation. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldScale(GUID entityID, const glm::vec3* inScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world scale. Entity is null");
	}
	
	void Script::Eagle_Entity_GetRelativeTransform(GUID entityID, Transform* outTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetRelativeTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative transform. Entity is null");
	}

	void Script::Eagle_Entity_GetRelativeLocation(GUID entityID, glm::vec3* outLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetRelativeLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative location. Entity is null");
	}

	void Script::Eagle_Entity_GetRelativeRotation(GUID entityID, Rotator* outRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetRelativeRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative rotation. Entity is null");
	}

	void Script::Eagle_Entity_GetRelativeScale(GUID entityID, glm::vec3* outScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetRelativeScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative scale. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeTransform(GUID entityID, const Transform* inTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative transform. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative location. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeRotation(GUID entityID, const Rotator* inRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative rotation. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeScale(GUID entityID, const glm::vec3* inScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative scale. Entity is null");
	}

	void Script::Eagle_Entity_SetTag(GUID entityID, MonoString* monoTag)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<TagComponent>().Tag = MonoStringHandler(monoTag).c_str();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set entity tag. Entity is null");
		}
	}

	MonoString* Script::Eagle_Entity_GetTag(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return mono_string_new(mono_domain_get(), entity.GetComponent<TagComponent>().Tag.c_str());
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get entity tag. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_Entity_SetOutline(GUID entityID, const glm::vec4* color, OutlineMode mode, float thickness, float hiddenOpacity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set outline. Entity is null");
			return;
		}

		if (!magic_enum::enum_contains(mode))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set outline. Invalid outline mode: {}", (int)mode);
			return;
		}

		OutlineSettings settings;
		settings.Color = *color;
		settings.Mode = mode;
		settings.Thickness = glm::clamp(thickness, 0.f, OutlineSettings::MaxThickness);
		settings.HiddenOpacity = glm::clamp(hiddenOpacity, 0.f, 1.f);
		scene->SetEntityOutline(entity, settings, OutlineLayer::Gameplay);
	}

	void Script::Eagle_Entity_RemoveOutline(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			scene->RemoveEntityOutline(entity, OutlineLayer::Gameplay);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove outline. Entity is null");
	}

	bool Script::Eagle_Entity_HasOutline(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `HasOutline`. Entity is null");
			return false;
		}
		return scene->HasEntityOutline(entity, OutlineLayer::Gameplay);
	}

	bool Script::Eagle_Entity_GetOutline(GUID entityID, glm::vec4* outColor, OutlineMode* outMode, float* outThickness, float* outHiddenOpacity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get outline. Entity is null");
			return false;
		}

		const OutlineSettings* settings = scene->GetEntityOutline(entity, OutlineLayer::Gameplay);
		if (!settings)
			return false;

		*outColor = settings->Color;
		*outMode = settings->Mode;
		*outThickness = settings->Thickness;
		*outHiddenOpacity = settings->HiddenOpacity;
		return true;
	}

	//-------------- Scene Component --------------
	void Script::Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*outTransform = comp->GetWorldTransform();
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetWorldLocation(GUID entityID, void* type, glm::vec3* outLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetWorldTransform();
			*outLocation = tempTransform.Location;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world location. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetWorldRotation(GUID entityID, void* type, Rotator* outRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetWorldTransform();
			*outRotation = tempTransform.Rotation;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world rotation. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetWorldScale(GUID entityID, void* type, glm::vec3* outScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetWorldTransform();
			*outScale = tempTransform.Scale3D;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world scale. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldTransform(GUID entityID, void* type, const Transform* inTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetWorldTransform(*inTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldLocation(GUID entityID, void* type, const glm::vec3* inLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetWorldTransform();
			tempTransform.Location = *inLocation;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetWorldTransform(*&tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world location. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldRotation(GUID entityID, void* type, const Rotator* inRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetWorldTransform();
			tempTransform.Rotation = *inRotation;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetWorldTransform(*&tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world rotation. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldScale(GUID entityID, void* type, const glm::vec3* inScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetWorldTransform();
			tempTransform.Scale3D = *inScale;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetWorldTransform(*&tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world scale. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRelativeTransform(GUID entityID, void* type, Transform* outTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*outTransform = comp->GetRelativeTransform();
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRelativeLocation(GUID entityID, void* type, glm::vec3* outLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetRelativeTransform();
			*outLocation = tempTransform.Location;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative location. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRelativeRotation(GUID entityID, void* type, Rotator* outRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetRelativeTransform();
			*outRotation = tempTransform.Rotation;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative rotation. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRelativeScale(GUID entityID, void* type, glm::vec3* outScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetRelativeTransform();
			*outScale = tempTransform.Scale3D;
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative scale. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeTransform(GUID entityID, void* type, const Transform* inTransform)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetRelativeTransform(*inTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeLocation(GUID entityID, void* type, const glm::vec3* inLocation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetRelativeTransform();
			tempTransform.Location = *inLocation;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetRelativeTransform(*&tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative location. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeRotation(GUID entityID, void* type, const Rotator* inRotation)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetRelativeTransform();
			tempTransform.Rotation = *inRotation;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetRelativeTransform(*&tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative rotation. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeScale(GUID entityID, void* type, const glm::vec3* inScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*&tempTransform = comp->GetRelativeTransform();
			tempTransform.Scale3D = *inScale;
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				comp->SetRelativeTransform(*&tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative scale. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetForwardVector(GUID entityID, void* type, glm::vec3* outVector)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*outVector = comp->GetForwardVector();
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' forward vector. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRightVector(GUID entityID, void* type, glm::vec3* outVector)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*outVector = comp->GetRightVector();
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' right vector. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetUpVector(GUID entityID, void* type, glm::vec3* outVector)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (SceneComponent* comp = ScriptEngineRegistry::GetSceneComponent(entity, monoType))
				*outVector = comp->GetUpVector();
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' up vector. Entity is null", typeName);
		}
	}

	//--------------Light Component--------------
	void Script::Eagle_LightComponent_GetLightColor(GUID entityID, void* type, glm::vec3* outLightColor)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				*outLightColor = comp->GetLightColor();
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'LightColor'. Entity is null");
	}

	bool Script::Eagle_LightComponent_GetAffectsWorld(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				return comp->DoesAffectWorld();
			return false;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'bAffectsWorld'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_LightComponent_GetCastsShadows(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				return comp->DoesCastShadows();
			return false;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'bCastsShadows'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_LightComponent_GetIsVolumetricLight(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				return comp->IsVolumetricLight();
			return false;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'bVolumetricLight'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_LightComponent_SetLightColor(GUID entityID, void* type, glm::vec3* inLightColor)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				comp->SetLightColor(*inLightColor);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'LightColor'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetAffectsWorld(GUID entityID, void* type, bool bAffectsWorld)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				comp->SetAffectsWorld(bAffectsWorld);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bAffectsWorld'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetCastsShadows(GUID entityID, void* type, bool bValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				comp->SetCastsShadows(bValue);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bCastsShadows'. Entity is null");
	}

	float Script::Eagle_LightComponent_GetIntensity(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				return comp->GetIntensity();
			return 0.f;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'Intensity'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_LightComponent_SetIntensity(GUID entityID, void* type, float inIntensity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				comp->SetIntensity(inIntensity);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Intensity'. Entity is null");
	}

	float Script::Eagle_LightComponent_GetVolumetricFogIntensity(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				return comp->GetVolumetricFogIntensity();
			return 0.f;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'Volumetric Fog Intensity'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_LightComponent_SetVolumetricFogIntensity(GUID entityID, void* type, float inIntensity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				comp->SetVolumetricFogIntensity(inIntensity);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Volumetric Fog Intensity'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetIsVolumetricLight(GUID entityID, void* type, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			if (LightComponent* comp = ScriptEngineRegistry::GetLightComponent(entity, monoType))
				comp->SetIsVolumetricLight(value);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bVolumetricLight'. Entity is null");
	}

	//--------------PointLight Component--------------
	float Script::Eagle_PointLightComponent_GetRadius(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<PointLightComponent>().GetRadius();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light radius. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_PointLightComponent_SetRadius(GUID entityID, float inRadius)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<PointLightComponent>().SetRadius(inRadius);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light radius. Entity is null");
	}

	//--------------SpotLight Component--------------
	float Script::Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpotLightComponent>().GetInnerCutOffAngle();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light inner cut off angle. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_SpotLightComponent_GetOuterCutoffAngle(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpotLightComponent>().GetOuterCutOffAngle();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light outer cut off angle. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_SpotLightComponent_GetDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpotLightComponent>().GetDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light distance. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_SpotLightComponent_SetInnerCutoffAngle(GUID entityID, float inInnerCutoffAngle)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().SetInnerCutOffAngle(inInnerCutoffAngle);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetOuterCutoffAngle(GUID entityID, float inOuterCutoffAngle)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().SetOuterCutOffAngle(inOuterCutoffAngle);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light outer cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetDistance(GUID entityID, float inDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().SetDistance(inDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light distance. Entity is null");
	}

	//--------------DirectionalLight Component--------------
	void Script::Eagle_DirectionalLightComponent_GetAmbient(GUID entityID, glm::vec3* outAmbient)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outAmbient = entity.GetComponent<DirectionalLightComponent>().GetAmbientColor();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'Ambient' of DirectionalLight Component. Entity is null");
		}
	}

	void Script::Eagle_DirectionalLightComponent_SetAmbient(GUID entityID, glm::vec3* inAmbient)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().SetAmbientColor(*inAmbient);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'Ambient' of DirectionalLight Component. Entity is null");
	}

	bool Script::Eagle_DirectionalLightComponent_GetCastsScreenSpaceShadows(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DirectionalLightComponent>().DoesCastScreenSpaceShadows();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'CastsScreenSpaceShadows' of DirectionalLight Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_DirectionalLightComponent_SetCastsScreenSpaceShadows(GUID entityID, bool bCasts)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().SetCastsScreenSpaceShadows(bCasts);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CastsScreenSpaceShadows' of DirectionalLight Component. Entity is null");
	}

	//--------------StaticMesh Component--------------
	void Script::Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for static mesh component. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<StaticMeshComponent>();
		if (assetID.IsNull())
		{
			component.SetMeshAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for static mesh component. Couldn't find an asset");
			return;
		}

		Ref<AssetStaticMesh> meshAsset = Cast<AssetStaticMesh>(asset);
		if (!meshAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for static mesh component. Provided asset is not a static mesh asset");
			return;
		}

		component.SetMeshAsset(meshAsset);
	}

	GUID Script::Eagle_StaticMeshComponent_GetMesh(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<StaticMeshComponent>();
		const auto& asset = component.GetMeshAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_StaticMeshComponent_GetMaterial(GUID entityID, uint32_t index, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		*outAssetID = GUID(0, 0);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<StaticMeshComponent>();
		if (index >= component.GetMaterialsSlotsCount())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh component material. Material slot `{}` is invalid!", index);
			return;
		}
		const auto& materialAsset = component.GetMaterialAsset(index);
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_StaticMeshComponent_SetMaterial(GUID entityID, uint32_t index, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set static mesh component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<StaticMeshComponent>();
		if (index >= component.GetMaterialsSlotsCount())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set static mesh component material. Material slot `{}` is invalid!", index);
			return;
		}

		if (assetID.IsNull())
		{
			component.SetMaterialAsset(index, nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set static mesh component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set static mesh component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(index, materialAsset);
	}

	uint32_t Script::Eagle_StaticMeshComponent_GetMaterialsSlotsCount(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh material slots count. Entity is null");
			return 0;
		}

		auto& component = entity.GetComponent<StaticMeshComponent>();
		return component.GetMaterialsSlotsCount();
	}

	void Script::Eagle_StaticMeshComponent_SetCastsShadows(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCastsShadows' for static mesh. Entity is null");
			return;
		}

		entity.GetComponent<StaticMeshComponent>().SetCastsShadows(value);
	}

	bool Script::Eagle_StaticMeshComponent_DoesCastShadows(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesCastShadows' for static mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<StaticMeshComponent>().DoesCastShadows();
	}

	void Script::Eagle_StaticMeshComponent_SetReceivesDecals(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetReceivesDecals' for static mesh. Entity is null");
			return;
		}

		entity.GetComponent<StaticMeshComponent>().SetReceivesDecals(value);
	}

	bool Script::Eagle_StaticMeshComponent_DoesReceiveDecals(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesReceiveDecals' for static mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<StaticMeshComponent>().DoesReceiveDecals();
	}

	void Script::Eagle_StaticMeshComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetVisible' for static mesh. Entity is null");
			return;
		}

		entity.GetComponent<StaticMeshComponent>().SetVisible(value);
	}

	bool Script::Eagle_StaticMeshComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsVisible' for static mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<StaticMeshComponent>().IsVisible();
	}
	
	//--------------SkeletalMesh Component--------------
	void Script::Eagle_SkeletalMeshComponent_SetMesh(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for skeletal mesh component. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<SkeletalMeshComponent>();
		if (assetID.IsNull())
		{
			component.SetMeshAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for skeletal mesh component. Couldn't find an asset");
			return;
		}

		Ref<AssetSkeletalMesh> meshAsset = Cast<AssetSkeletalMesh>(asset);
		if (!meshAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for skeletal mesh component. Provided asset is not a skeletal mesh asset");
			return;
		}

		component.SetMeshAsset(meshAsset);
	}

	GUID Script::Eagle_SkeletalMeshComponent_GetMesh(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get skeletal mesh. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<SkeletalMeshComponent>();
		const auto& asset = component.GetMeshAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SkeletalMeshComponent_GetMaterial(GUID entityID, uint32_t index, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get skeletal mesh component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<SkeletalMeshComponent>();
		if (index >= component.GetMaterialsSlotsCount())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get skeletal mesh component material. Material slot `{}` is invalid!", index);
			return;
		}
		const auto& materialAsset = component.GetMaterialAsset(index);
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SkeletalMeshComponent_SetMaterial(GUID entityID, uint32_t index, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<SkeletalMeshComponent>();
		if (index >= component.GetMaterialsSlotsCount())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component material. Material slot `{}` is invalid!", index);
			return;
		}
		if (assetID.IsNull())
		{
			component.SetMaterialAsset(index, nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(index, materialAsset);
	}
	
	void Script::Eagle_SkeletalMeshComponent_GetAnimation(GUID entityID, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get skeletal mesh component animation. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<SkeletalMeshComponent>();
		const auto& animationAsset = component.GetAnimationAsset();
		*outAssetID = animationAsset ? animationAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimation(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component animation. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<SkeletalMeshComponent>();
		if (assetID.IsNull())
		{
			component.SetAnimationAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component animation. Couldn't find an asset");
			return;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component animation. Provided asset is not a animation asset");
			return;
		}

		component.SetAnimationAsset(animationAsset);
	}

	void Script::Eagle_SkeletalMeshComponent_GetAnimationGraph(GUID entityID, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get skeletal mesh component animation graph. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<SkeletalMeshComponent>();
		const auto& animationAsset = component.GetAnimationGraphAsset();
		*outAssetID = animationAsset ? animationAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimationGraph(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component animation graph. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<SkeletalMeshComponent>();
		if (assetID.IsNull())
		{
			component.SetAnimationGraphAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component animation graph. Couldn't find an asset");
			return;
		}

		Ref<AssetAnimationGraph> animationAsset = Cast<AssetAnimationGraph>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set skeletal mesh component animation graph. Provided asset is not a animation asset");
			return;
		}

		component.SetAnimationGraphAsset(animationAsset);
	}

	uint32_t Script::Eagle_SkeletalMeshComponent_GetMaterialsSlotsCount(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get skeletal mesh material slots count. Entity is null");
			return 0;
		}

		auto& component = entity.GetComponent<SkeletalMeshComponent>();
		return component.GetMaterialsSlotsCount();
	}

	void Script::Eagle_SkeletalMeshComponent_SetCastsShadows(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCastsShadows' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetCastsShadows(value);
	}

	bool Script::Eagle_SkeletalMeshComponent_DoesCastShadows(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesCastShadows' for skeletal mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<SkeletalMeshComponent>().DoesCastShadows();
	}

	void Script::Eagle_SkeletalMeshComponent_SetReceivesDecals(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetReceivesDecals' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetReceivesDecals(value);
	}

	bool Script::Eagle_SkeletalMeshComponent_DoesReceiveDecals(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesReceiveDecals' for skeletal mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<SkeletalMeshComponent>().DoesReceiveDecals();
	}

	void Script::Eagle_SkeletalMeshComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetVisible' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetVisible(value);
	}

	bool Script::Eagle_SkeletalMeshComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsVisible' for skeletal mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<SkeletalMeshComponent>().IsVisible();
	}

	bool Script::Eagle_SkeletalMeshComponent_IsRootMotionLockFlagSet(GUID entityID, RootMotionLockFlag value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsRootMotionLockFlagSet' for skeletal mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<SkeletalMeshComponent>().IsRootMotionLockFlagSet(value);
	}

	void Script::Eagle_SkeletalMeshComponent_SetRootMotionLockFlagBool(GUID entityID, RootMotionLockFlag flag, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRootMotionLockFlag' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRootMotionLockFlag(flag, value);
	}

	void Script::Eagle_SkeletalMeshComponent_SetRootMotionLockFlag(GUID entityID, RootMotionLockFlag value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRootMotionLockFlag' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRootMotionLockFlag(value);
	}

	RootMotionLockFlag Script::Eagle_SkeletalMeshComponent_GetRootMotionLockFlags(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRootMotionLockFlags' for skeletal mesh. Entity is null");
			return RootMotionLockFlag::None;
		}

		return entity.GetComponent<SkeletalMeshComponent>().GetRootMotionLockFlags();
	}

	AnimationType Script::Eagle_SkeletalMeshComponent_GetAnimType(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'AnimType' for skeletal mesh. Entity is null");
			return {};
		}

		return entity.GetComponent<SkeletalMeshComponent>().AnimType;
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimType(GUID entityID, AnimationType value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'AnimType' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().AnimType = value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetCurrentClipPlayTime(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCurrentClipPlayTime' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().CurrentClipPlayTime = value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetClipPlaybackSpeed(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetClipPLaybackSpeed' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().ClipPlaybackSpeed = value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetIsClipLooping(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsClipLooping' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().bClipLooping = value;
	}

	float Script::Eagle_SkeletalMeshComponent_GetCurrentClipPlayTime(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetCurrentClipPlayTime' for skeletal mesh. Entity is null");
			return 0.f;
		}

		return entity.GetComponent<SkeletalMeshComponent>().CurrentClipPlayTime;
	}

	float Script::Eagle_SkeletalMeshComponent_GetClipPlaybackSpeed(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetClipPLaybackSpeed' for skeletal mesh. Entity is null");
			return 1.f;
		}

		return entity.GetComponent<SkeletalMeshComponent>().ClipPlaybackSpeed;
	}

	bool Script::Eagle_SkeletalMeshComponent_IsClipLooping(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsClipLooping' for skeletal mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<SkeletalMeshComponent>().bClipLooping;
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableBool(GUID entityID, MonoString* monoName, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableBool' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableBool' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableBool' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Bool)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableBool' for skeletal mesh. Variable '{}' is not a bool", name);
			return;
		}

		Cast<GraphVariableBool>(var)->Value = value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableInt(GUID entityID, MonoString* monoName, int value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableInt' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableInt' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableInt' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Int)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableInt' for skeletal mesh. Variable '{}' is not an integer", name);
			return;
		}

		Cast<GraphVariableInt>(var)->Value = value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableFloat(GUID entityID, MonoString* monoName, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableFloat' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableFloat' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableFloat' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Float)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableFloat' for skeletal mesh. Variable '{}' is not a float", name);
			return;
		}

		Cast<GraphVariableFloat>(var)->Value = value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableAnim(GUID entityID, MonoString* monoName, GUID animID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableAnimation' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableAnimation' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableAnimation' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Animation)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableAnimation' for skeletal mesh. Variable '{}' is not an animation", name);
			return;
		}

		Ref<AssetAnimation> animAsset;
		if (animID.IsNull() == false)
		{
			Ref<Asset> asset;
			AssetManager::Get(animID, &asset);
			if (!asset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableAnimation'. Couldn't find an AssetAnimation asset");
				return;
			}

			animAsset = Cast<AssetAnimation>(asset);
			if (!animAsset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableAnimation'. It's not an animation asset");
				return;
			}
		}

		Cast<GraphVariableAnimation>(var)->Value = animAsset;
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableString(GUID entityID, MonoString* monoName, MonoString* monoValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableString' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableString' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableString' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::String)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableString' for skeletal mesh. Variable '{}' is not a string", name);
			return;
		}

		Cast<GraphVariableString>(var)->Value = MonoStringHandler(monoValue).c_str();
	}

	void Script::Eagle_SkeletalMeshComponent_SetAnimGraphVariableVec4(GUID entityID, MonoString* monoName, const glm::vec4* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableVec4' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableVec4' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableVec4' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Vec4)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAnimGraphVariableVec4' for skeletal mesh. Variable '{}' is not a Vector4", name);
			return;
		}

		Cast<GraphVariableVec4>(var)->Value = *value;
	}

	bool Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableBool(GUID entityID, MonoString* monoName)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableBool' for skeletal mesh. Entity is null");
			return false;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableBool' for skeletal mesh. Graph is null");
			return false;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableBool' for skeletal mesh. Variable '{}' is not found", name);
			return false;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Bool)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableBool' for skeletal mesh. Variable '{}' is not a bool", name);
			return false;
		}

		return Cast<GraphVariableBool>(var)->Value;
	}

	int Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableInt(GUID entityID, MonoString* monoName)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableInt' for skeletal mesh. Entity is null");
			return 0;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableInt' for skeletal mesh. Graph is null");
			return 0;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableInt' for skeletal mesh. Variable '{}' is not found", name);
			return 0;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Int)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableInt' for skeletal mesh. Variable '{}' is not an integer", name);
			return 0;
		}

		return Cast<GraphVariableInt>(var)->Value;
	}

	float Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableFloat(GUID entityID, MonoString* monoName)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableFloat' for skeletal mesh. Entity is null");
			return 0.f;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableFloat' for skeletal mesh. Graph is null");
			return 0.f;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableFloat' for skeletal mesh. Variable '{}' is not found", name);
			return 0.f;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Float)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableFloat' for skeletal mesh. Variable '{}' is not a float", name);
			return 0.f;
		}

		return Cast<GraphVariableFloat>(var)->Value;
	}

	GUID Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableAnim(GUID entityID, MonoString* monoName)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableAnim' for skeletal mesh. Entity is null");
			return GUID(0, 0);
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableAnim' for skeletal mesh. Graph is null");
			return GUID(0, 0);
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableAnim' for skeletal mesh. Variable '{}' is not found", name);
			return GUID(0, 0);
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Animation)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableAnim' for skeletal mesh. Variable '{}' is not an animation", name);
			return GUID(0, 0);
		}

		const auto animVar = Cast<GraphVariableAnimation>(var);

		return animVar->Value ? animVar->Value->GetGUID() : GUID(0, 0);
	}

	MonoString* Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableString(GUID entityID, MonoString* monoName)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableString' for skeletal mesh. Entity is null");
			return nullptr;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableString' for skeletal mesh. Graph is null");
			return nullptr;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableString' for skeletal mesh. Variable '{}' is not found", name);
			return nullptr;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::String)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableString' for skeletal mesh. Variable '{}' is not a string", name);
			return nullptr;
		}

		return mono_string_new(mono_domain_get(), Cast<GraphVariableString>(var)->Value.c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_GetAnimGraphVariableVec4(GUID entityID, MonoString* monoName, glm::vec4* outResult)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableVec4' for skeletal mesh. Entity is null");
			return;
		}

		const auto& graph = entity.GetComponent<SkeletalMeshComponent>().GetAnimationGraph();
		if (!graph)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableVec4' for skeletal mesh. Graph is null");
			return;
		}

		const std::string name = MonoStringHandler(monoName).c_str();
		auto& var = graph->GetVariable(name);
		if (!var)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableVec4' for skeletal mesh. Variable '{}' is not found", name);
			return;
		}

		const GraphVariableType varType = var->GetType();
		if (varType != GraphVariableType::Vec4)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetAnimGraphVariableVec4' for skeletal mesh. Variable '{}' is not a Vector4", name);
			return;
		}

		*outResult = Cast<GraphVariableVec4>(var)->Value;
	}

	void Script::Eagle_SkeletalMeshComponent_SetRagdollEnabled(GUID entityID, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRagdollEnabled' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRagdollEnabled(bEnabled);
	}

	bool Script::Eagle_SkeletalMeshComponent_IsRagdollEnabled(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsRagdollEnabled' for skeletal mesh. Entity is null");
			return false;
		}

		return entity.GetComponent<SkeletalMeshComponent>().IsRagdollEnabled();
	}

	void Script::Eagle_SkeletalMeshComponent_GetRagdollBoneWorldTransform(GUID entityID, MonoString* monoName, Transform* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollBoneWorldTransform' for skeletal mesh. Entity is null");
			return;
		}

		*result = entity.GetComponent<SkeletalMeshComponent>().GetRagdollBoneWorldTransform(MonoStringHandler(monoName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_GetRagdollRootBoneWorldTransform(GUID entityID, Transform* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollRootBoneWorldTransform' for skeletal mesh. Entity is null");
			return;
		}

		*result = entity.GetComponent<SkeletalMeshComponent>().GetRagdollRootBoneWorldTransform();
	}

	void Script::Eagle_SkeletalMeshComponent_GetBoneWorldTransform(GUID entityID, MonoString* monoName, Transform* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetBoneWorldTransform' for skeletal mesh. Entity is null");
			return;
		}

		*result = entity.GetComponent<SkeletalMeshComponent>().GetBoneWorldTransform(MonoStringHandler(monoName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_GetBoneWorldLocation(GUID entityID, MonoString* monoName, glm::vec3* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetBoneWorldLocation' for skeletal mesh. Entity is null");
			return;
		}

		*result = entity.GetComponent<SkeletalMeshComponent>().GetBoneWorldLocation(MonoStringHandler(monoName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_GetBoneWorldRotation(GUID entityID, MonoString* monoName, Rotator* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetBoneWorldRotation' for skeletal mesh. Entity is null");
			return;
		}

		*result = entity.GetComponent<SkeletalMeshComponent>().GetBoneWorldRotation(MonoStringHandler(monoName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_GetBoneWorldScale(GUID entityID, MonoString* monoName, glm::vec3* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetBoneWorldScale' for skeletal mesh. Entity is null");
			return;
		}

		*result = entity.GetComponent<SkeletalMeshComponent>().GetBoneWorldScale(MonoStringHandler(monoName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_SetRagdollCollisionVisible(GUID entityID, bool bVisible)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRagdollCollisionVisible' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetShowRagdollCollision(bVisible);
	}

	void Script::Eagle_SkeletalMeshComponent_GetRagdollLinearVelocity(GUID entityID, glm::vec3* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollLinearVelocity' for skeletal mesh. Entity is null");
			return;
		}
		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			*result = ragdoll->GetLinearVelocity();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollLinearVelocity' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_GetRagdollAngularVelocity(GUID entityID, glm::vec3* result)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollAngularVelocity' for skeletal mesh. Entity is null");
			return;
		}
		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			*result = ragdoll->GetAngularVelocity();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollAngularVelocity' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_SetRagdollLinearVelocity(GUID entityID, const glm::vec3* velocity, bool bApplyToRootOnly)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRagdollLinearVelocity' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRagdollLinearVelocity(*velocity, bApplyToRootOnly);
	}

	void Script::Eagle_SkeletalMeshComponent_SetRagdollAngularVelocity(GUID entityID, const glm::vec3* velocity, bool bApplyToRootOnly)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRagdollAngularVelocity' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRagdollAngularVelocity(*velocity, bApplyToRootOnly);
	}

	void Script::Eagle_SkeletalMeshComponent_AddRagdollForce(GUID entityID, const glm::vec3* force, ForceMode forceMode, bool bApplyToRootOnly)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollForce' for skeletal mesh. Entity is null");
			return;
		}
		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			ragdoll->AddForce(*force, forceMode, bApplyToRootOnly);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollForce' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_AddRagdollForceAtLocation(GUID entityID, const glm::vec3* location, const glm::vec3* force, ForceMode forceMode, bool bApplyToRootOnly)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollForceAtLocation' for skeletal mesh. Entity is null");
			return;
		}
		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			ragdoll->AddForceAtLocation(*location, *force, forceMode, bApplyToRootOnly);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollForceAtLocation' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_AddRagdollTorque(GUID entityID, const glm::vec3* torque, ForceMode forceMode, bool bApplyToRootOnly)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollTorque' for skeletal mesh. Entity is null");
			return;
		}

		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			ragdoll->AddTorque(*torque, forceMode, bApplyToRootOnly);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollTorque' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_SetRagdollBoneLinearVelocity(GUID entityID, MonoString* boneName, const glm::vec3* velocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRagdollBoneLinearVelocity' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRagdollBoneLinearVelocity(MonoStringHandler(boneName).c_str(), *velocity);
	}

	void Script::Eagle_SkeletalMeshComponent_SetRagdollBoneAngularVelocity(GUID entityID, MonoString* boneName, const glm::vec3* velocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetRagdollBoneAngularVelocity' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().SetRagdollBoneAngularVelocity(MonoStringHandler(boneName).c_str(), *velocity);
	}

	void Script::Eagle_SkeletalMeshComponent_GetRagdollBoneLinearVelocity(GUID entityID, MonoString* boneName, glm::vec3* outVelocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollBoneLinearVelocity' for skeletal mesh. Entity is null");
			return;
		}

		*outVelocity = entity.GetComponent<SkeletalMeshComponent>().GetRagdollBoneLinearVelocity(MonoStringHandler(boneName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_GetRagdollBoneAngularVelocity(GUID entityID, MonoString* boneName, glm::vec3* outVelocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetRagdollBoneAngularVelocity' for skeletal mesh. Entity is null");
			return;
		}

		*outVelocity = entity.GetComponent<SkeletalMeshComponent>().GetRagdollBoneAngularVelocity(MonoStringHandler(boneName).c_str());
	}

	void Script::Eagle_SkeletalMeshComponent_AddRagdollBoneForce(GUID entityID, MonoString* boneName, const glm::vec3* force, ForceMode forceMode)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollBoneForce' for skeletal mesh. Entity is null");
			return;
		}

		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			ragdoll->AddBoneForce(MonoStringHandler(boneName).c_str(), *force, forceMode);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollBoneForce' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_AddRagdollBoneForceAtLocation(GUID entityID, MonoString* boneName, const glm::vec3* location, const glm::vec3* force, ForceMode forceMode)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollBoneForceAtLocation' for skeletal mesh. Entity is null");
			return;
		}

		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			ragdoll->AddBoneForceAtLocation(MonoStringHandler(boneName).c_str(), *location, *force, forceMode);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollBoneForceAtLocation' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_AddRagdollBoneTorque(GUID entityID, MonoString* boneName, const glm::vec3* torque, ForceMode forceMode)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollBoneTorque' for skeletal mesh. Entity is null");
			return;
		}

		if (auto& ragdoll = entity.GetComponent<SkeletalMeshComponent>().GetRagdollActor())
		{
			ragdoll->AddBoneTorque(MonoStringHandler(boneName).c_str(), *torque, forceMode);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'AddRagdollBoneTorque' for skeletal mesh. There's no ragdoll");
		}
	}

	void Script::Eagle_SkeletalMeshComponent_PutRagdollToSleep(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'PutRagdollToSleep' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().PutRagdollToSleep();
	}

	void Script::Eagle_SkeletalMeshComponent_WakeUpRagdoll(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'WakeUpRagdoll' for skeletal mesh. Entity is null");
			return;
		}

		entity.GetComponent<SkeletalMeshComponent>().WakeUpRagdoll();
	}
}
