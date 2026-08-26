#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsRagdollActor.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Classes/PSAutoDestroyScript.h"

#include <mono/jit/jit.h>

namespace Eagle 
{
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&)>> m_RemoveComponentFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	//SceneComponents
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetForwardVectorFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetRightVectorFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetUpVectorFunctions;

	//Light Component
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetLightColorFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetLightColorFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<float(Entity&)>> m_GetIntensityFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, float)>> m_SetIntensityFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<float(Entity&)>> m_GetVolumetricFogIntensityFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, float)>> m_SetVolumetricFogIntensityFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetCastsShadowsFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_GetCastsShadowsFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsVolumetricLightFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_GetIsVolumetricLightFunctions;

	//BaseColliderComponent
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, CollisionGroup)>> m_SetCollisionGroupsFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<CollisionGroup(Entity&)>> m_GetCollisionGroupsFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, CollisionGroup)>> m_SetInteractingCollisionGroupsFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<CollisionGroup(Entity&)>> m_GetInteractingCollisionGroupsFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsTriggerFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsTriggerFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionEnabledFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionEnabledFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetCollisionVisibleFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsCollisionVisibleFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, const Ref<AssetPhysicsMaterial>&)>> m_SetPhysicsMaterialFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<GUID(Entity&)>> m_GetPhysicsMaterialFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsNavMeshBuildFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_DoesAffectNavMeshBuildFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsObstacleFunctions;
	extern ankerl::unordered_dense::map<MonoType*, std::function<bool(Entity&)>> m_IsObstacleFunctions;

	// Scene
	extern ankerl::unordered_dense::map<MonoType*, std::function<std::vector<Entity>(const Ref<Scene>&)>> m_GetAllEntitiesWith;

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

	static void SetMaterial(const Ref<Material>& material,
		GUID albedoTexture, GUID metalnessTexture, GUID normalTexture, GUID roughnessTexture, GUID aoTexture, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec3* albedo, float metalness, float roughness, float ao, const glm::vec3* emissive, float opacity, float opacityMask,
		bool bUseAlbedoTexture, bool bUseMetalnessTexture, bool bUseRoughnessTexture, bool bUseAOTexture, bool bUseEmissiveTexture, bool bUseOpacityTexture, bool bUseOpacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, MaterialBlendMode blendMode, bool bDoubleSided,
		Material::TextureChannel metalnessTextureChannel, Material::TextureChannel roughnessTextureChannel, Material::TextureChannel aoTextureChannel,
		Material::TextureChannel opacityTextureChannel, Material::TextureChannel opacityMaskTextureChannel)
	{
		// Update textures
		{
			SetMaterialTexture(material, albedoTexture, &Material::SetAlbedoAsset);
			SetMaterialTexture(material, metalnessTexture, &Material::SetMetalnessAsset);
			SetMaterialTexture(material, normalTexture, &Material::SetNormalAsset);
			SetMaterialTexture(material, roughnessTexture, &Material::SetRoughnessAsset);
			SetMaterialTexture(material, aoTexture, &Material::SetAOAsset);
			SetMaterialTexture(material, emissiveTexture, &Material::SetEmissiveAsset);
			SetMaterialTexture(material, opacityTexture, &Material::SetOpacityAsset);
			SetMaterialTexture(material, opacityMaskTexture, &Material::SetOpacityMaskAsset);
		}

		material->SetMetalnessTextureChannel(metalnessTextureChannel);
		material->SetRoughnessTextureChannel(roughnessTextureChannel);
		material->SetAOTextureChannel(aoTextureChannel);
		material->SetOpacityTextureChannel(opacityTextureChannel);
		material->SetOpacityMaskTextureChannel(opacityMaskTextureChannel);

		// Update raw values
		{
			material->SetAlbedo(*albedo);
			material->SetMetalness(metalness);
			material->SetRoughness(roughness);
			material->SetAO(ao);
			material->SetEmissive(*emissive);
			material->SetOpacity(opacity);
			material->SetOpacityMask(opacityMask);

			material->SetRawAlbedoUsed(!bUseAlbedoTexture);
			material->SetRawMetalnessUsed(!bUseMetalnessTexture);
			material->SetRawRoughnessUsed(!bUseRoughnessTexture);
			material->SetRawAOUsed(!bUseAOTexture);
			material->SetRawEmissiveUsed(!bUseEmissiveTexture);
			material->SetRawOpacityUsed(!bUseOpacityTexture);
			material->SetRawOpacityMaskUsed(!bUseOpacityMaskTexture);
		}

		material->SetTintColor(*tint);
		material->SetEmissiveIntensity(*emissiveIntensity);
		material->SetTilingFactor(tilingFactor);
		material->SetBlendMode(blendMode);
		material->SetDoubleSided(bDoubleSided);
	}

	static void GetMaterial(const Ref<Material>& material,
		GUID* outAlbedoTexture, GUID* outMetalnessTexture, GUID* outNormalTexture, GUID* outRoughnessTexture, GUID* outAOTexture, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec3* albedo, float* metalness, float* roughness, float* ao, glm::vec3* emissive, float* opacity, float* opacityMask,
		bool* bUseAlbedoTexture, bool* bUseMetalnessTexture, bool* bUseRoughnessTexture, bool* bUseAOTexture, bool* bUseEmissiveTexture, bool* bUseOpacityTexture, bool* bUseOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, MaterialBlendMode* outBlendMode, bool* bDoubleSided,
		Material::TextureChannel* outMetalnessTextureChannel, Material::TextureChannel* outRoughnessTextureChannel, Material::TextureChannel* outAOTextureChannel,
		Material::TextureChannel* outOpacityTextureChannel, Material::TextureChannel* outOpacityMaskTextureChannel)
	{
		const GUID null(0, 0);

		// Get textures
		{
			// Albedo
			{
				const auto& asset = material->GetAlbedoAsset();
				*outAlbedoTexture = asset ? asset->GetGUID() : null;
			}
			// Metalness
			{
				const auto& asset = material->GetMetalnessAsset();
				*outMetalnessTexture = asset ? asset->GetGUID() : null;
			}
			// Normal
			{
				const auto& asset = material->GetNormalAsset();
				*outNormalTexture = asset ? asset->GetGUID() : null;
			}
			// Roughness
			{
				const auto& asset = material->GetRoughnessAsset();
				*outRoughnessTexture = asset ? asset->GetGUID() : null;
			}
			// AO
			{
				const auto& asset = material->GetAOAsset();
				*outAOTexture = asset ? asset->GetGUID() : null;
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
		
		*outMetalnessTextureChannel = material->GetMetalnessTextureChannel();
		*outRoughnessTextureChannel = material->GetRoughnessTextureChannel();
		*outAOTextureChannel = material->GetAOTextureChannel();
		*outOpacityTextureChannel = material->GetOpacityTextureChannel();
		*outOpacityMaskTextureChannel = material->GetOpacityMaskTextureChannel();

		// Get raw values
		{
			*albedo = material->GetAlbedo();
			*metalness = material->GetMetalness();
			*roughness = material->GetRoughness();
			*ao = material->GetAO();
			*emissive = material->GetEmissive();
			*opacity = material->GetOpacity();
			*opacityMask = material->GetOpacityMask();

			*bUseAlbedoTexture = !material->IsRawAlbedoUsed();
			*bUseMetalnessTexture = !material->IsRawMetalnessUsed();
			*bUseRoughnessTexture = !material->IsRawRoughnessUsed();
			*bUseAOTexture = !material->IsRawAOUsed();
			*bUseEmissiveTexture = !material->IsRawEmissiveUsed();
			*bUseOpacityTexture = !material->IsRawOpacityUsed();
			*bUseOpacityMaskTexture = !material->IsRawOpacityMaskUsed();
		}
		
		*outTint = material->GetTintColor();
		*outEmissiveIntensity = material->GetEmissiveIntensity();
		*outTilingFactor = material->GetTilingFactor();
		*outBlendMode = material->GetBlendMode();
		*bDoubleSided = material->IsDoubleSided();
	}
}

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

	void Script::Eagle_Entity_DestroyEntity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		const bool bAlreadyHasIt = m_HasComponentFunctions[monoType](entity);
		if (bAlreadyHasIt)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. This component already exists!");
			return;
		}

		if (entity)
		{
			m_AddComponentFunctions[monoType](entity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add component to Entity. Entity is null");
		}
	}

	void Script::Eagle_Entity_RemoveComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		const bool bAlreadyHasIt = m_HasComponentFunctions[monoType](entity);
		if (!bAlreadyHasIt)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component to Entity. This component doesn't exist!");
			return;
		}

		if (entity)
		{
			m_RemoveComponentFunctions[monoType](entity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't remove component from an Entity. Entity is null");
		}
	}

	bool Script::Eagle_Entity_HasComponent(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
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

	bool Script::Eagle_Entity_IsValid(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		return entity.IsValid();
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

	//-------------- Scene Component --------------
	void Script::Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetLightColorFunctions[monoType](entity, outLightColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'LightColor'. Entity is null");
	}

	bool Script::Eagle_LightComponent_GetAffectsWorld(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetLightColorFunctions[monoType](entity, inLightColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'LightColor'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetAffectsWorld(GUID entityID, void* type, bool bAffectsWorld)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetAffectsWorldFunctions[monoType](entity, bAffectsWorld);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bAffectsWorld'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetCastsShadows(GUID entityID, void* type, bool bValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetCastsShadowsFunctions[monoType](entity, bValue);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bCastsShadows'. Entity is null");
	}

	float Script::Eagle_LightComponent_GetIntensity(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetIntensityFunctions[monoType](entity, inIntensity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Intensity'. Entity is null");
	}

	float Script::Eagle_LightComponent_GetVolumetricFogIntensity(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetVolumetricFogIntensityFunctions[monoType](entity, inIntensity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Volumetric Fog Intensity'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetIsVolumetricLight(GUID entityID, void* type, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
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

	//--------------Sound--------------
	void Script::Eagle_Sound_SetSettings(GUID id, const SoundSettings* settings)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			sound->SetVolumeMultiplier(settings->VolumeMultiplier);
			sound->SetPitch(settings->Pitch);
			sound->SetPan(settings->Pan);
			sound->SetLoopCount(settings->LoopCount);
			sound->SetFFTSamples(settings->FFTSamples);
			sound->SetFFTType(settings->FFTType);
			sound->SetLooping(settings->IsLooping);
			sound->SetStreaming(settings->IsStreaming);
			sound->SetMuted(settings->IsMuted);
			sound->SetFFTEnabled(settings->bEnableFFT);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sound settings. Sound is not found");
	}

	void Script::Eagle_Sound_GetSettings(GUID id, SoundSettings* outSettings)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			*outSettings = sound->GetSettings();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sound settings. Sound is not found");
	}

	void Script::Eagle_Sound_Play(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play sound. Sound is not found");
	}

	void Script::Eagle_Sound_Stop(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop sound. Sound is not found");
	}

	void Script::Eagle_Sound_SetPaused(GUID id, bool bPaused)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPaused`. Sound is not found");
	}

	bool Script::Eagle_Sound_IsPlaying(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->IsPlaying();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsPlaying`. Sound is not found");
		return false;
	}

	void Script::Eagle_Sound_SetPosition(GUID id, uint32_t ms)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetPosition(ms);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPosition`. Sound is not found");
	}

	uint32_t Script::Eagle_Sound_GetPosition(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetPosition();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPosition`. Sound is not found");
		return 0u;
	}

	void Script::Eagle_Sound_SetFFTEnabled(GUID id, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetFFTEnabled(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFFTEnabled`. Sound is not found");
	}

	bool Script::Eagle_Sound_IsFFTEnabled(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->IsFFTEnabled();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsFFTEnabled`. Sound is not found");
		return false;
	}

	void Script::Eagle_Sound_SetFFTSamples(GUID id, uint32_t value)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetFFTSamples(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFFTSamples`. Sound is not found");
	}

	uint32_t Script::Eagle_Sound_GetFFTSamples(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetFFTSamples();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFFTSamples`. Sound is not found");
		return 0u;
	}

	void Script::Eagle_Sound_SetFFTType(GUID id, FFTWindowType value)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			sound->SetFFTType(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFFTType`. Sound is not found");
	}

	FFTWindowType Script::Eagle_Sound_GetFFTType(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
			return sound->GetFFTType();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFFTType`. Sound is not found");
		return FFTWindowType::Rect;
	}

	bool Script::Eagle_Sound_GetSpectrumData(GUID id, MonoArray* data, int channelIndex)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			const uint32_t monoLength = (uint32_t)mono_array_length(data);
			void* sampleData = mono_array_addr(data, float, 0);
			return sound->GetSpectrumData((float*)sampleData, monoLength, channelIndex);
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSpectrumData`. Sound is not found");
		return false;
	}

	float Script::Eagle_Sound_GetSampleRate(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			return sound->GetSampleRate();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSampleRate`. Sound is not found");
		return 0.f;
	}

	int Script::Eagle_Sound_GetChannelsCount(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = scene->GetSpawnedSound(id))
		{
			return sound->GetChannelsCount();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetChannelsCount`. Sound is not found");
		return 0;
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

		auto& scene = Scene::GetCurrentScene();
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

		auto& scene = Scene::GetCurrentScene();
		return scene->SpawnSound3D(audioAsset, *position, rolloff, *settings).ID;
	}

	void Script::Eagle_Sound3D_SetMinDistance(GUID id, float min)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMinDistance(min);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMinDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetMaxDistance(GUID id, float max)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMaxDistance(max);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMaxDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetMinMaxDistance(GUID id, float min, float max)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetMinMaxDistance(min, max);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMinMaxDistance`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetWorldPosition(GUID id, const glm::vec3* position)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetWorldPosition(*position);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetWorldPosition`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetVelocity(GUID id, const glm::vec3* velocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetVelocity(*velocity);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVelocity`. Sound is not found");
	}

	void Script::Eagle_Sound3D_SetRollOffModel(GUID id, RollOffModel rollOff)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			sound->SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRollOffModel`. Sound is not found");
	}

	float Script::Eagle_Sound3D_GetMinDistance(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetMinDistance();
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMinDistance`. Sound is not found");
		return 0.f;
	}

	float Script::Eagle_Sound3D_GetMaxDistance(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetMaxDistance();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMaxDistance`. Sound is not found");
		return 0.f;
	}

	void Script::Eagle_Sound3D_GetWorldPosition(GUID id, glm::vec3* outPosition)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			*outPosition = sound->GetWorldPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetWorldPosition`. Sound is not found");
	}

	void Script::Eagle_Sound3D_GetVelocity(GUID id, glm::vec3* outVelocity)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			*outVelocity = sound->GetVelocity();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetVelocity`. Sound is not found");
	}

	RollOffModel Script::Eagle_Sound3D_GetRollOffModel(GUID id)
	{
		const auto& scene = Scene::GetCurrentScene();
		if (auto sound = Cast<Sound3D>(scene->GetSpawnedSound(id)))
			return sound->GetRollOffModel();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetRollOffModel`. Sound is not found");
		return RollOffModel::Default;
	}

	//--------------AudioComponent--------------
	void Script::Eagle_AudioComponent_SetMinDistance(GUID entityID, float minDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinDistance(minDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMaxDistance(GUID entityID, float maxDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMaxDistance(maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's max distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMinMaxDistance(GUID entityID, float minDistance, float maxDistance)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinMaxDistance(minDistance, maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min-max distance. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetRollOffModel(GUID entityID, RollOffModel rollOff)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's roll off model. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetVolume(GUID entityID, float volume)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetVolume(volume);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's volume. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetPitch(GUID entityID, float pitch)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetPitch(pitch);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's pitch. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetLoopCount(GUID entityID, int loopCount)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLoopCount(loopCount);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's loop count. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetLooping(GUID entityID, bool bLooping)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLooping(bLooping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's looping mode. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetMuted(GUID entityID, bool bMuted)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMuted(bMuted);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetMuted' function. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetAudioAsset(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetStreaming(bStreaming);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's streaming mode. Entity is null");
	}

	void Script::Eagle_AudioComponent_Play(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play audio component's sound. Entity is null");
	}

	void Script::Eagle_AudioComponent_Stop(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop audio component's sound. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetPaused(GUID entityID, bool bPaused)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPaused' function. Entity is null");
	}

	void Script::Eagle_AudioComponent_SetDopplerEffectEnabled(GUID entityID, bool bEnable)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().bEnableDopplerEffect = bEnable;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetDopplerEffectEnabled' function. Entity is null");
	}

	float Script::Eagle_AudioComponent_GetMinDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetVolume();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's volume. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_AudioComponent_GetPitch(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetPitch();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get audio component's pitch. Entity is null");
			return 1.f;
		}
	}

	int Script::Eagle_AudioComponent_GetLoopCount(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().bEnableDopplerEffect;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsDopplerEffectEnabled' function. Entity is null");
			return false;
		}
	}

	void Script::Eagle_AudioComponent_SetFFTEnabled(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetFFTEnabled(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetFFTEnabled' function. Entity is null");
	}

	bool Script::Eagle_AudioComponent_IsFFTEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().IsFFTEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'IsFFTEnabled' function. Entity is null");
			return false;
		}
	}

	void Script::Eagle_AudioComponent_SetFFTSamples(GUID entityID, uint32_t value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetFFTSamples(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetFFTSamples' function. Entity is null");
	}

	uint32_t Script::Eagle_AudioComponent_GetFFTSamples(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetFFTSamples();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetFFTSamples' function. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_AudioComponent_SetFFTType(GUID entityID, FFTWindowType value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetFFTType(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetFFTType' function. Entity is null");
	}

	FFTWindowType Script::Eagle_AudioComponent_GetFFTType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<AudioComponent>().GetFFTType();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetFFTType' function. Entity is null");
			return FFTWindowType::Rect;
		}
	}

	bool Script::Eagle_AudioComponent_GetSpectrumData(GUID entityID, MonoArray* data, int channelIndex)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const uint32_t monoLength = (uint32_t)mono_array_length(data);
			void* sampleData = mono_array_addr(data, float, 0);
			return entity.GetComponent<AudioComponent>().GetSpectrumData((float*)sampleData, monoLength, channelIndex);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetSpectrumData' function. Entity is null");
			return false;
		}
	}

	float Script::Eagle_AudioComponent_GetSampleRate(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetSampleRate();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetSampleRate' function. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_AudioComponent_SetPosition(GUID entityID, uint32_t ms)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().SetPosition(ms);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPosition' function. Entity is null");
		}
	}

	uint32_t Script::Eagle_AudioComponent_GetPosition(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetPosition();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetPosition' function. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_AudioComponent_SetIs3D(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().SetIs3D(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetIs3D' function. Entity is null");
		}
	}

	bool Script::Eagle_AudioComponent_Is3D(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().Is3D();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'Is3D' function. Entity is null");
			return false;
		}
	}

	void Script::Eagle_AudioComponent_SetPan(GUID entityID, float pan)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().SetPan(pan);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPan' function. Entity is null");
		}
	}

	float Script::Eagle_AudioComponent_GetPan(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetPan();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetPan' function. Entity is null");
			return 0.f;
		}
	}

	int Script::Eagle_AudioComponent_GetChannelsCount(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<AudioComponent>().GetChannelsCount();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'GetChannelsCount' function. Entity is null");
			return 0;
		}
	}

	//--------------RigidBodyComponent--------------
	void Script::Eagle_RigidBodyComponent_SetBodyType(GUID entityID, PhysicsBodyType type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetBodyType(type);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics body type. Entity is null");
	}

	PhysicsBodyType Script::Eagle_RigidBodyComponent_GetBodyType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetBodyType();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get physics body type. Entity is null");
			return PhysicsBodyType::Static;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetCollisionDetectionType(GUID entityID, CollisionDetectionType type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetCollisionDetectionType(type);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision detection type. Entity is null");
	}

	CollisionDetectionType Script::Eagle_RigidBodyComponent_GetCollisionDetectionType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetCollisionDetectionType();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get collision detection type. Entity is null");
			return CollisionDetectionType::Discrete;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetPositionSolverIterations(GUID entityID, uint32_t iterations)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetPositionSolverIterations(iterations);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set position solver iterations. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetVelocitySolverIterations(GUID entityID, uint32_t iterations)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetVelocitySolverIterations(iterations);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set velocity solver iterations. Entity is null");
	}

	uint32_t Script::Eagle_RigidBodyComponent_GetPositionSolverIterations(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetPositionSolverIterations();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get position solver iterations. Entity is null");
			return 0u;
		}
	}

	uint32_t Script::Eagle_RigidBodyComponent_GetVelocitySolverIterations(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetVelocitySolverIterations();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get velocity solver iterations. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetMass(GUID entityID, float mass)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetMass(mass);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mass. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetMass(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLinearDamping(linearDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear damping. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetLinearDamping(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetAngularDamping(angularDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular damping. Entity is null");
	}

	float Script::Eagle_RigidBodyComponent_GetAngularDamping(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetEnableGravity(bEnable);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetEnableGravity`. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsGravityEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetIsKinematic(bKinematic);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsKinematic`. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsKinematic(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddForce(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. It's not a physics actor: {}", entity.GetName());
				return;
			}
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_RigidBodyComponent_AddForceAtLocation(GUID entityID, const glm::vec3* location, const glm::vec3* force, ForceMode forceMode)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddForceAtLocation(*location, *force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDynamic`. It's not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTarget` of Entity. It's is not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetLocation` of Entity. It's is not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetKinematicTargetRotation` of Entity. It's is not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTarget` of Entity. It's is not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetLocation` of Entity. It's is not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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
				EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetKinematicTargetRotation` of Entity. It's is not a physics actor: {}", entity.GetName());
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
		auto& scene = Scene::GetCurrentScene();
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

	CharacterControllerCollisionFlags Script::Eagle_CharacterControllerComponent_Move(GUID id, const glm::vec3* disp, float minDist, float elapsedTime)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().Move(*disp, minDist, elapsedTime);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Move`. Entity is null");
			return CharacterControllerCollisionFlags::None;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetSlopeLimit(GUID id, float degrees)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetSlopeLimit(degrees);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetSlopeLimit`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetSlopeLimit(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetSlopeLimit();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSlopeLimit`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetContactOffset(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetContactOffset(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetContactOffset`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetContactOffset(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetContactOffset();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetContactOffset`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetStepOffset(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetStepOffset(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetStepOffset`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetStepOffset(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetStepOffset();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetStepOffset`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetPhysicsMaterialAsset(GUID id, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material asset of CharacterControllerComponent. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<CharacterControllerComponent>();
		if (assetID.IsNull())
		{
			component.SetPhysicsMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material asset of CharacterControllerComponent. Couldn't find an asset");
			return;
		}

		Ref<AssetPhysicsMaterial> physicsMaterialAsset = Cast<AssetPhysicsMaterial>(asset);
		if (!physicsMaterialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set physics material asset of CharacterControllerComponent. Provided asset is not a physics material asset");
			return;
		}

		component.SetPhysicsMaterialAsset(physicsMaterialAsset);
	}

	GUID Script::Eagle_CharacterControllerComponent_GetPhysicsMaterialAsset(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get physics material asset of CharacterControllerComponent. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<CharacterControllerComponent>();
		const auto& asset = component.GetPhysicsMaterialAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_CharacterControllerComponent_SetShapeType(GUID id, CharacterControllerShape value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetShapeType(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetShapeType`. Entity is null");
		}
	}

	CharacterControllerShape Script::Eagle_CharacterControllerComponent_GetShapeType(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetShapeType();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetShapeType`. Entity is null");
			return CharacterControllerShape::Box;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCapsuleClimbingMode(GUID id, CapsuleClimbingMode value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCapsuleClimbingMode(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCapsuleClimbingMode`. Entity is null");
		}
	}

	CapsuleClimbingMode Script::Eagle_CharacterControllerComponent_GetCapsuleClimbingMode(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCapsuleClimbingMode();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCapsuleClimbingMode`. Entity is null");
			return CapsuleClimbingMode::Easy;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCapsuleRadius(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCapsuleRadius(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCapsuleRadius`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetCapsuleRadius(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCapsuleRadius();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCapsuleRadius`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCapsuleHeight(GUID id, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCapsuleHeight(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCapsuleHeight`. Entity is null");
		}
	}

	float Script::Eagle_CharacterControllerComponent_GetCapsuleHeight(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCapsuleHeight();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCapsuleHeight`. Entity is null");
			return 0.0;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetBoxSize(GUID id, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetBoxSize(*value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetBoxSize`. Entity is null");
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetBoxSize(GUID id, glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*value = entity.GetComponent<CharacterControllerComponent>().GetBoxSize();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetBoxSize`. Entity is null");
			*value = glm::vec3(0);
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetControllerWorldLocation(GUID id, glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*value = entity.GetComponent<CharacterControllerComponent>().GetControllerWorldLocation();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetControllerWorldLocation`. Entity is null");
			*value = glm::vec3(0);
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetControllerFootWorldLocation(GUID id, glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*value = entity.GetComponent<CharacterControllerComponent>().GetControllerFootWorldLocation();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetControllerFootWorldLocation`. Entity is null");
			*value = glm::vec3(0);
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetShowCollision(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetShowCollision(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetShowCollision`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_IsCollisionVisible(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().IsCollisionVisible();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsCollisionVisible`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetCollisionGroup(GUID id, CollisionGroup value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetCollisionGroup(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCollisionGroup`. Entity is null");
		}
	}

	CollisionGroup Script::Eagle_CharacterControllerComponent_GetCollisionGroup(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetCollisionGroup();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCollisionGroup`. Entity is null");
			return CollisionGroup::Object;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetInteractingCollisionGroup(GUID id, CollisionGroup value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetInteractingCollisionGroup(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetInteractingCollisionGroup`. Entity is null");
		}
	}

	CollisionGroup Script::Eagle_CharacterControllerComponent_GetInteractingCollisionGroup(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().GetInteractingCollisionGroup();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetInteractingCollisionGroup`. Entity is null");
			return CollisionGroup::Object;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetControllerUpDirection(GUID id, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetControllerUpDirection(*value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetControllerUpDirection`. Entity is null");
		}
	}

	void Script::Eagle_CharacterControllerComponent_GetControllerUpDirection(GUID id, glm::vec3* result)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			*result = entity.GetComponent<CharacterControllerComponent>().GetControllerUpDirection();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetControllerUpDirection`. Entity is null");
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetDoesCollideWithOtherControllers(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().SetDoesCollideWithOtherControllers(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetDoesCollideWithOtherControllers`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_DoesCollideWithOtherControllers(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().DoesCollideWithOtherControllers();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesCollideWithOtherControllers`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetMoveWholeEntity(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().bMoveWholeEntity = value;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bMoveWholeEntity`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_GetMoveWholeEntity(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().bMoveWholeEntity;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetMoveWholeEntity`. Entity is null");
			return false;
		}
	}

	void Script::Eagle_CharacterControllerComponent_SetUseFootLocation(GUID id, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			entity.GetComponent<CharacterControllerComponent>().bUseFootLocation = value;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bUseFootLocation`. Entity is null");
		}
	}

	bool Script::Eagle_CharacterControllerComponent_GetUseFootLocation(GUID id)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(id);
		if (entity)
		{
			return entity.GetComponent<CharacterControllerComponent>().bUseFootLocation;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetUseFootLocation`. Entity is null");
			return false;
		}
	}

	//--------------BaseColliderComponent--------------
	void Script::Eagle_BaseColliderComponent_SetCollisionGroup(GUID entityID, void* type, CollisionGroup groups)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetCollisionGroupsFunctions[monoType](entity, groups);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCollisionGroup'. Entity is null");
	}

	CollisionGroup Script::Eagle_BaseColliderComponent_GetCollisionGroup(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetCollisionGroupsFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetCollisionGroup'. Entity is null");
			return s_DefaultCollisionGroup;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetInteractingCollisionGroup(GUID entityID, void* type, CollisionGroup groups)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetInteractingCollisionGroupsFunctions[monoType](entity, groups);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetInteractingCollisionGroup'. Entity is null");
	}

	CollisionGroup Script::Eagle_BaseColliderComponent_GetInteractingCollisionGroup(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetInteractingCollisionGroupsFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetInteractingCollisionGroup'. Entity is null");
			return s_DefaultInteractingCollisionGroup;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetIsTrigger(GUID entityID, void* type, bool bTrigger)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetIsTriggerFunctions[monoType](entity, bTrigger);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTrigger'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsTrigger(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
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

	void Script::Eagle_BaseColliderComponent_SetCollisionEnabled(GUID entityID, void* type, bool bEnabled)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetCollisionEnabledFunctions[monoType](entity, bEnabled);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetCollisionEnabled'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsCollisionEnabled(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_IsCollisionEnabledFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsCollisionEnabled'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetCollisionVisible(GUID entityID, void* type, bool bShow)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_IsCollisionVisibleFunctions[monoType](entity);
		
		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsCollisionVisible'. Entity is null");
		return false;
	}

	GUID Script::Eagle_BaseColliderComponent_GetPhysicsMaterial(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetPhysicsMaterialFunctions[monoType](entity);

		EG_CORE_ERROR("[ScriptEngine] Couldn't get PhysicsMaterial. Entity is null");
		return GUID(0, 0);
	}

	void Script::Eagle_BaseColliderComponent_SetPhysicsMaterial(GUID entityID, void* type, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
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

	void Script::Eagle_BaseColliderComponent_SetAffectsNavMeshBuild(GUID entityID, void* type, bool bAffects)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			m_SetAffectsNavMeshBuildFunctions[monoType](entity, bAffects);
			return;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetAffectsNavMeshBuild'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_DoesAffectNavMeshBuild(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_DoesAffectNavMeshBuildFunctions[monoType](entity);

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'DoesAffectNavMeshBuild'. Entity is null");
		return false;
	}

	void Script::Eagle_BaseColliderComponent_SetIsObstacle(GUID entityID, void* type, bool bObstacle)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
		{
			m_SetIsObstacleFunctions[monoType](entity, bObstacle);
			return;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsObstacle'. Entity is null");
	}

	bool Script::Eagle_BaseColliderComponent_IsObstacle(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_IsObstacleFunctions[monoType](entity);

		EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsObstacle'. Entity is null");
		return false;
	}

	//--------------BoxColliderComponent--------------
	void Script::Eagle_BoxColliderComponent_SetSize(GUID entityID, const glm::vec3* size)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<BoxColliderComponent>().SetSize(*size);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set size of BoxCollider. Entity is null");
	}

	void Script::Eagle_BoxColliderComponent_GetSize(GUID entityID, glm::vec3* outSize)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<SphereColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of SphereCollider. Entity is null");
	}

	float Script::Eagle_SphereColliderComponent_GetRadius(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of CapsuleCollider. Entity is null");
	}

	float Script::Eagle_CapsuleColliderComponent_GetRadius(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetHeight(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set height of CapsuleCollider. Entity is null");
	}

	float Script::Eagle_CapsuleColliderComponent_GetHeight(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsConvex(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsConvex'. Entity is null");
	}

	bool Script::Eagle_MeshColliderComponent_IsConvex(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsTwoSided(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTwoSided'. Entity is null");
	}

	bool Script::Eagle_MeshColliderComponent_IsTwoSided(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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

		Ref<AssetStaticMesh> meshAsset = Cast<AssetStaticMesh>(asset);
		if (!meshAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set collision mesh. Provided asset is not a mesh asset");
			return;
		}

		component.SetCollisionMeshAsset(meshAsset);
	}

	GUID Script::Eagle_MeshColliderComponent_GetCollisionMesh(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Primary = val;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'IsPrimary'. Entity is null");
	}

	bool Script::Eagle_CameraComponent_GetIsPrimary(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveVerticalFOV(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveVerticalFOV'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetPerspectiveNearClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveNearClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetPerspectiveFarClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetShadowFarClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetShadowFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'ShadowFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetDirLightShadowFarClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetDirLightShadowFarClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'DirLightShadowFarClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetDirLightShadowFarClip(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetDirLightShadowFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'DirLightShadowFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetCascadesSplitAlpha(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetCascadesSplitAlpha(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CascadesSplitAlpha'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetCascadesSmoothTransitionAlpha(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetCascadesSmoothTransitionAlpha(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CascadesSmoothTransitionAlpha'. Entity is null");
	}

	CameraProjectionMode Script::Eagle_CameraComponent_GetCameraProjectionMode(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetProjectionMode(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'ProjectionMode'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetAspectRatio(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetAspectRatio();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call Camera Component 'GetAspectRatio'. Entity is null");
		return 0.f;
	}

	//--------------Reverb Component--------------
	bool Script::Eagle_ReverbComponent_IsActive(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetActive(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetActive'. Entity is null");
	}

	ReverbPreset Script::Eagle_ReverbComponent_GetReverbPreset(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetPreset(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'Preset'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMinDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetMinDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'MinDistance'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMaxDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetMaxDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'MaxDistance'. Entity is null");
	}

	//--------------Text Component--------------
	MonoString* Script::Eagle_TextComponent_GetText(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetText(MonoStringHandler(value).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_GetColor(GUID entityID, glm::vec3* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<TextComponent>().GetColor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get color of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_SetColor(GUID entityID, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetColor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set color of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetLineSpacing(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetLineSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set line spacing of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetKerning(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetKerning(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set kerning of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetMaxWidth(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetMaxWidth(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max width of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_GetIsLit(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetIsLit(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsLit` of Text Component. Entity is null");
	}
	
	bool Script::Eagle_TextComponent_DoesCastShadows(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetCastsShadows(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Text Component. Entity is null");
	}
	
	bool Script::Eagle_TextComponent_DoesReceiveDecals(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().DoesReceiveDecals();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesReceiveDecals` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVisible` of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().IsVisible();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetDoubleSided(GUID entityID, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetDoubleSided(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetDoubleSided` of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_IsDoubleSided(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().IsDoubleSided();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDoubleSided` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetReceivesDecals(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetReceivesDecals(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetReceivesDecals` of Text Component. Entity is null");
	}

	GUID Script::Eagle_TextComponent_GetFont(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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

	void Script::Eagle_TextComponent_GetMaterial(GUID entityID, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Text Component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<TextComponent>();
		const auto& materialAsset = component.GetMaterialAsset();
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_TextComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text Component component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<TextComponent>();

		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text Component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text Component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(materialAsset);
	}

	//--------------Text2D Component--------------
	GUID Script::Eagle_Text2DComponent_GetFont(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetText(MonoStringHandler(value).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set text of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetColor(GUID entityID, glm::vec3* outColor)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outColor = entity.GetComponent<Text2DComponent>().GetColor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get color of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetColor(GUID entityID, const glm::vec3* color)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetColor(*color);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set color of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetPosition(GUID entityID, glm::vec2* outPos)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outPos = entity.GetComponent<Text2DComponent>().GetPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get position of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetPosition(GUID entityID, const glm::vec2* pos)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetPosition(*pos);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set position of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetScale(GUID entityID, glm::vec2* outScale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetComponent<Text2DComponent>().GetScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get scale of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetScale(GUID entityID, const glm::vec2* scale)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetScale(*scale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set scale of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetRotation(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetRotation();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get rotation of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetRotation(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetRotation(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set rotation of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetLineSpacing(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetLineSpacing();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get line spacing of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetLineSpacing(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetLineSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set line spacing of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetKerning(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetKerning();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get kerning of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetKerning(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetKerning(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set kerning of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetMaxWidth(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetMaxWidth();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get MaxWidth of Text2D Component. Entity is null");
		return 0.f;
	}
	
	void Script::Eagle_Text2DComponent_SetMaxWidth(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetMaxWidth(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set MaxWidth of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetOpacity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Text2D Component. Entity is null");
		return 1.f;
	}

	void Script::Eagle_Text2DComponent_SetOpacity(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetIsVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetIsVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'IsVisible' of Text2D Component. Entity is null");
	}

	bool Script::Eagle_Text2DComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().IsVisible();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'IsVisible' of Text2D Component. Entity is null");
		return true;
	}

	//--------------Image2D Component--------------
	void Script::Eagle_Image2DComponent_SetTexture(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetTint();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get tint of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetTint(GUID entityID, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetTint(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set tint of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetPosition(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetPosition();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get position of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetPosition(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetPosition(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set position of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetScale(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get scale of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetScale(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetScale(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set scale of Image2D Component. Entity is null");
	}

	float Script::Eagle_Image2DComponent_GetRotation(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().GetRotation();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get rotation of Image2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Image2DComponent_SetRotation(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetRotation(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set rotation of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetOpacity(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Image2D Component. Entity is null");
	}

	float Script::Eagle_Image2DComponent_GetOpacity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Image2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Image2DComponent_SetIsVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetIsVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsVisible` of Image2D Component. Entity is null");
	}

	bool Script::Eagle_Image2DComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().IsVisible();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'IsVisible' of Image2D Component. Entity is null");
		return false;
	}

	//--------------Billboard Component--------------
	void Script::Eagle_BillboardComponent_SetTexture(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get billboard component texture. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<BillboardComponent>();
		return component.TextureAsset ? component.TextureAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_BillboardComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<BillboardComponent>().bVisible = value;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsVisible` of Billboard Component. Entity is null");
	}

	bool Script::Eagle_BillboardComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of billboard component. Entity is null");
			return false;
		}

		return entity.GetComponent<BillboardComponent>().bVisible;
	}

	//--------------Sprite Component--------------
	void Script::Eagle_SpriteComponent_GetMaterial(GUID entityID, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
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
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsAtlas`. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetIsAtlas(value);
	}

	bool Script::Eagle_SpriteComponent_DoesCastShadows(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetCastsShadows(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Sprite Component. Entity is null");
	}

	bool Script::Eagle_SpriteComponent_DoesReceiveDecals(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpriteComponent>().DoesReceiveDecals();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesReceiveDecals` of Sprite Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_SpriteComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVisible` of Sprite Component. Entity is null");
	}

	bool Script::Eagle_SpriteComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpriteComponent>().IsVisible();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of Sprite Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_SpriteComponent_SetReceivesDecals(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetReceivesDecals(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetReceivesDecals` of Sprite Component. Entity is null");
	}

	//--------------Script Component--------------
	void Script::Eagle_ScriptComponent_SetScript(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
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
		if (ScriptEngine::InstantiateEntityClass(entity))
			ScriptEngine::OnCreateEntity(entity);
	}

	MonoReflectionType* Script::Eagle_ScriptComponent_GetScriptType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
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
		if (!entityID.IsNull())
			return ScriptEngine::GetEntityMonoObject(entityID);

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetInstance`. Entity is null");
		return nullptr;
	}

	//--------------ParticleSystem Component--------------
	void Script::Eagle_ParticleSystemComponent_Spawn(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<ParticleSystemComponent>().Spawn();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Spawn` of Particle System Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_ParticleSystemComponent_Destroy(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<ParticleSystemComponent>().Destroy();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Destroy` of Particle System Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_ParticleSystemComponent_SetAsset(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `SetAsset`. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<ParticleSystemComponent>();
		if (assetID.IsNull())
		{
			component.SetAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `SetAsset`. Couldn't find an asset");
			return;
		}

		Ref<AssetParticleSystem> psAsset = Cast<AssetParticleSystem>(asset);
		if (!psAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `SetAsset`. Provided asset is not a Particle System asset");
			return;
		}

		component.SetAsset(psAsset);

		// Check if we need to notify particle system auto destruction script
		if (entity.HasComponent<NativeScriptComponent>())
		{
			auto& scriptComp = entity.GetComponent<NativeScriptComponent>();
			if (PSAutoDestroyScript* script = scriptComp.As<PSAutoDestroyScript>())
			{
				script->OnAssetChanged();
			}
		}
	}

	GUID Script::Eagle_ParticleSystemComponent_GetAsset(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `GetAsset`. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<ParticleSystemComponent>();
		const auto& asset = component.GetAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_ParticleSystemComponent_DuplicatePose(GUID entityID, uint32_t emitterIndex, GUID compEntityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Entity is null");
			return;
		}

		Entity skeletalEntity = scene->GetEntityByGUID(compEntityID);
		if (!skeletalEntity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Failed to get Entity of SkeletalMeshComponent");
			return;
		}

		if (!skeletalEntity.HasComponent<SkeletalMeshComponent>())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. SkeletalMeshComponent of entity is invalid: {}", skeletalEntity.GetName());
			return;
		}

		auto& ps = entity.GetComponent<ParticleSystemComponent>();
		if (emitterIndex >= ps.PerEmitterAnimData.size())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Emitter index is out of bounds! Index: {}; Emitters count: {}", emitterIndex, ps.PerEmitterAnimData.size());
			return;
		}

		const auto& emitter = ps.GetAsset()->GetEmitters()[emitterIndex];
		if (emitter.MeshAsset != skeletalEntity.GetComponent<SkeletalMeshComponent>().GetMeshAsset())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Source `Skeletal Mesh Component` and `Emitter` use different skeletal mesh assets! They must match.");
			return;
		}
		ps.PerEmitterAnimData[emitterIndex].SrcOfLastPose = skeletalEntity;
	}

	//--------------Decal Component--------------
	void Script::Eagle_DecalComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set decal component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<DecalComponent>();
		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set decal component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set decal component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(materialAsset);
	}

	GUID Script::Eagle_DecalComponent_GetMaterial(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& asset = entity.GetComponent<DecalComponent>().GetMaterialAsset();
			return asset ? asset->GetGUID() : GUID(0, 0);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material of Decal Component. Entity is null");
			return GUID(0, 0);
		}
	}

	void Script::Eagle_DecalComponent_SetAdjustAspectRatioEnabled(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<DecalComponent>().SetAdjustAspectRatioEnabled(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bAdjustAspectRatio` of Decal Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_DecalComponent_IsAdjustAspectRatioEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DecalComponent>().IsAdjustAspectRatioEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsAdjustAspectRatioEnabled` of Decal Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_DecalComponent_SetSortPriority(GUID entityID, uint32_t value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<DecalComponent>().SetSortPriority(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `SortPriority` of Decal Component. Entity is null");
			return;
		}
	}

	uint32_t Script::Eagle_DecalComponent_GetSortPriority(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DecalComponent>().GetSortPriority();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `SortPriority` of Decal Component. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_DecalComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<DecalComponent>().SetVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVisible` of Decal Component. Entity is null");
	}

	bool Script::Eagle_DecalComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DecalComponent>().IsVisible();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of Decal Component. Entity is null");
			return false;
		}
	}

	//--------------NavigationMesh Component--------------
	void Script::Eagle_NavigationMeshComponent_Build(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			scene->BuildNavMesh(&entity.GetComponent<NavigationMeshComponent>());
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Build` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_SetCrowdSettings(GUID entityID, const AINavigation::CrowdSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationMeshComponent>().SetCrowdSettings(*settings);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCrowdSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_GetCrowdSettings(GUID entityID, AINavigation::CrowdSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*settings = entity.GetComponent<NavigationMeshComponent>().GetCrowdSettings();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCrowdSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_SetSettings(GUID entityID, const AINavigation::MeshSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationMeshComponent>().SetSettings(*settings);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_GetSettings(GUID entityID, AINavigation::MeshSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*settings = entity.GetComponent<NavigationMeshComponent>().GetSettings();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_SetAutoRebuild(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationMeshComponent>().bAutoRebuild = value;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bAutoRebuild` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_NavigationMeshComponent_GetAutoRebuild(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationMeshComponent>().bAutoRebuild;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `bAutoRebuild` of Navigation Mesh Component. Entity is null");
			return false;
		}
	}

	//--------------NavigationCrowdAgent Component--------------
	void Script::Eagle_NavigationCrowdAgentComponent_TeleportAgent(GUID entityID, const glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().TeleportAgent(*location);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `TeleportAgent` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_SetMoveTarget(GUID entityID, const glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().SetMoveTarget(*location);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMoveTarget` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_ResetMoveTarget(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().ResetMoveTarget();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `ResetMoveTarget` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_NavigationCrowdAgentComponent_IsValid(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().IsValid();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsValid` of Navigation Crowd Agent Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_SetSettings(GUID entityID, const AINavigation::AgentSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().SetSettings(*settings);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetSettings` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_GetSettings(GUID entityID, AINavigation::AgentSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*settings = entity.GetComponent<NavigationCrowdAgentComponent>().GetSettings();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSettings` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_NavigationCrowdAgentComponent_GetLocation(GUID entityID, glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().GetLocation(location);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLocation` of Navigation Crowd Agent Component. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_NavigationCrowdAgentComponent_GetVelocity(GUID entityID, glm::vec3* velocity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().GetVelocity(velocity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetVelocity` of Navigation Crowd Agent Component. Entity is null");
			return false;
		}
	}

	MoveRequestState Script::Eagle_NavigationCrowdAgentComponent_GetTargetState(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().GetAgentTargetState();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetTargetState` of Navigation Crowd Agent Component. Entity is null");
			return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;
		}
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
		// TODO: Check if works
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
		auto& scene = Scene::GetCurrentScene();

		// TODO: Check if works
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

	void Script::Eagle_Renderer_SetGTAOSettings(uint32_t samples, uint32_t stepsPerSample, float radius, float fallollRange, bool bGenerateBentNormals, bool bHalfRes)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.GTAOSettings.Quality.NumberOfSamples = samples;
		options.GTAOSettings.Quality.StepsPerSample = stepsPerSample;
		options.GTAOSettings.Radius = radius;
		options.GTAOSettings.FalloffRange = fallollRange;
		options.GTAOSettings.bGenerateBentNormals = bGenerateBentNormals;
		options.GTAOSettings.bHalfRes = bHalfRes;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetGTAOSettings(uint32_t* outSamples, uint32_t* outStepsPerSample, float* outRadius, float* outFalloffRange, bool* outGenerateBentNormals, bool* outHalfRes)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.GTAOSettings.Quality.NumberOfSamples;
		*outStepsPerSample = options.GTAOSettings.Quality.StepsPerSample;
		*outRadius = options.GTAOSettings.Radius;
		*outFalloffRange = options.GTAOSettings.FalloffRange;
		*outGenerateBentNormals = options.GTAOSettings.bGenerateBentNormals;
		*outHalfRes = options.GTAOSettings.bHalfRes;
	}

	void Script::Eagle_Renderer_SetMSAASettings(MSAASamples samples, float edgeThreshold)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.MSAAParams.Samples = samples;
		options.MSAAParams.EdgeThreshold = edgeThreshold;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetMSAASettings(MSAASamples* outSamples, float* outEdgeThreshold)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSamples = options.MSAAParams.Samples;
		*outEdgeThreshold = options.MSAAParams.EdgeThreshold;
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

	void Script::Eagle_Renderer_SetAgXTonemappingSettings(const glm::vec3& slope, const glm::vec3& power, const glm::vec3& offset, float saturation)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.AgXTonemappingParams.Slope = slope;
		options.AgXTonemappingParams.Power = power;
		options.AgXTonemappingParams.Offset = offset;
		options.AgXTonemappingParams.Saturation = saturation;

		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetAgXTonemappingSettings(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		const auto& options = Scene::GetCurrentScene()->GetSceneRenderer()->GetOptions();
		*slope = options.AgXTonemappingParams.Slope;
		*power = options.AgXTonemappingParams.Power;
		*offset = options.AgXTonemappingParams.Offset;
		*saturation = options.AgXTonemappingParams.Saturation;
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

	bool Script::Eagle_Renderer_IsGeometricSpecularAAEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bGeometricSpecularAA;
	}

	void Script::Eagle_Renderer_SetGeometicSpecularAAEnabled(bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bGeometricSpecularAA = bEnabled;
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

	void Script::Eagle_Renderer_SetVisualizeLightTiles(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.bVisualizeLightTiles = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_GetVisualizeLightTiles()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		return options.bVisualizeLightTiles;
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
		const auto& sky = scene->GetSkySettings();

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
		scene->SetSkybox(sky);
	}

	void Script::Eagle_Renderer_SetUseSkyAsBackground(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->SetUseSkyAsBackground(value);
	}

	bool Script::Eagle_Renderer_GetUseSkyAsBackground()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->GetUseSkyAsBackground();
	}

	void Script::Eagle_Renderer_SetRenderSkyboxEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->SetRenderSkybox(value);
	}

	bool Script::Eagle_Renderer_IsRenderSkyboxEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->IsRenderSkyboxEnabled();
	}

	void Script::Eagle_Renderer_SetSkyboxEnabled(bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		scene->SetSkyboxEnabled(value);
	}

	bool Script::Eagle_Renderer_IsSkyboxEnabled()
	{
		const auto& scene = Scene::GetCurrentScene();
		return scene->IsSkyboxEnabled();
	}

	void Script::Eagle_Renderer_SetVolumetricLightsSettings(const glm::vec3* albedo, float anisotropy, uint32_t samples, float maxScatteringDist, float fogSpeed, bool bFogEnable, bool bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto settings = sceneRenderer->GetOptions();

		settings.VolumetricSettings.Albedo = *albedo;
		settings.VolumetricSettings.Anisotropy = anisotropy;
		settings.VolumetricSettings.Samples = samples;
		settings.VolumetricSettings.MaxScatteringDistance = maxScatteringDist;
		settings.VolumetricSettings.FogSpeed = fogSpeed;
		settings.VolumetricSettings.bFogEnable = bFogEnable;
		settings.VolumetricSettings.bEnable = bEnable;
		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_GetVolumetricLightsSettings(glm::vec3* albedo, float* anisotropy, uint32_t* outSamples, float* outMaxScatteringDist, float* fogSpeed, bool* bFogEnable, bool* bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().VolumetricSettings;

		*albedo = settings.Albedo;
		*anisotropy = settings.Anisotropy;
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
		size_t index = 0;
		for (auto& res : settings.DirLightShadowMapSizes)
			mono_array_set(result, uint32_t, index++, res);

		return result;
	}

	void Script::Eagle_Renderer_GetScreenSpaceShadowsSettings(uint32_t* samples, uint32_t* hardShadowSamples, uint32_t* fadeOutSamples, float* surfaceThickness, float* bilinearThreshold,
		float* shadowContrast, bool* bIgnoreEdgePixels, bool* bUsePrecisionOffset, bool* bBilinearSamplingOffsetMode, bool* bUseEarlyOut)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().ScreenSpaceShadows;

		*samples = settings.Samples;
		*hardShadowSamples = settings.HardShadowSamples;
		*fadeOutSamples = settings.FadeOutSamples;
		*surfaceThickness = settings.SurfaceThickness;
		*bilinearThreshold = settings.BilinearThreshold;
		*shadowContrast = settings.ShadowContrast;
		*bIgnoreEdgePixels = settings.bIgnoreEdgePixels;
		*bUsePrecisionOffset = settings.bUsePrecisionOffset;
		*bBilinearSamplingOffsetMode = settings.bBilinearSamplingOffsetMode;
		*bUseEarlyOut = settings.bUseEarlyOut;
	}

	void Script::Eagle_Renderer_GetDepthOfFieldSettings(glm::vec2* apertureShape, float* apertureSize, float* focalLength, float* COCScale, float* maxCOC)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().DOFSettings;

		*apertureShape = settings.ApertureShape;
		*apertureSize = settings.ApertureSize;
		*focalLength = settings.FocalLength;
		*COCScale = settings.COCScale;
		*maxCOC = settings.MaxCOC;
	}

	void Script::Eagle_Renderer_GetMotionBlurSettings(bool* bEnabled, uint32_t* numSamples, float* strength, float* noMotionBlurThreshold, float* lowMotionThreshold, bool* bUseCheapOnLowMotion)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().MotionBlur;

		*bEnabled = settings.bEnable;
		*numSamples = settings.NumSamples;
		*strength = settings.Strength;
		*noMotionBlurThreshold = settings.NoMotionBlurThreshold;
		*lowMotionThreshold = settings.LowMotionThreshold;
		*bUseCheapOnLowMotion = settings.bUseCheapOnLowMotion;
	}

	void Script::Eagle_Renderer_GetAutoExposureSettings(float* minLogLum, float* maxLogLum, float* adaptationSpeed, float* adaptationKey, bool* bEnabled, bool* bHalfResolution)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().AutoExposure;

		*minLogLum = settings.MinLogLum;
		*maxLogLum = settings.MaxLogLum;
		*adaptationSpeed = settings.AdaptationSpeed;
		*adaptationKey = settings.AdaptationKey;
		*bEnabled = settings.bEnable;
		*bHalfResolution = settings.bHalfResolution;
	}

	void Script::Eagle_Renderer_GetScreenSpaceReflectionsSettings(float* varianceThreshold, float* depthBufferThickness, float* temporalStabilityFactor, float* roughnessThreshold,
		uint32_t* samplesPerQuad, uint32_t* maxTraversalIterations, uint32_t* minTraversalOccupancy, bool* bTemporalVarianceGuidedTracing, bool* bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().ScreenSpaceReflections;

		*varianceThreshold = settings.VarianceThreshold;
		*depthBufferThickness = settings.DepthBufferThickness;
		*temporalStabilityFactor = settings.TemporalStabilityFactor;
		*roughnessThreshold = settings.RoughnessThreshold;
		*samplesPerQuad = settings.SamplesPerQuad;
		*maxTraversalIterations = settings.MaxTraversalIterations;
		*minTraversalOccupancy = settings.MinTraversalOccupancy;
		*bTemporalVarianceGuidedTracing = settings.bTemporalVarianceGuidedTracing;
		*bEnabled = settings.bEnable;
	}

	void Script::Eagle_Renderer_GetLensSettings(bool* bChromaticAberration, bool* bVignette, bool* bFilmGrain, float* chromaticIntensity, float* vignetteIntensity, float* filmGrainScale, float* filmGrainAmount, float* filmGrainSeedUpdateRate)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().Lens;

		*bChromaticAberration = settings.bEnableChromaticAberration;
		*bVignette = settings.bEnableVignette;
		*bFilmGrain = settings.bEnableFilmGrain;
		*chromaticIntensity = settings.ChromaticIntensity;
		*vignetteIntensity = settings.VignetteIntensity;
		*filmGrainScale = settings.FilmGrainScale;
		*filmGrainAmount = settings.FilmGrainAmount;
		*filmGrainSeedUpdateRate = settings.FilmGrainSeedUpdateRate;
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

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetScreenSpaceShadowsSettings(uint32_t samples, uint32_t hardShadowSamples, uint32_t fadeOutSamples, float surfaceThickness, float bilinearThreshold,
		float shadowContrast, bool bIgnoreEdgePixels, bool bUsePrecisionOffset, bool bBilinearSamplingOffsetMode, bool bUseEarlyOut)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.ScreenSpaceShadows.Samples = samples;
		settings.ScreenSpaceShadows.HardShadowSamples = hardShadowSamples;
		settings.ScreenSpaceShadows.FadeOutSamples = fadeOutSamples;
		settings.ScreenSpaceShadows.SurfaceThickness = surfaceThickness;
		settings.ScreenSpaceShadows.BilinearThreshold = bilinearThreshold;
		settings.ScreenSpaceShadows.ShadowContrast = shadowContrast;
		settings.ScreenSpaceShadows.bIgnoreEdgePixels = bIgnoreEdgePixels;
		settings.ScreenSpaceShadows.bUsePrecisionOffset = bUsePrecisionOffset;
		settings.ScreenSpaceShadows.bBilinearSamplingOffsetMode = bBilinearSamplingOffsetMode;
		settings.ScreenSpaceShadows.bUseEarlyOut = bUseEarlyOut;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetDepthOfFieldSettings(const glm::vec2* apertureShape, float apertureSize, float focalLength, float COCScale, float maxCOC)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.DOFSettings.ApertureShape = *apertureShape;
		settings.DOFSettings.ApertureSize = apertureSize;
		settings.DOFSettings.FocalLength = focalLength;
		settings.DOFSettings.COCScale = COCScale;
		settings.DOFSettings.MaxCOC = maxCOC;
		
		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetMotionBlurSettings(bool bEnabled, uint32_t numSamples, float strength, float noMotionBlurThreshold, float lowMotionThreshold, bool bUseCheapOnLowMotion)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.MotionBlur.bEnable = bEnabled;
		settings.MotionBlur.NumSamples = numSamples;
		settings.MotionBlur.Strength = strength;
		settings.MotionBlur.NoMotionBlurThreshold = noMotionBlurThreshold;
		settings.MotionBlur.LowMotionThreshold = lowMotionThreshold;
		settings.MotionBlur.bUseCheapOnLowMotion = bUseCheapOnLowMotion;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetAutoExposureSettings(float minLogLum, float maxLogLum, float adaptationSpeed, float adaptationKey, bool bEnabled, bool bHalfResolution)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.AutoExposure.MinLogLum = minLogLum;
		settings.AutoExposure.MaxLogLum = maxLogLum;
		settings.AutoExposure.AdaptationSpeed = adaptationSpeed;
		settings.AutoExposure.AdaptationKey = adaptationKey;
		settings.AutoExposure.bEnable = bEnabled;
		settings.AutoExposure.bHalfResolution = bHalfResolution;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetScreenSpaceReflectionsSettings(float varianceThreshold, float depthBufferThickness, float temporalStabilityFactor, float roughnessThreshold,
		uint32_t samplesPerQuad, uint32_t maxTraversalIterations, uint32_t minTraversalOccupancy, bool bTemporalVarianceGuidedTracing, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.ScreenSpaceReflections.VarianceThreshold = varianceThreshold;
		settings.ScreenSpaceReflections.DepthBufferThickness = depthBufferThickness;
		settings.ScreenSpaceReflections.TemporalStabilityFactor = temporalStabilityFactor;
		settings.ScreenSpaceReflections.RoughnessThreshold = roughnessThreshold;
		settings.ScreenSpaceReflections.SamplesPerQuad = samplesPerQuad;
		settings.ScreenSpaceReflections.MaxTraversalIterations = maxTraversalIterations;
		settings.ScreenSpaceReflections.MinTraversalOccupancy = minTraversalOccupancy;
		settings.ScreenSpaceReflections.bTemporalVarianceGuidedTracing = bTemporalVarianceGuidedTracing;
		settings.ScreenSpaceReflections.bEnable = bEnabled;

		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_SetLensSettings(bool bChromaticAberration, bool bVignette, bool bFilmGrain, float chromaticIntensity, float vignetteIntensity, float filmGrainScale, float filmGrainAmount, float filmGrainSeedUpdateRate)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();

		auto settings = sceneRenderer->GetOptions();
		settings.Lens.bEnableChromaticAberration = bChromaticAberration;
		settings.Lens.bEnableVignette = bVignette;
		settings.Lens.bEnableFilmGrain = bFilmGrain;
		settings.Lens.ChromaticIntensity = chromaticIntensity;
		settings.Lens.VignetteIntensity = vignetteIntensity;
		settings.Lens.FilmGrainScale = filmGrainScale;
		settings.Lens.FilmGrainAmount = filmGrainAmount;
		settings.Lens.FilmGrainSeedUpdateRate = filmGrainSeedUpdateRate;

		sceneRenderer->SetOptions(settings);
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

	void Script::Eagle_Renderer_GetCameraTransform(Transform* outTransform)
	{
		*outTransform = Scene::GetCurrentScene()->GetRuntimeCamera()->GetWorldTransform();
	}

	void Script::Eagle_Renderer_GetViewportSize(glm::vec2* outSize)
	{
		*outSize = glm::vec2(Scene::GetCurrentScene()->GetSceneRenderer()->GetViewportSize());
	}

	void Script::Eagle_Renderer_SetSkybox(GUID cubemapID)
	{
		auto& scene = Scene::GetCurrentScene();
		if (cubemapID.IsNull())
		{
			scene->SetSkybox(nullptr);
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
		
		scene->SetSkybox(cubemap);
	}

	GUID Script::Eagle_Renderer_GetSkybox()
	{
		const auto& skybox = Scene::GetCurrentScene()->GetSkybox();
		return skybox ? skybox->GetGUID() : GUID{0, 0};
	}

	void Script::Eagle_Renderer_SetCubemapIntensity(float intensity)
	{
		Scene::GetCurrentScene()->SetSkyboxIntensity(intensity);
	}

	float Script::Eagle_Renderer_GetCubemapIntensity()
	{
		return Scene::GetCurrentScene()->GetSkyboxIntensity();
	}

	void Script::Eagle_Renderer_DrawLine(const glm::vec3* startColor, const glm::vec3* endColor, const glm::vec3* start, const glm::vec3* end)
	{
		RendererLine line;
		line.Start.Color = *startColor;
		line.Start.Location = *start;
		line.End.Color = *endColor;
		line.End.Location = *end;
		Scene::GetCurrentScene()->DrawDebugLine(line);
	}

	void Script::Eagle_Renderer_DrawTriangle(const glm::vec3* v0Location, const glm::vec3* v0Color, const glm::vec3* v1Location, const glm::vec3* v1Color, const glm::vec3* v2Location, const glm::vec3* v2Color)
	{
		RendererTriangle triangle;
		triangle.Vertices[0].Location = *v0Location;
		triangle.Vertices[0].Color = *v0Color;
		triangle.Vertices[1].Location = *v1Location;
		triangle.Vertices[1].Color = *v1Color;
		triangle.Vertices[2].Location = *v2Location;
		triangle.Vertices[2].Color = *v2Color;

		Scene::GetCurrentScene()->DrawDebugTriangle(triangle);
	}

	void Script::Eagle_Renderer_DrawArrow(const glm::vec3* start, const glm::vec3* end, const glm::vec3* up)
	{
		Scene::GetCurrentScene()->DrawArrow(*start, *end, *up);
	}

	void Script::Eagle_Renderer_DrawAABB(const AABB* aabb, const Transform* transform)
	{
		Scene::GetCurrentScene()->DrawAABB(*aabb, *transform);
	}

	void Script::Eagle_Renderer_DrawBox(const AABB* aabb, const Transform* transform)
	{
		Scene::GetCurrentScene()->DrawBox(*aabb, *transform);
	}

	void Script::Eagle_Renderer_DrawCone(const glm::vec3* location, const glm::quat* rotation, float distance, float angleRad)
	{
		Scene::GetCurrentScene()->DrawCone(*location, *rotation, distance, angleRad);
	}

	void Script::Eagle_AgXTonemapping_GetDefaultLook(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		AgXTonemappingSettings agx = AgXTonemappingSettings::GetDefaultLook();

		*slope = agx.Slope;
		*power = agx.Power;
		*offset = agx.Offset;
		*saturation = agx.Saturation;
	}

	void Script::Eagle_AgXTonemapping_GetGoldenLook(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		AgXTonemappingSettings agx = AgXTonemappingSettings::GetGoldenLook();

		*slope = agx.Slope;
		*power = agx.Power;
		*offset = agx.Offset;
		*saturation = agx.Saturation;
	}

	void Script::Eagle_AgXTonemapping_GetPunchyLook(glm::vec3* slope, glm::vec3* power, glm::vec3* offset, float* saturation)
	{
		AgXTonemappingSettings agx = AgXTonemappingSettings::GetPunchyLook();

		*slope = agx.Slope;
		*power = agx.Power;
		*offset = agx.Offset;
		*saturation = agx.Saturation;
	}

	void Script::Eagle_GTAO_GetQuality_Low(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetLowQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}

	void Script::Eagle_GTAO_GetQuality_Medium(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetMediumQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}

	void Script::Eagle_GTAO_GetQuality_High(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetHighQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
	}

	void Script::Eagle_GTAO_GetQuality_Ultra(uint32_t* samples, uint32_t* stepsPerSample, uint32_t* numOfBlurPasses)
	{
		GTAOSettings::QualityParams quality = GTAOSettings::GetUltraQuality();
		*samples = quality.NumberOfSamples;
		*stepsPerSample = quality.StepsPerSample;
		*numOfBlurPasses = quality.NumberOfBlurPasses;
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

	void Script::Eagle_Renderer_SetSortOpaqueParticlesEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bSortOpaqueParticles = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsSortOpaqueParticlesEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bSortOpaqueParticles;
	}

	void Script::Eagle_Renderer_SetDebugLinesDepthTestEnabled(bool value)
	{
		auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();
		options.bEnableDebugLinesDepthTest = value;
		sceneRenderer->SetOptions(options);
	}

	bool Script::Eagle_Renderer_IsDebugLinesDepthTestEnabled()
	{
		const auto& sceneRenderer = Scene::GetCurrentScene()->GetSceneRenderer();
		return sceneRenderer->GetOptions().bEnableDebugLinesDepthTest;
	}

	//-------------- Project --------------
	MonoString* Script::Eagle_Project_GetProjectPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetProjectPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetBinariesPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetBinariesPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetConfigPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetConfigPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetContentPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetContentPath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetCachePath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetCachePath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetRendererCachePath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetRendererCachePath()).c_str());
	}

	MonoString* Script::Eagle_Project_GetSavedPath()
	{
		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(Project::GetSavedPath()).c_str());
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

	void Script::Eagle_Scene_QuitGame()
	{
		WindowCloseEvent e(true);
		Application::Get().OnEvent(e);
	}

	bool Script::Eagle_Scene_Raycast(const glm::vec3* origin, const glm::vec3* dir, float maxDistance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore,
		MonoObject** outHitEntity, glm::vec3* outPosition, glm::vec3* outNormal, float* outDistance)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		RaycastHit hit{};
		bool bHit = false;

		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			bHit = physicsScene->Raycast(*origin, *dir, maxDistance, query, collisionGroup, &hit, &entitiesToIgnore);
		}
		else
		{
			bHit = physicsScene->Raycast(*origin, *dir, maxDistance, query, collisionGroup, &hit);
		}


		*outHitEntity = hit.HitEntity ? ScriptEngine::GetEntityMonoObject(hit.HitEntity) : nullptr;
		*outPosition = hit.Position;
		*outNormal = hit.Normal;
		*outDistance = hit.Distance;

		return bHit;
	}

	MonoArray* Script::Eagle_Scene_OverlapBox(const Transform* transform, const glm::vec3* boxHalfSize, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->OverlapBox(*transform, *boxHalfSize, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->OverlapBox(*transform, *boxHalfSize, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_OverlapCapsule(const Transform* transform, float radius, float halfHeight, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->OverlapCapsule(*transform, radius, halfHeight, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->OverlapCapsule(*transform, radius, halfHeight, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_OverlapSphere(const Transform* transform, float radius, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->OverlapSphere(*transform, radius, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->OverlapSphere(*transform, radius, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_SweepBox(const Transform* transform, const glm::vec3* boxHalfSize, const glm::vec3* direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->SweepBox(*transform, *boxHalfSize, *direction, distance, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->SweepBox(*transform, *boxHalfSize, *direction, distance, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_SweepCapsule(const Transform* transform, float radius, float halfHeight, const glm::vec3* direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->SweepCapsule(*transform, radius, halfHeight, *direction, distance, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->SweepCapsule(*transform, radius, halfHeight, *direction, distance, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	MonoArray* Script::Eagle_Scene_SweepSphere(const Transform* transform, float radius, const glm::vec3* direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, MonoArray* monoEntitiesToIgnore)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& physicsScene = scene->GetPhysicsScene();

		UniqueQueryHits hits;
		if (monoEntitiesToIgnore)
		{
			std::set<GUID> entitiesToIgnore;
			const uint32_t length = (uint32_t)mono_array_length(monoEntitiesToIgnore);
			for (uint32_t i = 0; i < length; ++i)
			{
				GUID entityGUID = mono_array_get(monoEntitiesToIgnore, GUID, i);
				entitiesToIgnore.emplace(entityGUID);
			}
			hits = physicsScene->SweepSphere(*transform, radius, *direction, distance, query, collisionGroup, &entitiesToIgnore);
		}
		else
		{
			hits = physicsScene->SweepSphere(*transform, radius, *direction, distance, query, collisionGroup);
		}

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), hits.size());
		size_t index = 0;
		for (auto& hit : hits)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(hit.HitEntity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	void Script::Eagle_Scene_SetGravity(const glm::vec3* gravity)
	{
		Scene::GetCurrentScene()->SetGravity(*gravity);
	}

	void Script::Eagle_Scene_GetGravity(glm::vec3* gravity)
	{
		*gravity = Scene::GetCurrentScene()->GetGravity();
	}

	MonoArray* Script::Eagle_Scene_GetAllEntitiesWithComponent(void* type)
	{
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		auto it = m_GetAllEntitiesWith.find(monoType);
		if (it == m_GetAllEntitiesWith.end())
		{
			EG_CORE_ERROR("[ScriptEngine] Failed to call 'GetAllEntitiesWithComponent'. Unknown type");
			return mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), 0);
		}

		std::vector<Entity> entities = it->second(Scene::GetCurrentScene());

		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetEntityClass(), entities.size());
		size_t index = 0;
		for (auto& entity : entities)
		{
			MonoObject* obj = ScriptEngine::GetEntityMonoObject(entity);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	GUID Script::Eagle_Scene_SpawnEntity(MonoString* monoName)
	{
		auto& scene = Scene::GetCurrentScene();
		const std::string name = MonoStringHandler(monoName).c_str();
		return scene->CreateEntity(name).GetGUID();
	}

	MonoObject* Script::Eagle_Scene_SpawnEntityFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't spawn entity. Couldn't find an Entity asset");
			return nullptr;
		}

		if (Ref<AssetEntity> entityAsset = Cast<AssetEntity>(asset))
		{
			auto& scene = Scene::GetCurrentScene();
			return ScriptEngine::GetEntityMonoObject(scene->CreateFromEntityAsset(entityAsset));
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't spawn entity. It's not an Entity asset");
		return nullptr;
	}

	GUID Script::Eagle_Scene_SpawnParticleSystem(MonoString* monoName, const Transform* transform, GUID assetID, bool bAutoDestroy)
	{
		Ref<AssetParticleSystem> psAsset;
		if (!assetID.IsNull())
		{
			Ref<Asset> asset;
			AssetManager::Get(assetID, &asset);
			if (!asset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't spawn particle system. Couldn't find a Particle System asset");
				return GUID(0, 0);
			}

			psAsset = Cast<AssetParticleSystem>(asset);
			if (!psAsset)
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't spawn particle system. Provided asset is not a Particle System asset");
				return GUID(0, 0);
			}
		}

		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->CreateEntity(MonoStringHandler(monoName).c_str());
		entity.SetWorldTransform(*transform);
		auto& ps = entity.AddComponent<ParticleSystemComponent>();
		ps.bAutospawn = true;
		ps.SetAsset(psAsset);

		if (bAutoDestroy)
			entity.AddComponent<NativeScriptComponent>().Bind<PSAutoDestroyScript>();

		return entity.GetGUID();
	}

	//-------------- Navigation --------------
	MonoArray* Script::Eagle_Navigation_FindStraightPath(const glm::vec3* start, const glm::vec3* end, uint32_t maxPolys)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();

		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindStraightPath`. There's no a navigation mesh");
			return mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), 0);
		}

		std::vector<glm::vec3> path = navMesh->FindStraightPath(*start, *end, maxPolys);
		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), path.size());
		size_t index = 0;
		for (auto& point : path)
		{
			mono_array_set(result, glm::vec3, index++, point);
		}
		return result;
	}

	MonoArray* Script::Eagle_Navigation_FindSmoothPath(const glm::vec3* start, const glm::vec3* end, uint32_t maxPolys, uint32_t maxSmooth)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindSmoothPath`. There's no a navigation mesh");
			return mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), 0);
		}

		std::vector<glm::vec3> path = navMesh->FindSmoothPath(*start, *end, maxPolys);
		MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetVector3Class(), path.size());
		size_t index = 0;
		for (auto& point : path)
		{
			mono_array_set(result, glm::vec3, index++, point);
		}
		return result;
	}

	bool Script::Eagle_Navigation_FindDistanceToWall(const glm::vec3* pos, float maxRadius, glm::vec3* outHitPos, glm::vec3* outHitNormal, float* outHitDistance)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindDistanceToWall`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->FindDistanceToWall(*pos, maxRadius, outHitPos, outHitNormal, outHitDistance);
		return bSuccess;
	}

	bool Script::Eagle_Navigation_FindRandomPoint(glm::vec3* outRandomPoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindRandomPoint`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->FindRandomPoint(outRandomPoint);
		return bSuccess;
	}

	bool Script::Eagle_Navigation_FindRandomPointInCircle(const glm::vec3* pos, float radius, glm::vec3* outRandomPoint)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `FindRandomPointInCircle`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->FindRandomPointInCircle(*pos, radius, outRandomPoint);
		return bSuccess;
	}

	bool Script::Eagle_Navigation_IsValidPoint(const glm::vec3* pos)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsValidPoint`. There's no a navigation mesh");
			return false;
		}

		const bool bSuccess = navMesh->IsValidPoint(*pos);
		return bSuccess;
	}

	//-------------- CrowdNavigation --------------
	void Script::Eagle_CrowdNavigation_SetMoveTarget(const glm::vec3* pos)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMoveTarget`. There's no a navigation mesh");
			return;
		}

		navMesh->GetCrowd().SetMoveTarget(*pos);
	}

	void Script::Eagle_CrowdNavigation_ResetMoveTarget()
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& navMesh = scene->GetNavMesh();
		if (!navMesh)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMoveTarget`. There's no a navigation mesh");
			return;
		}

		navMesh->GetCrowd().ResetMoveTarget();
	}

	//-------------- Log --------------
	void Script::Eagle_Log_Trace(MonoString* message)
	{
		if (message)
			EG_TRACE(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Info(MonoString* message)
	{
		if (message)
			EG_INFO(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Warn(MonoString* message)
	{
		if (message)
			EG_WARN(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Error(MonoString* message)
	{
		if (message)
			EG_ERROR(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	void Script::Eagle_Log_Critical(MonoString* message)
	{
		if (message)
			EG_CRITICAL(MonoStringHandler(message).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't log the message. It's null");
	}

	//--------------Asset--------------
	bool Script::Eagle_Asset_Get(MonoString* path, AssetType* outType, GUID* outGUID)
	{
		Path filepath = MonoStringHandler(path).c_str();
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

		return mono_string_new(mono_domain_get(), Eagle::Utils::AsString(asset->GetPath()).c_str());
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

	void Script::Eagle_AssetTexture2D_SetFormat(GUID id, AssetTexture2DFormat value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set AssetTexture2D format. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set AssetTexture2D format. It's not a texture 2D asset");
			return;
		}

		if (textureAsset->IsCompressed())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set AssetTexture2D format. Compressed textures only support RGBA8 format. Current format: {}", Eagle::Utils::GetEnumName(textureAsset->GetFormat()));
			return;
		}
		textureAsset->SetFormat(value);
	}

	void Script::Eagle_AssetTexture2D_SetIsNormalMap(GUID id, bool value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsNormalMap` for AssetTexture2D. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsNormalMap` for AssetTexture2D. It's not a texture 2D asset");
			return;
		}

		textureAsset->SetIsNormalMap(value);
	}

	void Script::Eagle_AssetTexture2D_SetCompression(GUID id, TextureCompressor::Quality quality)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsCompressed` for AssetTexture2D. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsCompressed` for AssetTexture2D. It's not a texture 2D asset");
			return;
		}

		textureAsset->SetCompression(quality, textureAsset->GetTexture()->GetMipsCount());
	}

	AssetTexture2DFormat Script::Eagle_AssetTexture2D_GetFormat(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` of AssetTexture2D. Couldn't find an asset");
			return AssetTexture2DFormat::Default;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` of AssetTexture2D. It's not a texture 2D asset");
			return AssetTexture2DFormat::Default;
		}

		return textureAsset->GetFormat();
	}

	bool Script::Eagle_AssetTexture2D_IsNormalMap(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsNormalMap` of AssetTexture2D. Couldn't find an asset");
			return false;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsNormalMap` of AssetTexture2D. It's not a texture 2D asset");
			return false;
		}

		return textureAsset->IsNormalMap();
	}

	TextureCompressor::Quality Script::Eagle_AssetTexture2D_GetCompressionQuality(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCompressionQuality` of AssetTexture2D. Couldn't find an asset");
			return TextureCompressor::Quality::Disabled;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCompressionQuality` of AssetTexture2D. It's not a texture 2D asset");
			return TextureCompressor::Quality::Disabled;
		}

		return textureAsset->GetCompressionQuality();
	}

	//--------------AssetTextureCube--------------
	void Script::Eagle_AssetTextureCube_SetLayerSize(GUID id, uint32_t value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetLayerSize` for AssetTextureCube. Couldn't find an asset");
			return;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetLayerSize` for AssetTextureCube. It's not a texture cube asset");
			return;
		}

		textureAsset->SetLayerSize(value);
	}

	void Script::Eagle_AssetTextureCube_SetPrefilterSize(GUID id, uint32_t value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPrefilterSize` for AssetTextureCube. Couldn't find an asset");
			return;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetPrefilterSize` for AssetTextureCube. It's not a texture cube asset");
			return;
		}

		textureAsset->SetPrefilterSize(value);
	}

	bool Script::Eagle_AssetTextureCube_SetFormat(GUID id, AssetTextureCubeFormat value)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFormat` for AssetTextureCube. Couldn't find an asset");
			return false;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetFormat` for AssetTextureCube. It's not a texture cube asset");
			return false;
		}

		return textureAsset->SetFormat(value);
	}

	bool Script::Eagle_AssetTextureCube_SetCompressed(GUID id, bool bCompress)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCompressed` for AssetTextureCube. Couldn't find an asset");
			return false;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCompressed` for AssetTextureCube. It's not a texture cube asset");
			return false;
		}

		return textureAsset->SetCompressed(bCompress);
	}

	uint32_t Script::Eagle_AssetTextureCube_GetLayerSize(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLayerSize` for AssetTextureCube. Couldn't find an asset");
			return 0u;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLayerSize` for AssetTextureCube. It's not a texture cube asset");
			return 0u;
		}

		return textureAsset->GetTexture()->GetSize().x;
	}

	uint32_t Script::Eagle_AssetTextureCube_GetPrefilterSize(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPrefilterSize` for AssetTextureCube. Couldn't find an asset");
			return 0u;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetPrefilterSize` for AssetTextureCube. It's not a texture cube asset");
			return 0u;
		}

		return textureAsset->GetTexture()->GetPrefilterSize();
	}

	AssetTextureCubeFormat Script::Eagle_AssetTextureCube_GetFormat(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` for AssetTextureCube. Couldn't find an asset");
			return AssetTextureCubeFormat::Default;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetFormat` for AssetTextureCube. It's not a texture cube asset");
			return AssetTextureCubeFormat::Default;
		}

		return textureAsset->GetFormat();
	}

	bool Script::Eagle_AssetTextureCube_IsCompressed(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsCompressed` for AssetTextureCube. Couldn't find an asset");
			return false;
		}

		Ref<AssetTextureCube> textureAsset = Cast<AssetTextureCube>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsCompressed` for AssetTextureCube. It's not a texture cube asset");
			return false;
		}

		return textureAsset->IsCompressed();
	}

	//--------------AssetMaterial--------------
	GUID Script::Eagle_AssetMaterial_Create()
	{
		Ref<Material> material = Material::Create();
		Ref<AssetMaterial> materialAsset = AssetMaterial::Create(material);
		AssetManager::AddRuntimeAsset(materialAsset);
		return materialAsset->GetGUID();
	}

	GUID Script::Eagle_AssetMaterial_CreateFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create material asset. Couldn't find an asset");
			return GUID(0, 0);
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
		{
			Ref<Material> material = Material::Create(materialAsset->GetMaterial());
			Ref<AssetMaterial> newAsset = AssetMaterial::Create(material);
			AssetManager::AddRuntimeAsset(newAsset);
			return newAsset->GetGUID();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't create material asset. It's not a material asset");
		return GUID(0, 0);
	}

	void Script::Eagle_AssetMaterial_GetMaterial(GUID assetID,
		GUID* outAlbedoTexture, GUID* outMetalnessTexture, GUID* outNormalTexture, GUID* outRoughnessTexture, GUID* outAOTexture, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec3* albedo, float* metalness, float* roughness, float* ao, glm::vec3* emissive, float* opacity, float* opacityMask,
		bool* bUseAlbedoTexture, bool* bUseMetalnessTexture, bool* bUseRoughnessTexture, bool* bUseAOTexture, bool* bUseEmissiveTexture, bool* bUseOpacityTexture, bool* bUseOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, MaterialBlendMode* outBlendMode, bool* bDoubleSided,
		Material::TextureChannel* outMetalnessTextureChannel, Material::TextureChannel* outRoughnessTextureChannel, Material::TextureChannel* outAOTextureChannel,
		Material::TextureChannel* outOpacityTextureChannel, Material::TextureChannel* outOpacityMaskTextureChannel)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material. Couldn't find an asset");
			return;
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
		{
			Utils::GetMaterial(materialAsset->GetMaterial(),
				outAlbedoTexture, outMetalnessTexture, outNormalTexture, outRoughnessTexture, outAOTexture, outEmissiveTexture, outOpacityTexture, outOpacityMaskTexture,
				albedo, metalness, roughness, ao, emissive, opacity, opacityMask,
				bUseAlbedoTexture, bUseMetalnessTexture, bUseRoughnessTexture, bUseAOTexture, bUseEmissiveTexture, bUseOpacityTexture, bUseOpacityMaskTexture,
				outTint, outEmissiveIntensity, outTilingFactor, outBlendMode, bDoubleSided, outMetalnessTextureChannel, outRoughnessTextureChannel, outAOTextureChannel, outOpacityTextureChannel, outOpacityMaskTextureChannel);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material. It's not a material asset");
	}

	void Script::Eagle_AssetMaterial_SetMaterial(GUID assetID,
		GUID albedoTexture, GUID metalnessTexture, GUID normalTexture, GUID roughnessTexture, GUID aoTexture, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec3* albedo, float metalness, float roughness, float ao, const glm::vec3* emissive, float opacity, float opacityMask,
		bool bUseAlbedoTexture, bool bUseMetalnessTexture, bool bUseRoughnessTexture, bool bUseAOTexture, bool bUseEmissiveTexture, bool bUseOpacityTexture, bool bUseOpacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, MaterialBlendMode blendMode, bool bDoubleSided,
		Material::TextureChannel metalnessTextureChannel, Material::TextureChannel roughnessTextureChannel, Material::TextureChannel aoTextureChannel,
		Material::TextureChannel opacityTextureChannel, Material::TextureChannel opacityMaskTextureChannel)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set material. Couldn't find an asset");
			return;
		}

		if (Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset))
		{
			Utils::SetMaterial(materialAsset->GetMaterial(),
				albedoTexture, metalnessTexture, normalTexture, roughnessTexture, aoTexture, emissiveTexture, opacityTexture, opacityMaskTexture,
				albedo, metalness, roughness, ao, emissive, opacity, opacityMask,
				bUseAlbedoTexture, bUseMetalnessTexture, bUseRoughnessTexture, bUseAOTexture, bUseEmissiveTexture, bUseOpacityTexture, bUseOpacityMaskTexture,
				tint, emissiveIntensity, tilingFactor, blendMode, bDoubleSided, metalnessTextureChannel, roughnessTextureChannel, aoTextureChannel, opacityTextureChannel, opacityMaskTextureChannel);
		}
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

	void Script::Eagle_AssetAudio_SetPitch(GUID assetID, float pitch)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pitch. Couldn't find an asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			audioAsset->GetAudio()->SetPitch(pitch);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pitch. It's not an audio asset");
	}

	float Script::Eagle_AssetAudio_GetPitch(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pitch. Couldn't find an asset");
			return 1.f;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			return audioAsset->GetAudio()->GetPitch();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pitch. It's not an audio asset");
		return 1.f;
	}

	void Script::Eagle_AssetAudio_SetPan(GUID assetID, float pan)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pan. Couldn't find an asset");
			return;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			audioAsset->GetAudio()->SetPan(pan);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set asset pan. It's not an audio asset");
	}

	float Script::Eagle_AssetAudio_GetPan(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pan. Couldn't find an asset");
			return 0.f;
		}

		if (Ref<AssetAudio> audioAsset = Cast<AssetAudio>(asset))
			return audioAsset->GetAudio()->GetPan();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset pan. It's not an audio asset");
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

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			materialAsset->GetMaterial()->SetDynamicFriction(value);
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

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			materialAsset->GetMaterial()->SetBounciness(value);
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

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			materialAsset->GetMaterial()->SetStaticFriction(value);
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
			return material->GetMaterial()->GetStaticFriction();

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
			return material->GetMaterial()->GetDynamicFriction();

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
			return material->GetMaterial()->GetBounciness();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get asset bounciness. It's not a PhysicsMaterial asset");
		return 0.f;
	}

	GUID Script::Eagle_AssetPhysicsMaterial_Create(float staticFriction, float dynamicFriction, float bounciness)
	{
		Ref<PhysicsMaterial> material = PhysicsMaterial::Create(staticFriction, dynamicFriction, bounciness);
		Ref<AssetPhysicsMaterial> asset = AssetPhysicsMaterial::Create(material);
		AssetManager::AddRuntimeAsset(asset);
		return asset->GetGUID();
	}

	GUID Script::Eagle_AssetPhysicsMaterial_CreateFromAsset(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create physics material asset. Couldn't find an asset");
			return GUID(0, 0);
		}

		if (Ref<AssetPhysicsMaterial> materialAsset = Cast<AssetPhysicsMaterial>(asset))
		{
			const auto& material = materialAsset->GetMaterial();
			Ref<PhysicsMaterial> newMaterial = PhysicsMaterial::Create(material->GetStaticFriction(), material->GetDynamicFriction(), material->GetBounciness());
			Ref<AssetPhysicsMaterial> newAsset = AssetPhysicsMaterial::Create(newMaterial);
			AssetManager::AddRuntimeAsset(newAsset);
			return newAsset->GetGUID();
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't create physics material asset. It's not a PhysicsMaterial asset");
		return GUID(0, 0);
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

	//--------------AssetAnimation--------------
	void Script::Eagle_AssetAnimation_SetRootMotionMode(GUID id, RootMotionMode mode)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRootMotionEnabled` for AssetAnimation. Couldn't find an asset");
			return;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetRootMotionEnabled` for AssetAnimation. It's not an animation asset");
			return;
		}

		const auto& skeletalInfo = animationAsset->GetSkeletal()->GetMesh()->GetSkeletalMeshInfo();
		if (mode != RootMotionMode::Disabled)
			animationAsset->GetAnimation()->ExtractRootMotion(skeletalInfo, mode);
		else
			animationAsset->GetAnimation()->RemoveRootMotion(skeletalInfo);
	}

	float Script::Eagle_AssetAnimation_GetDuration(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetDuration` for AssetAnimation. Couldn't find an asset");
			return 0.f;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetDuration` for AssetAnimation. It's not an animation asset");
			return 0.f;
		}

		return animationAsset->GetAnimation()->Duration;
	}

	float Script::Eagle_AssetAnimation_GetTicksPerSecond(GUID id)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetTicksPerSecond` for AssetAnimation. Couldn't find an asset");
			return 0.f;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetTicksPerSecond` for AssetAnimation. It's not an animation asset");
			return 0.f;
		}

		return animationAsset->GetAnimation()->TicksPerSecond;
	}

	void Script::Eagle_AssetAnimation_AddAnimationEvent(GUID id, MonoString* name, float time)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `AddAnimationEvent` for AssetAnimation. Couldn't find an asset");
			return;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `AddAnimationEvent` for AssetAnimation. It's not an animation asset");
			return;
		}

		auto& event = animationAsset->GetAnimation()->Events.emplace_back();
		event.Name = MonoStringHandler(name).c_str();
		event.Time = time;
	}

	bool Script::Eagle_AssetAnimation_RemoveAnimationEvent(GUID id, MonoString* monoName)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveAnimationEvent` for AssetAnimation. Couldn't find an asset");
			return false;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveAnimationEvent` for AssetAnimation. It's not an animation asset");
			return false;
		}

		const std::string name = MonoStringHandler(monoName).c_str();

		auto& events = animationAsset->GetAnimation()->Events;
		auto it = std::find_if(events.begin(), events.end(), [&name](const AnimationEvent& a) { return a.Name == name; });
		if (it != events.end())
		{
			events.erase(it);
			return true;
		}

		return false;
	}

	bool Script::Eagle_AssetAnimation_HasAnimationEvent(GUID id, MonoString* monoName)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(id, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `HasAnimationEvent` for AssetAnimation. Couldn't find an asset");
			return false;
		}

		Ref<AssetAnimation> animationAsset = Cast<AssetAnimation>(asset);
		if (!animationAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `HasAnimationEvent` for AssetAnimation. It's not an animation asset");
			return false;
		}

		const std::string name = MonoStringHandler(monoName).c_str();

		const auto& events = animationAsset->GetAnimation()->Events;
		auto it = std::find_if(events.begin(), events.end(), [&name](const AnimationEvent& a) { return a.Name == name; });
		return it != events.end();
	}

	//--------------AssetParticleSystem--------------
	uint32_t Script::Eagle_AssetParticleSystem_GetEmittersCount(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmittersCount()`. Couldn't find a ParticleSystem asset");
			return 0u;
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
			return uint32_t(ps->GetEmitters().size());

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmittersCount()`. It's not a ParticleSystem asset");
		return 0u;
	}

	void Script::Eagle_AssetParticleSystem_RemoveEmitters(GUID assetID)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveEmitters()`. Couldn't find a ParticleSystem asset");
			return;
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
			return ps->SetEmitters({});

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `RemoveEmitters()`. It's not a ParticleSystem asset");
		return;
	}

	void* Script::Eagle_AssetParticleSystem_SetEmitters_Prepare(uint32_t count)
	{
		std::vector<ParticleEmitter>* emitters = new std::vector<ParticleEmitter>(count);
		return emitters;
	}

	void Script::Eagle_AssetParticleSystem_SetEmitters_Finish(GUID assetID, void* data)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			delete data;
			return;
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
		{
			std::vector<ParticleEmitter>* emitters = (std::vector<ParticleEmitter>*)data;
			ps->SetEmitters(std::move(*emitters));
		}

		delete data;
	}

	GUID Script::Eagle_AssetParticleSystem_Create()
	{
		Ref<AssetParticleSystem> asset = AssetParticleSystem::Create();
		AssetManager::AddRuntimeAsset(asset);
		return asset->GetGUID();
	}

	void Script::Eagle_AssetParticleSystem_SetEmitter(void* data, uint32_t index, GUID texture, const glm::vec4* colorStart, const glm::vec4* colorEnd,
		const glm::vec3* velocityMin, const glm::vec3* velocityMax, const glm::vec3* velocityCoefStart, const glm::vec3* velocityCoefEnd,
		float rotationZStart, float rotationZEnd, const glm::vec2* sizeStart, const glm::vec2* sizeEnd, const glm::vec2* colliderSizeRatio,
		float lifetimeMin, float lifetimeMax, float bouncinessMin, float bouncinessMax, MonoString* name, const Transform* relativeTransform,
		const AABB* visibilityAABB, uint32_t loopCount, float loopDuration, uint32_t spawnRate, float radialAcceleration, float tangentialAcceleration,
		float normalVelocityFactor, ParticleEmitter::EmissionShapeType emissionShape, const glm::vec3* sphereRadius, const glm::vec3* boxMin, const glm::vec3* boxMax,
		const glm::vec3* ringRadius, const glm::vec3* ringThickness, GUID mesh, ParticleEmitter::CollisionModeType collisionMode, const glm::uvec2* animationImagesNum,
		float animationSpeed, bool bDestroyImmediately, bool bEmit, bool bExplode, bool bApplyGravity, bool bAlphaBlending, bool bAdditive, bool bBlendAnimation, bool bFaceDirection)
	{
		std::vector<ParticleEmitter>* emitters = (std::vector<ParticleEmitter>*)data;
		EG_CORE_ASSERT(emitters->size() >= index);

		ParticleEmitter& emitter = (*emitters)[index];
		emitter.ColorStart = *colorStart;
		emitter.ColorEnd = *colorEnd;

		emitter.VelocityMin = *velocityMin;
		emitter.VelocityMax = *velocityMax;
		emitter.VelocityCoefStart = *velocityCoefStart;
		emitter.VelocityCoefEnd = *velocityCoefEnd;

		emitter.RotationZStart = rotationZStart;
		emitter.RotationZEnd = rotationZEnd;
		emitter.SizeStart = *sizeStart;
		emitter.SizeEnd = *sizeEnd;
		emitter.ColliderSizeRatio = *colliderSizeRatio;

		emitter.LifetimeMin = lifetimeMin;
		emitter.LifetimeMax = lifetimeMax;
		emitter.BouncinessMin = bouncinessMin;
		emitter.BouncinessMax = bouncinessMax;
		emitter.Name = MonoStringHandler(name).c_str();
		emitter.RelativeTransform = *relativeTransform;

		emitter.VisibilityAABB = *visibilityAABB;
		emitter.LoopCount = loopCount;
		emitter.LoopDuration = loopDuration;
		emitter.SpawnRate = spawnRate;
		emitter.RadialAcceleration = radialAcceleration;
		emitter.TangentialAcceleration = tangentialAcceleration;

		emitter.NormalVelocityFactor = normalVelocityFactor;
		emitter.EmissionShape = emissionShape;
		emitter.SphereRadius = *sphereRadius;
		emitter.BoxMin = *boxMin;
		emitter.BoxMax = *boxMax;

		emitter.RingRadius = *ringRadius;
		emitter.RingThickness = *ringThickness;
		emitter.CollisionMode = collisionMode;
		emitter.AnimationImagesNum = *animationImagesNum;
		
		emitter.AnimationSpeed = animationSpeed;
		emitter.bDestroyImmediately = bDestroyImmediately;
		emitter.bEmit = bEmit;
		emitter.bExplode = bExplode;
		emitter.bApplyGravity = bApplyGravity;
		emitter.bAlphaBlending = bAlphaBlending;
		emitter.bAdditive = bAdditive;
		emitter.bBlendAnimation = bBlendAnimation;
		emitter.bFaceDirection = bFaceDirection;

		if (texture.IsNull())
		{
			emitter.Texture.reset();
		}
		else
		{
			Ref<Asset> asset;
			if (AssetManager::Get(texture, &asset))
			{
				Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
				if (textureAsset)
				{
					emitter.Texture = std::move(textureAsset);
				}
				else
				{
					emitter.Texture.reset();
					EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Texture at index {}. Provided asset is not a Texture2D asset", index);
				}
			}
			else
			{
				emitter.Texture.reset();
				EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Texture at index {}. Couldn't find an asset", index);
			}
		}
		
		if (mesh.IsNull())
		{
			emitter.MeshAsset.reset();
		}
		else
		{
			Ref<Asset> asset;
			if (AssetManager::Get(mesh, &asset))
			{
				Ref<AssetStaticMesh> meshAsset = Cast<AssetStaticMesh>(asset);
				if (meshAsset)
				{
					emitter.MeshAsset = std::move(meshAsset);
				}
				else
				{
					emitter.MeshAsset.reset();
					EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Mesh at index {}. Provided asset is not a Static Mesh asset", index);
				}
			}
			else
			{
				emitter.MeshAsset.reset();
				EG_CORE_ERROR("[ScriptEngine] Couldn't set Emitter Mesh at index {}. Couldn't find an asset", index);
			}
		}
	}

	MonoString* Script::Eagle_AssetParticleSystem_GetEmitter(GUID assetID, uint32_t index, GUID* texture, glm::vec4* colorStart, glm::vec4* colorEnd,
		glm::vec3* velocityMin, glm::vec3* velocityMax, glm::vec3* velocityCoefStart, glm::vec3* velocityCoefEnd, float* rotationZStart, float* rotationZEnd,
		glm::vec2* sizeStart, glm::vec2* sizeEnd, glm::vec2* colliderSizeRatio, float* lifetimeMin, float* lifetimeMax, float* bouncinessMin, float* bouncinessMax,
		Transform* relativeTransform, AABB* visibilityAABB, uint32_t* loopCount, float* loopDuration, uint32_t* spawnRate, float* radialAcceleration,
		float* tangentialAcceleration, float* normalVelocityFactor, ParticleEmitter::EmissionShapeType* emissionShape, glm::vec3* sphereRadius, glm::vec3* boxMin,
		glm::vec3* boxMax, glm::vec3* ringRadius, glm::vec3* ringThickness, GUID* meshAsset, ParticleEmitter::CollisionModeType* collisionMode, glm::uvec2* animationImagesNum,
		float* animationSpeed, bool* bDestroyImmediately, bool* bEmit, bool* bExplode, bool* bApplyGravity, bool* bAlphaBlending, bool* bAdditive, bool* bBlendAnimation, bool* bFaceDirection)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmitter()`. Couldn't find a ParticleSystem asset");
			return mono_string_new(mono_domain_get(), "");
		}

		if (Ref<AssetParticleSystem> ps = Cast<AssetParticleSystem>(asset))
		{
			const auto& emitters = ps->GetEmitters();
			if (index >= uint32_t(emitters.size()))
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get Emitter at index {}. Size: {}", index, emitters.size());
				return mono_string_new(mono_domain_get(), "");
			}

			const auto& emitter = emitters[index];
			*texture = emitter.Texture ? emitter.Texture->GetGUID() : GUID(0, 0);
			*colorStart = emitter.ColorStart;
			*colorEnd = emitter.ColorEnd;

			*velocityMin = emitter.VelocityMin;
			*velocityMax = emitter.VelocityMax;
			*velocityCoefStart = emitter.VelocityCoefStart;
			*velocityCoefEnd = emitter.VelocityCoefEnd;
			*rotationZStart = emitter.RotationZStart;
			*rotationZEnd = emitter.RotationZEnd;

			*sizeStart = emitter.SizeStart;
			*sizeEnd = emitter.SizeEnd;
			*colliderSizeRatio = emitter.ColliderSizeRatio;
			*lifetimeMin = emitter.LifetimeMin;
			*lifetimeMax = emitter.LifetimeMax;
			*bouncinessMin = emitter.BouncinessMin;
			*bouncinessMax = emitter.BouncinessMax;

			*relativeTransform = emitter.RelativeTransform;
			*visibilityAABB = emitter.VisibilityAABB;
			*loopCount = emitter.LoopCount;
			*loopDuration = emitter.LoopDuration;
			*spawnRate = emitter.SpawnRate;
			*radialAcceleration = emitter.RadialAcceleration;

			*tangentialAcceleration = emitter.TangentialAcceleration;
			*normalVelocityFactor = emitter.NormalVelocityFactor;
			*emissionShape = emitter.EmissionShape;
			*sphereRadius = emitter.SphereRadius;
			*boxMin = emitter.BoxMin;

			*boxMax = emitter.BoxMax;
			*ringRadius = emitter.RingRadius;
			*ringThickness = emitter.RingThickness;
			*meshAsset = emitter.MeshAsset ? emitter.MeshAsset->GetGUID() : GUID(0, 0);
			*collisionMode = emitter.CollisionMode;
			*animationImagesNum = emitter.AnimationImagesNum;

			*animationSpeed = emitter.AnimationSpeed;
			*bDestroyImmediately = emitter.bDestroyImmediately;
			*bEmit = emitter.bEmit;
			*bExplode = emitter.bExplode;
			*bApplyGravity = emitter.bApplyGravity;
			*bAlphaBlending = emitter.bAlphaBlending;
			*bAdditive = emitter.bAdditive;
			*bBlendAnimation = emitter.bBlendAnimation;
			*bFaceDirection = emitter.bFaceDirection;

			return mono_string_new(mono_domain_get(), emitter.Name.c_str());
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetEmitter()`. It's not a ParticleSystem asset");
		return mono_string_new(mono_domain_get(), "");
	}

	void Script::Eagle_AssetBehaviorGraph_CreateTaskManager(GUID assetID, MonoObject** outTaskManager)
	{
		Ref<Asset> asset;
		AssetManager::Get(assetID, &asset);
		if (!asset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `CreateTaskManager()`. Couldn't find a AssetBehaviorGraph asset");
			*outTaskManager = nullptr;
			return;
		}

		if (Ref<AssetBehaviorGraph> bg = Cast<AssetBehaviorGraph>(asset))
		{
			Scope<MonoInstance> taskManager = ScriptEngine::InstantiateBehaviorGraph(bg->GetRoot());
			*outTaskManager = taskManager->GetInstance();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `CreateTaskManager()`. It's not an AssetBehaviorGraph asset");
			*outTaskManager = nullptr;
		}
	}
	
	//--------------Math--------------
	glm::vec3 Script::Eagle_Math_GetForwardVector(const Rotator* rotator)
	{
		return Math::GetForwardVector(*rotator);
	}

	glm::vec3 Script::Eagle_Math_GetUpVector(const Rotator* rotator)
	{
		return Math::GetUpVector(*rotator);
	}

	glm::vec3 Script::Eagle_Math_GetRightVector(const Rotator* rotator)
	{
		return Math::GetRightVector(*rotator);
	}

	float Script::Eagle_Math_CalculateDirection(const glm::vec3* velocity, const Rotator* rotator)
	{
		return Math::CalculateDirection(*velocity, *rotator);
	}

	glm::quat Script::Eagle_Math_SlerpQuat(const glm::quat* x, const glm::quat* y, float alpha)
	{
		return glm::slerp(*x, *y, alpha);
	}

	glm::quat Script::Eagle_Math_LookAt(const glm::vec3* dir)
	{
		constexpr glm::vec3 up = glm::vec3(0, 1, 0);
		constexpr glm::vec3 right = glm::vec3(1, 0, 0);

		// If dir is nearly parallel to up, pick a different up vector
		const glm::vec3 axis = glm::abs(glm::dot(*dir, up)) > 0.999f ? right : up;
		return glm::quatLookAt(*dir, axis);
	}

	glm::quat Script::Eagle_Math_LookAtY(const glm::vec3* dir)
	{

		glm::vec3 dirTemp = *dir;
		dirTemp.y = 0.0f;

		const float len2 = glm::length2(dirTemp);
		// Handle degenerate case: if dir is zero (target is directly above/below)
		if (len2 < 1e-6f)
			return Rotator::Unit().GetQuat();

		dirTemp = dirTemp * glm::inversesqrt(len2); // Normalize

		constexpr glm::vec3 forward(0, 0, -1);
		return glm::rotation(forward, dirTemp);
	}

	glm::quat Script::Eagle_Math_UpAt(const glm::vec3* dir)
	{
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		return glm::rotation(up, *dir);
	}

	glm::vec3 Script::Eagle_Math_GetDirectionToPixel(const glm::vec2* pixelCoord)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& renderer = scene->GetSceneRenderer();
		const glm::vec2 size = renderer->GetViewportSize();

		const glm::vec2 uv = glm::clamp(*pixelCoord / size, glm::vec2(0), glm::vec2(1));
		const glm::vec3 ndc = glm::vec3(uv * 2.f - 1.f, 0.f);
		const CameraComponent* camera = scene->GetRuntimeCamera();

		glm::vec4 worldPos = glm::inverse(camera->GetViewProjection()) * glm::vec4(ndc, 1.f);
		worldPos /= worldPos.w;
		return glm::normalize(glm::vec3(worldPos) - camera->GetWorldTransform().Location);
	}

	glm::quat Script::Eagle_Quat_Mul(const glm::quat& left, const glm::quat& right)
	{
		return left * right;
	}
	
	glm::vec3 Script::Eagle_Quat_EulerAngles(const glm::quat* q)
	{
		return glm::eulerAngles(*q);
	}
	
	glm::quat Script::Eagle_Quat_FromEulerAngles(const glm::vec3* rads)
	{
		return Rotator::FromEulerAngles(*rads).GetQuat();
	}
}
