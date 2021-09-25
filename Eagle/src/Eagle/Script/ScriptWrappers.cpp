#include "egpch.h"
#include "ScriptWrappers.h"
#include "ScriptEngine.h"
#include "Eagle/Physics/PhysicsActor.h"
#include <mono/jit/jit.h>

namespace Eagle 
{
	extern std::unordered_map<MonoType*, std::function<void(Entity&)>> m_AddComponentFunctions;
	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> m_HasComponentFunctions;

	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetWorldTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, const Transform*)>> m_SetRelativeTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetWorldTransformFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&, Transform*)>> m_GetRelativeTransformFunctions;
}

namespace Eagle::Script
{
	//--------------Entity--------------
	GUID Eagle_Entity_GetParent(GUID entityID)
	{
		const Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));

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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<EntitySceneNameComponent>().Name.c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Entity name. Entity is null");
			return nullptr;
		}
	}

	//Entity-Physics
	void Eagle_Entity_WakeUp(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outTransform = entity.GetWorldTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world transform. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLocation = entity.GetWorldLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world location. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldRotation(GUID entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outRotation = entity.GetWorldRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world rotation. Entity is null");
	}

	void Eagle_TransformComponent_GetWorldScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outScale = entity.GetWorldScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get world scale. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world transform. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world location. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldRotation(GUID entityID, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world rotation. Entity is null");
	}

	void Eagle_TransformComponent_SetWorldScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetWorldScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set world scale. Entity is null");
	}
	
	void Eagle_TransformComponent_GetRelativeTransform(GUID entityID, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outTransform = entity.GetRelativeTransform();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative transform. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeLocation(GUID entityID, glm::vec3* outLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLocation = entity.GetRelativeLocation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative location. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeRotation(GUID entityID, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outRotation = entity.GetRelativeRotation();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative rotation. Entity is null");
	}

	void Eagle_TransformComponent_GetRelativeScale(GUID entityID, glm::vec3* outScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outScale = entity.GetRelativeScale();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get relative scale. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeTransform(GUID entityID, const Transform* inTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeTransform(*inTransform);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative transform. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeLocation(GUID entityID, const glm::vec3* inLocation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeLocation(*inLocation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative location. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeRotation(GUID entityID, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeRotation(*inRotation);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative rotation. Entity is null");
	}

	void Eagle_TransformComponent_SetRelativeScale(GUID entityID, const glm::vec3* inScale)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.SetRelativeScale(*inScale);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set relative scale. Entity is null");
	}

	//Scene Component
	void Eagle_SceneComponent_GetWorldTransform(GUID entityID, void* type, Transform* outTransform)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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

	void Eagle_SceneComponent_GetWorldRotation(GUID entityID, void* type, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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

	void Eagle_SceneComponent_SetWorldRotation(GUID entityID, void* type, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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

	void Eagle_SceneComponent_GetRelativeRotation(GUID entityID, void* type, glm::vec3* outRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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

	void Eagle_SceneComponent_SetRelativeRotation(GUID entityID, void* type, const glm::vec3* inRotation)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
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

	//--------------PointLight Component--------------
	void Script::Eagle_PointLightComponent_GetLightColor(GUID entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<PointLightComponent>().LightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<PointLightComponent>().Ambient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light ambient color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_GetSpecularColor(GUID entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<PointLightComponent>().Specular;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light specular color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_GetDistance(GUID entityID, float* outDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outDistance = entity.GetComponent<PointLightComponent>().Distance;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get point light distance. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetLightColor(GUID entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().LightColor = *inLightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Ambient = *inAmbientColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light ambient color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetSpecularColor(GUID entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Specular = *inSpecularColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light specular color. Entity is null");
	}

	void Script::Eagle_PointLightComponent_SetDistance(GUID entityID, float inDistance)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<PointLightComponent>().Distance = inDistance;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set point light distance. Entity is null");
	}

	//--------------DirectionalLight Component--------------
	void Script::Eagle_DirectionalLightComponent_GetLightColor(GUID entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<DirectionalLightComponent>().LightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get directional light color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<DirectionalLightComponent>().Ambient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get directional light ambient color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_GetSpecularColor(GUID entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<DirectionalLightComponent>().Specular;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get directional light specular color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_SetLightColor(GUID entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().LightColor = *inLightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set directional light color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Ambient = *inAmbientColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set directional light ambient color. Entity is null");
	}

	void Script::Eagle_DirectionalLightComponent_SetSpecularColor(GUID entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<DirectionalLightComponent>().Specular = *inSpecularColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set directional light specular color. Entity is null");
	}

	//--------------SpotLight Component--------------
	void Script::Eagle_SpotLightComponent_GetLightColor(GUID entityID, glm::vec3* outLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outLightColor = entity.GetComponent<SpotLightComponent>().LightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetAmbientColor(GUID entityID, glm::vec3* outAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outAmbientColor = entity.GetComponent<SpotLightComponent>().Ambient;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light ambient color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetSpecularColor(GUID entityID, glm::vec3* outSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outSpecularColor = entity.GetComponent<SpotLightComponent>().Specular;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light specular color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetInnerCutoffAngle(GUID entityID, float* outInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outInnerCutoffAngle = entity.GetComponent<SpotLightComponent>().InnerCutOffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_GetOuterCutoffAngle(GUID entityID, float* outOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			*outOuterCutoffAngle = entity.GetComponent<SpotLightComponent>().OuterCutOffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get spot light outer cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetLightColor(GUID entityID, glm::vec3* inLightColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().LightColor = *inLightColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetAmbientColor(GUID entityID, glm::vec3* inAmbientColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().Ambient = *inAmbientColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light ambient color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetSpecularColor(GUID entityID, glm::vec3* inSpecularColor)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().Specular = *inSpecularColor;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light specular color. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetInnerCutoffAngle(GUID entityID, float inInnerCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().InnerCutOffAngle = inInnerCutoffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light inner cut off angle. Entity is null");
	}

	void Script::Eagle_SpotLightComponent_SetOuterCutoffAngle(GUID entityID, float inOuterCutoffAngle)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (entity)
			entity.GetComponent<SpotLightComponent>().OuterCutOffAngle = inOuterCutoffAngle;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set spot light outer cut off angle. Entity is null");
	}

	bool Eagle_Texture_IsValid(GUID guid)
	{
		return TextureLibrary::Exist(guid);
	}

	//Texture2D
	GUID Eagle_Texture2D_Create(MonoString* texturePath)
	{
		Ref<Texture> texture;
		std::filesystem::path path = mono_string_to_utf8(texturePath);
		if (TextureLibrary::Get(path, &texture) == false)
		{
			texture = Texture2D::Create(path);
			if (texture)
				return texture->GetGUID();
			else return {0, 0};
		}
		return texture->GetGUID();
	}

	//Static Mesh
	GUID Eagle_StaticMesh_Create(MonoString* meshPath)
	{
		Ref<StaticMesh> staticMesh;
		char* temp = mono_string_to_utf8(meshPath);
		std::filesystem::path path = temp;
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
					staticMesh->Material->DiffuseTexture = texture;
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

				staticMesh->Material->DiffuseTexture = texture;
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
					staticMesh->Material->SpecularTexture = texture;
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

				staticMesh->Material->SpecularTexture = texture;
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
					staticMesh->Material->NormalTexture = texture;
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

				staticMesh->Material->NormalTexture = texture;
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
					if (staticMesh->Material->DiffuseTexture)
						*diffuse = staticMesh->Material->DiffuseTexture->GetGUID();
					else
						*diffuse = { 0, 0 };

					if (staticMesh->Material->SpecularTexture)
						*specular = staticMesh->Material->SpecularTexture->GetGUID();
					else
						*specular = { 0, 0 };

					if (staticMesh->Material->NormalTexture)
						*normal = staticMesh->Material->NormalTexture->GetGUID();
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
				if (staticMesh->Material->DiffuseTexture)
					*diffuse = staticMesh->Material->DiffuseTexture->GetGUID();
				else
					*diffuse = { 0, 0 };

				if (staticMesh->Material->SpecularTexture)
					*specular = staticMesh->Material->SpecularTexture->GetGUID();
				else
					*specular = { 0, 0 };

				if (staticMesh->Material->NormalTexture)
					*normal = staticMesh->Material->NormalTexture->GetGUID();
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

	//StaticMesh Component
	void Eagle_StaticMeshComponent_SetMesh(GUID entityID, GUID guid)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Static Mesh. Entity is null");

		Ref<StaticMesh> staticMesh;
		StaticMeshLibrary::Get(guid, &staticMesh);
		entity.GetComponent<StaticMeshComponent>().StaticMesh = staticMesh;
	}

	GUID Eagle_StaticMeshComponent_GetMesh(GUID entityID)
	{
		Ref<Scene>& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(GUID(entityID));
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Static Mesh. Entity is null");

		const Ref<StaticMesh>& staticMesh = entity.GetComponent<StaticMeshComponent>().StaticMesh;
		if (staticMesh)
			return staticMesh->GetGUID();
		else
			return {0, 0};
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