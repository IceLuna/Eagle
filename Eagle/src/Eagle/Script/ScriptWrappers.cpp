#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include <mono/jit/jit.h>

namespace Eagle 
{
	extern std::unordered_map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	//SceneComponents
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetForwardVectorFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetRightVectorFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetUpVectorFunctions;

	//Light Component
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetLightColorFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetLightColorFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetIntensityFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetIntensityFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetVolumetricFogIntensityFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetVolumetricFogIntensityFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetCastsShadowsFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetCastsShadowsFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsVolumetricLightFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetIsVolumetricLightFunctions;

	//BaseColliderComponent
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsTriggerFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsTriggerFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionVisibleFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionVisibleFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Ref<AssetPhysicsMaterial>&)>> m_SetPhysicsMaterialFunctions;
	extern std::unordered_map<MonoType*, std::function<GUID(Entity&)>> m_GetPhysicsMaterialFunctions;

	extern MonoImage* s_AppAssemblyImage;
}

namespace Eagle::Script::Utils
{
	void SetMaterialTexture(const Ref<Material>& material, GUID guid, void (Material::*textureSetter)(const Ref<AssetTexture2D>&))
	{
		Ref<Asset> asset;
		Material* rawMaterial = material.get();

		if (AssetManager::Get(guid, &asset))
		{
			if (auto asset2D = Cast<AssetTexture2D>(asset))
				(rawMaterial->*textureSetter)(asset2D);
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Failed to update material. Provided asset is not a texture 2D");
				(rawMaterial->*textureSetter)(nullptr);
			}
		}
		else
			(rawMaterial->*textureSetter)(nullptr);
	};

	static void SetMaterial(const Ref<Material>& material, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode)
	{
		// Update textures
		{
			SetMaterialTexture(material, albedo, &Material::SetAlbedoAsset);
			SetMaterialTexture(material, metallness, &Material::SetMetallnessAsset);
			SetMaterialTexture(material, normal, &Material::SetNormalAsset);
			SetMaterialTexture(material, roughness, &Material::SetRoughnessAsset);
			SetMaterialTexture(material, ao, &Material::SetAOAsset);
			SetMaterialTexture(material, emissiveTexture, &Material::SetEmissiveAsset);
			SetMaterialTexture(material, opacityTexture, &Material::SetOpacityAsset);
			SetMaterialTexture(material, opacityMaskTexture, &Material::SetOpacityMaskAsset);
		}
		material->SetTintColor(*tint);
		material->SetEmissiveIntensity(*emissiveIntensity);
		material->SetTilingFactor(tilingFactor);
		material->SetBlendMode(blendMode);
	}

	static void GetMaterial(const Ref<Material>& material, GUID* outAlbedo, GUID* outMetallness, GUID* outNormal, GUID* outRoughness, GUID* outAO, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode)
	{
		const GUID null(0, 0);

		// Get textures
		{
			// Albedo
			{
				const auto& asset = material->GetAlbedoAsset();
				*outAlbedo = asset ? asset->GetGUID() : null;
			}
			// Metallness
			{
				const auto& asset = material->GetMetallnessAsset();
				*outMetallness = asset ? asset->GetGUID() : null;
			}
			// Normal
			{
				const auto& asset = material->GetNormalAsset();
				*outNormal = asset ? asset->GetGUID() : null;
			}
			// Roughness
			{
				const auto& asset = material->GetRoughnessAsset();
				*outRoughness = asset ? asset->GetGUID() : null;
			}
			// AO
			{
				const auto& asset = material->GetAOAsset();
				*outAO = asset ? asset->GetGUID() : null;
			}
			// Emissive
			{
				const auto& asset = material->GetEmissiveAsset();
				*outEmissiveTexture = asset ? asset->GetGUID() : null;
			}
			// Opacity
			{
				const auto& asset = material->GetOpacityAsset();
				*outOpacityTexture = asset ? asset->GetGUID() : null;
			}
			// Opacity Mask
			{
				const auto& asset = material->GetOpacityMaskAsset();
				*outOpacityMaskTexture = asset ? asset->GetGUID() : null;
			}
		}
		*outTint = material->GetTintColor();
		*outEmissiveIntensity = material->GetEmissiveIntensity();
		*outTilingFactor = material->GetTilingFactor();
		*outBlendMode = material->GetBlendMode();
	}

	static void OnPhysicsMaterialChanged(const Ref<AssetPhysicsMaterial>& material)
	{
		// TODO: not the best solution...

		const auto& scene = Scene::GetCurrentScene();

		// BoxColliderComponent
		{
			auto view = scene->GetAllEntitiesWith<BoxColliderComponent>();
			for (auto e : view)
			{
				auto& component = view.get<BoxColliderComponent>(e);
				if (component.GetPhysicsMaterialAsset() == material)
					component.SetPhysicsMaterialAsset(material);
			}
		}

		// SphereColliderComponent
		{
			auto view = scene->GetAllEntitiesWith<SphereColliderComponent>();
			for (auto e : view)
			{
				auto& component = view.get<SphereColliderComponent>(e);
				if (component.GetPhysicsMaterialAsset() == material)
					component.SetPhysicsMaterialAsset(material);
			}
		}

		// CapsuleColliderComponent
		{
			auto view = scene->GetAllEntitiesWith<CapsuleColliderComponent>();
			for (auto e : view)
			{
				auto& component = view.get<CapsuleColliderComponent>(e);
				if (component.GetPhysicsMaterialAsset() == material)
					component.SetPhysicsMaterialAsset(material);
			}
		}

		// MeshColliderComponent
		{
			auto view = scene->GetAllEntitiesWith<MeshColliderComponent>();
			for (auto e : view)
			{
				auto& component = view.get<MeshColliderComponent>(e);
				if (component.GetPhysicsMaterialAsset() == material)
					component.SetPhysicsMaterialAsset(material);
			}
		}
	}
}

namespace Eagle
{
	//--------------Entity--------------
	GUID Script::Eagle_Entity_GetParent(GUID entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get parent. Entity is null");
			return {0, 0};
		}

		if (Entity& parent = entity.GetParent())
			return parent.GetGUID();

		return {0, 0};
	}

	void Script::Eagle_Entity_SetParent(GUID entityID, GUID parentID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
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
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const std::vector<Entity>& children = entity.GetChildren();
			if (children.empty())
				return nullptr;

			MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), children.size());

			uint32_t index = 0;
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
		return nullptr;
	}

	void Script::Eagle_Entity_DestroyEntity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			scene->DestroyEntity(entity);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't destroy entity. Entity is null");
	}

	void Script::Eagle_Entity_AddComponent(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
		{
			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			m_AddComponentFunctions[monoType](entity);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. Entity is null");
	}

	bool Script::Eagle_Entity_HasComponent(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		
		if (entity)
		{
			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			auto it = m_HasComponentFunctions.find(monoType);
			if (it == m_HasComponentFunctions.end())
			{
				EG_CORE_ERROR("[ScriptEngine] Failed to call 'HasComponent'. It's not a component");
				return false;
			}
			return it->second(entity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'HasComponent'. Entity is null");
			return false;
		}

	}

	MonoString* Script::Eagle_Entity_GetEntityName(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<EntitySceneNameComponent>().Name.c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Entity name. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_Entity_GetForwardVector(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*result = entity.GetForwardVector();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get forward vector of Entity. Entity is null");
	}

	void Script::Eagle_Entity_GetRightVector(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*result = entity.GetRightVector();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get right vector of Entity. Entity is null");
	}

	void Script::Eagle_Entity_GetUpVector(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*result = entity.GetUpVector();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get up vector of Entity. Entity is null");
	}

	GUID Script::Eagle_Entity_GetChildrenByName(GUID entityID, MonoString* monoName)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call GetChildrenByName. Entity is null");
			return {};
		}

		const auto& children = entity.GetChildren();
		if (children.empty())
			return { 0, 0 };

		const std::string name = mono_string_to_utf8(monoName);

		for (auto& child : children)
			if (child.GetSceneName() == name)
				return child.GetGUID();

		return { 0, 0 };
	}

	bool Script::Eagle_Entity_IsMouseHovered(GUID entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		glm::vec2 mousePos = ImGuiLayer::GetMousePos() - scene->ViewportBounds[0];
		return Eagle_Entity_IsMouseHoveredByCoord(entityID, &mousePos);
	}

	bool Script::Eagle_Entity_IsMouseHoveredByCoord(GUID entityID, const glm::vec2* pos)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		if (!sceneRenderer->GetOptions().bEnableObjectPicking)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call IsMouseHovered. Object picking is disabled");
			return false;
		}

		Entity entity = scene->GetEntityByGUID(entityID);

		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call IsMouseHovered. Entity is null");
			return false;
		}

		#ifndef EG_WITH_EDITOR
			#error "Check if works"
		#endif

		const glm::vec2 viewportSize = scene->ViewportBounds[1] - scene->ViewportBounds[0];

		const int mouseX = int(pos->x);
		const int mouseY = int(pos->y);

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
		{
			Ref<Image>& image = sceneRenderer->GetGBuffer().ObjectIDCopy;
			int data = -1;

			const ImageSubresourceLayout imageLayout = image->GetImageSubresourceLayout();
			uint8_t* mapped = (uint8_t*)image->Map();
			mapped += imageLayout.Offset;
			mapped += imageLayout.RowPitch * mouseY;
			memcpy(&data, ((uint32_t*)mapped) + mouseX, sizeof(int));
			image->Unmap();

			if (data >= 0)
			{
				const entt::entity enttID = (entt::entity)data;
				return entity.GetEnttID() == enttID;
			}
		}

		return false;
	}

	GUID Script::Eagle_Entity_SpawnEntity(MonoString* monoName)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		const std::string name = mono_string_to_utf8(monoName);
		return scene->CreateEntity(name).GetGUID();
	}

	GUID Script::Eagle_Entity_SpawnEntityFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't spawn entity. Couldn't find an Entity asset");
			return GUID(0, 0);
		}

		if (Ref<AssetEntity> entityAsset = Cast<AssetEntity>(asset))
		{
			Ref<Scene>& scene = Scene::GetCurrentScene();
			return scene->CreateFromEntityAsset(entityAsset).GetGUID();
		}
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't set paused. It's not a SoundGroup asset");
		return GUID(0, 0);
	}

	//-------------- Entity Transforms --------------
	void Script::Eagle_Entity_GetWorldTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetWorldTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world transform. Entity is null");
	}

	void Script::Eagle_Entity_GetWorldLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetWorldLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world location. Entity is null");
	}

	void Script::Eagle_Entity_GetWorldRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetWorldRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world rotation. Entity is null");
	}

	void Script::Eagle_Entity_GetWorldScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetWorldScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world scale. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world transform. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world location. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldRotation(GUID entityID, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world rotation. Entity is null");
	}

	void Script::Eagle_Entity_SetWorldScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world scale. Entity is null");
	}
	
	void Script::Eagle_Entity_GetRelativeTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetRelativeTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative transform. Entity is null");
	}

	void Script::Eagle_Entity_GetRelativeLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetRelativeLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative location. Entity is null");
	}

	void Script::Eagle_Entity_GetRelativeRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetRelativeRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative rotation. Entity is null");
	}

	void Script::Eagle_Entity_GetRelativeScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetRelativeScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative scale. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative transform. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative location. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeRotation(GUID entityID, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative rotation. Entity is null");
	}

	void Script::Eagle_Entity_SetRelativeScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative scale. Entity is null");
	}

	//-------------- Scene Component --------------
	void Script::Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetWorldTransformFunctions[monoType](entity, outTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' world transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetWorldLocation(GUID entityID, void* type, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetWorldTransformFunctions[monoType](entity, inTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldLocation(GUID entityID, void* type, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Location = *inLocation;
			m_SetWorldTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world location. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldRotation(GUID entityID, void* type, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Rotation = *inRotation;
			m_SetWorldTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world rotation. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetWorldScale(GUID entityID, void* type, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetWorldTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Scale3D = *inScale;
			m_SetWorldTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' world scale. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRelativeTransform(GUID entityID, void* type, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetRelativeTransformFunctions[monoType](entity, outTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' relative transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRelativeLocation(GUID entityID, void* type, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetRelativeTransformFunctions[monoType](entity, inTransform);
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative transform. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeLocation(GUID entityID, void* type, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Location = *inLocation;
			m_SetRelativeTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative location. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeRotation(GUID entityID, void* type, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Rotation = *inRotation;
			m_SetRelativeTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative rotation. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_SetRelativeScale(GUID entityID, void* type, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			Transform tempTransform;
			m_GetRelativeTransformFunctions[monoType](entity, &tempTransform);
			tempTransform.Scale3D = *inScale;
			m_SetRelativeTransformFunctions[monoType](entity, &tempTransform);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't set '{0}' relative scale. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetForwardVector(GUID entityID, void* type, glm::vec3* outVector)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			m_GetForwardVectorFunctions[monoType](entity, outVector);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' forward vector. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetRightVector(GUID entityID, void* type, glm::vec3* outVector)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			m_GetRightVectorFunctions[monoType](entity, outVector);
		}
		else
		{
			const char* typeName = mono_type_get_name(monoType);
			EG_CORE_ERROR("[ScriptEngine] Couldn't get '{0}' right vector. Entity is null", typeName);
		}
	}

	void Script::Eagle_SceneComponent_GetUpVector(GUID entityID, void* type, glm::vec3* outVector)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			m_GetUpVectorFunctions[monoType](entity, outVector);
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetLightColorFunctions[monoType](entity, outLightColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'LightColor'. Entity is null");
	}

	bool Script::Eagle_LightComponent_GetAffectsWorld(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetAffectsWorldFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'bAffectsWorld'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_LightComponent_GetCastsShadows(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetCastsShadowsFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'bCastsShadows'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_LightComponent_GetIsVolumetricLight(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetIsVolumetricLightFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'bVolumetricLight'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_LightComponent_SetLightColor(GUID entityID, void* type, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetLightColorFunctions[monoType](entity, inLightColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'LightColor'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetAffectsWorld(GUID entityID, void* type, bool bAffectsWorld)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetAffectsWorldFunctions[monoType](entity, bAffectsWorld);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bAffectsWorld'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetCastsShadows(GUID entityID, void* type, bool bValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetCastsShadowsFunctions[monoType](entity, bValue);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bCastsShadows'. Entity is null");
	}

	float Script::Eagle_LightComponent_GetIntensity(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetIntensityFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'Intensity'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_LightComponent_SetIntensity(GUID entityID, void* type, float inIntensity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetIntensityFunctions[monoType](entity, inIntensity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Intensity'. Entity is null");
	}

	float Script::Eagle_LightComponent_GetVolumetricFogIntensity(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetVolumetricFogIntensityFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'Volumetric Fog Intensity'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_LightComponent_SetVolumetricFogIntensity(GUID entityID, void* type, float inIntensity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetVolumetricFogIntensityFunctions[monoType](entity, inIntensity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Volumetric Fog Intensity'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetIsVolumetricLight(GUID entityID, void* type, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetIsVolumetricLightFunctions[monoType](entity, value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bVolumetricLight'. Entity is null");
	}

	//--------------PointLight Component--------------
	float Script::Eagle_PointLightComponent_GetRadius(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<PointLightComponent>().SetRadius(inRadius);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light radius. Entity is null");
	}

	//--------------SpotLight Component--------------
	float Script::Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
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
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().SetInnerCutOffAngle(inInnerCutoffAngle);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetOuterCutoffAngle(GUID entityID, float inOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().SetOuterCutOffAngle(inOuterCutoffAngle);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light outer cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetDistance(GUID entityID, float inDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().SetDistance(inDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light distance. Entity is null");
	}

	//--------------DirectionalLight Component--------------
	void Script::Eagle_DirectionalLightComponent_GetAmbient(GUID entityID, glm::vec3* outAmbient)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outAmbient = entity.GetComponent<DirectionalLightComponent>().Ambient;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'Ambient' of DirectionalLight Component. Entity is null");
		}
	}

	void Script::Eagle_DirectionalLightComponent_SetAmbient(GUID entityID, glm::vec3* inAmbient)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Ambient = *inAmbient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'Ambient' of DirectionalLight Component. Entity is null");
	}

	//--------------StaticMesh Component--------------
	void Script::Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
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

		Ref<AssetMesh> meshAsset = Cast<AssetMesh>(asset);
		if (!meshAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mesh for static mesh component. Provided asset is not a mesh asset");
			return;
		}

		component.SetMeshAsset(meshAsset);
	}

	GUID Script::Eagle_StaticMeshComponent_GetMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get collision mesh. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<StaticMeshComponent>();
		const auto& asset = component.GetMeshAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_StaticMeshComponent_GetMaterial(GUID entityID, GUID* outAssetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<StaticMeshComponent>();
		const auto& materialAsset = component.GetMaterialAsset();
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_StaticMeshComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set static mesh component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<StaticMeshComponent>();
		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
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

		component.SetMaterialAsset(materialAsset);
	}

	void Script::Eagle_StaticMeshComponent_SetCastsShadows(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCastsShadows'. Entity is null");

		entity.GetComponent<StaticMeshComponent>().SetCastsShadows(value);
	}

	bool Script::Eagle_StaticMeshComponent_DoesCastShadows(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesCastShadows'. Entity is null");
			return false;
		}

		return entity.GetComponent<StaticMeshComponent>().DoesCastShadows();
	}

	//--------------Sound--------------
	void Script::Eagle_Sound_SetSettings(GUID id, const SoundSettings* settings)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			sound->SetVolumeMultiplier(settings->VolumeMultiplier);
			sound->SetPan(settings->Pan);
			sound->SetLoopCount(settings->LoopCount);
			sound->SetLooping(settings->IsLooping);
			sound->SetStreaming(settings->IsStreaming);
			sound->SetMuted(settings->IsMuted);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound settings. Sound is not found");
	}

	void Script::Eagle_Sound_GetSettings(GUID id, SoundSettings* outSettings)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			*outSettings = sound->GetSettings();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sound settings. Sound is not found");
	}

	void Script::Eagle_Sound_Play(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play sound. Sound is not found");
	}

	void Script::Eagle_Sound_Stop(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop sound. Sound is not found");
	}

	void Script::Eagle_Sound_SetPaused(GUID id, bool bPaused)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPaused`. Sound is not found");
	}

	bool Script::Eagle_Sound_IsPlaying(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->IsPlaying();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsPlaying`. Sound is not found");
		return false;
	}

	void Script::Eagle_Sound_SetPosition(GUID id, uint32_t ms)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetPosition(ms);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPosition`. Sound is not found");
	}

	uint32_t Script::Eagle_Sound_GetPosition(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetPosition();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPosition`. Sound is not found");
		return 0u;
	}

	//--------------Sound2D--------------
	GUID Script::Eagle_Sound2D_Create(GUID assetID, const SoundSettings* settings)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Couldn't find an asset");
			return GUID(0, 0);
		}

		Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset);
		if (!audioAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Provided asset is not an audio asset");
			return GUID(0, 0);
		}

		Ref<Scene>& scene = Scene::GetCurrentScene();
		return scene->SpawnSound2D(audioAsset, *settings).ID;
	}

	//--------------Sound3D--------------
	GUID Script::Eagle_Sound3D_Create(GUID assetID, const glm::vec3* position, RollOffModel rolloff, const SoundSettings* settings)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Couldn't find an asset");
			return GUID(0, 0);
		}

		Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset);
		if (!audioAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create a sound 2D. Provided asset is not an audio asset");
			return GUID(0, 0);
		}

		Ref<Scene>& scene = Scene::GetCurrentScene();
		return scene->SpawnSound3D(audioAsset, *position, rolloff, *settings).ID;
	}

	void Script::Eagle_Sound3D_SetMinDistance(GUID id, float min)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMinDistance(min);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMinDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetMaxDistance(GUID id, float max)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMaxDistance(max);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMaxDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetMinMaxDistance(GUID id, float min, float max)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMinMaxDistance(min, max);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMinMaxDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetWorldPosition(GUID id, const glm::vec3* position)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetPosition(*position);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetWorldPosition`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetVelocity(GUID id, const glm::vec3* velocity)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetVelocity(*velocity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVelocity`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetRollOffModel(GUID id, RollOffModel rollOff)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRollOffModel`. Sound is not found");
	}

	float Script::Eagle_Sound3D_GetMinDistance(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetMinDistance();
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMinDistance`. Sound is not found");
		return 0.f;
	}

	float Script::Eagle_Sound3D_GetMaxDistance(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetMaxDistance();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMaxDistance`. Sound is not found");
		return 0.f;
	}

	void Script::Eagle_Sound3D_GetWorldPosition(GUID id, glm::vec3* outPosition)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			*outPosition = sound->GetWorldPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetWorldPosition`. Sound is not found");
	}

	void Script::Eagle_Sound3D_GetVelocity(GUID id, glm::vec3* outVelocity)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			*outVelocity = sound->GetVelocity();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetVelocity`. Sound is not found");
	}

	RollOffModel Script::Eagle_Sound3D_GetRollOffModel(GUID id)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetRollOffModel();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetRollOffModel`. Sound is not found");
		return RollOffModel::Default;
	}

	//--------------AudioComponent--------------
	void Script::Eagle_AudioComponent_SetMinDistance(GUID entityID, float minDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinDistance(minDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMaxDistance(GUID entityID, float maxDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMaxDistance(maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's max distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMinMaxDistance(GUID entityID, float minDistance, float maxDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinMaxDistance(minDistance, maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min-max distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetRollOffModel(GUID entityID, RollOffModel rollOff)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's roll off model. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetVolume(GUID entityID, float volume)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetVolume(volume);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's volume. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetLoopCount(GUID entityID, int loopCount)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLoopCount(loopCount);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's loop count. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetLooping(GUID entityID, bool bLooping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLooping(bLooping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's looping mode. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMuted(GUID entityID, bool bMuted)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMuted(bMuted);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetMuted' function. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetAudioAsset(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio asset of AudioComponent. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<AudioComponent>();
		if (assetID.IsNull())
		{
			component.SetAudioAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio asset of AudioComponent. Couldn't find an asset");
			return;
		}

		Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset);
		if (!audioAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio asset of AudioComponent. Provided asset is not an audio 2D asset");
			return;
		}

		component.SetAudioAsset(audioAsset);
	}

	GUID Script::Eagle_AudioComponent_GetAudioAsset(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio asset of AudioComponent. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<AudioComponent>();
		const auto& asset = component.GetAudioAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_AudioComponent_SetStreaming(GUID entityID, bool bStreaming)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetStreaming(bStreaming);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's streaming mode. Entity is null");
	}

	void Script::Eagle_AudioComponent_Play(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play audio component's sound. Entity is null");
	}

	void Script::Eagle_AudioComponent_Stop(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop audio component's sound. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetPaused(GUID entityID, bool bPaused)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPaused' function. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetDopplerEffectEnabled(GUID entityID, bool bEnable)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().bEnableDopplerEffect = bEnable;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetDopplerEffectEnabled' function. Entity is null");
	}

	float Script::Eagle_AudioComponent_GetMinDistance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetMinDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's min distance. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_AudioComponent_GetMaxDistance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetMaxDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's max distance. Entity is null");
			return 0.f;
		}
	}

	RollOffModel Script::Eagle_AudioComponent_GetRollOffModel(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetRollOffModel();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's roll off model. Entity is null");
			return RollOffModel::Default;
		}
	}

	float Script::Eagle_AudioComponent_GetVolume(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetVolume();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's volume. Entity is null");
			return 0.f;
		}
	}

	int Script::Eagle_AudioComponent_GetLoopCount(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetLoopCount();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's loop count. Entity is null");
			return 0;
		}
	}

	bool Script::Eagle_AudioComponent_IsLooping(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsLooping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's looping mode. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsMuted(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsMuted();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsMuted' function. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsStreaming(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsStreaming();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsStreaming' function. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsPlaying(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsPlaying();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsPlaying' function. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_AudioComponent_IsDopplerEffectEnabled(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().bEnableDopplerEffect;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsDopplerEffectEnabled' function. Entity is null");
			return false;
		}
	}

	//--------------RigidBodyComponent--------------
	void Script::Eagle_RigidBodyComponent_SetBodyType(GUID entityID, RigidBodyComponent::Type type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().BodyType = type;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics body type. Entity is null");
	}

	RigidBodyComponent::Type Script::Eagle_RigidBodyComponent_GetBodyType(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().BodyType;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get physics body type. Entity is null");
			return RigidBodyComponent::Type::Static;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMass(GUID entityID, float mass)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetMass(mass);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mass. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetMass(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetMass();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mass. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLinearDamping(GUID entityID, float linearDamping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLinearDamping(linearDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear damping. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetLinearDamping(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetLinearDamping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get linear damping. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetAngularDamping(GUID entityID, float angularDamping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetAngularDamping(angularDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular damping. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetAngularDamping(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetAngularDamping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get angular damping. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetEnableGravity(GUID entityID, bool bEnable)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetEnableGravity(bEnable);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetEnableGravity`. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsGravityEnabled(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsGravityEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsGravityEnabled`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetIsKinematic(GUID entityID, bool bKinematic)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetIsKinematic(bKinematic);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsKinematic`. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsKinematic(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsKinematic();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsKinematic`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_RigidBodyComponent_WakeUp(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->WakeUp();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_PutToSleep(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->PutToSleep();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_AddForce(GUID entityID, const glm::vec3* force, ForceMode forceMode)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddForce(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_AddTorque(GUID entityID, const glm::vec3* force, ForceMode forceMode)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddTorque(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetLinearVelocity(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*result = physicsActor->GetLinearVelocity();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLinearVelocity(GUID entityID, const glm::vec3* velocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetLinearVelocity(*velocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set linear velocity of Entity. Entity is not a physics actor");
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetAngularVelocity(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*result = physicsActor->GetAngularVelocity();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetAngularVelocity(GUID entityID, const glm::vec3* velocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetAngularVelocity(*velocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. It's not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_RigidBodyComponent_GetMaxLinearVelocity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().GetMaxLinearVelocity();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max linear velocity of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMaxLinearVelocity(GUID entityID, float maxVelocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<RigidBodyComponent>().SetMaxLinearVelocity(maxVelocity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max linear velocity of Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_RigidBodyComponent_GetMaxAngularVelocity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().GetMaxAngularVelocity();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max angular velocity of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMaxAngularVelocity(GUID entityID, float maxVelocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<RigidBodyComponent>().SetMaxAngularVelocity(maxVelocity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max angular velocity of Entity. Entity is null");
			return;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsDynamic(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->IsDynamic();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDynamic`. It's not a physics actor: {}", entity.GetSceneName());
				return false;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDynamic` of Entity. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsLockFlagSet(GUID entityID, ActorLockFlag flag)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().IsLockFlagSet(flag);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsLockFlagSet`. Entity is null");
			return false;
		}
	}

	ActorLockFlag Script::Eagle_RigidBodyComponent_GetLockFlags(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<RigidBodyComponent>().GetLockFlags();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLockFlags`. Entity is null");
			return ActorLockFlag();
		}
	}

	void Script::Eagle_RigidBodyComponent_GetKinematicTarget(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*outTransform = physicsActor->GetKinematicTarget();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTarget` of Entity. It's is not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTarget` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetKinematicTargetLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*outLocation = physicsActor->GetKinematicTargetLocation();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetLocation` of Entity. It's is not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetLocation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_GetKinematicTargetRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				*outRotation = physicsActor->GetKinematicTargetRotation();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetRotation` of Entity. It's is not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetRotation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetKinematicTarget(GUID entityID, const glm::vec3* location, const Rotator* rotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematicTarget(*location, *rotation);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTarget` of Entity. It's is not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTarget` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetKinematicTargetLocation(GUID entityID, const glm::vec3* location)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematicTargetLocation(*location);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetLocation` of Entity. It's is not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetLocation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetKinematicTargetRotation(GUID entityID, const Rotator* rotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetComponent<RigidBodyComponent>().GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematicTargetRotation(*rotation);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetRotation` of Entity. It's is not a physics actor: {}", entity.GetSceneName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetRotation` of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLockFlag(GUID entityID, ActorLockFlag flag, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<RigidBodyComponent>().SetLockFlag(flag, value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetLockFlag`. Entity is null");
			return;
		}
	}

	//--------------BaseColliderComponent--------------
	void Script::Eagle_BaseColliderComponent_SetIsTrigger(GUID entityID, void* type, bool bTrigger)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetIsTriggerFunctions[monoType](entity, bTrigger);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTrigger'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsTrigger(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_IsTriggerFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsTrigger'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetCollisionVisible(GUID entityID, void* type, bool bShow)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			m_SetCollisionVisibleFunctions[monoType](entity, bShow);
			return;
		}
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCollisionVisible'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsCollisionVisible(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_IsCollisionVisibleFunctions[monoType](entity);
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsCollisionVisible'. Entity is null");
		return false;
	}

	GUID Script::Eagle_BaseColliderComponent_GetPhysicsMaterial(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetPhysicsMaterialFunctions[monoType](entity);

		EG_CORE_ERROR("[ScriptEngine] Couldn't get PhysicsMaterial. Entity is null");
		return GUID(0, 0);
	}

	void Script::Eagle_BaseColliderComponent_SetPhysicsMaterial(GUID entityID, void* type, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material. Entity is null");
			return;
		}

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (assetID.IsNull())
		{
			m_SetPhysicsMaterialFunctions[monoType](entity, nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material. Couldn't find an asset");
			return;
		}

		Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset);
		if (!material)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material. Provided asset is not a PhysicsMaterial asset");
			return;
		}

		m_SetPhysicsMaterialFunctions[monoType](entity, material);
	}

	//--------------BoxColliderComponent--------------
	void Script::Eagle_BoxColliderComponent_SetSize(GUID entityID, const glm::vec3* size)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<BoxColliderComponent>().SetSize(*size);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set size of BoxCollider. Entity is null");
	}

	void Script::Eagle_BoxColliderComponent_GetSize(GUID entityID, glm::vec3* outSize)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			*outSize = entity.GetComponent<BoxColliderComponent>().GetSize();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get size of BoxCollider. Entity is null");
			*outSize = glm::vec3{0.f};
		}
	}

	//--------------SphereColliderComponent--------------
	void Script::Eagle_SphereColliderComponent_SetRadius(GUID entityID, float val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<SphereColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of SphereCollider. Entity is null");
	}

	float Script::Eagle_SphereColliderComponent_GetRadius(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<SphereColliderComponent>().GetRadius();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get radius of SphereCollider. Entity is null");
			return 0.f;
		}
	}

	//--------------CapsuleColliderComponent--------------
	void Script::Eagle_CapsuleColliderComponent_SetRadius(GUID entityID, float val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of CapsuleCollider. Entity is null");
	}

	float Script::Eagle_CapsuleColliderComponent_GetRadius(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CapsuleColliderComponent>().GetRadius();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get radius of CapsuleCollider. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CapsuleColliderComponent_SetHeight(GUID entityID, float val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetHeight(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set height of CapsuleCollider. Entity is null");
	}

	float Script::Eagle_CapsuleColliderComponent_GetHeight(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CapsuleColliderComponent>().GetHeight();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get height of CapsuleCollider. Entity is null");
			return 0.f;
		}
	}

	//--------------MeshColliderComponent--------------
	void Script::Eagle_MeshColliderComponent_SetIsConvex(GUID entityID, bool val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsConvex(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsConvex'. Entity is null");
	}

	bool Script::Eagle_MeshColliderComponent_IsConvex(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<MeshColliderComponent>().IsConvex();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsConvex'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_MeshColliderComponent_SetIsTwoSided(GUID entityID, bool val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsTwoSided(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTwoSided'. Entity is null");
	}

	bool Script::Eagle_MeshColliderComponent_IsTwoSided(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<MeshColliderComponent>().IsTwoSided();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsTwoSided'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_MeshColliderComponent_SetCollisionMesh(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<MeshColliderComponent>();
		if (assetID.IsNull())
		{
			component.SetCollisionMeshAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Couldn't find an asset");
			return;
		}

		Ref<AssetMesh> meshAsset = Cast<AssetMesh>(asset);
		if (!meshAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Provided asset is not a mesh asset");
			return;
		}

		component.SetCollisionMeshAsset(meshAsset);
	}

	GUID Script::Eagle_MeshColliderComponent_GetCollisionMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get collision mesh. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<MeshColliderComponent>();
		const auto& asset = component.GetCollisionMeshAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	//--------------Camera Component--------------
	void Script::Eagle_CameraComponent_SetIsPrimary(GUID entityID, bool val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Primary = val;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'IsPrimary'. Entity is null");
	}

	bool Script::Eagle_CameraComponent_GetIsPrimary(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Primary;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'IsPrimary'. Entity is null");
			return false;
		}
	}

	float Script::Eagle_CameraComponent_GetPerspectiveVerticalFOV(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveVerticalFOV();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'PerspectiveVerticalFOV'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetPerspectiveVerticalFOV(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveVerticalFOV(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveVerticalFOV'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetPerspectiveNearClip(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveNearClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'PerspectiveNearClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetPerspectiveNearClip(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveNearClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetPerspectiveFarClip(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveFarClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'PerspectiveFarClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetPerspectiveFarClip(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetShadowFarClip(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetShadowFarClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'ShadowFarClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetShadowFarClip(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetShadowFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'ShadowFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetCascadesSplitAlpha(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetCascadesSplitAlpha();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'CascadesSplitAlpha'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetCascadesSplitAlpha(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetCascadesSplitAlpha(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CascadesSplitAlpha'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetCascadesSmoothTransitionAlpha(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetCascadesSmoothTransitionAlpha();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'CascadesSmoothTransitionAlpha'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetCascadesSmoothTransitionAlpha(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetCascadesSmoothTransitionAlpha(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CascadesSmoothTransitionAlpha'. Entity is null");
	}

	CameraProjectionMode Script::Eagle_CameraComponent_GetCameraProjectionMode(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetProjectionMode();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'ProjectionMode'. Entity is null");
			return CameraProjectionMode::Perspective;
		}
	}

	void Script::Eagle_CameraComponent_SetCameraProjectionMode(GUID entityID, CameraProjectionMode value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetProjectionMode(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'ProjectionMode'. Entity is null");
	}

	//--------------Reverb Component--------------
	bool Script::Eagle_ReverbComponent_IsActive(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().IsActive();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsActive'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_ReverbComponent_SetIsActive(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetActive(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetActive'. Entity is null");
	}

	ReverbPreset Script::Eagle_ReverbComponent_GetReverbPreset(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().GetPreset();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'Preset'. Entity is null");
			return ReverbPreset::Generic;
		}
	}

	void Script::Eagle_ReverbComponent_SetReverbPreset(GUID entityID, ReverbPreset value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetPreset(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'Preset'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMinDistance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().GetMinDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'MinDistance'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_ReverbComponent_SetMinDistance(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetMinDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'MinDistance'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMaxDistance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().GetMaxDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'MaxDistance'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_ReverbComponent_SetMaxDistance(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetMaxDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'MaxDistance'. Entity is null");
	}

	//--------------Text Component--------------
	MonoString* Script::Eagle_TextComponent_GetText(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<TextComponent>().GetText().c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Text of Text Component. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_TextComponent_SetText(GUID entityID, MonoString* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetText(mono_string_to_utf8(value));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text of Text Component. Entity is null");
	}

	Material::BlendMode Script::Eagle_TextComponent_GetBlendMode(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetBlendMode();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get BlendMode of Text Component. Entity is null");
			return Material::BlendMode::Opaque;
		}
	}

	void Script::Eagle_TextComponent_SetBlendMode(GUID entityID, Material::BlendMode value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetBlendMode(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set BlendMode of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_GetColor(GUID entityID, glm::vec3* outValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<TextComponent>().GetColor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get color of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_SetColor(GUID entityID, const glm::vec3* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetColor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set color of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetLineSpacing(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetLineSpacing();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get line spacing of Text Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_TextComponent_SetLineSpacing(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetLineSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set line spacing of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetKerning(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetKerning();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get kerning of Text Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_TextComponent_SetKerning(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetKerning(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set kerning of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetMaxWidth(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetMaxWidth();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max width of Text Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_TextComponent_SetMaxWidth(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetMaxWidth(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max width of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_GetAlbedo(GUID entityID, glm::vec3* outValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<TextComponent>().GetAlbedoColor();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get albedo color of Text Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_TextComponent_SetAlbedo(GUID entityID, const glm::vec3* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetAlbedoColor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set albedo color of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_GetEmissive(GUID entityID, glm::vec3* outValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<TextComponent>().GetEmissiveColor();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get emissive color of Text Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_TextComponent_SetEmissive(GUID entityID, const glm::vec3* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetEmissiveColor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set emissive color of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetMetallness(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetMetallness();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get metallness of Text Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_TextComponent_SetMetallness(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetMetallness(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set metallness of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetRoughness(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetRoughness();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get roughness of Text Component. Entity is null");
		return 1.f;
	}

	void Script::Eagle_TextComponent_SetRoughness(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetRoughness(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set roughness of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetAO(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetAO();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get ambient occlusion of Text Component. Entity is null");
		return 1.f;
	}

	void Script::Eagle_TextComponent_SetAO(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetAO(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set ambient occlusion of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_GetIsLit(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().IsLit();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsLit` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetIsLit(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetIsLit(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsLit` of Text Component. Entity is null");
	}
	
	bool Script::Eagle_TextComponent_DoesCastShadows(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().DoesCastShadows();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesCastShadows` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetCastsShadows(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetCastsShadows(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_SetOpacity(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetOpacity` of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetOpacity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetOpacity();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetOpacity` of Text Component. Entity is null");
			return 1.f;
		}
	}

	void Script::Eagle_TextComponent_SetOpacityMask(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetOpacityMask(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetOpacityMask` of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetOpacityMask(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetOpacityMask();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetOpacityMask` of Text Component. Entity is null");
			return 1.f;
		}
	}

	GUID Script::Eagle_TextComponent_GetFont(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get TextComponent font. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<TextComponent>();
		const auto& asset = component.GetFontAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_TextComponent_SetFont(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set TextComponent font. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<TextComponent>();
		if (assetID.IsNull())
		{
			component.SetFontAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set TextComponent texture. Couldn't find an asset");
			return;
		}

		Ref<AssetFont> fontAsset = Cast<AssetFont>(asset);
		if (!fontAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set TextComponent texture. Provided asset is not a font asset");
			return;
		}

		component.SetFontAsset(fontAsset);
	}

	//--------------Text2D Component--------------
	GUID Script::Eagle_Text2DComponent_GetFont(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Text2DComponent font. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<Text2DComponent>();
		const auto& asset = component.GetFontAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_Text2DComponent_SetFont(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text2DComponent font. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<Text2DComponent>();
		if (assetID.IsNull())
		{
			component.SetFontAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text2DComponent texture. Couldn't find an asset");
			return;
		}

		Ref<AssetFont> fontAsset = Cast<AssetFont>(asset);
		if (!fontAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text2DComponent texture. Provided asset is not a font asset");
			return;
		}

		component.SetFontAsset(fontAsset);
	}

	MonoString* Script::Eagle_Text2DComponent_GetText(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<Text2DComponent>().GetText().c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get text of Text2D Component. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_Text2DComponent_SetText(GUID entityID, MonoString* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetText(mono_string_to_utf8(value));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set text of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetColor(GUID entityID, glm::vec3* outColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outColor = entity.GetComponent<Text2DComponent>().GetColor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get color of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetColor(GUID entityID, const glm::vec3* color)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetColor(*color);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set color of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetPosition(GUID entityID, glm::vec2* outPos)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outPos = entity.GetComponent<Text2DComponent>().GetPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get position of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetPosition(GUID entityID, const glm::vec2* pos)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetPosition(*pos);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set position of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetScale(GUID entityID, glm::vec2* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetComponent<Text2DComponent>().GetScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get scale of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetScale(GUID entityID, const glm::vec2* scale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetScale(*scale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set scale of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetRotation(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetRotation();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get rotation of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetRotation(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetRotation(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set rotation of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetLineSpacing(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetLineSpacing();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get line spacing of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetLineSpacing(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetLineSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set line spacing of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetKerning(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetKerning();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get kerning of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetKerning(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetKerning(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set kerning of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetMaxWidth(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetMaxWidth();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get MaxWidth of Text2D Component. Entity is null");
		return 0.f;
	}
	
	void Script::Eagle_Text2DComponent_SetMaxWidth(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetMaxWidth(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set MaxWidth of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetOpacity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Text2D Component. Entity is null");
		return 1.f;
	}

	void Script::Eagle_Text2DComponent_SetOpacity(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetIsVisible(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetIsVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'IsVisible' of Text2D Component. Entity is null");
	}

	bool Script::Eagle_Text2DComponent_IsVisible(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().IsVisible();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'IsVisible' of Text2D Component. Entity is null");
		return true;
	}

	//--------------Image2D Component--------------
	void Script::Eagle_Image2DComponent_SetTexture(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Image2D component texture. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<Image2DComponent>();
		if (assetID.IsNull())
		{
			component.SetTextureAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Image2D component texture. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Image2D component texture. Provided asset is not a texture 2D asset");
			return;
		}

		component.SetTextureAsset(textureAsset);
	}

	GUID Script::Eagle_Image2DComponent_GetTexture(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Image2D component texture. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<Image2DComponent>();
		const auto& asset = component.GetTextureAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_Image2DComponent_GetTint(GUID entityID, glm::vec3* outValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetTint();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get tint of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetTint(GUID entityID, const glm::vec3* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetTint(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set tint of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetPosition(GUID entityID, glm::vec2* outValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get position of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetPosition(GUID entityID, const glm::vec2* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetPosition(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set position of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetScale(GUID entityID, glm::vec2* outValue)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get scale of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetScale(GUID entityID, const glm::vec2* value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetScale(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set scale of Image2D Component. Entity is null");
	}

	float Script::Eagle_Image2DComponent_GetRotation(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().GetRotation();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get rotation of Image2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Image2DComponent_SetRotation(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetRotation(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set rotation of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetOpacity(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Image2D Component. Entity is null");
	}

	float Script::Eagle_Image2DComponent_GetOpacity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Image2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Image2DComponent_SetIsVisible(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetIsVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsVisible` of Image2D Component. Entity is null");
	}

	bool Script::Eagle_Image2DComponent_IsVisible(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().IsVisible();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'IsVisible' of Image2D Component. Entity is null");
		return false;
	}

	//--------------Billboard Component--------------
	void Script::Eagle_BillboardComponent_SetTexture(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard component texture. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<BillboardComponent>();
		if (assetID.IsNull())
		{
			component.TextureAsset.reset();
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard component texture. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard component texture. Provided asset is not a texture 2D asset");
			return;
		}

		component.TextureAsset = textureAsset;
	}

	GUID Script::Eagle_BillboardComponent_GetTexture(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get billboard component texture. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<BillboardComponent>();
		return component.TextureAsset ? component.TextureAsset->GetGUID() : GUID(0, 0);
	}

	//--------------Sprite Component--------------
	void Script::Eagle_SpriteComponent_GetMaterial(GUID entityID, GUID* outAssetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<SpriteComponent>();
		const auto& materialAsset = component.GetMaterialAsset();
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SpriteComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sprite component material. Entity is null");
			return;
		}
		
		auto& component = entity.GetComponent<SpriteComponent>();
		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sprite component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sprite component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(materialAsset);
	}

	void Script::Eagle_SpriteComponent_GetAtlasSpriteCoords(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get atlas sprite coords. Entity is null");
			return;
		}

		*outValue = entity.GetComponent<SpriteComponent>().GetAtlasSpriteCoords();
	}

	void Script::Eagle_SpriteComponent_SetAtlasSpriteCoords(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set atlas sprite coords. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetAtlasSpriteCoords(*value);
	}

	void Script::Eagle_SpriteComponent_GetAtlasSpriteSize(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get atlas sprite size. Entity is null");
			return;
		}

		*outValue = entity.GetComponent<SpriteComponent>().GetAtlasSpriteSize();
	}

	void Script::Eagle_SpriteComponent_SetAtlasSpriteSize(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set atlas sprite size. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetAtlasSpriteSize(*value);
	}

	void Script::Eagle_SpriteComponent_GetAtlasSpriteSizeCoef(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get atlas sprite size coef. Entity is null");
			return;
		}

		*outValue = entity.GetComponent<SpriteComponent>().GetAtlasSpriteSizeCoef();
	}

	void Script::Eagle_SpriteComponent_SetAtlasSpriteSizeCoef(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set atlas sprite size coef. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetAtlasSpriteSizeCoef(*value);
	}

	bool Script::Eagle_SpriteComponent_GetIsAtlas(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetIsAtlas`. Entity is null");
			return false;
		}

		return entity.GetComponent<SpriteComponent>().IsAtlas();
	}

	void Script::Eagle_SpriteComponent_SetIsAtlas(GUID entityID, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsAtlas`. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetIsAtlas(value);
	}

	bool Script::Eagle_SpriteComponent_DoesCastShadows(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpriteComponent>().DoesCastShadows();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesCastShadows` of Sprite Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_SpriteComponent_SetCastsShadows(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetCastsShadows(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Sprite Component. Entity is null");
	}

	//--------------Script Component--------------
	void Script::Eagle_ScriptComponent_SetScript(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetScript`. Entity is null");
			return;
		}
		
		auto& sc = entity.GetComponent<ScriptComponent>();
		const bool bOldExist = ScriptEngine::ModuleExists(sc.ModuleName);
		if (bOldExist)
		{
			ScriptEngine::OnDestroyEntity(entity);
			ScriptEngine::RemoveEntityScript(entity);
		}

		if (type == nullptr)
		{
			sc.ModuleName.clear();
			return;
		}

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		std::string moduleName = mono_type_get_name_full(monoType, MonoTypeNameFormat::MONO_TYPE_NAME_FORMAT_FULL_NAME);
		if (!ScriptEngine::ModuleExists(moduleName))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetScript`. '{}' is an invalid script", moduleName);
			return;
		}

		sc.ModuleName = std::move(moduleName);
		ScriptEngine::InstantiateEntityClass(entity);
		ScriptEngine::OnCreateEntity(entity);
	}

	MonoReflectionType* Script::Eagle_ScriptComponent_GetScriptType(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& script = entity.GetComponent<ScriptComponent>();
			if (!ScriptEngine::ModuleExists(script.ModuleName))
				return nullptr;

			MonoType* monoType = mono_reflection_type_from_name(script.ModuleName.data(), s_AppAssemblyImage);
			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), monoType);
			return reflectionType;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetScriptType`. Entity is null");
		return nullptr;
	}

	MonoObject* Script::Eagle_ScriptComponent_GetInstance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return ScriptEngine::GetEntityMonoObject(entity);

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetInstance`. Entity is null");
		return nullptr;
	}

	//--------------Input--------------
	bool Script::Eagle_Input_IsMouseButtonPressed(Mouse button)
	{
		return Input::IsMouseButtonPressed(button);
	}

	bool Script::Eagle_Input_IsKeyPressed(Key keyCode)
	{
		return Input::IsKeyPressed(keyCode);
	}

	void Script::Eagle_Input_GetMousePosition(glm::vec2* outPosition)
	{
		auto [x, y] = Input::GetMousePosition();
		*outPosition = {x, y};
	}

	void Script::Eagle_Input_GetMousePositionInViewport(glm::vec2* outPosition)
	{
#ifndef EG_WITH_EDITOR
#error "Check if works"
#endif

		const auto& scene = Scene::GetCurrentScene();
		glm::vec2 mousePos = ImGuiLayer::GetMousePos();
		mousePos -= scene->ViewportBounds[0];
		const glm::vec2 viewportSize = scene->ViewportBounds[1] - scene->ViewportBounds[0];
		mousePos = glm::clamp(mousePos, glm::vec2(0.f), viewportSize);
		*outPosition = mousePos;
	}

	void Script::Eagle_Input_SetMousePosition(const glm::vec2* position)
	{
		Input::SetMousePos(position->x, position->y);
	}

	void Script::Eagle_Input_SetMousePositionInViewport(const glm::vec2* position)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();

		#ifndef EG_WITH_EDITOR
		#error "Check if works"
		#endif

		const glm::vec2 viewportSize = scene->ViewportBounds[1] - scene->ViewportBounds[0];
		const glm::vec2 mousePos = scene->ViewportBounds[0] + glm::clamp(*position, glm::vec2(0), viewportSize);
		Input::SetMousePos(mousePos.x, mousePos.y);
	}

	void Script::Eagle_Input_SetCursorMode(CursorMode mode)
	{
		Input::SetShowMouse(mode == CursorMode::Normal);
	}

	CursorMode Script::Eagle_Input_GetCursorMode()
	{
		return Input::IsMouseVisible() ? CursorMode::Normal : CursorMode::Hidden;
	}
	
	//-------------- Renderer --------------
	void Script::Eagle_Renderer_SetFogSettings(const glm::vec3* color, float minDistance, float maxDistance, float density, FogEquation equation, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.FogSettings.Color = *color;
		options.FogSettings.MinDistance = minDistance;
		options.FogSettings.MaxDistance = maxDistance;
		options.FogSettings.Density = density;
		options.FogSettings.Equation = equation;
		options.FogSettings.bEnable = bEnabled;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetFogSettings(glm::vec3* outcolor, float* outMinDistance, float* outMaxDistance, float* outDensity, FogEquation* outEquation, bool* outbEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outcolor = options.FogSettings.Color;
		*outMinDistance = options.FogSettings.MinDistance;
		*outMaxDistance = options.FogSettings.MaxDistance;
		*outDensity = options.FogSettings.Density;
		*outEquation = options.FogSettings.Equation;
		*outbEnabled = options.FogSettings.bEnable;
	}

	void Script::Eagle_Renderer_SetBloomSettings(GUID dirtID, float threashold, float intensity, float dirtIntensity, float knee, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		
		Ref<Asset> asset;
		Ref<AssetTexture2D> textureAsset;
		if (AssetManager::Get(dirtID, &asset))
		{
			textureAsset = Cast<AssetTexture2D>(asset);
			if (!textureAsset)
				EG_CORE_ERROR("[ScriptEngine] Couldn't set bloom settings dirt texture. Provided asset is not a texture 2D");
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set bloom settings dirt texture. Couldn't find an asset");
		}
		
		options.BloomSettings.Dirt = textureAsset;
		options.BloomSettings.Threshold = threashold;
		options.BloomSettings.Intensity = intensity;
		options.BloomSettings.DirtIntensity = dirtIntensity;
		options.BloomSettings.Knee = knee;
		options.BloomSettings.bEnable = bEnabled;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetBloomSettings(GUID* outDirtTexture, float* outThreashold, float* outIntensity, float* outDirtIntensity, float* outKnee, bool* outbEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outDirtTexture = options.BloomSettings.Dirt ? options.BloomSettings.Dirt->GetGUID() : GUID{ 0, 0 };
		*outThreashold = options.BloomSettings.Threshold;
		*outIntensity = options.BloomSettings.Intensity;
		*outDirtIntensity = options.BloomSettings.DirtIntensity;
		*outKnee = options.BloomSettings.Knee;
		*outbEnabled = options.BloomSettings.bEnable;
	}

	void Script::Eagle_Renderer_SetSSAOSettings(uint32_t samples, float radius, float bias)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.SSAOSettings.SetNumberOfSamples(samples);
		options.SSAOSettings.SetRadius(radius);
		options.SSAOSettings.SetBias(bias);
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetSSAOSettings(uint32_t* outSamples, float* outRadius, float* outBias)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.SSAOSettings.GetNumberOfSamples();
		*outRadius = options.SSAOSettings.GetRadius();
		*outBias = options.SSAOSettings.GetBias();
	}

	void Script::Eagle_Renderer_SetGTAOSettings(uint32_t samples, float radius)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.GTAOSettings.SetNumberOfSamples(samples);
		options.GTAOSettings.SetRadius(radius);
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetGTAOSettings(uint32_t* outSamples, float* outRadius)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.GTAOSettings.GetNumberOfSamples();
		*outRadius = options.GTAOSettings.GetRadius();
	}

	void Script::Eagle_Renderer_SetPhotoLinearTonemappingSettings(float sensitivity, float exposureTime, float fStop)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.PhotoLinearTonemappingParams.Sensitivity = sensitivity;
		options.PhotoLinearTonemappingParams.ExposureTime = exposureTime;
		options.PhotoLinearTonemappingParams.FStop = fStop;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetPhotoLinearTonemappingSettings(float* outSensitivity, float* outExposureTime, float* outfStop)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSensitivity = options.PhotoLinearTonemappingParams.Sensitivity;
		*outExposureTime = options.PhotoLinearTonemappingParams.ExposureTime;
		*outfStop = options.PhotoLinearTonemappingParams.FStop;
	}

	void Script::Eagle_Renderer_SetFilmicTonemappingSettings(float whitePoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.FilmicTonemappingParams.WhitePoint = whitePoint;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetFilmicTonemappingSettings(float* outWhitePoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outWhitePoint = options.FilmicTonemappingParams.WhitePoint;
	}

	float Script::Eagle_Renderer_GetGamma()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.Gamma;
	}

	void Script::Eagle_Renderer_SetGamma(float value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.Gamma = value;
		sceneRenderer->SetOptions(options);
	}

	float Script::Eagle_Renderer_GetExposure()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.Exposure;
	}

	void Script::Eagle_Renderer_SetExposure(float value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.Exposure = value;
		sceneRenderer->SetOptions(options);
	}

	float Script::Eagle_Renderer_GetLineWidth()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.LineWidth;
	}

	void Script::Eagle_Renderer_SetLineWidth(float value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.LineWidth = value;
		sceneRenderer->SetOptions(options);
	}

	TonemappingMethod Script::Eagle_Renderer_GetTonemappingMethod()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.Tonemapping;
	}

	void Script::Eagle_Renderer_SetTonemappingMethod(TonemappingMethod value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.Tonemapping = value;
		sceneRenderer->SetOptions(options);
	}
	
	AAMethod Script::Eagle_Renderer_GetAAMethod()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.AA;
	}

	void Script::Eagle_Renderer_SetAAMethod(AAMethod value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.AA = value;
		sceneRenderer->SetOptions(options);
	}

	AmbientOcclusion Script::Eagle_Renderer_GetAO()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->GetSceneRenderer()->GetOptions().AO;
	}

	void Script::Eagle_Renderer_SetAO(AmbientOcclusion value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.AO = value;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_SetVSyncEnabled(bool value)
	{
		Application::Get().GetWindow().SetVSync(value);
	}

	bool Script::Eagle_Renderer_GetVSyncEnabled()
	{
		return Application::Get().GetWindow().IsVSync();
	}

	void Script::Eagle_Renderer_SetSoftShadowsEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bEnableSoftShadows = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetSoftShadowsEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bEnableSoftShadows;
	}

	void Script::Eagle_Renderer_SetCSMSmoothTransitionEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bEnableCSMSmoothTransition = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetCSMSmoothTransitionEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bEnableCSMSmoothTransition;
	}

	void Script::Eagle_Renderer_SetVisualizeCascades(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bVisualizeCascades = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetVisualizeCascades()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bVisualizeCascades;
	}

	void Script::Eagle_Renderer_SetTransparencyLayers(uint32_t value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.TransparencyLayers = value;
		sceneRenderer->SetOptions(options);
	}

	uint32_t Script::Eagle_Renderer_GetTransparencyLayers()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.TransparencyLayers;
	}

	void Script::Eagle_Renderer_GetSkySettings(glm::vec3* sunPos, glm::vec3* cloudsColor, float* skyIntensity, float* cloudsIntensity, float* scattering, float* cirrus, float* cumulus, uint32_t* cumulusLayers, bool* bCirrus, bool* bCumulus)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& sky = sceneRenderer->GetSkySettings();

		*sunPos = sky.SunPos;
		*cloudsColor = sky.CloudsColor;
		*skyIntensity = sky.SkyIntensity;
		*cloudsIntensity = sky.CloudsIntensity;
		*scattering = sky.Scattering;
		*cirrus = sky.Cirrus;
		*cumulus = sky.Cumulus;
		*cumulusLayers = sky.CumulusLayers;
		*bCirrus = sky.bEnableCirrusClouds;
		*bCumulus = sky.bEnableCumulusClouds;
	}

	void Script::Eagle_Renderer_SetSkySettings(const glm::vec3* sunPos, const glm::vec3* cloudsColor, float skyIntensity, float cloudsIntensity, float scattering, float cirrus, float cumulus, uint32_t cumulusLayers, bool bEnableCirrusClouds, bool bEnableCumulusClouds)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();

		SkySettings sky;
		sky.SunPos = *sunPos;
		sky.CloudsColor = *cloudsColor;
		sky.SkyIntensity = skyIntensity;
		sky.CloudsIntensity = cloudsIntensity;
		sky.Scattering = scattering;
		sky.Cirrus = cirrus;
		sky.Cumulus = cumulus;
		sky.CumulusLayers = cumulusLayers;
		sky.bEnableCirrusClouds = bEnableCirrusClouds;
		sky.bEnableCumulusClouds = bEnableCumulusClouds;
		sceneRenderer->SetSkybox(sky);
	}

	void Script::Eagle_Renderer_SetUseSkyAsBackground(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->GetSceneRenderer()->SetUseSkyAsBackground(value);
	}

	bool Script::Eagle_Renderer_GetUseSkyAsBackground()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->GetSceneRenderer()->GetUseSkyAsBackground();
	}

	void Script::Eagle_Renderer_SetSkyboxEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->GetSceneRenderer()->SetSkyboxEnabled(value);
	}

	bool Script::Eagle_Renderer_IsSkyboxEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->GetSceneRenderer()->IsSkyboxEnabled();
	}

	void Script::Eagle_Renderer_SetVolumetricLightsSettings(uint32_t samples, float maxScatteringDist, float fogSpeed, bool bFogEnable, bool bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto settings = sceneRenderer->GetOptions();

		settings.VolumetricSettings.Samples = samples;
		settings.VolumetricSettings.MaxScatteringDistance = maxScatteringDist;
		settings.VolumetricSettings.FogSpeed = fogSpeed;
		settings.VolumetricSettings.bFogEnable = bFogEnable;
		settings.VolumetricSettings.bEnable = bEnable;
		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_GetVolumetricLightsSettings(uint32_t* outSamples, float* outMaxScatteringDist, float* fogSpeed, bool* bFogEnable, bool* bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().VolumetricSettings;

		*outSamples = settings.Samples;
		*outMaxScatteringDist = settings.MaxScatteringDistance;
		*fogSpeed = settings.FogSpeed;
		*bFogEnable = settings.bFogEnable;
		*bEnable = settings.bEnable;
	}

	MonoArray* Script::Eagle_Renderer_GetShadowMapsSettings(uint32_t* outPointLightSize, uint32_t* outSpotLightSize)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().ShadowsSettings;

		*outPointLightSize = settings.PointLightShadowMapSize;
		*outSpotLightSize = settings.SpotLightShadowMapSize;

		MonoClass* uintClass = mono_get_uint32_class();
		MonoArray* result = mono_array_new(mono_domain_get(), uintClass, RendererConfig::CascadesCount);
		uint32_t index = 0;
		for (auto& res : settings.DirLightShadowMapSizes)
			mono_array_set(result, uint32_t, index++, res);

		return result;
	}

	void Script::Eagle_Renderer_SetShadowMapsSettings(uint32_t pointLightSize, uint32_t spotLightSize, MonoArray* dirLightSizes)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.ShadowsSettings.PointLightShadowMapSize = glm::max(pointLightSize, ShadowMapsSettings::MinPointLightShadowMapSize);
		settings.ShadowsSettings.SpotLightShadowMapSize = glm::max(spotLightSize, ShadowMapsSettings::MinSpotLightShadowMapSize);

		const uint32_t monoLength = (uint32_t)mono_array_length(dirLightSizes);
		const uint32_t length = glm::min(monoLength, RendererConfig::CascadesCount);

		for (uint32_t i = 0; i < length; ++i)
		{
			uint32_t val = mono_array_get(dirLightSizes, uint32_t, i);
			settings.ShadowsSettings.DirLightShadowMapSizes[i] = glm::max(val, ShadowMapsSettings::MinDirLightShadowMapSize);
		}
	}

	void Script::Eagle_Renderer_SetStutterlessShaders(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bStutterlessShaders = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetStutterlessShaders()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bStutterlessShaders;
	}

	void Script::Eagle_Renderer_SetTranslucentShadowsEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bTranslucentShadows = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetTranslucentShadowsEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bTranslucentShadows;
	}

	void Script::Eagle_Renderer_GetViewportSize(glm::vec2* outSize)
	{
		*outSize = glm::vec2(Scene::GetCurrentScene()->GetSceneRenderer()->GetViewportSize());
	}

	void Script::Eagle_Renderer_SetSkybox(GUID cubemapID)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		if (cubemapID.IsNull())
		{
			sceneRenderer->SetSkybox(nullptr);
			return;
		}
		
		Ref<Asset> asset;
		if (AssetManager::Get(cubemapID, &asset) == false)
		{
			EG_CORE_ERROR("Could set skybox. The asset is not found!");
			return;
		}
		
		Ref<AssetTextureCube> cubemap = Cast<AssetTextureCube>(asset);
		if (!cubemap)
		{
			EG_CORE_ERROR("Couldn't set the skybox. Provided asset is not a cube texture!");
			return;
		}
		
		sceneRenderer->SetSkybox(cubemap);
	}

	GUID Script::Eagle_Renderer_GetSkybox()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		const auto& skybox = sceneRenderer->GetSkybox();
		return skybox ? skybox->GetGUID() : GUID{0, 0};
	}

	void Script::Eagle_Renderer_SetCubemapIntensity(float intensity)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		sceneRenderer->SetSkyboxIntensity(intensity);
	}

	float Script::Eagle_Renderer_GetCubemapIntensity()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetSkyboxIntensity();
	}
	
	void Script::Eagle_Renderer_SetObjectPickingEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bEnableObjectPicking = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsObjectPickingEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bEnableObjectPicking;
	}

	void Script::Eagle_Renderer_Set2DObjectPickingEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bEnable2DObjectPicking = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_Is2DObjectPickingEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bEnable2DObjectPicking;
	}

	//-------------- Project --------------
	MonoString* Script::Eagle_Project_GetProjectPath()
	{
		return mono_string_new(mono_domain_get(), Project::GetProjectPath().u8string().c_str());
	}

	MonoString* Script::Eagle_Project_GetContentPath()
	{
		return mono_string_new(mono_domain_get(), Project::GetContentPath().u8string().c_str());
	}

	MonoString* Script::Eagle_Project_GetCachePath()
	{
		return mono_string_new(mono_domain_get(), Project::GetCachePath().u8string().c_str());
	}

	MonoString* Script::Eagle_Project_GetRendererCachePath()
	{
		return mono_string_new(mono_domain_get(), Project::GetRendererCachePath().u8string().c_str());
	}

	MonoString* Script::Eagle_Project_GetSavedPath()
	{
		return mono_string_new(mono_domain_get(), Project::GetSavedPath().u8string().c_str());
	}

	//-------------- Scene --------------
	void Script::Eagle_Scene_OpenScene(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't open a scene. Couldn't find an asset");
			return;
		}

		Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset);
		if (!sceneAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't open a scene. It's not a scene asset");
			return;
		}

		Scene::OpenScene(sceneAsset, true, true);
	}

	bool Script::Eagle_Scene_Raycast(const glm::vec3* origin, const glm::vec3* dir, float maxDistance, GUID* outHitEntity, glm::vec3* outPosition, glm::vec3* outNormal, float* outDistance)
	{
		const auto& physicsScene = Scene::GetCurrentScene()->GetPhysicsScene();
		RaycastHit hit{};
		const bool bHit = physicsScene->Raycast(*origin, *dir, maxDistance, &hit);

		*outHitEntity = hit.HitEntity;
		*outPosition = hit.Position;
		*outNormal = hit.Normal;
		*outDistance = hit.Distance;

		return bHit;
	}

	void Script::Eagle_Scene_DrawLine(const glm::vec3* color, const glm::vec3* start, const glm::vec3* end)
	{
		Scene::GetCurrentScene()->DrawDebugLine({ *color, *start, *end });
	}

	//-------------- Log --------------
	void Script::Eagle_Log_Trace(MonoString* message)
	{
		if (message)
			EG_TRACE(mono_string_to_utf8(message));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Info(MonoString* message)
	{
		if (message)
			EG_INFO(mono_string_to_utf8(message));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Warn(MonoString* message)
	{
		if (message)
			EG_WARN(mono_string_to_utf8(message));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Error(MonoString* message)
	{
		if (message)
			EG_ERROR(mono_string_to_utf8(message));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Critical(MonoString* message)
	{
		if (message)
			EG_CRITICAL(mono_string_to_utf8(message));
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	//--------------Asset--------------
	bool Script::Eagle_Asset_Get(MonoString* path, AssetType* outType, GUID* outGUID)
	{
		Path filepath = mono_string_to_utf8(path);
		Ref<Asset> asset;
		if (AssetManager::Get(filepath, &asset))
		{
			*outType = asset->GetAssetType();
			*outGUID = asset->GetGUID();
			return true;
		}
		return false;
	}

	MonoString* Script::Eagle_Asset_GetPath(GUID guid)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(guid, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get an asset path. Couldn't find an asset");
			return nullptr;
		}

		return mono_string_new(mono_domain_get(), asset->GetPath().u8string().c_str());
	}

	AssetType Script::Eagle_Asset_GetAssetType(GUID guid)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(guid, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get an asset type. Couldn't find an asset");
			return AssetType::None;
		}

		return asset->GetAssetType();
	}

	//--------------AssetTexture2D--------------
	void Script::Eagle_AssetTexture2D_SetAnisotropy(GUID id, float anisotropy)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set anisotropy. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set anisotropy. It's not a texture 2D asset");
			return;
		}

		textureAsset->GetTexture()->SetAnisotropy(anisotropy);
	}

	float Script::Eagle_AssetTexture2D_GetAnisotropy(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get anisotropy. Couldn't find an asset");
			return 1.f;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get anisotropy. It's not a texture 2D asset");
			return 1.f;
		}

		return textureAsset->GetTexture()->GetAnisotropy();
	}

	void Script::Eagle_AssetTexture2D_SetFilterMode(GUID id, FilterMode filterMode)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set filter mode. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set filter mode. It's not a texture 2D asset");
			return;
		}

		textureAsset->GetTexture()->SetFilterMode(filterMode);
	}

	FilterMode Script::Eagle_AssetTexture2D_GetFilterMode(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get filter mode. Couldn't find an asset");
			return FilterMode::Point;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get filter mode. It's not a texture 2D asset");
			return FilterMode::Point;
		}

		return textureAsset->GetTexture()->GetFilterMode();
	}

	void Script::Eagle_AssetTexture2D_SetAddressMode(GUID id, AddressMode addressMode)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set address mode. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set address mode. It's not a texture 2D asset");
			return;
		}

		textureAsset->GetTexture()->SetAddressMode(addressMode);
	}

	AddressMode Script::Eagle_AssetTexture2D_GetAddressMode(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get address mode. Couldn't find an asset");
			return AddressMode::Wrap;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get address mode. It's not a texture 2D asset");
			return AddressMode::Wrap;
		}

		return textureAsset->GetTexture()->GetAddressMode();
	}

	void Script::Eagle_AssetTexture2D_SetMipsCount(GUID id, uint32_t mipsCount)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mips count. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mips count. It's not a texture 2D asset");
			return;
		}

		constexpr uint32_t minMips = 1u;
		auto& texture = textureAsset->GetTexture();
		const uint32_t maxMips = CalculateMipCount(texture->GetSize());
		mipsCount = glm::clamp(mipsCount, minMips, maxMips);
		texture->GenerateMips(mipsCount);
	}

	uint32_t Script::Eagle_AssetTexture2D_GetMipsCount(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mips count. Couldn't find an asset");
			return 1u;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mips count. It's not a texture 2D asset");
			return 1u;
		}

		return textureAsset->GetTexture()->GetMipsCount();
	}

	//--------------AssetMaterial--------------
	void Script::Eagle_AssetMaterial_GetMaterial(GUID assetID, GUID* outAlbedo, GUID* outMetallness, GUID* outNormal, GUID* outRoughness, GUID* outAO, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material. Couldn't find an asset");
			return;
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
			Utils::GetMaterial(materialAsset->GetMaterial(), outAlbedo, outMetallness, outNormal, outRoughness, outAO, outEmissiveTexture, outOpacityTexture, outOpacityMaskTexture, outTint, outEmissiveIntensity, outTilingFactor, outBlendMode);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material. It's not a material asset");
	}

	void Script::Eagle_AssetMaterial_SetMaterial(GUID assetID, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set material. Couldn't find an asset");
			return;
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
			Utils::SetMaterial(materialAsset->GetMaterial(), albedo, metallness, normal, roughness, ao, emissiveTexture, opacityTexture, opacityMaskTexture, tint, emissiveIntensity, tilingFactor, blendMode);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set material. It's not a material asset");
	}

	//--------------AssetAudio--------------
	void Script::Eagle_AssetAudio_SetVolume(GUID assetID, float volume)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset volume. Couldn't find an asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			audioAsset->GetAudio()->SetVolume(volume);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset volume. It's not an audio asset");
	}

	float Script::Eagle_AssetAudio_GetVolume(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset volume. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			return audioAsset->GetAudio()->GetVolume();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset volume. It's not an audio asset");
		return 0.f;
	}

	void Script::Eagle_AssetAudio_SetSoundGroup(GUID audioID, GUID soundGroupID)
	{
		Ref<Asset> asset;
		AssetManager::Get(audioID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. Couldn't find an audio asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
		{
			AssetManager::Get(soundGroupID, &asset);
			if (!asset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. Couldn't find a SoundGroup asset");
				return;
			}

			if (Ref<AssetSoundGroup> soundGroupAsset = Cast<AssetSoundGroup>(asset))
				audioAsset->SetSoundGroupAsset(soundGroupAsset);
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. It's not a SoundGroup asset");
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound group. It's not an audio asset");
	}

	GUID Script::Eagle_AssetAudio_GetSoundGroup(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sound group. Couldn't find an asset");
			return GUID(0, 0);
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
		{
			const auto& soundGroup = audioAsset->GetSoundGroupAsset();
			return soundGroup ? soundGroup->GetGUID() : GUID(0, 0);
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't get sound group. It's not an audio asset");
		return GUID(0, 0);
	}

	//--------------AssetPhysicsMaterial--------------
	void Script::Eagle_AssetPhysicsMaterial_SetDynamicFriction(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset dynamic friction. Couldn't find an asset");
			return;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
		{
			material->GetMaterial()->DynamicFriction = value;
			Utils::OnPhysicsMaterialChanged(material);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset dynamic friction. It's not a PhysicsMaterial asset");
	}

	void Script::Eagle_AssetPhysicsMaterial_SetBounciness(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset bounciness. Couldn't find an asset");
			return;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
		{
			material->GetMaterial()->Bounciness = value;
			Utils::OnPhysicsMaterialChanged(material);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset bounciness. It's not a PhysicsMaterial asset");
	}

	void Script::Eagle_AssetPhysicsMaterial_SetStaticFriction(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset static friction. Couldn't find an asset");
			return;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
		{
			material->GetMaterial()->StaticFriction = value;
			Utils::OnPhysicsMaterialChanged(material);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset static friction. It's not a PhysicsMaterial asset");
	}

	float Script::Eagle_AssetPhysicsMaterial_GetStaticFriction(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset static friction. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
			return material->GetMaterial()->StaticFriction;

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset static friction. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	float Script::Eagle_AssetPhysicsMaterial_GetDynamicFriction(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset dynamic friction. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
			return material->GetMaterial()->DynamicFriction;

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset dynamic friction. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	float Script::Eagle_AssetPhysicsMaterial_GetBounciness(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset bounciness. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetPhysicsMaterial> material = Cast<AssetPhysicsMaterial>(asset))
			return material->GetMaterial()->Bounciness;

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset bounciness. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	//--------------AssetSoundGroup--------------
	void Script::Eagle_AssetSoundGroup_Stop(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop a sound group. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop a sound group. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetPaused(GUID assetID, bool value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set paused. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetPaused(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set paused. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetVolume(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set volume. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetVolume(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set volume. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetMuted(GUID assetID, bool value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set muted. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetMuted(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set muted. It's not a SoundGroup asset");
	}

	void Script::Eagle_AssetSoundGroup_SetPitch(GUID assetID, float value)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set pitch. Couldn't find a SoundGroup asset");
			return;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			soundGroup->GetSoundGroup()->SetPitch(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set pitch. It's not a SoundGroup asset");
	}

	float Script::Eagle_AssetSoundGroup_GetVolume(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get volume. Couldn't find a SoundGroup asset");
			return 0.f;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->GetVolume();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get volume. It's not a SoundGroup asset");
		return 0.f;
	}

	float Script::Eagle_AssetSoundGroup_GetPitch(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get pitch. Couldn't find a SoundGroup asset");
			return 0.f;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->GetPitch();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get pitch. It's not a SoundGroup asset");
		return 0.f;
	}

	bool Script::Eagle_AssetSoundGroup_IsPaused(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsPaused`. Couldn't find a SoundGroup asset");
			return false;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->IsPaused();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsPaused`. It's not a SoundGroup asset");
		return false;
	}

	bool Script::Eagle_AssetSoundGroup_IsMuted(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsMuted`. Couldn't find a SoundGroup asset");
			return false;
		}

		if (Ref<AssetSoundGroup> soundGroup = Cast<AssetSoundGroup>(asset))
			return soundGroup->GetSoundGroup()->IsMuted();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsMuted`. It's not a SoundGroup asset");
		return false;
	}
}