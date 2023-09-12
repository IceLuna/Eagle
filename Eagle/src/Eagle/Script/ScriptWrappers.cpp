#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Core/Project.h"

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
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetStaticFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetDynamicFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetBouncinessFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetStaticFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetDynamicFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetBouncinessFunctions;

	extern MonoImage* s_AppAssemblyImage;
}

namespace Eagle::Script::Utils
{
	static void SetMaterial(const Ref<Material>& material, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode)
	{
		// Update textures
		{
			// Albedo
			{
				Ref<Texture> texture;

				// Check if it's a default texture
				TextureLibrary::GetDefault(albedo, &texture);
				if (!texture)
					TextureLibrary::Get(albedo, &texture);
				material->SetAlbedoTexture(Cast<Texture2D>(texture));
			}
			// Metallnes
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(metallness, &texture);
				if (!texture)
					TextureLibrary::Get(metallness, &texture);
				material->SetMetallnessTexture(Cast<Texture2D>(texture));
			}
			// Normal
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(normal, &texture);
				if (!texture)
					TextureLibrary::Get(normal, &texture);
				material->SetNormalTexture(Cast<Texture2D>(texture));
			}
			// Roughness
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(roughness, &texture);
				if (!texture)
					TextureLibrary::Get(roughness, &texture);
				material->SetRoughnessTexture(Cast<Texture2D>(texture));
			}
			// AO
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(ao, &texture);
				if (!texture)
					TextureLibrary::Get(ao, &texture);
				material->SetAOTexture(Cast<Texture2D>(texture));
			}
			// Emissive
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(emissiveTexture, &texture);
				if (!texture)
					TextureLibrary::Get(emissiveTexture, &texture);
				material->SetEmissiveTexture(Cast<Texture2D>(texture));
			}
			// Opacity
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(opacityTexture, &texture);
				if (!texture)
					TextureLibrary::Get(opacityTexture, &texture);
				material->SetOpacityTexture(Cast<Texture2D>(texture));
			}
			// Opacity Mask
			{
				Ref<Texture> texture;
				TextureLibrary::GetDefault(opacityMaskTexture, &texture);
				if (!texture)
					TextureLibrary::Get(opacityMaskTexture, &texture);
				material->SetOpacityMaskTexture(Cast<Texture2D>(texture));
			}
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
				const auto& texture = material->GetAlbedoTexture();
				*outAlbedo = texture ? texture->GetGUID() : null;
			}
			// Metallness
			{
				const auto& texture = material->GetMetallnessTexture();
				*outMetallness = texture ? texture->GetGUID() : null;
			}
			// Normal
			{
				const auto& texture = material->GetNormalTexture();
				*outNormal = texture ? texture->GetGUID() : null;
			}
			// Roughness
			{
				const auto& texture = material->GetRoughnessTexture();
				*outRoughness = texture ? texture->GetGUID() : null;
			}
			// AO
			{
				const auto& texture = material->GetAOTexture();
				*outAO = texture ? texture->GetGUID() : null;
			}
			// Emissive
			{
				const auto& texture = material->GetEmissiveTexture();
				*outEmissiveTexture = texture ? texture->GetGUID() : null;
			}
			// Opacity
			{
				const auto& texture = material->GetOpacityTexture();
				*outOpacityTexture = texture ? texture->GetGUID() : null;
			}
			// Opacity Mask
			{
				const auto& texture = material->GetOpacityMaskTexture();
				*outOpacityMaskTexture = texture ? texture->GetGUID() : null;
			}
		}
		*outTint = material->GetTintColor();
		*outEmissiveIntensity = material->GetEmissiveIntensity();
		*outTilingFactor = material->GetTilingFactor();
		*outBlendMode = material->GetBlendMode();
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

			MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetCoreClass("Eagle", "Entity"), children.size());

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

	GUID Script::Eagle_Entity_CreateEntity()
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->CreateEntity();
		return entity.GetGUID();
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
			return m_HasComponentFunctions[monoType](entity);
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
		{
			*result = entity.GetForwardVector();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get forward vector of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_GetRightVector(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*result = entity.GetRightVector();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get right vector of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_GetUpVector(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*result = entity.GetUpVector();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get up vector of Entity. Entity is null");
			return;
		}
	}

	GUID Script::Eagle_Entity_GetChildrenByName(GUID entityID, MonoString* monoName)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		const auto& children = entity.GetChildren();
		if (children.empty())
			return { 0, 0 };

		const std::string name = mono_string_to_utf8(monoName);

		for (auto& child : children)
			if (child.GetSceneName() == name)
				return child.GetGUID();

		return { 0, 0 };
	}

	//Entity-Physics
	void Script::Eagle_Entity_WakeUp(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->WakeUp();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't wake up Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_PutToSleep(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->PutToSleep();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't put to sleep Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_Entity_GetMass(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->GetMass();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get mass of Entity. Entity is not a physics actor");
				return 0.f;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get mass of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_Entity_SetMass(GUID entityID, float mass)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->SetMass(mass);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set mass of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mass of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_AddForce(GUID entityID, const glm::vec3* force, ForceMode forceMode)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddForce(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add force to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_AddTorque(GUID entityID, const glm::vec3* force, ForceMode forceMode)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->AddTorque(*force, forceMode);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't add torque to Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_GetLinearVelocity(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				*result = physicsActor->GetLinearVelocity();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get linear velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_SetLinearVelocity(GUID entityID, const glm::vec3* velocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
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

	void Script::Eagle_Entity_GetAngularVelocity(GUID entityID, glm::vec3* result)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				*result = physicsActor->GetAngularVelocity();
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get angular velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_SetAngularVelocity(GUID entityID, const glm::vec3* velocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetAngularVelocity(*velocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular velocity of Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_Entity_GetMaxLinearVelocity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->GetMaxLinearVelocity();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get max linear velocity of Entity. Entity is not a physics actor");
				return 0.f;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max linear velocity of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_Entity_SetMaxLinearVelocity(GUID entityID, float maxVelocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetMaxLinearVelocity(maxVelocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set max linear velocity of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max linear velocity of Entity. Entity is null");
			return;
		}
	}

	float Script::Eagle_Entity_GetMaxAngularVelocity(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->GetMaxAngularVelocity();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get max angular velocity of Entity. Entity is not a physics actor");
				return 0.f;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max angular velocity of Entity. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_Entity_SetMaxAngularVelocity(GUID entityID, float maxVelocity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetMaxAngularVelocity(maxVelocity);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set max angular velocity of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max angular velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_SetLinearDamping(GUID entityID, float damping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetLinearDamping(damping);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set linear damping of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear damping velocity of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_SetAngularDamping(GUID entityID, float damping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetAngularDamping(damping);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't set angular damping of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular damping velocity of Entity. Entity is null");
			return;
		}
	}

	bool Script::Eagle_Entity_IsDynamic(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->IsDynamic();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get IsDynamic of Entity. Entity is not a physics actor");
				return false;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get IsDynamic of Entity. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_Entity_IsKinematic(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->IsKinematic();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get IsKinematic of Entity. Entity is not a physics actor");
				return false;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get IsKinematic of Entity. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_Entity_IsGravityEnabled(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->IsGravityEnabled();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get IsGravityEnabled of Entity. Entity is not a physics actor");
				return false;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get IsGravityEnabled of Entity. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_Entity_IsLockFlagSet(GUID entityID, ActorLockFlag flag)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->IsLockFlagSet(flag);
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get IsLockFlagSet of Entity. Entity is not a physics actor");
				return false;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get IsLockFlagSet of Entity. Entity is null");
			return false;
		}
	}

	ActorLockFlag Script::Eagle_Entity_GetLockFlags(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				return physicsActor->GetLockFlags();
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't get GetLockFlags of Entity. Entity is not a physics actor");
				return ActorLockFlag();
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get GetLockFlags of Entity. Entity is null");
			return ActorLockFlag();
		}
	}

	void Script::Eagle_Entity_SetKinematic(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetKinematic(value);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call SetKinematic of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SetKinematic of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_SetGravityEnabled(GUID entityID, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetGravityEnabled(value);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call SetGravityEnabled of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SetGravityEnabled of Entity. Entity is null");
			return;
		}
	}

	void Script::Eagle_Entity_SetLockFlag(GUID entityID, ActorLockFlag flag, bool value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto physicsActor = entity.GetPhysicsActor();
			if (physicsActor)
			{
				physicsActor->SetLockFlag(flag, value);
				return;
			}
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Couldn't call SetLockFlag of Entity. Entity is not a physics actor");
				return;
			}
		}

		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SetLockFlag of Entity. Entity is null");
			return;
		}
	}

	GUID Script::Eagle_Entity_SpawnEntity(MonoString* monoName)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		const std::string name = mono_string_to_utf8(monoName);
		return scene->CreateEntity(name).GetGUID();
	}

	//--------------Transform Component--------------
	void Script::Eagle_TransformComponent_GetWorldTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetWorldTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world transform. Entity is null");
	}

	void Script::Eagle_TransformComponent_GetWorldLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetWorldLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world location. Entity is null");
	}

	void Script::Eagle_TransformComponent_GetWorldRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetWorldRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world rotation. Entity is null");
	}

	void Script::Eagle_TransformComponent_GetWorldScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetWorldScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world scale. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetWorldTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world transform. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetWorldLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world location. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetWorldRotation(GUID entityID, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world rotation. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetWorldScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world scale. Entity is null");
	}
	
	void Script::Eagle_TransformComponent_GetRelativeTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetRelativeTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative transform. Entity is null");
	}

	void Script::Eagle_TransformComponent_GetRelativeLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetRelativeLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative location. Entity is null");
	}

	void Script::Eagle_TransformComponent_GetRelativeRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetRelativeRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative rotation. Entity is null");
	}

	void Script::Eagle_TransformComponent_GetRelativeScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetRelativeScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative scale. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetRelativeTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative transform. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative location. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetRelativeRotation(GUID entityID, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative rotation. Entity is null");
	}

	void Script::Eagle_TransformComponent_SetRelativeScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative scale. Entity is null");
	}

	//Scene Component
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
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bAffectsWorld'. Entity is null");
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

	// DirectionalLightComponent
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

	//--------------Texture--------------
	bool Script::Eagle_Texture_IsValid(GUID guid)
	{
		return TextureLibrary::Exist(guid);
	}

	//--------------Texture2D--------------
	GUID Script::Eagle_Texture2D_Create(MonoString* texturePath)
	{
		Ref<Texture> texture;
		Path path = mono_string_to_utf8(texturePath);
		if (TextureLibrary::Get(path, &texture) == false)
		{
			texture = Texture2D::Create(path);
			if (texture)
				return texture->GetGUID();
			else return {0, 0};
		}
		return texture->GetGUID();
	}

	GUID Script::Eagle_Texture2D_GetBlackTexture()
	{
		return Texture2D::BlackTexture->GetGUID();
	}

	GUID Script::Eagle_Texture2D_GetWhiteTexture()
	{
		return Texture2D::WhiteTexture->GetGUID();
	}

	GUID Script::Eagle_Texture2D_GetGrayTexture()
	{
		return Texture2D::GrayTexture->GetGUID();
	}

	GUID Script::Eagle_Texture2D_GetRedTexture()
	{
		return Texture2D::RedTexture->GetGUID();
	}

	GUID Script::Eagle_Texture2D_GetGreenTexture()
	{
		return Texture2D::GreenTexture->GetGUID();
	}

	GUID Script::Eagle_Texture2D_GetBlueTexture()
	{
		return Texture2D::BlueTexture->GetGUID();
	}

	//--------------Static Mesh--------------
	GUID Script::Eagle_StaticMesh_Create(MonoString* meshPath)
	{
		Ref<StaticMesh> staticMesh;
		char* temp = mono_string_to_utf8(meshPath);
		Path path = temp;
		if (StaticMeshLibrary::Get(path, &staticMesh) == false)
			staticMesh = StaticMesh::Create(path, false, true, false);

		if ((staticMesh && staticMesh->IsValid()) == false)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't create mesh '{0}'", path);
			return {0, 0};
		}
		return staticMesh->GetGUID();
	}

	bool Script::Eagle_StaticMesh_IsValid(GUID guid)
	{
		return StaticMeshLibrary::Exists(guid);
	}

	//--------------StaticMesh Component--------------
	void Script::Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Static Mesh. Entity is null");

		Ref<StaticMesh> staticMesh;
		StaticMeshLibrary::Get(guid, &staticMesh);
		entity.GetComponent<StaticMeshComponent>().SetStaticMesh(staticMesh);
	}

	GUID Script::Eagle_StaticMeshComponent_GetMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Static Mesh. Entity is null");
			return {0, 0};
		}

		const Ref<StaticMesh>& staticMesh = entity.GetComponent<StaticMeshComponent>().GetStaticMesh();
		if (staticMesh)
			return staticMesh->GetGUID();

		return {0, 0};
	}

	void Script::Eagle_StaticMeshComponent_GetMaterial(GUID entityID, GUID* outAlbedo, GUID* outMetallness, GUID* outNormal, GUID* outRoughness, GUID* outAO, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<StaticMeshComponent>();
		Utils::GetMaterial(component.GetMaterial(), outAlbedo, outMetallness, outNormal, outRoughness, outAO, outEmissiveTexture, outOpacityTexture, outOpacityMaskTexture, outTint, outEmissiveIntensity, outTilingFactor, outBlendMode);
	}

	void Script::Eagle_StaticMeshComponent_SetMaterial(GUID entityID, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set static mesh component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<StaticMeshComponent>();
		Utils::SetMaterial(component.GetMaterial(), albedo, metallness, normal, roughness, ao, emissiveTexture, opacityTexture, opacityMaskTexture, tint, emissiveIntensity, tilingFactor, blendMode);
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
	void Script::Eagle_Sound2D_Play(MonoString* audioPath, float volume, int loopCount)
	{
		char* temp = mono_string_to_utf8(audioPath);
		Path path = temp;
		SoundSettings settings;
		settings.Volume = volume;
		settings.LoopCount = loopCount;
		AudioEngine::PlaySound2D(path, settings);
	}

	void Script::Eagle_Sound3D_Play(MonoString* audioPath, const glm::vec3* position, float volume, int loopCount)
	{
		char* temp = mono_string_to_utf8(audioPath);
		Path path = temp;
		SoundSettings settings;
		settings.Volume = volume;
		settings.LoopCount = loopCount;
		AudioEngine::PlaySound3D(path, *position, RollOffModel::Default, settings);
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

	void Script::Eagle_AudioComponent_SetSound(GUID entityID, MonoString* filepath)
	{
		char* temp = mono_string_to_utf8(filepath);
		Path path = temp;

		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetSound(path);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's sound. Entity is null");
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

	//RigidBodyComponent
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
			EG_CORE_ERROR("[ScriptEngine] Couldn't change gravity state. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsGravityEnabled(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsGravityEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get gravity state. Entity is null");
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
			EG_CORE_ERROR("[ScriptEngine] Couldn't change kinematic state. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsKinematic(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().GetAngularDamping();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get kinematic state. Entity is null");
			return false;
		}
	}

	void Script::Eagle_RigidBodyComponent_SetLockPosition(GUID entityID, bool bLockX, bool bLockY, bool bLockZ)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPosition(bLockX, bLockY, bLockZ);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock position. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockPositionX(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPositionX(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock X position. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockPositionY(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPositionY(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Y position. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockPositionZ(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPositionZ(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Z position. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockRotation(GUID entityID, bool bLockX, bool bLockY, bool bLockZ)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotation(bLockX, bLockY, bLockZ);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock rotation. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockRotationX(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotationX(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock X rotation. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockRotationY(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotationY(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Y rotation. Entity is null");
	}

	void Script::Eagle_RigidBodyComponent_SetLockRotationZ(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotationZ(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Z rotation. Entity is null");
	}

	bool Script::Eagle_RigidBodyComponent_IsPositionXLocked(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsPositionXLocked();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsPositionXLocked'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsPositionYLocked(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsPositionYLocked();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsPositionYLocked'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsPositionZLocked(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsPositionZLocked();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsPositionZLocked'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsRotationXLocked(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsRotationXLocked();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsRotationXLocked'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsRotationYLocked(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsRotationYLocked();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsRotationYLocked'. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_RigidBodyComponent_IsRotationZLocked(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<RigidBodyComponent>().IsRotationZLocked();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsRotationZLocked'. Entity is null");
			return false;
		}
	}

	//BaseColliderComponent
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
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetIsTrigger'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_BaseColliderComponent_SetStaticFriction(GUID entityID, void* type, float staticFriction)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetStaticFrictionFunctions[monoType](entity, staticFriction);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetStaticFriction'. Entity is null");
	}

	void Script::Eagle_BaseColliderComponent_SetDynamicFriction(GUID entityID, void* type, float dynamicFriction)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetDynamicFrictionFunctions[monoType](entity, dynamicFriction);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetDynamicFriction'. Entity is null");
	}

	void Script::Eagle_BaseColliderComponent_SetBounciness(GUID entityID, void* type, float bounciness)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetBouncinessFrictionFunctions[monoType](entity, bounciness);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetBounciness'. Entity is null");
	}

	float Script::Eagle_BaseColliderComponent_GetStaticFriction(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetStaticFrictionFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetStaticFriction'. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_BaseColliderComponent_GetDynamicFriction(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetDynamicFrictionFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetDynamicFriction'. Entity is null");
			return 0.f;
		}
	}

	float Script::Eagle_BaseColliderComponent_GetBounciness(GUID entityID, void* type)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			return m_GetBouncinessFunctions[monoType](entity);
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetBounciness'. Entity is null");
			return 0.f;
		}
	}

	//BoxColliderComponent
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
			EG_CORE_ERROR("[ScriptEngine] Couldn't set size of BoxCollider. Entity is null");
			*outSize = glm::vec3{0.f};
		}
	}

	//SphereColliderComponent
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

	void Script::Eagle_MeshColliderComponent_SetCollisionMesh(GUID entityID, GUID meshGUID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Collision Mesh. Entity is null");

		Ref<StaticMesh> staticMesh;
		StaticMeshLibrary::Get(meshGUID, &staticMesh);
		entity.GetComponent<MeshColliderComponent>().SetCollisionMesh(staticMesh);
	}

	GUID Script::Eagle_MeshColliderComponent_GetCollisionMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Collision Mesh. Entity is null");

		const Ref<StaticMesh>& staticMesh = entity.GetComponent<MeshColliderComponent>().GetCollisionMesh();
		if (staticMesh)
			return staticMesh->GetGUID();
		else
			return { 0, 0 };
	}

	// Camera Component
	void Script::Eagle_CameraComponent_SetIsPrimary(GUID entityID, bool val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Primary = val;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'Primary'. Entity is null");
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

	// Reverb Component
	bool Script::Eagle_ReverbComponent_IsActive(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().Reverb->IsActive();
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
			entity.GetComponent<ReverbComponent>().Reverb->SetActive(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetActive'. Entity is null");
	}

	ReverbPreset Script::Eagle_ReverbComponent_GetReverbPreset(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().Reverb->GetPreset();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsActive'. Entity is null");
			return ReverbPreset::Generic;
		}
	}

	void Script::Eagle_ReverbComponent_SetReverbPreset(GUID entityID, ReverbPreset value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().Reverb->SetPreset(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetReverbPreset'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMinDistance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().Reverb->GetMinDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetMinDistance'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_ReverbComponent_SetMinDistance(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().Reverb->SetMinDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetMinDistance'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMaxDistance(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().Reverb->GetMaxDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'GetMaxDistance'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_ReverbComponent_SetMaxDistance(GUID entityID, float value)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().Reverb->SetMaxDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetMaxDistance'. Entity is null");
	}

	// Text Component
	MonoString* Script::Eagle_TextComponent_GetText(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<TextComponent>().GetText().c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get text of Text Component. Entity is null");
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
			EG_CORE_ERROR("[ScriptEngine] Couldn't set text of Text Component. Entity is null");
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

	// Text2D Component
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

	// Billboard component
	void Script::Eagle_BillboardComponent_SetTexture(GUID entityID, GUID textureID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			Ref<Texture> texture;
			TextureLibrary::Get(textureID, &texture);
			entity.GetComponent<BillboardComponent>().Texture = Cast<Texture2D>(texture);
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard texture. Entity is null");
	}

	GUID Script::Eagle_BillboardComponent_GetTexture(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get billboard texture. Entity is null");
			return { 0, 0 };
		}

		auto& billboard = entity.GetComponent<BillboardComponent>();
		if (billboard.Texture)
			return billboard.Texture->GetGUID();
		return { 0, 0 };
	}

	// Sprite Component
	void Script::Eagle_SpriteComponent_GetMaterial(GUID entityID, GUID* outAlbedo, GUID* outMetallness, GUID* outNormal, GUID* outRoughness, GUID* outAO, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, float* outTilingFactor, Material::BlendMode* outBlendMode)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't update material. Entity is null");
			return;
		}

		const GUID null(0, 0);
		auto& sprite = entity.GetComponent<SpriteComponent>();
		Utils::GetMaterial(sprite.GetMaterial(), outAlbedo, outMetallness, outNormal, outRoughness, outAO, outEmissiveTexture, outOpacityTexture, outOpacityMaskTexture, outTint, outEmissiveIntensity, outTilingFactor, outBlendMode);
	}

	void Script::Eagle_SpriteComponent_SetMaterial(GUID entityID, GUID albedo, GUID metallness, GUID normal, GUID roughness, GUID ao, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, float tilingFactor, Material::BlendMode blendMode)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't update material. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		Utils::SetMaterial(sprite.GetMaterial(), albedo, metallness, normal, roughness, ao, emissiveTexture, opacityTexture, opacityMaskTexture, tint, emissiveIntensity, tilingFactor, blendMode);
	}

	void Script::Eagle_SpriteComponent_SetSubtexture(GUID entityID, GUID subtexture)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set subtexture. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		Ref<Texture> texture;
		TextureLibrary::Get(subtexture, &texture);
		if (!texture)
			sprite.SetSubTexture(nullptr);
		else
			sprite.SetSubTexture(SubTexture2D::CreateFromCoords(Cast<Texture2D>(texture), sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef));
	}

	GUID Script::Eagle_SpriteComponent_GetSubtexture(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get subtexture. Entity is null");
			return { 0, 0 };
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		const auto& subtexture = sprite.GetSubTexture();
		if (!subtexture)
			return { 0, 0 };

		if (const auto& atlas = subtexture->GetTexture())
			return atlas->GetGUID();

		return { 0, 0 };
	}

	void Script::Eagle_SpriteComponent_GetSubtextureCoords(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get subtexture coords. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		*outValue = sprite.SubTextureCoords;
	}

	void Script::Eagle_SpriteComponent_SetSubtextureCoords(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set subtexture coords. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		sprite.SubTextureCoords = *value;
		if (const auto& subTexture = sprite.GetSubTexture())
			subTexture->UpdateCoords(sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
	}

	void Script::Eagle_SpriteComponent_GetSpriteSize(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sprite size. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		*outValue = sprite.SpriteSize;
	}

	void Script::Eagle_SpriteComponent_SetSpriteSize(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set subtexture sprite size. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		sprite.SpriteSize = *value;
		if (const auto& subTexture = sprite.GetSubTexture())
			subTexture->UpdateCoords(sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
	}

	void Script::Eagle_SpriteComponent_GetSpriteSizeCoef(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get sprite size coef. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		*outValue = sprite.SpriteSizeCoef;
	}

	void Script::Eagle_SpriteComponent_SetSpriteSizeCoef(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set subtexture sprite size coef. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		sprite.SpriteSizeCoef = *value;
		if (const auto& subTexture = sprite.GetSubTexture())
			subTexture->UpdateCoords(sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
	}

	bool Script::Eagle_SpriteComponent_GetIsSubtexture(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetIsSubtexture`. Entity is null");
			return false;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		return sprite.IsSubTexture();
	}

	void Script::Eagle_SpriteComponent_SetIsSubtexture(GUID entityID, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity& entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsSubtexture`. Entity is null");
			return;
		}

		auto& sprite = entity.GetComponent<SpriteComponent>();
		sprite.SetIsSubTexture(value);
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

	// Script component
	MonoReflectionType* Script::Eagle_ScriptComponent_GetScriptType(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& script = entity.GetComponent<ScriptComponent>();
			MonoType* monoType = mono_reflection_type_from_name(script.ModuleName.data(), s_AppAssemblyImage);
			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), monoType);
			return reflectionType;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Sprite Component. Entity is null");
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

	void Script::Eagle_Input_SetCursorMode(CursorMode mode)
	{
		Input::SetShowCursor(mode == CursorMode::Normal);
	}

	CursorMode Script::Eagle_Input_GetCursorMode()
	{
		return Input::IsCursorVisible() ? CursorMode::Normal : CursorMode::Hidden;
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

	void Script::Eagle_Renderer_SetBloomSettings(GUID dirt, float threashold, float intensity, float dirtIntensity, float knee, bool bEnabled)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		Ref<Texture> dirtTexture;
		TextureLibrary::Get(dirt, &dirtTexture);

		options.BloomSettings.Dirt = Cast<Texture2D>(dirtTexture);
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

	void Script::Eagle_Renderer_SetPhotoLinearTonemappingSettings(float sensetivity, float exposureTime, float fStop)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto options = sceneRenderer->GetOptions();

		options.PhotoLinearTonemappingParams.Sensetivity = sensetivity;
		options.PhotoLinearTonemappingParams.ExposureTime = exposureTime;
		options.PhotoLinearTonemappingParams.FStop = fStop;
		sceneRenderer->SetOptions(options);
	}

	void Script::Eagle_Renderer_GetPhotoLinearTonemappingSettings(float* outSensetivity, float* outExposureTime, float* outfStop)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& options = sceneRenderer->GetOptions();

		*outSensetivity = options.PhotoLinearTonemappingParams.Sensetivity;
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

	void Script::Eagle_Renderer_SetVolumetricLightsSettings(uint32_t samples, float maxScatteringDist, bool bFogEnable, bool bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		auto& sceneRenderer = scene->GetSceneRenderer();
		auto settings = sceneRenderer->GetOptions();

		settings.VolumetricSettings.Samples = samples;
		settings.VolumetricSettings.MaxScatteringDistance = maxScatteringDist;
		settings.VolumetricSettings.bFogEnable = bFogEnable;
		settings.VolumetricSettings.bEnable = bEnable;
		sceneRenderer->SetOptions(settings);
	}

	void Script::Eagle_Renderer_GetVolumetricLightsSettings(uint32_t* outSamples, float* outMaxScatteringDist, bool* bFogEnable, bool* bEnable)
	{
		const auto& scene = Scene::GetCurrentScene();
		const auto& sceneRenderer = scene->GetSceneRenderer();
		const auto& settings = sceneRenderer->GetOptions().VolumetricSettings;

		*outSamples = settings.Samples;
		*outMaxScatteringDist = settings.MaxScatteringDistance;
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
	void Script::Eagle_Scene_OpenScene(MonoString* monoPath)
	{
		const Path path = mono_string_to_utf8(monoPath);
		if (std::filesystem::exists(path))
			Scene::OpenScene(path, true, true);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't open a scene {}. The file doesn't exist!", path);
	}

	//-------------- Log --------------
	void Script::Eagle_Log_Trace(MonoString* message)
	{
		EG_TRACE(mono_string_to_utf8(message));
	}

	void Script::Eagle_Log_Info(MonoString* message)
	{
		EG_INFO(mono_string_to_utf8(message));
	}

	void Script::Eagle_Log_Warn(MonoString* message)
	{
		EG_WARN(mono_string_to_utf8(message));
	}

	void Script::Eagle_Log_Error(MonoString* message)
	{
		EG_ERROR(mono_string_to_utf8(message));
	}

	void Script::Eagle_Log_Critical(MonoString* message)
	{
		EG_CRITICAL(mono_string_to_utf8(message));
	}
}