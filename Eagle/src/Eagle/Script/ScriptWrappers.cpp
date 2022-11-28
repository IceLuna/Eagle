#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Audio/AudioEngine.h"

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
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetAmbientFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetAmbientFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const glm::vec3*)>> m_SetSpecularFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, glm::vec3*)>> m_GetSpecularFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetAffectsWorldFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_GetAffectsWorldFunctions;

	//BaseColliderComponent
	extern std::unordered_map<MonoType*, std::function<void(Entity&, bool)>> m_SetIsTriggerFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_IsTriggerFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetStaticFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetDynamicFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, float)>> m_SetBouncinessFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetStaticFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetDynamicFrictionFunctions;
	extern std::unordered_map<MonoType*, std::function<float(Entity&)>> m_GetBouncinessFunctions;
}

namespace Eagle::Script
{

	//--------------Entity--------------
	GUID Eagle_Entity_GetParent(GUID entityID)
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

	void Eagle_Entity_SetParent(GUID entityID, GUID parentID)
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

	MonoArray* Eagle_Entity_GetChildren(GUID entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const std::vector<Entity>& children = entity.GetChildren();
			MonoArray* result = mono_array_new(mono_domain_get(), ScriptEngine::GetCoreClass("Eagle", "Entity"), children.size());

			uint32_t index = 0;
			for (auto& child : children)
			{
				GUID guid = child.GetGUID();
				void* data[] = 
				{
					&guid
				};
				MonoObject* obj = ScriptEngine::Construct("Eagle.Entity:.ctor(ulong)", true, data);
				mono_array_set(result, MonoObject*, index++, obj);
			}
			return result;
		}
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get children. Entity is null");
		return nullptr;
	}

	GUID Eagle_Entity_CreateEntity()
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->CreateEntity();
		return entity.GetGUID();
	}

	void Eagle_Entity_DestroyEntity(GUID entityID)
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

	void Eagle_Entity_AddComponent(GUID entityID, void* type)
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

	bool Eagle_Entity_HasComponent(GUID entityID, void* type)
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

	MonoString* Eagle_Entity_GetEntityName(GUID entityID)
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

	void Eagle_Entity_GetForwardVector(GUID entityID, glm::vec3* result)
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

	void Eagle_Entity_GetRightVector(GUID entityID, glm::vec3* result)
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

	void Eagle_Entity_GetUpVector(GUID entityID, glm::vec3* result)
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

	//Entity-Physics
	void Eagle_Entity_WakeUp(GUID entityID)
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

	void Eagle_Entity_PutToSleep(GUID entityID)
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

	float Eagle_Entity_GetMass(GUID entityID)
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

	void Eagle_Entity_SetMass(GUID entityID, float mass)
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

	void Eagle_Entity_AddForce(GUID entityID, const glm::vec3* force, ForceMode forceMode)
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

	void Eagle_Entity_AddTorque(GUID entityID, const glm::vec3* force, ForceMode forceMode)
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

	void Eagle_Entity_GetLinearVelocity(GUID entityID, glm::vec3* result)
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

	void Eagle_Entity_SetLinearVelocity(GUID entityID, const glm::vec3* velocity)
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

	void Eagle_Entity_GetAngularVelocity(GUID entityID, glm::vec3* result)
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

	void Eagle_Entity_SetAngularVelocity(GUID entityID, const glm::vec3* velocity)
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

	float Eagle_Entity_GetMaxLinearVelocity(GUID entityID)
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

	void Eagle_Entity_SetMaxLinearVelocity(GUID entityID, float maxVelocity)
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

	float Eagle_Entity_GetMaxAngularVelocity(GUID entityID)
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

	void Eagle_Entity_SetMaxAngularVelocity(GUID entityID, float maxVelocity)
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

	void Eagle_Entity_SetLinearDamping(GUID entityID, float damping)
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

	void Eagle_Entity_SetAngularDamping(GUID entityID, float damping)
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

	bool Eagle_Entity_IsDynamic(GUID entityID)
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

	bool Eagle_Entity_IsKinematic(GUID entityID)
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

	bool Eagle_Entity_IsGravityEnabled(GUID entityID)
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

	bool Eagle_Entity_IsLockFlagSet(GUID entityID, ActorLockFlag flag)
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

	ActorLockFlag Eagle_Entity_GetLockFlags(GUID entityID)
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

	void Eagle_Entity_SetKinematic(GUID entityID, bool value)
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

	void Eagle_Entity_SetGravityEnabled(GUID entityID, bool value)
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

	void Eagle_Entity_SetLockFlag(GUID entityID, ActorLockFlag flag, bool value)
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

	//--------------Transform Component--------------
	void Eagle_TransformComponent_GetWorldTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetWorldTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world transform. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetWorldLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world location. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetWorldRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world rotation. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetWorldScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world scale. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world transform. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world location. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldRotation(GUID entityID, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world rotation. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetWorldScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world scale. Entity is null");
	}
	
	void Eagle_TransformComponent_GetRelativeTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTransform = entity.GetRelativeTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative transform. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outLocation = entity.GetRelativeLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative location. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeRotation(GUID entityID, Rotator* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outRotation = entity.GetRelativeRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative rotation. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outScale = entity.GetRelativeScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative scale. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative transform. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative location. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeRotation(GUID entityID, const Rotator* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative rotation. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.SetRelativeScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative scale. Entity is null");
	}

	//Scene Component
	void Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform)
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

	void Eagle_SceneComponent_GetWorldLocation(GUID entityID, void* type, glm::vec3* outLocation)
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

	void Eagle_SceneComponent_GetWorldRotation(GUID entityID, void* type, Rotator* outRotation)
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

	void Eagle_SceneComponent_GetWorldScale(GUID entityID, void* type, glm::vec3* outScale)
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

	void Eagle_SceneComponent_SetWorldTransform(GUID entityID, void* type, const Transform* inTransform)
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

	void Eagle_SceneComponent_SetWorldLocation(GUID entityID, void* type, const glm::vec3* inLocation)
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

	void Eagle_SceneComponent_SetWorldRotation(GUID entityID, void* type, const Rotator* inRotation)
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

	void Eagle_SceneComponent_SetWorldScale(GUID entityID, void* type, const glm::vec3* inScale)
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

	void Eagle_SceneComponent_GetRelativeTransform(GUID entityID, void* type, Transform* outTransform)
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

	void Eagle_SceneComponent_GetRelativeLocation(GUID entityID, void* type, glm::vec3* outLocation)
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

	void Eagle_SceneComponent_GetRelativeRotation(GUID entityID, void* type, Rotator* outRotation)
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

	void Eagle_SceneComponent_GetRelativeScale(GUID entityID, void* type, glm::vec3* outScale)
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

	void Eagle_SceneComponent_SetRelativeTransform(GUID entityID, void* type, const Transform* inTransform)
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

	void Eagle_SceneComponent_SetRelativeLocation(GUID entityID, void* type, const glm::vec3* inLocation)
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

	void Eagle_SceneComponent_SetRelativeRotation(GUID entityID, void* type, const Rotator* inRotation)
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

	void Eagle_SceneComponent_SetRelativeScale(GUID entityID, void* type, const glm::vec3* inScale)
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

	void Eagle_SceneComponent_GetForwardVector(GUID entityID, void* type, glm::vec3* outVector)
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

	void Script::Eagle_LightComponent_GetAmbientColor(GUID entityID, void* type, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetAmbientFunctions[monoType](entity, outAmbientColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'Ambient'. Entity is null");
	}

	void Script::Eagle_LightComponent_GetSpecularColor(GUID entityID, void* type, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_GetSpecularFunctions[monoType](entity, outSpecularColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get value of 'Specular'. Entity is null");
	}

	bool Eagle_LightComponent_GetAffectsWorld(GUID entityID, void* type)
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

	void Script::Eagle_LightComponent_SetAmbientColor(GUID entityID, void* type, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetAmbientFunctions[monoType](entity, inAmbientColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Ambient'. Entity is null");
	}

	void Script::Eagle_LightComponent_SetSpecularColor(GUID entityID, void* type, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetSpecularFunctions[monoType](entity, inSpecularColor);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'Specular'. Entity is null");
	}

	void Eagle_LightComponent_SetAffectsWorld(GUID entityID, void* type, bool bAffectsWorld)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetAffectsWorldFunctions[monoType](entity, bAffectsWorld);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set value of 'bAffectsWorld'. Entity is null");
	}

	//--------------PointLight Component--------------
	void Script::Eagle_PointLightComponent_GetIntensity(GUID entityID, float* outIntensity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outIntensity = entity.GetComponent<PointLightComponent>().Intensity;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light intensity. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetIntensity(GUID entityID, float inIntensity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<PointLightComponent>().Intensity = inIntensity;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light intensity. Entity is null");
	}

	//--------------DirectionalLight Component--------------
	
	//--------------SpotLight Component--------------
	void Script::Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID, float* outInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outInnerCutoffAngle = entity.GetComponent<SpotLightComponent>().InnerCutOffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetOuterCutoffAngle(GUID entityID, float* outOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outOuterCutoffAngle = entity.GetComponent<SpotLightComponent>().OuterCutOffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light outer cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetInnerCutoffAngle(GUID entityID, float inInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().InnerCutOffAngle = inInnerCutoffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetOuterCutoffAngle(GUID entityID, float inOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().OuterCutOffAngle = inOuterCutoffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light outer cut off angle. Entity is null");
	}

	void Eagle_SpotLightComponent_SetIntensity(GUID entityID, float intensity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpotLightComponent>().Intensity = intensity;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light intensity. Entity is null");
	}

	void Eagle_SpotLightComponent_GetIntensity(GUID entityID, float* outIntensity)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outIntensity = entity.GetComponent<SpotLightComponent>().Intensity;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light intensity. Entity is null");
	}

	//--------------Texture--------------
	bool Eagle_Texture_IsValid(GUID guid)
	{
		return TextureLibrary::Exist(guid);
	}

	//--------------Texture2D--------------
	GUID Eagle_Texture2D_Create(MonoString* texturePath)
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

	//--------------Static Mesh--------------
	GUID Eagle_StaticMesh_Create(MonoString* meshPath)
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

	bool Eagle_StaticMesh_IsValid(GUID guid)
	{
		return StaticMeshLibrary::Exists(guid);
	}

	void Eagle_StaticMesh_SetDiffuseTexture(GUID parentID, GUID meshID, GUID textureID)
	{
		if (parentID.IsNull() == false)
		{
			const auto& scene = Scene::GetCurrentScene();
			Entity& entity = scene->GetEntityByGUID(parentID);
			if (entity)
			{
				auto& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
				if (staticMesh)
				{
					Ref<Texture> texture;
					TextureLibrary::Get(textureID, &texture);
					staticMesh->Material->SetDiffuseTexture(Cast<Texture2D>(texture));
				}
				else
					EG_CORE_ERROR("[ScriptEngine] Couldn't set diffuse texture. StaticMesh is null");
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set diffuse texture. Entity is null");
		}
		else
		{
			Ref<StaticMesh> staticMesh;
			if (StaticMeshLibrary::Get(meshID, &staticMesh))
			{
				Ref<Texture> texture;
				TextureLibrary::Get(textureID, &texture);

				staticMesh->Material->SetDiffuseTexture(Cast<Texture2D>(texture));
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set diffuse texture. StaticMesh is null");
		}
	}

	void Eagle_StaticMesh_SetSpecularTexture(GUID parentID, GUID meshID, GUID textureID)
	{
		if (parentID.IsNull() == false)
		{
			const auto& scene = Scene::GetCurrentScene();
			Entity& entity = scene->GetEntityByGUID(parentID);
			if (entity)
			{
				auto& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
				if (staticMesh)
				{
					Ref<Texture> texture;
					TextureLibrary::Get(textureID, &texture);
					staticMesh->Material->SetSpecularTexture(Cast<Texture2D>(texture));
				}
				else
					EG_CORE_ERROR("[ScriptEngine] Couldn't set specular texture. StaticMesh is null");
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set specular texture. Entity is null");
		}
		else
		{
			Ref<StaticMesh> staticMesh;
			if (StaticMeshLibrary::Get(meshID, &staticMesh))
			{
				Ref<Texture> texture;
				TextureLibrary::Get(textureID, &texture);

				staticMesh->Material->SetSpecularTexture(Cast<Texture2D>(texture));
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set specular texture. StaticMesh is null");
		}
	}

	void Eagle_StaticMesh_SetNormalTexture(GUID parentID, GUID meshID, GUID textureID)
	{
		if (parentID.IsNull() == false)
		{
			const auto& scene = Scene::GetCurrentScene();
			Entity& entity = scene->GetEntityByGUID(parentID);
			if (entity)
			{
				auto& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
				if (staticMesh)
				{
					Ref<Texture> texture;
					TextureLibrary::Get(textureID, &texture);
					staticMesh->Material->SetNormalTexture(Cast<Texture2D>(texture));
				}
				else
					EG_CORE_ERROR("[ScriptEngine] Couldn't set normal texture. StaticMesh is null");
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set normal texture. Entity is null");
		}
		else
		{
			Ref<StaticMesh> staticMesh;
			if (StaticMeshLibrary::Get(meshID, &staticMesh))
			{
				Ref<Texture> texture;
				TextureLibrary::Get(textureID, &texture);

				staticMesh->Material->SetNormalTexture(Cast<Texture2D>(texture));
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set normal texture. StaticMesh is null");
		}
	}

	void Eagle_StaticMesh_SetScalarMaterialParams(GUID parentID, GUID meshID, const glm::vec4* tintColor, float tilingFactor, float shininess)
	{
		if (parentID.IsNull() == false)
		{
			const auto& scene = Scene::GetCurrentScene();
			Entity& entity = scene->GetEntityByGUID(parentID);
			if (entity)
			{
				auto& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
				if (staticMesh)
				{
					staticMesh->Material->TintColor = *tintColor;
					staticMesh->Material->TilingFactor = tilingFactor;
					staticMesh->Material->Shininess = shininess;
				}
				else
					EG_CORE_ERROR("[ScriptEngine] Couldn't set material scalar params. StaticMesh is null");
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set material scalar params. Entity is null");
		}
		else
		{
			Ref<StaticMesh> staticMesh;
			if (StaticMeshLibrary::Get(meshID, &staticMesh))
			{
				staticMesh->Material->TintColor = *tintColor;
				staticMesh->Material->TilingFactor = tilingFactor;
				staticMesh->Material->Shininess = shininess;
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't set material scalar params. StaticMesh is null");
		}
	}

	void Eagle_StaticMesh_GetMaterial(GUID parentID, GUID meshID, GUID* diffuse, GUID* specular, GUID* normal, glm::vec4* tint, float* tilingFactor, float* shininess)
	{
		if (parentID.IsNull() == false)
		{
			const auto& scene = Scene::GetCurrentScene();
			Entity& entity = scene->GetEntityByGUID(parentID);
			if (entity)
			{
				auto& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
				if (staticMesh)
				{
					if (auto& texture = staticMesh->Material->GetDiffuseTexture())
						*diffuse = texture->GetGUID();
					else
						*diffuse = { 0, 0 };

					if (auto& texture = staticMesh->Material->GetSpecularTexture())
						*specular = texture->GetGUID();
					else
						*specular = { 0, 0 };

					if (auto& texture = staticMesh->Material->GetNormalTexture())
						*normal = texture->GetGUID();
					else
						*normal = { 0, 0 };

					*tint = staticMesh->Material->TintColor;
					*tilingFactor = staticMesh->Material->TilingFactor;
					*shininess = staticMesh->Material->Shininess;
				}
				else
					EG_CORE_ERROR("[ScriptEngine] Couldn't get material. StaticMesh is null");
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't get material. Entity is null");
		}

		else
		{
			Ref<StaticMesh> staticMesh;
			if (StaticMeshLibrary::Get(meshID, &staticMesh))
			{
				if (auto& texture = staticMesh->Material->GetDiffuseTexture())
					*diffuse = texture->GetGUID();
				else
					*diffuse = { 0, 0 };

				if (auto& texture = staticMesh->Material->GetSpecularTexture())
					*specular = texture->GetGUID();
				else
					*specular = { 0, 0 };

				if (auto& texture = staticMesh->Material->GetNormalTexture())
					*normal = texture->GetGUID();
				else
					*normal = { 0, 0 };

				*tint = staticMesh->Material->TintColor;
				*tilingFactor = staticMesh->Material->TilingFactor;
				*shininess = staticMesh->Material->Shininess;
			}
			else
				EG_CORE_ERROR("[ScriptEngine] Couldn't get material. StaticMesh is null");
		}
		
	}

	//--------------StaticMesh Component--------------
	void Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Static Mesh. Entity is null");

		Ref<StaticMesh> staticMesh;
		StaticMeshLibrary::Get(guid, &staticMesh);
		entity.GetComponent<StaticMeshComponent>().StaticMesh = staticMesh;
	}

	GUID Eagle_StaticMeshComponent_GetMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Static Mesh. Entity is null");

		const Ref<StaticMesh>& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
		if (staticMesh)
			return staticMesh->GetGUID();
		else
			return {0, 0};
	}

	//--------------Sound--------------
	void Eagle_Sound2D_Play(MonoString* audioPath, float volume, int loopCount)
	{
		char* temp = mono_string_to_utf8(audioPath);
		Path path = temp;
		SoundSettings settings;
		settings.Volume = volume;
		settings.LoopCount = loopCount;
		AudioEngine::PlaySound2D(path, settings);
	}

	void Eagle_Sound3D_Play(MonoString* audioPath, const glm::vec3* position, float volume, int loopCount)
	{
		char* temp = mono_string_to_utf8(audioPath);
		Path path = temp;
		SoundSettings settings;
		settings.Volume = volume;
		settings.LoopCount = loopCount;
		AudioEngine::PlaySound3D(path, *position, RollOffModel::Default, settings);
	}

	//--------------AudioComponent--------------
	void Eagle_AudioComponent_SetMinDistance(GUID entityID, float minDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinDistance(minDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min distance. Entity is null");
	}

	void Eagle_AudioComponent_SetMaxDistance(GUID entityID, float maxDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMaxDistance(maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's max distance. Entity is null");
	}

	void Eagle_AudioComponent_SetMinMaxDistance(GUID entityID, float minDistance, float maxDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMinMaxDistance(minDistance, maxDistance);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's min-max distance. Entity is null");
	}

	void Eagle_AudioComponent_SetRollOffModel(GUID entityID, RollOffModel rollOff)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetRollOffModel(rollOff);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's roll off model. Entity is null");
	}

	void Eagle_AudioComponent_SetVolume(GUID entityID, float volume)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetVolume(volume);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's volume. Entity is null");
	}

	void Eagle_AudioComponent_SetLoopCount(GUID entityID, int loopCount)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLoopCount(loopCount);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's loop count. Entity is null");
	}

	void Eagle_AudioComponent_SetLooping(GUID entityID, bool bLooping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetLooping(bLooping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's looping mode. Entity is null");
	}

	void Eagle_AudioComponent_SetMuted(GUID entityID, bool bMuted)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetMuted(bMuted);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetMuted' function. Entity is null");
	}

	void Eagle_AudioComponent_SetSound(GUID entityID, MonoString* filepath)
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

	void Eagle_AudioComponent_SetStreaming(GUID entityID, bool bStreaming)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetStreaming(bStreaming);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set audio component's streaming mode. Entity is null");
	}

	void Eagle_AudioComponent_Play(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Play();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't play audio component's sound. Entity is null");
	}

	void Eagle_AudioComponent_Stop(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().Stop();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't stop audio component's sound. Entity is null");
	}

	void Eagle_AudioComponent_SetPaused(GUID entityID, bool bPaused)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<AudioComponent>().SetPaused(bPaused);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call audio component's 'SetPaused' function. Entity is null");
	}

	float Eagle_AudioComponent_GetMinDistance(GUID entityID)
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

	float Eagle_AudioComponent_GetMaxDistance(GUID entityID)
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

	RollOffModel Eagle_AudioComponent_GetRollOffModel(GUID entityID)
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

	float Eagle_AudioComponent_GetVolume(GUID entityID)
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

	int Eagle_AudioComponent_GetLoopCount(GUID entityID)
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

	bool Eagle_AudioComponent_IsLooping(GUID entityID)
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

	bool Eagle_AudioComponent_IsMuted(GUID entityID)
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

	bool Eagle_AudioComponent_IsStreaming(GUID entityID)
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

	bool Eagle_AudioComponent_IsPlaying(GUID entityID)
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
	void Eagle_RigidBodyComponent_SetMass(GUID entityID, float mass)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetMass(mass);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set mass. Entity is null");
	}

	float Eagle_RigidBodyComponent_GetMass(GUID entityID)
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

	void Eagle_RigidBodyComponent_SetLinearDamping(GUID entityID, float linearDamping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLinearDamping(linearDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set linear damping. Entity is null");
	}

	float Eagle_RigidBodyComponent_GetLinearDamping(GUID entityID)
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

	void Eagle_RigidBodyComponent_SetAngularDamping(GUID entityID, float angularDamping)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetAngularDamping(angularDamping);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set angular damping. Entity is null");
	}

	float Eagle_RigidBodyComponent_GetAngularDamping(GUID entityID)
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

	void Eagle_RigidBodyComponent_SetEnableGravity(GUID entityID, bool bEnable)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetEnableGravity(bEnable);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't change gravity state. Entity is null");
	}

	bool Eagle_RigidBodyComponent_IsGravityEnabled(GUID entityID)
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

	void Eagle_RigidBodyComponent_SetIsKinematic(GUID entityID, bool bKinematic)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetIsKinematic(bKinematic);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't change kinematic state. Entity is null");
	}

	bool Eagle_RigidBodyComponent_IsKinematic(GUID entityID)
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

	void Eagle_RigidBodyComponent_SetLockPosition(GUID entityID, bool bLockX, bool bLockY, bool bLockZ)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPosition(bLockX, bLockY, bLockZ);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock position. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockPositionX(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPositionX(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock X position. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockPositionY(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPositionY(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Y position. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockPositionZ(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockPositionZ(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Z position. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockRotation(GUID entityID, bool bLockX, bool bLockY, bool bLockZ)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotation(bLockX, bLockY, bLockZ);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock rotation. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockRotationX(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotationX(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock X rotation. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockRotationY(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotationY(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Y rotation. Entity is null");
	}

	void Eagle_RigidBodyComponent_SetLockRotationZ(GUID entityID, bool bLock)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<RigidBodyComponent>().SetLockRotationZ(bLock);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't lock Z rotation. Entity is null");
	}

	bool Eagle_RigidBodyComponent_IsPositionXLocked(GUID entityID)
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

	bool Eagle_RigidBodyComponent_IsPositionYLocked(GUID entityID)
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

	bool Eagle_RigidBodyComponent_IsPositionZLocked(GUID entityID)
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

	bool Eagle_RigidBodyComponent_IsRotationXLocked(GUID entityID)
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

	bool Eagle_RigidBodyComponent_IsRotationYLocked(GUID entityID)
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

	bool Eagle_RigidBodyComponent_IsRotationZLocked(GUID entityID)
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
	void Eagle_BaseColliderComponent_SetIsTrigger(GUID entityID, void* type, bool bTrigger)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetIsTriggerFunctions[monoType](entity, bTrigger);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsTrigger'. Entity is null");
	}

	bool Eagle_BaseColliderComponent_IsTrigger(GUID entityID, void* type)
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

	void Eagle_BaseColliderComponent_SetStaticFriction(GUID entityID, void* type, float staticFriction)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetStaticFrictionFunctions[monoType](entity, staticFriction);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetStaticFriction'. Entity is null");
	}

	void Eagle_BaseColliderComponent_SetDynamicFriction(GUID entityID, void* type, float dynamicFriction)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetStaticFrictionFunctions[monoType](entity, dynamicFriction);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetDynamicFriction'. Entity is null");
	}

	void Eagle_BaseColliderComponent_SetBounciness(GUID entityID, void* type, float bounciness)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);

		if (entity)
			m_SetBouncinessFrictionFunctions[monoType](entity, bounciness);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetBounciness'. Entity is null");
	}

	float Eagle_BaseColliderComponent_GetStaticFriction(GUID entityID, void* type)
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

	float Eagle_BaseColliderComponent_GetDynamicFriction(GUID entityID, void* type)
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

	float Eagle_BaseColliderComponent_GetBounciness(GUID entityID, void* type)
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
	void Eagle_BoxColliderComponent_SetSize(GUID entityID, const glm::vec3* size)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<BoxColliderComponent>().SetSize(*size);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set size of BoxCollider. Entity is null");
	}

	void Eagle_BoxColliderComponent_GetSize(GUID entityID, glm::vec3* outSize)
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
	void Eagle_SphereColliderComponent_SetRadius(GUID entityID, float val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<SphereColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of SphereCollider. Entity is null");
	}

	float Eagle_SphereColliderComponent_GetRadius(GUID entityID)
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

	void Eagle_CapsuleColliderComponent_SetRadius(GUID entityID, float val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetRadius(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set radius of CapsuleCollider. Entity is null");
	}

	float Eagle_CapsuleColliderComponent_GetRadius(GUID entityID)
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

	void Eagle_CapsuleColliderComponent_SetHeight(GUID entityID, float val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CapsuleColliderComponent>().SetHeight(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set height of CapsuleCollider. Entity is null");
	}

	float Eagle_CapsuleColliderComponent_GetHeight(GUID entityID)
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

	void Eagle_MeshColliderComponent_SetIsConvex(GUID entityID, bool val)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<MeshColliderComponent>().SetIsConvex(val);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetIsConvex'. Entity is null");
	}

	bool Eagle_MeshColliderComponent_IsConvex(GUID entityID)
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

	void Eagle_MeshColliderComponent_SetCollisionMesh(GUID entityID, GUID meshGUID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Collision Mesh. Entity is null");

		Ref<StaticMesh> staticMesh;
		StaticMeshLibrary::Get(meshGUID, &staticMesh);
		entity.GetComponent<StaticMeshComponent>().StaticMesh = staticMesh;
	}

	GUID Eagle_MeshColliderComponent_GetCollisionMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Collision Mesh. Entity is null");

		const Ref<StaticMesh>& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
		if (staticMesh)
			return staticMesh->GetGUID();
		else
			return { 0, 0 };
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
}