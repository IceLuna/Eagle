#include "egpch.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Components/Components.h"
#include "ScriptEngine.h"
#include "ScriptEngineRegistry.h"
#include "PublicField.h"

#include "Eagle/Physics/PhysicsEngine.h"

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

namespace Eagle
{
	static MonoDomain* s_RootDomain = nullptr;
	static MonoDomain* s_CurrentMonoDomain = nullptr;
	static Path s_CoreAssemblyPath;

	static MonoAssembly* s_AppAssembly = nullptr;
	static MonoAssembly* s_CoreAssembly = nullptr;

	MonoImage* s_AppAssemblyImage = nullptr;
	MonoImage* s_CoreAssemblyImage = nullptr;

	static MonoMethod* s_ExceptionMethod = nullptr;
	static MonoClass* s_EntityClass = nullptr;
	static MonoClass* s_AssetClass = nullptr;
	static MonoClass* s_Vector3Class = nullptr;

	static MonoClass* s_AttrUINameClass = nullptr;
	static MonoClass* s_AttrToolTipClass = nullptr;

	static MonoClass* s_AITaskClass = nullptr;
	static MonoClass* s_AIDecoratorClass = nullptr;
	static MonoClass* s_AICompositeClass = nullptr;

	static MonoClass* s_AITaskManagerClass = nullptr;
	static MonoMethod* s_AITaskManagerSetRootMethod = nullptr;
	static MonoMethod* s_AICompositeAddChildMethod = nullptr;
	static MonoMethod* s_AINodeAddDecoratorMethod = nullptr;

	std::unordered_map<GUID, EntityInstance> ScriptEngine::s_EntityInstanceDataMap;
	std::vector<std::string> ScriptEngine::s_AvailableModuleNames;
	std::map<std::string, MonoClass*> ScriptEngine::s_CoreAIClasses; // Fullname to MonoClass mapping
	std::map<std::string, MonoClass*> ScriptEngine::s_AllAIClasses; // Fullname to MonoClass mapping
	AIBehaviorClasses ScriptEngine::s_AvailableCoreAIClasses;
	AIBehaviorClasses ScriptEngine::s_AvailableUserAIClasses;
	std::unordered_map<GUID, std::function<void()>> ScriptEngine::s_AppAssemblyReloadedCallbacks;

	static std::unordered_map<MonoClass*, FieldType> s_BuiltInEagleTypes;

	static bool s_EnableDebugging = false;

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

#if 0 // print all members of a class
			{
				MonoClass* klass = ScriptEngine::GetCoreClass("Eagle", "Asset");
				void* iter = NULL;
				MonoClassField* property;
				while (property = mono_class_get_fields(klass, &iter))
				{
					EG_CORE_TRACE("{}", mono_field_get_name(property));

				}
				EG_DEBUGBREAK();
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
			case MONO_TYPE_CLASS:
			{
				if (MonoClass* klass = mono_type_get_class(monoType))
				{
					auto it = s_BuiltInEagleTypes.find(klass);

					if (it != s_BuiltInEagleTypes.end())
						return it->second;
				}
				return FieldType::ClassReference;
			}
			case MONO_TYPE_VALUETYPE:
			{
				if (mono_type_is_struct(monoType))
				{
					MonoClass* klass = mono_type_get_class(monoType);
					auto it = s_BuiltInEagleTypes.find(klass);

					if (it != s_BuiltInEagleTypes.end())
						return it->second;
				}
				else if (MonoClass* testClass = mono_type_get_class(monoType))
				{
					if (mono_class_is_enum(testClass))
						return FieldType::Enum;
				}
			}
		}
		return FieldType::None;
	}

	static ScriptEnumFields GetEnumFields(MonoType* enumType)
	{
		ScriptEnumFields result;

		MonoClass* testClass = mono_type_get_class(enumType);
		if (testClass && mono_class_is_enum(testClass))
		{
			MonoClassField* iter = nullptr;
			void* ptr = nullptr;

			MonoVTable* classVTable = mono_class_vtable(s_RootDomain, testClass);
			bool bSkipFirst = true;
			while (iter = mono_class_get_fields(testClass, &ptr), iter != nullptr)
			{
				// Skip first since it contains irrelevant data
				if (bSkipFirst)
				{
					bSkipFirst = false;
					continue;
				}

				int value;
				mono_field_static_get_value(classVTable, iter, &value);
				const char* fieldName = mono_field_get_name(iter);

				result[value] = fieldName;
			}
		}

		return result;
	}

	static bool IsPublicClass(MonoClass* klass)
	{
		const uint32_t visibility = mono_class_get_flags(klass) & MONO_TYPE_ATTR_VISIBILITY_MASK;
		return visibility == MONO_TYPE_ATTR_PUBLIC || visibility == MONO_TYPE_ATTR_NESTED_PUBLIC;
	}

	// Helper to get string property value from attribute
	static std::string GetAttributeStringProperty(MonoObject* attribute, const char* propertyName)
	{
		if (!attribute)
			return {};

		MonoClass* attrClass = mono_object_get_class(attribute);
		MonoProperty* prop = mono_class_get_property_from_name(attrClass, propertyName);
		if (!prop)
			return {};

		MonoObject* result = mono_property_get_value(prop, attribute, nullptr, nullptr);
		if (!result)
			return {};

		std::string str = MonoStringHandler((MonoString*)result).c_str();
		return str;
	}

	void ScriptEngine::LoadAIClassPublicFields(std::vector<AIBehaviorClassData>& classes)
	{
		for (auto& klass : classes)
		{
			auto it = s_AllAIClasses.find(klass.FullName);
			if (it == s_AllAIClasses.end())
			{
				EG_CORE_ERROR("Failed to load public field of an AI class. Unknown class: {}", klass.FullName);
				continue;
			}
			MonoClass* monoClass = it->second;

			Scope<MonoInstance> instance = MonoInstance::Create(monoClass, klass.FullName);
			if (!instance->IsValid())
			{
				EG_CORE_ERROR("Failed to instantiate {}", klass.FullName);
				continue;
			}
			ScriptEngine::ParsePublicFields(monoClass, instance->GetInstance(), klass.Fields, {});
		}
	}

	void ScriptEngine::ParsePublicFields(MonoClass* klass, MonoObject* instance, std::map<std::string, PublicField>& publicFields, std::map<std::string, PublicField>&& oldPublicFields)
	{
		do
		{
			MonoClassField* iter = nullptr;
			void* ptr = nullptr;

			if (!IsPublicClass(klass))
				break;

			while ((iter = mono_class_get_fields(klass, &ptr)) != nullptr)
			{
				std::string fieldName = mono_field_get_name(iter);
				uint32_t fieldFlags = mono_field_get_flags(iter);
				if ((fieldFlags & MONO_FIELD_ATTR_PUBLIC) != MONO_FIELD_ATTR_PUBLIC)
					continue;

				MonoType* monoFieldType = mono_field_get_type(iter);
				FieldType fieldType = MonoTypeToFieldType(monoFieldType);
				if (fieldType == FieldType::None) // Not supported
					continue;

				std::string toolTip;
				// Get custom attributes for the field
				if (MonoCustomAttrInfo* attrs = mono_custom_attrs_from_field(klass, iter))
				{
					for (int i = 0; i < attrs->num_attrs; ++i)
					{
						MonoClass* attrClass = mono_method_get_class(attrs->attrs[i].ctor);

						if (attrClass == s_AttrUINameClass)
						{
							MonoObject* attrObj = mono_custom_attrs_get_attr(attrs, attrClass);
							fieldName = GetAttributeStringProperty(attrObj, "Name");
						}
						else if (attrClass == s_AttrToolTipClass)
						{
							MonoObject* attrObj = mono_custom_attrs_get_attr(attrs, attrClass);
							toolTip = GetAttributeStringProperty(attrObj, "Text");
						}
					}
					mono_custom_attrs_free(attrs);
				}

				const char* typeName = mono_type_get_name(monoFieldType);

				auto oldField = oldPublicFields.find(fieldName);
				if ((oldField != oldPublicFields.end()) && (oldField->second.TypeName == typeName))
				{
					PublicField& field = publicFields.emplace(fieldName, std::move(oldField->second)).first->second;
					field.m_MonoClassField = iter;
					field.EnumFields = fieldType == FieldType::Enum ? GetEnumFields(monoFieldType) : ScriptEnumFields{};

					// Check if the current enum value is still valid. If not, change it
					if (fieldType == FieldType::Enum && field.EnumFields.size())
					{
						const int storedValue = field.GetStoredValue<int>();
						bool bValid = false;
						for (auto& [value, name] : field.EnumFields)
						{
							if (storedValue == value)
							{
								bValid = true;
								break;
							}
						}
						// It has changed
						if (!bValid)
							field.SetStoredValue<int>(field.EnumFields.begin()->first);
					}
					continue;
				}

				if (fieldType == FieldType::ClassReference)
					continue;

				PublicField& publicField = publicFields[fieldName];
				publicField = PublicField(std::move(fieldName), std::move(typeName), std::move(toolTip), fieldType);
				publicField.m_MonoClassField = iter;
				publicField.CopyStoredValueFromRuntime(instance);
				publicField.EnumFields = fieldType == FieldType::Enum ? GetEnumFields(monoFieldType) : ScriptEnumFields{};

				//EG_CORE_INFO("[ScriptEngine] Script '{0}' - Field type '{1}', Field Name '{2}', Flags: {3}", scriptClass.FullName, typeName, fieldName, fieldFlags);
			}

			// Iterate over parent scripts classes
			klass = mono_class_get_parent(klass);
			if (klass)
			{
				if (s_EntityClass == klass || !mono_class_is_subclass_of(klass, s_EntityClass, true))
				{
					// Not an entity class, terminate
					klass = nullptr;
				}
			}
		} while (klass);
	}

	AIBehaviorClassData ScriptEngine::GetAIClassData(const std::string& fullName)
	{
		auto it = s_AllAIClasses.find(fullName);
		if (it == s_AllAIClasses.end())
			return {};

		AIBehaviorClassData result;

		auto findFunc = [&fullName, &result](const std::vector<AIBehaviorClassData>& classes) -> bool
		{
			auto it = std::find_if(classes.begin(), classes.end(), [&fullName](const AIBehaviorClassData& data)
			{
				return data.FullName == fullName;
			});

			if (it == classes.end())
				return false;

			result = *it;
			return true;
		};

		if (findFunc(s_AvailableUserAIClasses.Tasks))
			return result;
		if (findFunc(s_AvailableUserAIClasses.Composites))
			return result;
		if (findFunc(s_AvailableUserAIClasses.Decorators))
			return result;
		if (findFunc(s_AvailableCoreAIClasses.Tasks))
			return result;
		if (findFunc(s_AvailableCoreAIClasses.Composites))
			return result;
		if (findFunc(s_AvailableCoreAIClasses.Decorators))
			return result;

		return result;
	}

	void ScriptEngine::Init(const Path& assemblyPath)
	{
		// Enabling debugging if it's not a game
		s_EnableDebugging = !Application::Get().IsGame();
		if (s_EnableDebugging)
		{
			const char* argv[2] = {
				"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log",
				"--soft-breakpoints"
			};

			mono_jit_parse_options(2, (char**)argv);
			mono_debug_init(MONO_DEBUG_FORMAT_MONO);
		}
		//Init mono
		mono_set_assemblies_path("mono/lib");
		s_RootDomain = mono_jit_init("EagleJIT");

		if (s_EnableDebugging)
			mono_debug_domain_create(s_RootDomain);

		mono_thread_set_main(mono_thread_current());

		LoadCoreAssembly(assemblyPath);
	}

	void ScriptEngine::Shutdown()
	{
		s_EntityInstanceDataMap.clear();
		s_AvailableModuleNames.clear();
		s_CoreAIClasses.clear();
		s_AllAIClasses.clear();
		s_AvailableCoreAIClasses.Clear();
		s_AvailableUserAIClasses.Clear();
		s_AppAssemblyReloadedCallbacks.clear();

		// These function calls are disabled because mono_jit_cleanup() might crash for some reason
		//mono_domain_set(mono_get_root_domain(), false);
		//mono_domain_unload(s_CurrentMonoDomain);
		mono_jit_cleanup(s_RootDomain);
		
		s_RootDomain = nullptr;
		s_CurrentMonoDomain = nullptr;
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
		const std::string fullName = namespaceName.empty() ? className : (namespaceName + "." + className);
		
		auto it = classes.find(fullName);
		if (it != classes.end())
			return it->second;

		MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());
		if (!monoClass)
			EG_CORE_ERROR("[ScriptEngine]::GetCoreClass. Couldn't find class {0}.{1}", namespaceName, className);
		else
			classes[std::move(fullName)] = monoClass;

		return monoClass;
	}

	MonoClass* ScriptEngine::GetEntityClass()
	{
		return s_EntityClass;
	}

	MonoClass* ScriptEngine::GetAssetClass()
	{
		return s_AssetClass;
	}

	MonoClass* ScriptEngine::GetVector3Class()
	{
		return s_Vector3Class;
	}

	//TODO: Rewrite to Construct(namespace, class, bCallConstructor, params)
	MonoObject* ScriptEngine::Construct(const std::string& fullName, bool callConstructor, void** parameters)
	{
		std::string namespaceName;
		std::string className;

		if (const size_t firstDot = fullName.find("."); firstDot != std::string::npos)
		{
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
			HandleException(exception);
			mono_method_desc_free(desc);
		}

		return obj;
	}

	void ScriptEngine::Reset()
	{
		EG_CORE_ASSERT(s_EntityInstanceDataMap.empty());
		s_EntityInstanceDataMap.clear();
	}

	void ScriptEngine::InstantiateEntityClass(Entity entity)
	{
		GUID guid = entity.GetComponent<IDComponent>().ID;
		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		EntityInstance& entityInstance = GetEntityInstance(entity);
		entityInstance.Instance = MonoInstance::Create(entityInstance.ScriptClass.Class, entityInstance.ScriptClass.FullName);

		void* param[] = { &guid };
		CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass.Constructor, param);

		for (auto& it : scriptComponent.PublicFields)
		{
			it.second.CopyStoredValueToRuntime(entityInstance.GetMonoInstance());
		}
	}

	void ScriptEngine::OnCreateEntity(const Entity& entity)
	{
		typedef void (*OnCreateFunc)(MonoObject*, MonoObject**);

		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnCreateMethod)
		{
			OnCreateFunc function = (OnCreateFunc)entityInstance.ScriptClass.OnCreateMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance.GetMonoInstance(), &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnUpdateEntity(const Entity& entity, Timestep ts)
	{
		typedef void (*UpdateFunc)(MonoObject*, float, MonoObject**);

		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnUpdateMethod)
		{
			UpdateFunc function = (UpdateFunc)entityInstance.ScriptClass.OnUpdateMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance.GetMonoInstance(), ts, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnEventEntity(const Entity& entity, void* eventObj)
	{
		typedef void (*OnEventFunc)(MonoObject*, void*, MonoObject**);

		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnEventMethod)
		{
			OnEventFunc function = (OnEventFunc)entityInstance.ScriptClass.OnEventMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance.GetMonoInstance(), eventObj, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnAnimationEventEntity(const Entity& entity, const std::string& eventName, float time)
	{
		typedef void (*OnAnimationEventFunc)(MonoObject*, MonoString*, float, MonoObject**);

		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnAnimationEventMethod)
		{
			OnAnimationEventFunc function = (OnAnimationEventFunc)entityInstance.ScriptClass.OnAnimationEventMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance.GetMonoInstance(), mono_string_new(mono_domain_get(), eventName.c_str()), time, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnPhysicsUpdateEntity(const Entity& entity, Timestep ts)
	{
		typedef void (*PhysicsUpdateFunc)(MonoObject*, float, MonoObject**);

		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnPhysicsUpdateMethod)
		{
			PhysicsUpdateFunc function = (PhysicsUpdateFunc)entityInstance.ScriptClass.OnPhysicsUpdateMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance.GetMonoInstance(), ts, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnDestroyEntity(const Entity& entity)
	{
		typedef void (*OnDestroyFunc)(MonoObject*, MonoObject**);

		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnDestroyMethod)
		{
			OnDestroyFunc function = (OnDestroyFunc)entityInstance.ScriptClass.OnDestroyMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance.GetMonoInstance(), &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnCollisionBegin(const Entity& entity, const Entity& other, const CollisionInfo& collisionInfo)
	{
		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnCollisionBeginMethod)
		{
			const void* params[] = { GetEntityMonoObject(other), &collisionInfo.Position[0], &collisionInfo.Normal[0], &collisionInfo.Impulse[0], &collisionInfo.Force[0]};
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass.OnCollisionBeginMethod, (void**)params);
		}
	}

	void ScriptEngine::OnCollisionEnd(const Entity& entity, const Entity& other, const CollisionInfo& collisionInfo)
	{
		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnCollisionEndMethod)
		{
			const void* params[] = { GetEntityMonoObject(other), &collisionInfo.Position[0], &collisionInfo.Normal[0], &collisionInfo.Impulse[0], &collisionInfo.Force[0]};
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass.OnCollisionEndMethod, (void**)params);
		}
	}

	void ScriptEngine::OnTriggerBegin(const Entity& entity, const Entity& other)
	{
		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnTriggerBeginMethod)
		{
			void* params[] = { GetEntityMonoObject(other) };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass.OnTriggerBeginMethod, params);
		}
	}

	void ScriptEngine::OnTriggerEnd(const Entity& entity, const Entity& other)
	{
		EntityInstance& entityInstance = GetEntityInstance(entity);
		if (entityInstance.ScriptClass.OnTriggerEndMethod)
		{
			void* params[] = { GetEntityMonoObject(other) };
			CallMethod(entityInstance.GetMonoInstance(), entityInstance.ScriptClass.OnTriggerEndMethod, params);
		}
	}

	void ScriptEngine::InitEntityScript(Entity entity)
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
			EG_CORE_ERROR("[ScriptEngine] Invalid module name '{0}'!", moduleName);
			return;
		}

		GUID entityGUID = entity.GetComponent<IDComponent>().ID;
		EntityInstance& entityInstance = s_EntityInstanceDataMap[entityGUID];

		EntityScriptClass& scriptClass = entityInstance.ScriptClass;
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

		// Default construct an instance of the script class
		// We then use this to set initial values for any public fields that are
		// not already in the fieldMap
		entityInstance.Instance = MonoInstance::Create(scriptClass.Class, scriptClass.FullName);

		void* param[] = { &entityGUID };
		CallMethod(entityInstance.GetMonoInstance(), scriptClass.Constructor, param);

		ParsePublicFields(scriptClass.Class, entityInstance.GetMonoInstance(), entityPublicFields, std::move(oldPublicFields));
	}

	void ScriptEngine::RemoveEntityScript(const Entity& entity)
	{
		const GUID& entityGUID = entity.GetGUID();
		auto it = s_EntityInstanceDataMap.find(entityGUID);
		if (it == s_EntityInstanceDataMap.end())
			return;

		s_EntityInstanceDataMap.erase(it);
	}

	// TODO: Optimize. We should be able to reuse `s_AvailableModuleNames`
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
			
			// Core assembly needs to be reloaded to create a new domain
			if (!LoadCoreAssembly(s_CoreAssemblyPath))
				return false;
		}

		auto appAssemply = LoadAssembly(path);
		if (!appAssemply)
		{
			EG_CORE_ERROR("[ScriptEngine] Error loading assembly at '{0}'!", path.u8string());
			return false;
		}

		auto appAssemplyImage = GetAssemblyImage(appAssemply);
		ScriptEngineRegistry::RegisterAll();
		
		s_AppAssembly = appAssemply;
		s_AppAssemblyImage = appAssemplyImage;

		LoadListOfAppAssemblyClasses();

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

		for (const auto& [_, callback] : s_AppAssemblyReloadedCallbacks)
			callback();

		return true;
	}

	bool ScriptEngine::IsValidAppAssembly()
	{
		return s_AppAssembly != nullptr;
	}

	void ScriptEngine::LoadListOfAppAssemblyClasses()
	{
		s_AvailableModuleNames.clear();
		s_AvailableUserAIClasses.Clear();
		s_AllAIClasses.clear();
		s_AllAIClasses.insert(s_CoreAIClasses.begin(), s_CoreAIClasses.end());

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

			if (IsPublicClass(monoClass))
			{
				std::string fullName;
				if (strlen(nameSpace) != 0)
					fullName = fmt::format("{}.{}", nameSpace, className);
				else
					fullName = className;

				if (mono_class_is_subclass_of(monoClass, s_EntityClass, false))
				{
					s_AvailableModuleNames.emplace_back(std::move(fullName));
				}
				else if (mono_class_is_subclass_of(monoClass, s_AITaskClass, false))
				{
					s_AllAIClasses[fullName] = monoClass;
					auto& data = s_AvailableUserAIClasses.Tasks.emplace_back();
					data.FullName = std::move(fullName);
					data.Name = className;
					data.Type = AIBehaviorClassData::ClassType::Task;
					data.bUserClass = true;
				}
				else if (mono_class_is_subclass_of(monoClass, s_AIDecoratorClass, false))
				{
					s_AllAIClasses[fullName] = monoClass;
					auto& data = s_AvailableUserAIClasses.Decorators.emplace_back();
					data.FullName = std::move(fullName);
					data.Name = className;
					data.Type = AIBehaviorClassData::ClassType::Decorator;
					data.bUserClass = true;
				}
				else if (mono_class_is_subclass_of(monoClass, s_AICompositeClass, false))
				{
					s_AllAIClasses[fullName] = monoClass;
					auto& data = s_AvailableUserAIClasses.Composites.emplace_back();
					data.FullName = std::move(fullName);
					data.Name = className;
					data.Type = AIBehaviorClassData::ClassType::Composite;
					data.bUserClass = true;
				}
			}
		}

		LoadAIClassPublicFields(s_AvailableUserAIClasses.Tasks);
		LoadAIClassPublicFields(s_AvailableUserAIClasses.Decorators);
		LoadAIClassPublicFields(s_AvailableUserAIClasses.Composites);
	}

	void ScriptEngine::LoadListOfCoreAIClasses()
	{
		s_AvailableCoreAIClasses.Clear();
		s_CoreAIClasses.clear();
		s_AllAIClasses.clear();

		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(s_CoreAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(s_CoreAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* className = mono_metadata_string_heap(s_CoreAssemblyImage, cols[MONO_TYPEDEF_NAME]);

			MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, nameSpace, className);
			if (!monoClass)
				continue;

			if (IsPublicClass(monoClass))
			{
				std::string fullName;
				if (strlen(nameSpace) != 0)
					fullName = fmt::format("{}.{}", nameSpace, className);
				else
					fullName = className;

				if (monoClass != s_AITaskClass && mono_class_is_subclass_of(monoClass, s_AITaskClass, false))
				{
					s_CoreAIClasses[fullName] = monoClass;
					auto& data = s_AvailableCoreAIClasses.Tasks.emplace_back();
					data.FullName = std::move(fullName);
					data.Name = className;
					data.Type = AIBehaviorClassData::ClassType::Task;
					data.bUserClass = false;
				}
				else if (monoClass != s_AIDecoratorClass && mono_class_is_subclass_of(monoClass, s_AIDecoratorClass, false))
				{
					s_CoreAIClasses[fullName] = monoClass;
					auto& data = s_AvailableCoreAIClasses.Decorators.emplace_back();
					data.FullName = std::move(fullName);
					data.Name = className;
					data.Type = AIBehaviorClassData::ClassType::Decorator;
					data.bUserClass = false;
				}
				else if (monoClass != s_AICompositeClass && mono_class_is_subclass_of(monoClass, s_AICompositeClass, false))
				{
					s_CoreAIClasses[fullName] = monoClass;
					auto& data = s_AvailableCoreAIClasses.Composites.emplace_back();
					data.FullName = std::move(fullName);
					data.Name = className;
					data.Type = AIBehaviorClassData::ClassType::Composite;
					data.bUserClass = false;
				}
			}
		}
		
		s_AllAIClasses.insert(s_CoreAIClasses.begin(), s_CoreAIClasses.end());
		LoadAIClassPublicFields(s_AvailableCoreAIClasses.Tasks);
		LoadAIClassPublicFields(s_AvailableCoreAIClasses.Decorators);
		LoadAIClassPublicFields(s_AvailableCoreAIClasses.Composites);
	}

	static void AddDecorators(const std::map<std::string, MonoClass*>& monoClasses, const AIBehaviorNode& node, MonoObject* instance)
	{
		for (auto& decorator : node.AttachedDecorators)
		{
			auto it = monoClasses.find(decorator.FullName);
			if (it == monoClasses.end())
			{
				EG_CORE_ERROR("Failed to instantiate AI decorator. Unknown class: {}", decorator.FullName);
				continue;
			}

			MonoClass* monoClass = it->second;
			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), mono_class_get_type(monoClass));
			void* params[] = { reflectionType };
			MonoObject* decoratorObj = ScriptEngine::CallMethod(instance, s_AINodeAddDecoratorMethod, params);
			if (decoratorObj)
			{
				for (auto& [_, field] : decorator.Fields)
				{
					field.CopyStoredValueToRuntime(decoratorObj);
				}
				EG_CORE_TRACE("Decorator : {}", mono_class_get_name(mono_object_get_class(decoratorObj)));
			}
			else
			{
				EG_CORE_ERROR("Failed to instantiate AI decorator: {}", decorator.FullName);
				continue;
			}
		}
	}

	static void InstantiateAINode(const std::map<std::string, MonoClass*>& monoClasses, const AIBehaviorNode& node, Scope<MonoInstance>* outRootInstance, MonoObject* parentInstance = nullptr)
	{
		auto it = monoClasses.find(node.Data.FullName);
		if (it == monoClasses.end())
		{
			EG_CORE_ERROR("Failed to instantiate AI class. Unknown class: {}", node.Data.FullName);
			return;
		}

		MonoClass* monoClass = it->second;
		const bool bCompositeNode = node.Data.Type == AIBehaviorClassData::ClassType::Composite;

		if (parentInstance == nullptr)
		{
			Scope<MonoInstance> instance = MonoInstance::Create(monoClass, node.Data.FullName);
			if (MonoObject* monoInstance = instance->GetInstance())
			{
				for (auto& [_, field] : node.Data.Fields)
				{
					field.CopyStoredValueToRuntime(monoInstance);
				}
				EG_CORE_INFO(bCompositeNode ? "Composite Node: {}" : "Task: {}", mono_class_get_name(mono_object_get_class(monoInstance)));
				AddDecorators(monoClasses, node, monoInstance);

				if (bCompositeNode)
				{
					for (auto& child : node.Children)
					{
						InstantiateAINode(monoClasses, child, outRootInstance, monoInstance);
					}
				}
			}

			*outRootInstance = std::move(instance);
			return;
		}

		if (bCompositeNode)
		{
			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), mono_class_get_type(monoClass));
			void* params[] = { reflectionType };
			MonoObject* compositeNode = ScriptEngine::CallMethod(parentInstance, s_AICompositeAddChildMethod, params);
			if (compositeNode)
			{
				for (auto& [_, field] : node.Data.Fields)
				{
					field.CopyStoredValueToRuntime(compositeNode);
				}
				EG_CORE_INFO("Composite Node: {}", mono_class_get_name(mono_object_get_class(compositeNode)));
				AddDecorators(monoClasses, node, compositeNode);

				for (auto& child : node.Children)
				{
					InstantiateAINode(monoClasses, child, outRootInstance, compositeNode);
				}
			}
		}
		else
		{
			// If it's not a composite node, then it should be a task node
			EG_CORE_ASSERT(mono_class_is_subclass_of(monoClass, s_AITaskClass, false));
			EG_CORE_ASSERT(node.Data.Type == AIBehaviorClassData::ClassType::Task);

			// Tasks can't have leafs, which means parent is a composite class
			EG_CORE_ASSERT(mono_class_is_subclass_of(mono_object_get_class(parentInstance), s_AICompositeClass, false));

			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), mono_class_get_type(monoClass));
			void* params[] = { reflectionType };

			MonoObject* task = ScriptEngine::CallMethod(parentInstance, s_AICompositeAddChildMethod, params);
			if (task)
			{
				for (auto& [_, field] : node.Data.Fields)
				{
					field.CopyStoredValueToRuntime(task);
				}
				EG_CORE_INFO("Task: {}", mono_class_get_name(mono_object_get_class(task)));
				AddDecorators(monoClasses, node, task);
			}
			return;
		}
	}

	Scope<MonoInstance> ScriptEngine::InstantiateBehaviorGraph(const AIBehaviorNode& root)
	{
		if (!s_AITaskManagerClass)
		{
			EG_CORE_ERROR("Failed to instantiate behavior graph. AITaskManager class is invalid");
			return nullptr;
		}

		Scope<MonoInstance> taskManagerInstance = MonoInstance::Create(s_AITaskManagerClass, "AITaskManager");
		MonoObject* taskManager = taskManagerInstance->GetInstance();

		Scope<MonoInstance> resultRoot;
		InstantiateAINode(s_AllAIClasses, root, &resultRoot);
		if (resultRoot && resultRoot->IsValid())
		{
			void* params[] = { resultRoot->GetInstance() };
			CallMethod(taskManager, s_AITaskManagerSetRootMethod, params);
		}

		return taskManagerInstance;
	}

	bool ScriptEngine::LoadCoreAssembly(const Path& assemblyPath)
	{
		s_CoreAssemblyPath = assemblyPath;

		if (s_CurrentMonoDomain)
		{
			mono_domain_set(s_RootDomain, false);
			mono_domain_unload(s_CurrentMonoDomain);

			s_CurrentMonoDomain = mono_domain_create_appdomain("Eagle Runtime", nullptr);
			mono_domain_set(s_CurrentMonoDomain, false);
		}
		else
		{
			s_CurrentMonoDomain = mono_domain_create_appdomain("Eagle Runtime", nullptr);
			mono_domain_set(s_CurrentMonoDomain, false);
		}

		s_CoreAssembly = LoadAssembly(s_CoreAssemblyPath);
		if (!s_CoreAssembly)
			return false;

		s_CoreAssemblyImage = GetAssemblyImage(s_CoreAssembly);
		s_ExceptionMethod = GetMethod(s_CoreAssemblyImage, "Eagle.RuntimeException:OnException(object)");
		s_EntityClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Entity");
		s_AssetClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Asset");
		s_Vector3Class = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Vector3");
		s_AITaskClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AITask");
		s_AIDecoratorClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AINodeDecorator");
		s_AICompositeClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AICompositeNode");
		s_AITaskManagerClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AITaskManager");
		s_AttrUINameClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "UINameAttribute");
		s_AttrToolTipClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "ToolTipAttribute");

		s_BuiltInEagleTypes.clear();
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Vector2")]                  = FieldType::Vec2;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Vector2")]                  = FieldType::Vec2;
		s_BuiltInEagleTypes[s_Vector3Class]                                                                 = FieldType::Vec3;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Vector4")]                  = FieldType::Vec4;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Color3")]                   = FieldType::Color3;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Color4")]                   = FieldType::Color4;
		s_BuiltInEagleTypes[s_AssetClass]                                                                   = FieldType::Asset;
		s_BuiltInEagleTypes[s_EntityClass]                                                                  = FieldType::Entity;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetTexture2D")]           = FieldType::AssetTexture2D;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetTextureCube")]         = FieldType::AssetTextureCube;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetStaticMesh")]          = FieldType::AssetStaticMesh;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetSkeletalMesh")]        = FieldType::AssetSkeletalMesh;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetAudio")]               = FieldType::AssetAudio;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetSoundGroup")]          = FieldType::AssetSoundGroup;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetFont")]                = FieldType::AssetFont;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetMaterial")]            = FieldType::AssetMaterial;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetPhysicsMaterial")]     = FieldType::AssetPhysicsMaterial;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetEntity")]              = FieldType::AssetEntity;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetScene")]               = FieldType::AssetScene;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetAnimation")]           = FieldType::AssetAnimation;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetAnimationGraph")]      = FieldType::AssetAnimationGraph;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetParticleSystem")]      = FieldType::AssetParticleSystem;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetAnimationBlendSpace")] = FieldType::AssetAnimationBlendSpace;
		s_BuiltInEagleTypes[mono_class_from_name(s_CoreAssemblyImage, "Eagle", "AssetBehaviorGraph")]       = FieldType::AssetBehaviorGraph;

		s_AITaskManagerSetRootMethod = ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.AITaskManager:SetRoot(AINode)");
		s_AICompositeAddChildMethod = ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.AICompositeNode:AddChild_Interop(Type)");
		s_AINodeAddDecoratorMethod = ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.AINode:AddDecorator_Interop(Type)");

		LoadListOfCoreAIClasses();

		//PrintAssemblyTypes(s_CoreAssembly);

		return true;
	}

	MonoAssembly* ScriptEngine::LoadAssembly(const Path& assemblyPath)
	{
		MonoAssembly* assembly = LoadAssemblyFromFile(assemblyPath);

		if (assembly)
			EG_CORE_INFO("[ScriptEngine] Successfully loaded assembly at {0}!", assemblyPath.u8string());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't load assembly at {0}!", assemblyPath.u8string());

		return assembly;
	}

	MonoAssembly* ScriptEngine::LoadAssemblyFromFile(const Path& assemblyPath)
	{
		if (!std::filesystem::exists(assemblyPath))
		{
			EG_CORE_WARN("[ScriptEngine] Failed to load C# assembly. File doesn't exist: {}", assemblyPath.u8string());
			return nullptr;
		}

		ScopedDataBuffer assemblyData(Eagle::FileSystem::Read(assemblyPath));
		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char*>(assemblyData.Data()), uint32_t(assemblyData.Size()), 1, &status, 0);
		if (status != MONO_IMAGE_OK)
		{
			return NULL;
		}

		if (s_EnableDebugging)
		{
			Path pdbPath = assemblyPath;
			pdbPath.replace_extension(".pdb");
			if (std::filesystem::exists(pdbPath))
			{
				ScopedDataBuffer data(Eagle::FileSystem::Read(pdbPath));
				mono_debug_open_image_from_memory(image, (const mono_byte*)data.Data(), uint32_t(data.Size()));
				EG_CORE_INFO("[ScriptEngine] Loaded PDB-file for debugging: {}", pdbPath.u8string());
			}
			else
			{
				EG_CORE_WARN("[ScriptEngine] Failed to load PDB-file for debugging: {}", pdbPath.u8string());
			}
		}

		MonoAssembly* assemb = mono_assembly_load_from_full(image, assemblyPath.u8string().c_str(), &status, 0);
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

	UnmanagedMethod ScriptEngine::GetMethodUnmanaged(MonoImage* image, const std::string& methodDesc)
	{
		UnmanagedMethod result;
		result.Method = ScriptEngine::GetMethod(image, methodDesc);
		if (result.Method)
			result.Thunk = mono_method_get_unmanaged_thunk(result.Method);

		return result;
	}

	void ScriptEngine::HandleException(MonoObject* exception)
	{
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
		MonoObject* result = mono_runtime_invoke(method, object, params, &exception);
		HandleException(exception);
		return result;
	}

	MonoObject* ScriptEngine::InstantiateEntityUnmanaged(GUID entityID)
	{
		MonoObject* instance = mono_object_new(s_CurrentMonoDomain, GetEntityClass());
		if (!instance)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't Instantiate an Entity");
			return nullptr;
		}
		
		void* parameters[] =
		{
			&entityID
		};

		MonoMethodDesc* desc = mono_method_desc_new("Eagle.Entity:.ctor(Eagle.GUID)", true);
		EG_CORE_ASSERT(desc);
		MonoMethod* constructor = mono_method_desc_search_in_class(desc, GetEntityClass());
		EG_CORE_ASSERT(constructor);

		MonoObject* exception = nullptr;
		mono_runtime_invoke(constructor, instance, parameters, &exception);
		HandleException(exception);
		mono_method_desc_free(desc);

		return instance;
	}

	uint32_t ScriptEngine::Instantiate(MonoClass* klass, std::string_view debugName)
	{
		MonoObject* instance = mono_object_new(s_CurrentMonoDomain, klass);
		if (!instance)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't Instantiate object '{0}'", debugName);
			return 0;
		}

		mono_runtime_object_init(instance);
		return mono_gchandle_new(instance, false);
	}

	void ScriptEngine::FreeHandle(uint32_t handle)
	{
		mono_gchandle_free(handle);
	}

	MonoObject* ScriptEngine::GetHandleInstance(uint32_t handle)
	{
		return mono_gchandle_get_target(handle);
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

		return result ? std::string(MonoStringHandler(result).c_str()) : "";
	}

	EntityInstance& ScriptEngine::GetEntityInstance(const Entity& entity)
	{
		const GUID& entityGUID = entity.GetGUID();
		auto it = s_EntityInstanceDataMap.find(entityGUID);
		if (it == s_EntityInstanceDataMap.end())
			InitEntityScript(entity);

		return s_EntityInstanceDataMap[entityGUID];
	}

	MonoObject* ScriptEngine::GetEntityMonoObject(Entity entity)
	{
		return GetEntityMonoObject(entity.GetGUID());
	}

	MonoObject* ScriptEngine::GetEntityMonoObject(GUID entityID)
	{
		auto it = s_EntityInstanceDataMap.find(entityID);
		if (it == s_EntityInstanceDataMap.end())
			return InstantiateEntityUnmanaged(entityID);

		return s_EntityInstanceDataMap[entityID].GetMonoInstance();
	}
	
	void EntityScriptClass::InitClassMethods(MonoImage* image)
	{
		Constructor				= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:.ctor(GUID)");
		OnCreateMethod			= ScriptEngine::GetMethodUnmanaged(image, FullName + ":OnCreate()");
		OnDestroyMethod			= ScriptEngine::GetMethodUnmanaged(image, FullName + ":OnDestroy()");
		OnUpdateMethod			= ScriptEngine::GetMethodUnmanaged(image, FullName + ":OnUpdate(single)");
		OnEventMethod           = ScriptEngine::GetMethodUnmanaged(image, FullName + ":OnEvent(Event)");
		OnPhysicsUpdateMethod	= ScriptEngine::GetMethodUnmanaged(image, FullName + ":OnPhysicsUpdate(single)");
		OnAnimationEventMethod  = ScriptEngine::GetMethodUnmanaged(image, FullName + ":OnAnimationEvent(string)");

		OnCollisionBeginMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnCollisionBegin(GUID,Vector3,Vector3,Vector3,Vector3)");
		OnCollisionEndMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnCollisionEnd(GUID,Vector3,Vector3,Vector3,Vector3)");
		OnTriggerBeginMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnTriggerBegin(GUID)");
		OnTriggerEndMethod		= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnTriggerEnd(GUID)");
	}
	
	MonoStringHandler::MonoStringHandler(MonoString* monoStr)
	{
		m_Str = mono_string_to_utf8(monoStr);
	}
	
	MonoStringHandler::~MonoStringHandler()
	{
		mono_free(m_Str);
	}
}
