#pragma once

#include "Eagle/Core/Timestep.h"

extern "C" {
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

	class ScriptEngine
	{
	public:
		static void Init(const std::filesystem::path& assemblyPath);
		static void Shutdown();

		static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass);
		static MonoClass* GetCoreClass(const std::string& namespaceName, const std::string& className);
		static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);
		static MonoObject* Construct(const std::string& fullName, bool callConstructor, void** parameters);

		static void InstantiateEntityClass(const Entity& entity);

		static void OnCreateEntity(const Entity& entity);
		static void OnUpdateEntity(const Entity& entity, Timestep ts);
		static void OnDestroyEntity(const Entity& entity);

		static void InitEntityScript(const Entity& entity);
		static bool ModuleExists(const std::string& moduleName);

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
		static EntityInstanceData& GetEntityInstanceData(const Entity& entity);
	};
}