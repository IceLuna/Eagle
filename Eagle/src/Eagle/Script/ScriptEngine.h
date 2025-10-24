#pragma once

#include "ScriptUtils.h"

#include "Eagle/Core/Timestep.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/AI/BehaviorGraph.h"

namespace Eagle
{
	class Entity;
	struct CollisionInfo;

	struct EntityInstance
	{
		EntityScriptMethods Methods;
		Scope<MonoInstance> Instance;

		MonoObject* GetMonoInstance() const { return Instance ? Instance->GetInstance() : nullptr; }
	};

	class ScriptEngine
	{
	public:
		static void Init(const Path& assemblyPath);
		static void Shutdown();

		static MonoClass* GetEntityClass();
		static MonoClass* GetAssetClass();
		static MonoClass* GetVector3Class();
		static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);
		static UnmanagedMethod GetMethodUnmanaged(MonoImage* image, const std::string& methodDesc);
		static MonoObject* Construct(const std::string& fullName, bool callConstructor, void** parameters);
		static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr);
		static std::string GetStringProperty(const std::string& propertyName, MonoClass* classType, MonoObject* object);

		static void Reset();

		static bool InstantiateEntityClass(Entity entity);
		static EntityInstance* CreateEntityInstance(const Entity& entity);
		static EntityInstance* GetEntityInstance(const Entity& entity);
		static MonoObject* GetEntityMonoObject(Entity entity);
		static MonoObject* GetEntityMonoObject(GUID entityID);

		[[nodiscard]] static uint32_t Instantiate(MonoClass* klass, std::string_view debugName);
		static void FreeHandle(uint32_t handle);
		static MonoObject* GetHandleInstance(uint32_t handle);

		static void OnCreateEntity(const Entity& entity);
		static void OnUpdateEntity(const Entity& entity, Timestep ts);
		static void OnEventEntity(const Entity& entity, void* eventObj);
		static void OnAnimationEventEntity(const Entity& entity, const std::string& eventName, float time);
		static void OnPhysicsUpdateEntity(const Entity& entity, Timestep ts);
		static void OnDestroyEntity(const Entity& entity);

		static void OnCollisionBegin(const Entity& entity, const Entity& other, const CollisionInfo& collisionInfo);
		static void OnCollisionEnd(const Entity& entity, const Entity& other, const CollisionInfo& collisionInfo);
		static void OnTriggerBegin(const Entity& entity, const Entity& other);
		static void OnTriggerEnd(const Entity& entity, const Entity& other);

		static void UpdateEntityPublicFields(Entity entity);
		static void RemoveEntityScript(const Entity& entity);
		static bool ModuleExists(const std::string& moduleName);
		static bool IsEntityModuleValid(const Entity& entity);

		static bool LoadAppAssembly(const Path& path);
		static bool IsValidAppAssembly();
		static void ParsePublicFields(MonoClass* klass, MonoObject* instance, std::vector<PublicField>& publicFields);

		static const std::map<std::string, EntityScriptClass>& GetEntityClasses() { return s_EntityClasses; }
		static const AIBehaviorClasses& GetCoreAIClasses() { return s_CoreAIClasses; }
		static const AIBehaviorClasses& GetUserAIClasses() { return s_UserAIClasses; }
		static AIBehaviorClassData GetAIClassData(const std::string& fullName);

		[[nodiscard]] static GUID AddOnAppAssemblyReloadedCallback(const std::function<void()>& callback)
		{
			GUID id{};
			s_AppAssemblyReloadedCallbacks[id] = callback;
			return id;
		}

		[[nodiscard]] static void AddOnAppAssemblyReloadedCallback(const GUID& id, const std::function<void()>& callback)
		{
			s_AppAssemblyReloadedCallbacks[id] = callback;
		}

		static void RemoveOnAppAssemblyReloadedCallback(GUID id)
		{
			s_AppAssemblyReloadedCallbacks.erase(id);
		}

		// Returns AITaskManager
		static Scope<MonoInstance> InstantiateBehaviorGraph(const AIBehaviorNode& root);
		static void UpdateAIBehaviorNodePublicFields(AIBehaviorNode& node);
		static void UpdateAIClassPublicFields(AIBehaviorClassData& classData);

	private:
		static bool LoadCoreAssembly(const Path& assemblyPath);
		static MonoAssembly* LoadAssembly(const Path& assemblyPath);
		static MonoAssembly* LoadAssemblyFromFile(const Path& assemblyPath);
		static MonoImage* GetAssemblyImage(MonoAssembly* assembly);

		static MonoClass* GetAIMonoClass(const AIBehaviorClassData& classData);
		static void InstantiateAINode(const AIBehaviorNode& node, Scope<MonoInstance>* outRootInstance, MonoObject* parentInstance = nullptr);
		static void AddDecorators(const AIBehaviorNode& node, MonoObject* instance);

		static bool InitEntityScript(Entity entity);
		static MonoObject* InstantiateEntityUnmanaged(GUID entityID); // Can be GarbageCollected. ScriptEngine just creates it and forgets aboit it

		static void HandleException(MonoObject* exception);

		static void LoadListOfAppAssemblyClasses();
		static void LoadListOfCoreAIClasses();
		static std::vector<PublicField> LoadClassPublicFields(MonoClass* monoClass, std::string_view debugName);
		static void TryToRestoreOldValues(std::vector<PublicField>& publicFields, const std::vector<PublicField>& oldValues);

	private:
		static std::map<std::string, EntityScriptClass> s_EntityClasses; // FullName -> Data
		static std::unordered_map<GUID, EntityInstance> s_EntityInstanceDataMap;

		static AIBehaviorClasses s_CoreAIClasses;
		static AIBehaviorClasses s_UserAIClasses;

		static std::unordered_map<GUID, std::function<void()>> s_AppAssemblyReloadedCallbacks;
	};
}