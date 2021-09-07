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

	struct EntityScriptClass
	{
		std::string FullName;
		std::string NamespaceName;
		std::string ClassName;

		MonoClass* Class = nullptr;
		MonoMethod* Constructor = nullptr;
		MonoMethod* OnCreateMethod = nullptr;
		MonoMethod* OnDestroyMethod = nullptr;
		MonoMethod* OnUpdateMethod = nullptr;
		MonoMethod* OnPhysicsUpdateMethod = nullptr;

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
		static void Init(const std::filesystem::path& assemblyPath);
		static void Shutdown();

		static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass);
		static MonoClass* GetCoreClass(const std::string& namespaceName, const std::string& className);
		static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);
		static MonoObject* Construct(const std::string& fullName, bool callConstructor, void** parameters);

		static void InstantiateEntityClass(Entity& entity);
		static EntityInstanceData& GetEntityInstanceData(Entity& entity);

		static void OnCreateEntity(Entity& entity);
		static void OnUpdateEntity(Entity& entity, Timestep ts);
		static void OnPhysicsUpdateEntity(Entity& entity, Timestep ts);
		static void OnDestroyEntity(Entity& entity);

		static void InitEntityScript(Entity& entity);
		static bool ModuleExists(const std::string& moduleName);
		static bool IsEntityModuleValid(const Entity& entity);

		static bool LoadAppAssembly(const std::filesystem::path& path);

	private:
		static bool LoadRuntimeAssembly(const std::filesystem::path& assemblyPath);
		static bool ReloadAssembly(const std::filesystem::path& path);
		static MonoAssembly* LoadAssembly(const std::filesystem::path& assemblyPath);
		static MonoAssembly* LoadAssemblyFromFile(const char* assemblyPath);
		static MonoImage* GetAssemblyImage(MonoAssembly* assembly);

		static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr);
		static uint32_t Instantiate(EntityScriptClass& scriptClass);
		static std::string GetStringProperty(const std::string& propertyName, MonoClass* classType, MonoObject* object);
	};
}