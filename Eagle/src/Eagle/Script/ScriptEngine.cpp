#include "egpch.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Components/Components.h"
#include "ScriptEngine.h"
#include "ScriptEngineRegistry.h"
#include "PublicField.h"

#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Audio/Sound3D.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#ifdef EG_WITH_EDITOR
#define EG_SCRIPTS_ENABLE_DEBUGGING 1
#endif

namespace Eagle
{
	static MonoDomain* s_RootDomain = nullptr;
	static MonoDomain* s_CurrentMonoDomain = nullptr;
	static MonoDomain* s_NewMonoDomain = nullptr;
	static Path s_CoreAssemblyPath;
	static bool s_PostLoadCleanup = false;

	static MonoAssembly* s_AppAssembly = nullptr;
	static MonoAssembly* s_CoreAssembly = nullptr;

	MonoImage* s_AppAssemblyImage = nullptr;
	MonoImage* s_CoreAssemblyImage = nullptr;

	static MonoMethod* s_ExceptionMethod = nullptr;
	static MonoClass* s_EntityClass = nullptr;

	static std::unordered_map<std::string, EntityScriptClass> s_EntityClassMap;
	static std::unordered_map<GUID, EntityInstanceData> s_EntityInstanceDataMap;
	static std::vector<std::string> s_AvailableModuleNames;

	std::vector<Ref<Sound>> s_ScriptSounds;

	static void PrintAssemblyTypes(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

			MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, nameSpace, name);
#if 1 // print all methods of class
			void* iter = NULL;
			MonoMethod* method2;
			while (method2 = mono_class_get_methods(monoClass, &iter))
			{
				EG_CORE_TRACE("{}", mono_method_full_name(method2, 1));
			}
#endif

			printf("%s.%s\n", nameSpace, name);
		}
	}

	static FieldType MonoTypeToFieldType(MonoType* monoType)
	{
		int type = mono_type_get_type(monoType);
		switch (type)
		{
		case MONO_TYPE_BOOLEAN: return FieldType::Bool;
		case MONO_TYPE_I4: return FieldType::Int;
		case MONO_TYPE_U4: return FieldType::UnsignedInt;
		case MONO_TYPE_R4: return FieldType::Float;
		case MONO_TYPE_STRING: return FieldType::String;
		case MONO_TYPE_CLASS: return FieldType::ClassReference;
		case MONO_TYPE_VALUETYPE:
		{
			const char* typeName = mono_type_get_name(monoType);
			if (strcmp("Eagle.Vector2", typeName) == 0) return FieldType::Vec2;
			if (strcmp("Eagle.Vector3", typeName) == 0) return FieldType::Vec3;
			if (strcmp("Eagle.Vector4", typeName) == 0) return FieldType::Vec4;
			if (strcmp("Eagle.Color3", typeName) == 0) return FieldType::Color3;
			if (strcmp("Eagle.Color4", typeName) == 0) return FieldType::Color4;
		}
		}
		return FieldType::None;
	}

	void ScriptEngine::Init(const Path& assemblyPath)
	{
#if EG_SCRIPTS_ENABLE_DEBUGGING
		{
			const char* argv[2] = {
				"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log",
				"--soft-breakpoints"
			};

			mono_jit_parse_options(2, (char**)argv);
			mono_debug_init(MONO_DEBUG_FORMAT_MONO);
		}
#endif
		//Init mono
		mono_set_assemblies_path("mono/lib");
		s_RootDomain = mono_jit_init("EagleJIT");

#if EG_SCRIPTS_ENABLE_DEBUGGING
		mono_debug_domain_create(s_RootDomain);
#endif
		mono_thread_set_main(mono_thread_current());

		//Load assembly
		LoadRuntimeAssembly(assemblyPath);
	}

	void ScriptEngine::Shutdown()
	{
		s_ScriptSounds.clear();
		s_EntityInstanceDataMap.clear();
		s_AvailableModuleNames.clear();

		mono_domain_set(mono_get_root_domain(), false);

		mono_domain_unload(s_CurrentMonoDomain);
		s_CurrentMonoDomain = nullptr;

		mono_jit_cleanup(s_RootDomain);
		s_RootDomain = nullptr;
	}

	MonoClass* ScriptEngine::GetClass(MonoImage* image, const EntityScriptClass& scriptClass)
	{
		MonoClass* monoClass = mono_class_from_name(image, scriptClass.NamespaceName.c_str(), scriptClass.ClassName.c_str());
		if (!monoClass)
			EG_CORE_ERROR("[ScriptEngine]::GetCoreClass. Couldn't find class {0}.{1}", scriptClass.NamespaceName, scriptClass.ClassName);

		return monoClass;
	}

	MonoClass* ScriptEngine::GetCoreClass(const std::string& namespaceName, const std::string& className)
	{
		static std::unordered_map<std::string, MonoClass*> classes;
		const std::string& fullName = namespaceName.empty() ? className : (namespaceName + "." + className);
		
		auto it = classes.find(fullName);
		if (it != classes.end())
			return it->second;

		MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());
		if (!monoClass)
			EG_CORE_ERROR("[ScriptEngine]::GetCoreClass. Couldn't find class {0}.{1}", namespaceName, className);
		else
			classes[fullName] = monoClass;

		return monoClass;
	}

	//TODO: Rewrite to Construct(namespace, class, bCallConstructor, params)
	MonoObject* ScriptEngine::Construct(const std::string& fullName, bool callConstructor, void** parameters)
	{
		std::string namespaceName;
		std::string className;

		if (fullName.find(".") != std::string::npos)
		{
			const size_t firstDot = fullName.find_first_of('.');
			namespaceName = fullName.substr(0, firstDot);
			className = fullName.substr(firstDot + 1, (fullName.find_first_of(':') - firstDot) - 1);
		}

		MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());
		MonoObject* obj = mono_object_new(mono_domain_get(), monoClass);

		if (callConstructor)
		{
			MonoMethodDesc* desc = mono_method_desc_new(fullName.c_str(), true);
			MonoMethod* constructor = mono_method_desc_search_in_class(desc, monoClass);
			MonoObject* exception = nullptr;
			mono_runtime_invoke(constructor, obj, parameters, &exception);
			mono_method_desc_free(desc);
		}

		return obj;
	}

	void ScriptEngine::Reset()
	{
		s_EntityInstanceDataMap.clear();
	}

	void ScriptEngine::InstantiateEntityClass(Entity& entity)
	{
		GUID guid = entity.GetComponent<IDComponent>().ID;
		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		auto& moduleName = scriptComponent.ModuleName;
		EntityInstanceData& entityInstanceData = GetEntityInstanceData(entity);
		EntityInstance& entityInstance = entityInstanceData.Instance;
		EG_CORE_ASSERT(entityInstance.ScriptClass, "No script class");
		entityInstance.Handle = Instantiate(*entityInstance.ScriptClass);

		void* param[] = { &guid };
		CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->Constructor, param);

		for (auto& it : scriptComponent.PublicFields)
		{
			it.second.CopyStoredValueToRuntime(entityInstance);
		}
	}

	void ScriptEngine::OnCreateEntity(Entity& entity)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnCreateMethod)
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnCreateMethod);
	}

	void ScriptEngine::OnUpdateEntity(Entity& entity, Timestep ts)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnUpdateMethod)
		{
			void* params[] = { &ts };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnUpdateMethod, params);
		}
	}

	void ScriptEngine::OnEventEntity(Entity& entity, void* eventObj)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnEventMethod)
		{
			void* params[] = { eventObj };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnEventMethod, params);
		}
	}

	void ScriptEngine::OnPhysicsUpdateEntity(Entity& entity, Timestep ts)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnPhysicsUpdateMethod)
		{
			void* params[] = { &ts };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnPhysicsUpdateMethod, params);
		}
	}

	void ScriptEngine::OnDestroyEntity(Entity& entity)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnDestroyMethod)
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnDestroyMethod);
	}

	void ScriptEngine::OnCollisionBegin(Entity& entity, const Entity& other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnCollisionBeginMethod)
		{
			GUID otherEntityGUID = other.GetGUID();
			void* params[] = { &otherEntityGUID };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnCollisionBeginMethod, params);
		}
	}

	void ScriptEngine::OnCollisionEnd(Entity& entity, const Entity& other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnCollisionEndMethod)
		{
			GUID otherEntityGUID = other.GetGUID();
			void* params[] = { &otherEntityGUID };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnCollisionEndMethod, params);
		}
	}

	void ScriptEngine::OnTriggerBegin(Entity& entity, const Entity& other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnTriggerBeginMethod)
		{
			GUID otherEntityGUID = other.GetGUID();
			void* params[] = { &otherEntityGUID };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnTriggerBeginMethod, params);
		}
	}

	void ScriptEngine::OnTriggerEnd(Entity& entity, const Entity& other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity).Instance;
		if (entityInstance.ScriptClass->OnTriggerEndMethod)
		{
			GUID otherEntityGUID = other.GetGUID();
			void* params[] = { &otherEntityGUID };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass->OnTriggerEndMethod, params);
		}
	}

	void ScriptEngine::InitEntityScript(Entity& entity)
	{
		EG_CORE_ASSERT(entity.HasComponent<ScriptComponent>(), "Entity doesn't have a Script Component");

		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		const std::string& moduleName = scriptComponent.ModuleName;
		auto& entityPublicFields = scriptComponent.PublicFields;
		auto oldPublicFields = std::move(entityPublicFields);
		entityPublicFields.clear();
		if (moduleName.empty())
			return;

		if (!ModuleExists(moduleName))
		{
			//EG_CORE_ERROR("[ScriptEngine] Invalid module name '{0}'!", moduleName);
			return;
		}

		EntityScriptClass& scriptClass = s_EntityClassMap[moduleName];
		scriptClass.FullName = moduleName;
		if (moduleName.find('.'))
		{
			const size_t lastDotPos = moduleName.find_last_of('.');
			scriptClass.NamespaceName = moduleName.substr(0, lastDotPos);
			scriptClass.ClassName = moduleName.substr(lastDotPos + 1);
		}
		else
		{
			scriptClass.ClassName = moduleName;
		}

		scriptClass.Class = GetClass(s_AppAssemblyImage, scriptClass);
		scriptClass.InitClassMethods(s_AppAssemblyImage);

		GUID entityGUID = entity.GetComponent<IDComponent>().ID;
		EntityInstanceData& entityInstanceData = s_EntityInstanceDataMap[entityGUID];
		EntityInstance& entityInstance = entityInstanceData.Instance;
		entityInstance.ScriptClass = &scriptClass;

		// Default construct an instance of the script class
		// We then use this to set initial values for any public fields that are
		// not already in the fieldMap
		entityInstance.Handle = Instantiate(scriptClass);

		void* param[] = { &entityGUID };
		CallMethod(entityInstance.GetMonoInstance(), scriptClass.Constructor, param);

		{
			MonoClassField* iter = nullptr;
			void* ptr = nullptr;

			while ((iter = mono_class_get_fields(scriptClass.Class, &ptr)) != nullptr)
			{
				const char* fieldName = mono_field_get_name(iter);
				uint32_t fieldFlags = mono_field_get_flags(iter);
				if ((fieldFlags & MONO_FIELD_ATTR_PUBLIC) == 0)
					continue;

				MonoType* monoFieldType = mono_field_get_type(iter);
				FieldType fieldType = MonoTypeToFieldType(monoFieldType);
				if (fieldType == FieldType::None) // Not supported
					continue;

				const char* typeName = mono_type_get_name(monoFieldType);

				auto oldField = oldPublicFields.find(fieldName);
				if ((oldField != oldPublicFields.end()) && (oldField->second.TypeName == typeName))
				{
					entityPublicFields.emplace(fieldName, std::move(oldField->second));
					PublicField& field = entityPublicFields[fieldName];
					field.m_MonoClassField = iter;
					continue;
				}

				if (fieldType == FieldType::ClassReference)
					continue;

				PublicField publicField = { fieldName, typeName, fieldType };
				publicField.m_MonoClassField = iter;
				publicField.CopyStoredValueFromRuntime(entityInstance);

				entityPublicFields[fieldName] = std::move(publicField);
				//EG_CORE_INFO("[ScriptEngine] Script '{0}' - Field type '{1}', Field Name '{2}'", scriptClass.FullName, typeName, fieldName);
			}
		}

		{
			MonoProperty* iter = nullptr;
			void* ptr = nullptr;
			while ((iter = mono_class_get_properties(scriptClass.Class, &ptr)) != nullptr)
			{
				const char* propertyName = mono_property_get_name(iter);
				EG_CORE_INFO("[ScriptEngine] Property: Script '{0}' - Property Name '{1}'", scriptClass.FullName, propertyName);
			}
		}
	}

	void ScriptEngine::RemoveEntityScript(Entity& entity)
	{
		s_EntityInstanceDataMap.erase(entity.GetGUID());
	}

	bool ScriptEngine::ModuleExists(const std::string& moduleName)
	{
		if (!s_AppAssemblyImage)
			return false;

		std::string namespaceName, className;
		if (moduleName.find('.') != std::string::npos)
		{
			const size_t lastDotPos = moduleName.find_last_of('.');
			namespaceName = moduleName.substr(0, lastDotPos);
			className = moduleName.substr(lastDotPos + 1);
		}
		else
		{
			className = moduleName;
		}

		MonoClass* monoClass = mono_class_from_name(s_AppAssemblyImage, namespaceName.c_str(), className.c_str());
		if (!monoClass)
			return false;

		bool bEntitySubclass = mono_class_is_subclass_of(monoClass, s_EntityClass, false);
		return bEntitySubclass;
	}

	bool ScriptEngine::IsEntityModuleValid(const Entity& entity)
	{
		return entity.HasComponent<ScriptComponent>() && ModuleExists(entity.GetComponent<ScriptComponent>().ModuleName);
	}

	bool ScriptEngine::LoadAppAssembly(const Path& path)
	{
		if (s_AppAssembly)
		{
			s_AppAssembly = nullptr;
			s_AppAssemblyImage = nullptr;
			return ReloadAssembly(path);
		}

		auto appAssemply = LoadAssembly(path);
		if (!appAssemply)
		{
			EG_CORE_ERROR("[ScriptEngine] Error loading assemply at '{0}'!", path.u8string());
			return false;
		}

		auto appAssemplyImage = GetAssemblyImage(appAssemply);
		ScriptEngineRegistry::RegisterAll();

		if (s_PostLoadCleanup)
		{
			mono_domain_unload(s_CurrentMonoDomain);
			s_CurrentMonoDomain = s_NewMonoDomain;
			s_NewMonoDomain = nullptr;
		}
		
		s_AppAssembly = appAssemply;
		s_AppAssemblyImage = appAssemplyImage;

		LoadListOfAppAssemblyClasses();

		return true;
	}

	const std::vector<std::string>& ScriptEngine::GetScriptsNames()
	{
		return s_AvailableModuleNames;
	}

	void ScriptEngine::LoadListOfAppAssemblyClasses()
	{
		s_AvailableModuleNames.clear();

		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(s_AppAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(s_AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* className = mono_metadata_string_heap(s_AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);

			MonoClass* monoClass = mono_class_from_name(s_AppAssemblyImage, nameSpace, className);
			if (!monoClass)
				continue;

			bool bEntitySubclass = mono_class_is_subclass_of(monoClass, s_EntityClass, false);
			if (!bEntitySubclass)
				continue;

			std::string fullName;
			if (strlen(nameSpace) != 0)
				fullName = fmt::format("{}.{}", nameSpace, className);
			else
				fullName = className;

			s_AvailableModuleNames.push_back(fullName);
		}
	}

	bool ScriptEngine::LoadRuntimeAssembly(const Path& assemblyPath)
	{
		s_CoreAssemblyPath = assemblyPath;

		if (s_CurrentMonoDomain)
		{
			s_NewMonoDomain = mono_domain_create_appdomain("Eagle Runtime", nullptr);
			mono_domain_set(s_NewMonoDomain, false);
			s_PostLoadCleanup = true;
		}
		else
		{
			s_CurrentMonoDomain = mono_domain_create_appdomain("Eagle Runtime", nullptr);
			mono_domain_set(s_CurrentMonoDomain, false);
			s_PostLoadCleanup = false;
		}

		s_CoreAssembly = LoadAssembly(s_CoreAssemblyPath);
		if (!s_CoreAssembly)
			return false;

		s_CoreAssemblyImage = GetAssemblyImage(s_CoreAssembly);
		s_ExceptionMethod = GetMethod(s_CoreAssemblyImage, "Eagle.RuntimeException:OnException(object)");
		s_EntityClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Entity");

		return true;
	}

	bool ScriptEngine::ReloadAssembly(const Path& path)
	{
		if (!LoadRuntimeAssembly(s_CoreAssemblyPath))
			return false;
		if (!LoadAppAssembly(path))
			return false;

		if (s_EntityInstanceDataMap.size())
		{
			const Ref<Scene>& currentScene = Scene::GetCurrentScene();

			for (auto it = s_EntityInstanceDataMap.begin(); it != s_EntityInstanceDataMap.end();)
			{
				Entity entity = currentScene->GetEntityByGUID(it->first);
				if (entity.IsValid())
				{
					if (entity.HasComponent<ScriptComponent>())
					{
						InitEntityScript(entity);
						++it;
					}
					else
						it = s_EntityInstanceDataMap.erase(it);
				}
				else
					it = s_EntityInstanceDataMap.erase(it);
			}
		}

		return true;
	}

	MonoAssembly* ScriptEngine::LoadAssembly(const Path& assemblyPath)
	{
		const std::string u8path = assemblyPath.u8string();
		MonoAssembly* assembly = LoadAssemblyFromFile(u8path.c_str());

		if (assembly)
			EG_CORE_INFO("[ScriptEngine] Successfully loaded assembly at {0}!", u8path);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't load assembly at {0}!", u8path);

		return assembly;
	}

	MonoAssembly* ScriptEngine::LoadAssemblyFromFile(const char* assemblyPath)
	{
		if (!assemblyPath)
			return nullptr;

		ScopedDataBuffer assemblyData(Eagle::FileSystem::Read(assemblyPath));
		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char*>(assemblyData.Data()), uint32_t(assemblyData.Size()), 1, &status, 0);
		if (status != MONO_IMAGE_OK)
		{
			return NULL;
		}

#if EG_SCRIPTS_ENABLE_DEBUGGING
		{
			Path pdbPath = assemblyPath;
			pdbPath.replace_extension(".pdb");
			if (std::filesystem::exists(pdbPath))
			{
				ScopedDataBuffer data(Eagle::FileSystem::Read(pdbPath));
				mono_debug_open_image_from_memory(image, (const mono_byte*)data.Data(), uint32_t(data.Size()));
				EG_CORE_INFO("[ScriptEngine] Loaded PDB-file for debugging: {}", pdbPath);
			}
			else
			{
				EG_CORE_WARN("[ScriptEngine] Failed to load PDB-file for debugging: {}", pdbPath);
			}
		}
#endif

		MonoAssembly* assemb = mono_assembly_load_from_full(image, assemblyPath, &status, 0);
		mono_image_close(image);
		return assemb;
	}

	MonoMethod* ScriptEngine::GetMethod(MonoImage* image, const std::string& methodDesc)
	{
		MonoMethodDesc* desc = mono_method_desc_new(methodDesc.c_str(), false);
		MonoMethod* method = mono_method_desc_search_in_image(desc, image);
		mono_method_desc_free(desc);

		return method;
	}

	MonoImage* ScriptEngine::GetAssemblyImage(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		if (!image)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get assembly image!");
		return image;
	}
	
	MonoObject* ScriptEngine::CallMethod(MonoObject* object, MonoMethod* method, void** params)
	{
		MonoObject* exception = nullptr;
		mono_runtime_invoke(method, object, params, &exception);
		if (exception)
		{
			MonoClass* exceptionClass = mono_object_get_class(exception);
			MonoType* exceptionType = mono_class_get_type(exceptionClass);
			const char* typeName = mono_type_get_name(exceptionType);
			std::string message = GetStringProperty("Message", exceptionClass, exception);
			std::string stackTrace = GetStringProperty("StackTrace", exceptionClass, exception);

			EG_CORE_ERROR("[ScriptEngine] {0}: {1}. Stack Trace: {2}", typeName, message, stackTrace);

			void* args[] = { exception };
			mono_runtime_invoke(s_ExceptionMethod, nullptr, args, nullptr);
		}
		return nullptr;
	}

	uint32_t ScriptEngine::Instantiate(EntityScriptClass& scriptClass)
	{
		MonoObject* instance = mono_object_new(s_CurrentMonoDomain, scriptClass.Class);
		if (!instance)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't Instantiate object '{0}'", scriptClass.FullName);
			return 0;
		}

		mono_runtime_object_init(instance);
		return mono_gchandle_new(instance, false);
	}

	std::string ScriptEngine::GetStringProperty(const std::string& propertyName, MonoClass* classType, MonoObject* object)
	{
		MonoObject* exception = nullptr;
		MonoProperty* monoProperty = mono_class_get_property_from_name(classType, propertyName.c_str());
		MonoMethod* getterMethod = mono_property_get_get_method(monoProperty);
		MonoString* result = (MonoString*)mono_runtime_invoke(getterMethod, object, nullptr, &exception);

		if (exception)
		{
			MonoClass* exceptionClass = mono_object_get_class(exception);
			MonoType* exceptionType = mono_class_get_type(exceptionClass);
			const char* typeName = mono_type_get_name(exceptionType);
			std::string message = GetStringProperty("Message", exceptionClass, exception);
			std::string stackTrace = GetStringProperty("StackTrace", exceptionClass, exception);

			EG_CORE_ERROR("[ScriptEngine] {0}: {1}. Stack Trace: {2}", typeName, message, stackTrace);
		}

		return result ? std::string(mono_string_to_utf8(result)) : "";
	}

	EntityInstanceData& ScriptEngine::GetEntityInstanceData(Entity& entity)
	{
		const GUID& entityGUID = entity.GetGUID();
		auto it = s_EntityInstanceDataMap.find(entityGUID);
		if (it == s_EntityInstanceDataMap.end())
			InitEntityScript(entity);

		return s_EntityInstanceDataMap[entityGUID];
	}
	
	void EntityScriptClass::InitClassMethods(MonoImage* image)
	{
		Constructor				= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:.ctor(GUID)");
		OnCreateMethod			= ScriptEngine::GetMethod(image, FullName + ":OnCreate()");
		OnDestroyMethod			= ScriptEngine::GetMethod(image, FullName + ":OnDestroy()");
		OnUpdateMethod			= ScriptEngine::GetMethod(image, FullName + ":OnUpdate(single)");
		OnEventMethod           = ScriptEngine::GetMethod(image, FullName + ":OnEvent(Event)");
		OnPhysicsUpdateMethod	= ScriptEngine::GetMethod(image, FullName + ":OnPhysicsUpdate(single)");

		OnCollisionBeginMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnCollisionBegin(GUID)");
		OnCollisionEndMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnCollisionEnd(GUID)");
		OnTriggerBeginMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnTriggerBegin(GUID)");
		OnTriggerEndMethod		= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnTriggerEnd(GUID)");
	}
	
	MonoObject* EntityInstance::GetMonoInstance()
	{
		EG_CORE_ASSERT(Handle, "Entity has not been instantiated!"); 
		return mono_gchandle_get_target(Handle);
	}
}
