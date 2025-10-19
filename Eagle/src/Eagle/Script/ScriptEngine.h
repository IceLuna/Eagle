#pragma once

#include "Eagle/Core/Timestep.h"
#include "Eagle/Core/GUID.h"
#include "PublicField.h"
#include "Eagle/AI/BehaviorGraph.h"

extern "C" 
{
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoString MonoString;
}

namespace Eagle
{
	class Entity;
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
		UnmanagedMethod OnAnimationEventMethod;
		
		MonoMethod* OnCollisionBeginMethod = nullptr;
		MonoMethod* OnCollisionEndMethod = nullptr;
		MonoMethod* OnTriggerBeginMethod = nullptr;
		MonoMethod* OnTriggerEndMethod = nullptr;

		void InitClassMethods(MonoImage* image);
	};

	struct MonoStringHandler
	{
		MonoStringHandler(MonoString* monoStr);
		~MonoStringHandler();

		MonoStringHandler(const MonoStringHandler&) = delete;
		MonoStringHandler(MonoStringHandler&&) = delete;
		MonoStringHandler& operator= (const MonoStringHandler&) = delete;
		MonoStringHandler& operator= (MonoStringHandler&&) = delete;

		char* c_str() { return m_Str; }
		const char* c_str() const { return m_Str; }

	private:
		char* m_Str = nullptr;
	};

	struct EntityInstance;
	class ScriptEngine
	{
	public:
		static void Init(const Path& assemblyPath);
		static void Shutdown();

		static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass);
		static MonoClass* GetCoreClass(const std::string& namespaceName, const std::string& className);
		static MonoClass* GetEntityClass();
		static MonoClass* GetAssetClass();
		static MonoClass* GetVector3Class();
		static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);
		static UnmanagedMethod GetMethodUnmanaged(MonoImage* image, const std::string& methodDesc);
		static MonoObject* Construct(const std::string& fullName, bool callConstructor, void** parameters);
		static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr);

		static void Reset();

		static void InstantiateEntityClass(Entity entity);
		static EntityInstance& GetEntityInstance(const Entity& entity);
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

		static void InitEntityScript(Entity entity);
		static void RemoveEntityScript(const Entity& entity);
		static bool ModuleExists(const std::string& moduleName);
		static bool IsEntityModuleValid(const Entity& entity);

		static bool LoadAppAssembly(const Path& path);
		static bool IsValidAppAssembly();
		static void ParsePublicFields(MonoClass* klass, MonoObject* instance, std::map<std::string, PublicField>& publicFields, std::map<std::string, PublicField>&& oldPublicFields);

		static const std::vector<std::string>& GetScriptsNames() { return s_AvailableModuleNames; }
		static const AIBehaviorClasses& GetCoreAIClasses() { return s_AvailableCoreAIClasses; }
		static const AIBehaviorClasses& GetUserAIClasses() { return s_AvailableUserAIClasses; }
		static AIBehaviorClassData GetAIClassData(const std::string& fullName);

		[[nodiscard]] static GUID AddOnAppAssemblyReloadedCallback(const std::function<void()>& callback)
		{
			GUID id{};
			s_AppAssemblyReloadedCallbacks[id] = callback;
			return id;
		}

		static void RemoveOnAppAssemblyReloadedCallback(GUID id)
		{
			s_AppAssemblyReloadedCallbacks.erase(id);
		}

		// Returns AITaskManager
		static Scope<class MonoInstance> InstantiateBehaviorGraph(const AIBehaviorNode& root);

	private:
		static bool LoadCoreAssembly(const Path& assemblyPath);
		static MonoAssembly* LoadAssembly(const Path& assemblyPath);
		static MonoAssembly* LoadAssemblyFromFile(const Path& assemblyPath);
		static MonoImage* GetAssemblyImage(MonoAssembly* assembly);

		static MonoObject* InstantiateEntityUnmanaged(GUID entityID); // Can be GarbageCollected. ScriptEngine just creates it and forgets aboit it
		static std::string GetStringProperty(const std::string& propertyName, MonoClass* classType, MonoObject* object);

		static void HandleException(MonoObject* exception);

		static void LoadListOfAppAssemblyClasses();
		static void LoadListOfCoreAIClasses();
		static void LoadAIClassPublicFields(std::vector<AIBehaviorClassData>& classes);

	private:
		static std::unordered_map<GUID, EntityInstance> s_EntityInstanceDataMap;
		static std::vector<std::string> s_AvailableModuleNames;

		static std::map<std::string, MonoClass*> s_CoreAIClasses; // Fullname to MonoClass mapping
		static std::map<std::string, MonoClass*> s_AllAIClasses; // Fullname to MonoClass mapping
		static AIBehaviorClasses s_AvailableCoreAIClasses;
		static AIBehaviorClasses s_AvailableUserAIClasses;

		static std::unordered_map<GUID, std::function<void()>> s_AppAssemblyReloadedCallbacks;
	};

	class MonoInstance
	{
	public:
		~MonoInstance()
		{
			if (m_Handle != 0)
			{
				ScriptEngine::FreeHandle(m_Handle);
				m_Handle = 0;
				m_Instance = nullptr;
			}
		}

		MonoObject* GetInstance() const { return m_Instance; }
		bool IsValid() const { return m_Handle != 0; }

		static Scope<MonoInstance> Create(MonoClass* klass, std::string_view debugName)
		{
			class LocalMonoInstance : public MonoInstance {};
			Scope<MonoInstance> res = MakeScope<LocalMonoInstance>();
			res->m_Handle = ScriptEngine::Instantiate(klass, debugName);
			res->m_Instance = nullptr;
			if (res->m_Handle != 0)
			{
				res->m_Instance = ScriptEngine::GetHandleInstance(res->m_Handle);
			}

			return res;
		}

	private:
		MonoInstance() = default;
		MonoInstance(const MonoInstance&) = delete;
		MonoInstance(MonoInstance&& other) noexcept
		{
			m_Handle = other.m_Handle;
			m_Instance = other.m_Instance;

			other.m_Handle = 0;
			other.m_Instance = nullptr;
		}

		MonoInstance& operator=(const MonoInstance&) = delete;
		MonoInstance& operator=(MonoInstance&& other) noexcept
		{
			if (this == &other)
				return *this;

			m_Handle = other.m_Handle;
			m_Instance = other.m_Instance;

			other.m_Handle = 0;
			other.m_Instance = nullptr;

			return *this;
		}

		uint32_t m_Handle = 0u;
		MonoObject* m_Instance = nullptr;
	};

	struct EntityInstance
	{
		EntityScriptClass ScriptClass;
		Scope<class MonoInstance> Instance;

		MonoObject* GetMonoInstance() const { return Instance ? Instance->GetInstance() : nullptr; }
	};
}