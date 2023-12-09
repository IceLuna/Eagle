#pragma once

#include "Eagle/Core/Timestep.h"

extern "C" 
{
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoAssembly MonoAssembly;
}

namespace Eagle
{
	class Entity;
	struct EntityScriptClass;
	struct EntityInstanceData;
	struct CollisionInfo;

	struct UnmanagedMethod
	{
		MonoMethod* Method = nullptr;
		void* Thunk = nullptr;

		operator bool() const
		{
			return Method != nullptr;
		}
	};

	struct EntityScriptClass
	{
		std::string FullName;
		std::string NamespaceName;
		std::string ClassName;

		MonoClass* Class = nullptr;
		MonoMethod* Constructor = nullptr;
		UnmanagedMethod OnCreateMethod;
		UnmanagedMethod OnDestroyMethod;
		UnmanagedMethod OnUpdateMethod;
		UnmanagedMethod OnEventMethod;
		UnmanagedMethod OnPhysicsUpdateMethod;
		
		MonoMethod* OnCollisionBeginMethod = nullptr;
		MonoMethod* OnCollisionEndMethod = nullptr;
		MonoMethod* OnTriggerBeginMethod = nullptr;
		MonoMethod* OnTriggerEndMethod = nullptr;

		void InitClassMethods(MonoImage* image);
	};

	struct EntityInstance
	{
		EntityScriptClass* ScriptClass = nullptr;
		uint32_t Handle = 0u;
		MonoObject* GetMonoInstance();
		bool IsRuntimeAvailable() const { return Handle != 0; }
	};

	struct EntityInstanceData
	{
		EntityInstance Instance;
	};

	class ScriptEngine
	{
	public:
		static void Init(const Path& assemblyPath);
		static void Shutdown();

		static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass);
		static MonoClass* GetCoreClass(const std::string& namespaceName, const std::string& className);
		static MonoClass* GetEntityClass();
		static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);
		static UnmanagedMethod GetMethodUnmanaged(MonoImage* image, const std::string& methodDesc);
		static MonoObject* Construct(const std::string& fullName, bool callConstructor, void** parameters);

		static void Reset();

		static void InstantiateEntityClass(Entity& entity);
		static EntityInstanceData& GetEntityInstanceData(Entity& entity);
		static MonoObject* GetEntityMonoObject(Entity entity);

		static void OnCreateEntity(Entity& entity);
		static void OnUpdateEntity(Entity& entity, Timestep ts);
		static void OnEventEntity(Entity& entity, void* eventObj);
		static void OnPhysicsUpdateEntity(Entity& entity, Timestep ts);
		static void OnDestroyEntity(Entity& entity);

		static void OnCollisionBegin(Entity& entity, const Entity& other, const CollisionInfo& collisionInfo);
		static void OnCollisionEnd(Entity& entity, const Entity& other, const CollisionInfo& collisionInfo);
		static void OnTriggerBegin(Entity& entity, const Entity& other);
		static void OnTriggerEnd(Entity& entity, const Entity& other);

		static void InitEntityScript(Entity& entity);
		static void RemoveEntityScript(Entity& entity);
		static bool ModuleExists(const std::string& moduleName);
		static bool IsEntityModuleValid(const Entity& entity);

		static bool LoadAppAssembly(const Path& path);

		static const std::vector<std::string>& GetScriptsNames();

	private:
		static bool LoadRuntimeAssembly(const Path& assemblyPath);
		static MonoAssembly* LoadAssembly(const Path& assemblyPath);
		static MonoAssembly* LoadAssemblyFromFile(const char* assemblyPath);
		static MonoImage* GetAssemblyImage(MonoAssembly* assembly);

		static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr);
		static uint32_t Instantiate(EntityScriptClass& scriptClass);
		static std::string GetStringProperty(const std::string& propertyName, MonoClass* classType, MonoObject* object);

		static void HandleException(MonoObject* exception);

		static void LoadListOfAppAssemblyClasses();
	};
}