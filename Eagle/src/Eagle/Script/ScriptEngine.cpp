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
	static MonoClass* s_AttrTooltipClass = nullptr;

	static MonoClass* s_AITaskClass = nullptr;
	static MonoClass* s_AIDecoratorClass = nullptr;
	static MonoClass* s_AICompositeClass = nullptr;

	static MonoClass* s_AITaskManagerClass = nullptr;
	static MonoMethod* s_AITaskManagerSetRootMethod = nullptr;
	static MonoMethod* s_AICompositeAddChildMethod = nullptr;
	static MonoMethod* s_AINodeAddDecoratorMethod = nullptr;

	std::mutex ScriptEngine::s_Mutex;

	std::map<std::string, EntityScriptClass> ScriptEngine::s_EntityClasses;
	std::unordered_map<GUID, EntityInstance> ScriptEngine::s_EntityInstanceDataMap;
	AIBehaviorClasses ScriptEngine::s_CoreAIClasses;
	AIBehaviorClasses ScriptEngine::s_UserAIClasses;
	std::unordered_map<GUID, std::function<void()>> ScriptEngine::s_AppAssemblyReloadedCallbacks;

	static std::unordered_map<MonoClass*, FieldType> s_BuiltInEagleTypes;

	static bool s_EnableDebugging = false;

	static void PrintAssemblyTypes(MonoAssembly* assembly)
	{
#if 0 // Enable to print the data
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
				MonoClass* klass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "Asset");
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
#endif
	}

	static std::string ToFullName(const char* nameSpace, const char* className)
	{
		std::string fullName;
		if (strlen(nameSpace) != 0)
			fullName = fmt::format("{}.{}", nameSpace, className);
		else
			fullName = className;

		return fullName;
	}

	static bool IsArray(MonoType* type)
	{
		return mono_type_get_type(type) == MONO_TYPE_SZARRAY;
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
				return FieldType::None;
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

	static FieldType MonoTypeToFieldType(MonoType* type, bool* outIsArray, const char** outTypeName)
	{
		*outIsArray = IsArray(type);

		FieldType fieldType = FieldType::None;
		if (*outIsArray)
		{
			if (MonoClass* fieldClass = mono_class_from_mono_type(type))
			{
				if (MonoClass* elementClass = mono_class_get_element_class(fieldClass))
				{
					MonoType* elementType = mono_class_get_type(elementClass);
					fieldType = MonoTypeToFieldType(elementType);
					*outTypeName = mono_type_get_name(elementType);
				}
			}
		}
		else
		{
			fieldType = MonoTypeToFieldType(type);
			*outTypeName = mono_type_get_name(type);
		}

		return fieldType;
	}

	static void GetAttributes(MonoCustomAttrInfo* attrs, std::string* outName, std::string* outTooltip)
	{
		for (int i = 0; i < attrs->num_attrs; ++i)
		{
			MonoClass* attrClass = mono_method_get_class(attrs->attrs[i].ctor);

			if (outName && attrClass == s_AttrUINameClass)
			{
				MonoObject* attrObj = mono_custom_attrs_get_attr(attrs, attrClass);
				std::string name = ScriptEngine::GetStringProperty("Name", attrClass, attrObj);
				if (!name.empty())
				{
					*outName = std::move(name);
				}
			}
			else if (outTooltip && attrClass == s_AttrTooltipClass)
			{
				MonoObject* attrObj = mono_custom_attrs_get_attr(attrs, attrClass);
				std::string text = ScriptEngine::GetStringProperty("Text", attrClass, attrObj);
				if (!text.empty())
				{
					*outTooltip = std::move(text);
				}
			}
		}
	}

	static ScriptEnumFields GetEnumFields(MonoType* enumType)
	{
		ScriptEnumFields result;

		MonoClass* enumClass = mono_type_get_class(enumType);
		if (enumClass && mono_class_is_enum(enumClass))
		{
			MonoClassField* iter = nullptr;
			void* ptr = nullptr;

			MonoVTable* classVTable = mono_class_vtable(mono_domain_get(), enumClass);
			bool bSkipFirst = true;
			while (iter = mono_class_get_fields(enumClass, &ptr), iter != nullptr)
			{
				// Skip first since it contains irrelevant data
				if (bSkipFirst)
				{
					bSkipFirst = false;
					continue;
				}

				int value;
				mono_field_static_get_value(classVTable, iter, &value);

				auto& data = result[value];
				data.Name = mono_field_get_name(iter);

				if (MonoCustomAttrInfo* attrs = mono_custom_attrs_from_field(enumClass, iter))
				{
					GetAttributes(attrs, &data.Name, &data.Tooltip);
					mono_custom_attrs_free(attrs);
				}
			}
		}

		return result;
	}

	static bool IsPublicClass(MonoClass* klass)
	{
		const uint32_t visibility = mono_class_get_flags(klass) & MONO_TYPE_ATTR_VISIBILITY_MASK;
		return visibility == MONO_TYPE_ATTR_PUBLIC || visibility == MONO_TYPE_ATTR_NESTED_PUBLIC;
	}

	static bool IsPublicMethod(MonoMethod* method)
	{
		const uint32_t visibility = mono_method_get_flags(method, NULL) & MONO_METHOD_ATTR_ACCESS_MASK;
		return visibility == MONO_METHOD_ATTR_PUBLIC;
	}

	static void GetClassAttributes(MonoClass* klass, std::string* outName, const char* nameFallback, std::string* outTooltip)
	{
		*outName = nameFallback;

		MonoCustomAttrInfo* attrs = mono_custom_attrs_from_class(klass);
		if (!attrs)
			return;

		GetAttributes(attrs, outName, outTooltip);
		
		mono_custom_attrs_free(attrs);
	}

	std::vector<PublicField> ScriptEngine::LoadClassPublicFields(MonoClass* monoClass, std::string_view debugName)
	{
		Scope<MonoInstance> instance = MonoInstance::Create(monoClass, debugName);
		if (!instance->IsValid())
		{
			return {};
		}

		std::vector<PublicField> result;
		ScriptEngine::ParsePublicFields(monoClass, instance->GetInstance(), result);
		return result;
	}

	void ScriptEngine::ParsePublicFields(MonoClass* klass, MonoObject* instance, std::vector<PublicField>& publicFields)
	{
		if (!klass)
			return;

		// Don't parse built in types
		if (s_BuiltInEagleTypes.find(klass) != s_BuiltInEagleTypes.end())
			return;

		if (!IsPublicClass(klass))
			return;

		{
			// Iterate over parent scripts classes
			ParsePublicFields(mono_class_get_parent(klass), instance, publicFields);

			// Parse fields
			{
				MonoClassField* fieldIter = nullptr;
				void* fieldPtr = nullptr;

				// Parse fields
				while ((fieldIter = mono_class_get_fields(klass, &fieldPtr)) != nullptr)
				{
					uint32_t fieldFlags = mono_field_get_flags(fieldIter);
					if ((fieldFlags & MONO_FIELD_ATTR_PUBLIC) != MONO_FIELD_ATTR_PUBLIC)
						continue;

					bool bArray = false;
					const char* typeName = nullptr;
					MonoType* monoFieldType = mono_field_get_type(fieldIter);
					FieldType fieldType = MonoTypeToFieldType(monoFieldType, &bArray, &typeName);
					if (fieldType == FieldType::None) // Not supported
						continue;

					std::string fullName = mono_field_get_name(fieldIter);
					std::string uiName = fullName;
					std::string tooltip;
					// Get custom attributes for the field
					if (MonoCustomAttrInfo* attrs = mono_custom_attrs_from_field(klass, fieldIter))
					{
						GetAttributes(attrs, &uiName, &tooltip);
						mono_custom_attrs_free(attrs);
					}

					const size_t arrayLength = bArray ? ScriptEngine::GetMonoArrayLength(instance, fieldIter) : 1;
					PublicField& publicField = publicFields.emplace_back(std::move(fullName), std::move(uiName), typeName, std::move(tooltip), fieldType, bArray, arrayLength);
					publicField.m_MonoClassField = fieldIter;
					publicField.EnumFields = fieldType == FieldType::Enum ? GetEnumFields(monoFieldType) : ScriptEnumFields{};
					publicField.CopyStoredValueFromRuntime(instance);

					//EG_CORE_INFO("[ScriptEngine] Script '{0}' - Field type '{1}', Field Name '{2}', Flags: {3}", scriptClass.FullName, typeName, fieldName, fieldFlags);
				}
			}

			// Parse properties
			{
				MonoProperty* propertyIter = nullptr;
				void* propertyPtr = nullptr;

				while ((propertyIter = mono_class_get_properties(klass, &propertyPtr)) != nullptr)
				{
					MonoMethod* setter = mono_property_get_set_method(propertyIter);
					MonoMethod* getter = mono_property_get_get_method(propertyIter);
					if (!setter || !getter)
						continue;

					if (!IsPublicMethod(setter))
						continue;

					std::string fullName = mono_property_get_name(propertyIter);
					MonoType* propertyType = nullptr;
					if (MonoMethodSignature* signature = mono_method_signature(getter))
					{
						propertyType = mono_signature_get_return_type(signature);
					}
					if (!propertyType)
					{
						EG_CORE_ERROR("Failed to get the propety type of a C# property: {}", fullName);
						continue;
					}

					bool bArray = false;
					const char* typeName = nullptr;
					FieldType fieldType = MonoTypeToFieldType(propertyType, &bArray, &typeName);
					if (fieldType == FieldType::None) // Not supported
						continue;

					std::string uiName = fullName;
					std::string tooltip;
					// Get custom attributes for the property
					if (MonoCustomAttrInfo* attrs = mono_custom_attrs_from_property(klass, propertyIter))
					{
						GetAttributes(attrs, &uiName, &tooltip);
						mono_custom_attrs_free(attrs);
					}

					const size_t arrayLength = bArray ? GetMonoArrayLength(instance, propertyIter) : 1;
					PublicField& publicField = publicFields.emplace_back(std::move(fullName), std::move(uiName), typeName, std::move(tooltip), fieldType, bArray, arrayLength);
					publicField.m_MonoProperty = propertyIter;
					publicField.EnumFields = fieldType == FieldType::Enum ? GetEnumFields(propertyType) : ScriptEnumFields{};
					publicField.CopyStoredValueFromRuntime(instance);
				}
			}
		}
	}

	AIBehaviorClassData ScriptEngine::GetAIClassData(const std::string& fullName)
	{
		AIBehaviorClassData result;

		auto findFunc = [&fullName, &result](const std::vector<AIBehaviorClassData>& classes) -> bool
		{
			auto it = std::find_if(classes.begin(), classes.end(), [&fullName](const AIBehaviorClassData& data)
			{
				return data.ClassData.FullName == fullName;
			});

			if (it == classes.end())
				return false;

			result = *it;
			return true;
		};

		if (findFunc(s_UserAIClasses.Tasks))
			return result;
		if (findFunc(s_UserAIClasses.Composites))
			return result;
		if (findFunc(s_UserAIClasses.Decorators))
			return result;
		if (findFunc(s_CoreAIClasses.Tasks))
			return result;
		if (findFunc(s_CoreAIClasses.Composites))
			return result;
		if (findFunc(s_CoreAIClasses.Decorators))
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
		s_EntityClasses.clear();
		s_EntityInstanceDataMap.clear();
		s_CoreAIClasses.Clear();
		s_UserAIClasses.Clear();
		s_AppAssemblyReloadedCallbacks.clear();

		// These function calls are disabled because mono_jit_cleanup() might crash for some reason
		//mono_domain_set(mono_get_root_domain(), false);
		//mono_domain_unload(s_CurrentMonoDomain);
		mono_jit_cleanup(s_RootDomain);
		
		s_RootDomain = nullptr;
		s_CurrentMonoDomain = nullptr;
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

	bool ScriptEngine::InstantiateEntityClass(Entity entity)
	{
		EntityInstance* entityInstance = CreateEntityInstance(entity);
		if (entityInstance == nullptr)
			return false;

		GUID guid = entity.GetGUID();
		void* param[] = { &guid };
		CallMethod(entityInstance->GetMonoInstance(), entityInstance->Methods.Constructor, param);

		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		for (auto& field : scriptComponent.PublicFields)
		{
			field.CopyStoredValueToRuntime(entityInstance->GetMonoInstance());
		}

		return true;
	}

	void ScriptEngine::OnCreateEntity(const Entity& entity)
	{
		typedef void (*OnCreateFunc)(MonoObject*, MonoObject**);

		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnCreateMethod)
		{
			OnCreateFunc function = (OnCreateFunc)entityInstance->Methods.OnCreateMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance->GetMonoInstance(), &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnUpdateEntity(const Entity& entity, Timestep ts)
	{
		typedef void (*UpdateFunc)(MonoObject*, float, MonoObject**);

		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnUpdateMethod)
		{
			UpdateFunc function = (UpdateFunc)entityInstance->Methods.OnUpdateMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance->GetMonoInstance(), ts, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnEventEntity(const Entity& entity, void* eventObj)
	{
		typedef void (*OnEventFunc)(MonoObject*, void*, MonoObject**);

		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnEventMethod)
		{
			OnEventFunc function = (OnEventFunc)entityInstance->Methods.OnEventMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance->GetMonoInstance(), eventObj, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnAnimationEventEntity(const Entity& entity, const std::string& eventName, float time)
	{
		typedef void (*OnAnimationEventFunc)(MonoObject*, MonoString*, float, MonoObject**);

		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnAnimationEventMethod)
		{
			OnAnimationEventFunc function = (OnAnimationEventFunc)entityInstance->Methods.OnAnimationEventMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance->GetMonoInstance(), mono_string_new(mono_domain_get(), eventName.c_str()), time, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnPhysicsUpdateEntity(const Entity& entity, Timestep ts)
	{
		typedef void (*PhysicsUpdateFunc)(MonoObject*, float, MonoObject**);

		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnPhysicsUpdateMethod)
		{
			PhysicsUpdateFunc function = (PhysicsUpdateFunc)entityInstance->Methods.OnPhysicsUpdateMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance->GetMonoInstance(), ts, &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnDestroyEntity(const Entity& entity)
	{
		typedef void (*OnDestroyFunc)(MonoObject*, MonoObject**);

		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnDestroyMethod)
		{
			OnDestroyFunc function = (OnDestroyFunc)entityInstance->Methods.OnDestroyMethod.Thunk;
			MonoObject* exception = nullptr;
			function(entityInstance->GetMonoInstance(), &exception);
			HandleException(exception);
		}
	}

	void ScriptEngine::OnCollisionBegin(const Entity& entity, const Entity& other, const CollisionInfo& collisionInfo)
	{
		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnCollisionBeginMethod)
		{
			const void* params[] = { GetEntityMonoObject(other), &collisionInfo.Position[0], &collisionInfo.Normal[0], &collisionInfo.Impulse[0], &collisionInfo.Force[0]};
			CallMethod(entityInstance->GetMonoInstance(), entityInstance->Methods.OnCollisionBeginMethod, (void**)params);
		}
	}

	void ScriptEngine::OnCollisionEnd(const Entity& entity, const Entity& other, const CollisionInfo& collisionInfo)
	{
		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnCollisionEndMethod)
		{
			const void* params[] = { GetEntityMonoObject(other), &collisionInfo.Position[0], &collisionInfo.Normal[0], &collisionInfo.Impulse[0], &collisionInfo.Force[0]};
			CallMethod(entityInstance->GetMonoInstance(), entityInstance->Methods.OnCollisionEndMethod, (void**)params);
		}
	}

	void ScriptEngine::OnTriggerBegin(const Entity& entity, const Entity& other)
	{
		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnTriggerBeginMethod)
		{
			void* params[] = { GetEntityMonoObject(other) };
			CallMethod(entityInstance->GetMonoInstance(), entityInstance->Methods.OnTriggerBeginMethod, params);
		}
	}

	void ScriptEngine::OnTriggerEnd(const Entity& entity, const Entity& other)
	{
		EntityInstance* entityInstance = GetEntityInstance(entity);
		if (entityInstance && entityInstance->Methods.OnTriggerEndMethod)
		{
			void* params[] = { GetEntityMonoObject(other) };
			CallMethod(entityInstance->GetMonoInstance(), entityInstance->Methods.OnTriggerEndMethod, params);
		}
	}

	void ScriptEngine::TryToRestoreOldValues(std::vector<PublicField>& publicFields, const std::vector<PublicField>& oldValues)
	{
		for (auto& field : publicFields)
		{
			auto oldField = std::find_if(oldValues.begin(), oldValues.end(), [&field](const PublicField& v)
			{
				return field.FullName == v.FullName;
			});

			if ((oldField != oldValues.end()) && (oldField->Type == field.Type))
			{
				field.CopyStoredValue(*oldField);
				// Check if the current enum value is still valid. If not, change it
				if (field.Type == FieldType::Enum)
				{
					if (field.EnumFields.size())
					{
						const int storedValue = field.GetStoredValue<int>();
						bool bValid = false;
						for (auto& [value, _] : field.EnumFields)
						{
							if (storedValue == value)
							{
								bValid = true;
								break;
							}
						}

						if (!bValid)
							field.SetStoredValue<int>(field.EnumFields.empty() ? 0 : field.EnumFields.begin()->first);
					}
				}
			}
		}
	}

	bool ScriptEngine::InitEntityScript(Entity entity)
	{
		EG_CORE_ASSERT(entity.HasComponent<ScriptComponent>(), "Entity doesn't have a Script Component");

		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		const std::string& moduleName = scriptComponent.ModuleName;
		auto& entityPublicFields = scriptComponent.PublicFields;
		auto oldPublicFields = std::move(entityPublicFields);
		entityPublicFields.clear();
		if (moduleName.empty())
		{
			RemoveEntityScript(entity);
			return false;
		}

		auto itClassData = s_EntityClasses.find(moduleName);
		if (itClassData == s_EntityClasses.end())
		{
			EG_CORE_ERROR("[ScriptEngine] Invalid module name '{0}'!", moduleName);
			RemoveEntityScript(entity);
			return false;
		}

		const auto& entityClassData = itClassData->second;
		MonoClass* monoClass = entityClassData.ClassData.Class;

		GUID entityGUID = entity.GetGUID();
		EntityInstance& entityInstance = s_EntityInstanceDataMap[entityGUID];
		entityInstance.Methods = entityClassData.Methods;

		// Default construct an instance of the script class
		// We then use this to set initial values for any public fields that are
		// not already in the fieldMap
		entityInstance.Instance = MonoInstance::Create(monoClass, entityClassData.ClassData.FullName);
		EG_CORE_ASSERT(entityInstance.Instance->IsValid());

		void* param[] = { &entityGUID };
		CallMethod(entityInstance.GetMonoInstance(), entityInstance.Methods.Constructor, param);
		entityPublicFields = entityClassData.ClassData.Fields;

		// Restore old values if possible
		TryToRestoreOldValues(entityPublicFields, oldPublicFields);

		return true;
	}

	void ScriptEngine::UpdateEntityPublicFields(Entity entity)
	{
		EG_CORE_ASSERT(entity.HasComponent<ScriptComponent>(), "Entity doesn't have a Script Component");

		auto& scriptComponent = entity.GetComponent<ScriptComponent>();
		const std::string& moduleName = scriptComponent.ModuleName;
		auto& entityPublicFields = scriptComponent.PublicFields;
		auto oldPublicFields = std::move(entityPublicFields);
		entityPublicFields.clear();
		if (moduleName.empty())
		{
			RemoveEntityScript(entity);
			return;
		}

		auto itClassData = s_EntityClasses.find(moduleName);
		if (itClassData == s_EntityClasses.end())
		{
			EG_CORE_ERROR("[ScriptEngine] Invalid module name '{0}'!", moduleName);
			RemoveEntityScript(entity);
			return;
		}

		const auto& entityClassData = itClassData->second;
		entityPublicFields = entityClassData.ClassData.Fields;

		// Restore old values if possible
		TryToRestoreOldValues(entityPublicFields, oldPublicFields);
	}

	void ScriptEngine::RemoveEntityScript(const Entity& entity)
	{
		const GUID& entityGUID = entity.GetGUID();
		auto it = s_EntityInstanceDataMap.find(entityGUID);
		if (it == s_EntityInstanceDataMap.end())
			return;

		s_EntityInstanceDataMap.erase(it);
	}

	bool ScriptEngine::ModuleExists(const std::string& moduleName)
	{
		return s_EntityClasses.find(moduleName) != s_EntityClasses.end();
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
			EG_CORE_ERROR("[ScriptEngine] Error loading assembly at '{0}'!", path);
			return false;
		}

		auto appAssemplyImage = GetAssemblyImage(appAssemply);
		ScriptEngineRegistry::RegisterAll();
		
		s_AppAssembly = appAssemply;
		s_AppAssemblyImage = appAssemplyImage;

		LoadListOfAppAssemblyClasses();

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
		s_UserAIClasses.Clear();
		s_EntityClasses.clear();
		s_EntityInstanceDataMap.clear();

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
				const std::string fullName = ToFullName(nameSpace, className);
				if (mono_class_is_subclass_of(monoClass, s_EntityClass, false))
				{
					EntityScriptClass data;
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = fullName;
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = true;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.InitClassMethods();

					s_EntityClasses[std::move(fullName)] = std::move(data);
				}
				else if (mono_class_is_subclass_of(monoClass, s_AITaskClass, false))
				{
					auto& data = s_UserAIClasses.Tasks.emplace_back();
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = std::move(fullName);
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = true;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.Type = AIBehaviorClassData::ClassType::Task;
				}
				else if (mono_class_is_subclass_of(monoClass, s_AIDecoratorClass, false))
				{
					auto& data = s_UserAIClasses.Decorators.emplace_back();
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = std::move(fullName);
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = true;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.Type = AIBehaviorClassData::ClassType::Decorator;
				}
				else if (mono_class_is_subclass_of(monoClass, s_AICompositeClass, false))
				{
					auto& data = s_UserAIClasses.Composites.emplace_back();
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = std::move(fullName);
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = true;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.Type = AIBehaviorClassData::ClassType::Composite;
				}
			}
		}
	}

	void ScriptEngine::LoadListOfCoreAIClasses()
	{
		s_CoreAIClasses.Clear();

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
				const std::string fullName = ToFullName(nameSpace, className);
				if (monoClass != s_AITaskClass && mono_class_is_subclass_of(monoClass, s_AITaskClass, false))
				{
					auto& data = s_CoreAIClasses.Tasks.emplace_back();
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = std::move(fullName);
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = false;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.Type = AIBehaviorClassData::ClassType::Task;
				}
				else if (monoClass != s_AIDecoratorClass && mono_class_is_subclass_of(monoClass, s_AIDecoratorClass, false))
				{
					auto& data = s_CoreAIClasses.Decorators.emplace_back();
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = std::move(fullName);
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = false;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.Type = AIBehaviorClassData::ClassType::Decorator;
				}
				else if (monoClass != s_AICompositeClass && mono_class_is_subclass_of(monoClass, s_AICompositeClass, false))
				{
					auto& data = s_CoreAIClasses.Composites.emplace_back();
					data.ClassData.Class = monoClass;
					data.ClassData.FullName = std::move(fullName);
					GetClassAttributes(monoClass, &data.ClassData.UIName, className, &data.ClassData.Tooltip);
					data.ClassData.bUserClass = false;
					data.ClassData.Fields = LoadClassPublicFields(monoClass, data.ClassData.FullName);
					data.Type = AIBehaviorClassData::ClassType::Composite;
				}
			}
		}
	}

	MonoClass* ScriptEngine::GetAIMonoClass(const AIBehaviorClassData& classData)
	{
		MonoClass* monoClass = nullptr;
		const bool bUserClass = classData.ClassData.bUserClass;

		switch (classData.Type)
		{
		case AIBehaviorClassData::ClassType::Task:
		{
			const auto& classes = bUserClass ? s_UserAIClasses.Tasks : s_CoreAIClasses.Tasks;
			auto it = std::find(classes.begin(), classes.end(), classData);
			if (it != classes.end())
			{
				monoClass = it->ClassData.Class;
			}
			break;
		}
		case AIBehaviorClassData::ClassType::Composite:
		{
			const auto& classes = bUserClass ? s_UserAIClasses.Composites : s_CoreAIClasses.Composites;
			auto it = std::find(classes.begin(), classes.end(), classData);
			if (it != classes.end())
			{
				monoClass = it->ClassData.Class;
			}
			break;
		}
		case AIBehaviorClassData::ClassType::Decorator:
		{
			const auto& classes = bUserClass ? s_UserAIClasses.Decorators : s_CoreAIClasses.Decorators;
			auto it = std::find(classes.begin(), classes.end(), classData);
			if (it != classes.end())
			{
				monoClass = it->ClassData.Class;
			}
			break;
		}
		}

		return monoClass;
	}

	void ScriptEngine::AddDecorators(const AIBehaviorNode& node, MonoObject* instance)
	{
		for (auto& decorator : node.AttachedDecorators)
		{
			MonoClass* monoClass = GetAIMonoClass(decorator);
			if (monoClass == nullptr)
			{
				EG_CORE_ERROR("Failed to instantiate AI decorator. Unknown class: {}", decorator.ClassData.FullName);
				continue;
			}

			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), mono_class_get_type(monoClass));
			void* params[] = { reflectionType };
			MonoObject* decoratorObj = ScriptEngine::CallMethod(instance, s_AINodeAddDecoratorMethod, params);
			if (decoratorObj)
			{
				for (auto& field : decorator.ClassData.Fields)
				{
					field.CopyStoredValueToRuntime(decoratorObj);
				}
				//EG_CORE_TRACE("Decorator : {}", mono_class_get_name(mono_object_get_class(decoratorObj)));
			}
			else
			{
				EG_CORE_ERROR("Failed to instantiate AI decorator: {}", decorator.ClassData.FullName);
				continue;
			}
		}
	}

	void ScriptEngine::InstantiateAINode(const AIBehaviorNode& node, Scope<MonoInstance>* outRootInstance, MonoObject* parentInstance)
	{
		MonoClass* monoClass = GetAIMonoClass(node.Data);
		if (monoClass == nullptr)
		{
			EG_CORE_ERROR("Failed to instantiate AI class. Unknown class: {}", node.Data.ClassData.FullName);
			return;
		}

		const bool bCompositeNode = node.Data.Type == AIBehaviorClassData::ClassType::Composite;
		if (parentInstance == nullptr)
		{
			Scope<MonoInstance> instance = MonoInstance::Create(monoClass, node.Data.ClassData.FullName);
			if (MonoObject* monoInstance = instance->GetInstance())
			{
				for (auto& field : node.Data.ClassData.Fields)
				{
					field.CopyStoredValueToRuntime(monoInstance);
				}
				//EG_CORE_INFO(bCompositeNode ? "Composite Node: {}" : "Task: {}", mono_class_get_name(mono_object_get_class(monoInstance)));
				AddDecorators(node, monoInstance);

				if (bCompositeNode)
				{
					for (auto& child : node.Children)
					{
						InstantiateAINode(child, outRootInstance, monoInstance);
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
				for (auto& field : node.Data.ClassData.Fields)
				{
					field.CopyStoredValueToRuntime(compositeNode);
				}
				//EG_CORE_INFO("Composite Node: {}", mono_class_get_name(mono_object_get_class(compositeNode)));
				AddDecorators(node, compositeNode);

				for (auto& child : node.Children)
				{
					InstantiateAINode(child, outRootInstance, compositeNode);
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
				for (auto& field : node.Data.ClassData.Fields)
				{
					field.CopyStoredValueToRuntime(task);
				}
				//EG_CORE_INFO("Task: {}", mono_class_get_name(mono_object_get_class(task)));
				AddDecorators(node, task);
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
		InstantiateAINode(root, &resultRoot);
		if (resultRoot && resultRoot->IsValid())
		{
			void* params[] = { resultRoot->GetInstance() };
			CallMethod(taskManager, s_AITaskManagerSetRootMethod, params);
		}

		return taskManagerInstance;
	}

	void ScriptEngine::UpdateAIClassPublicFields(AIBehaviorClassData& classData)
	{
		const bool bUserClass = classData.ClassData.bUserClass;
		switch (classData.Type)
		{
			case AIBehaviorClassData::ClassType::Task:
			{
				const auto& classes = bUserClass ? s_UserAIClasses.Tasks : s_CoreAIClasses.Tasks;
				auto it = std::find(classes.begin(), classes.end(), classData);
				if (it != classes.end())
				{
					auto oldPublicFields = std::move(classData.ClassData.Fields);
					classData.ClassData = it->ClassData;
					TryToRestoreOldValues(classData.ClassData.Fields, oldPublicFields);
				}
				break;
			}
			case AIBehaviorClassData::ClassType::Composite:
			{
				const auto& classes = bUserClass ? s_UserAIClasses.Composites : s_CoreAIClasses.Composites;
				auto it = std::find(classes.begin(), classes.end(), classData);
				if (it != classes.end())
				{
					auto oldPublicFields = std::move(classData.ClassData.Fields);
					classData.ClassData = it->ClassData;
					TryToRestoreOldValues(classData.ClassData.Fields, oldPublicFields);
				}
				break;
			}
			case AIBehaviorClassData::ClassType::Decorator:
			{
				const auto& classes = bUserClass ? s_UserAIClasses.Decorators : s_CoreAIClasses.Decorators;
				auto it = std::find(classes.begin(), classes.end(), classData);
				if (it != classes.end())
				{
					auto oldPublicFields = std::move(classData.ClassData.Fields);
					classData.ClassData = it->ClassData;
					TryToRestoreOldValues(classData.ClassData.Fields, oldPublicFields);
				}
				break;
			}
		}
	}

	size_t ScriptEngine::GetMonoArrayLength(MonoObject* instance, MonoClassField* field)
	{
		size_t length = 0;
		MonoArray* array = nullptr;
		mono_field_get_value(instance, field, &array);
		if (array)
			length = mono_array_length(array);

		return length;
	}

	size_t ScriptEngine::GetMonoArrayLength(MonoObject* instance, MonoProperty* property)
	{
		size_t length = 0;
		MonoArray* array = (MonoArray*)mono_property_get_value(property, instance, nullptr, nullptr);
		if (array)
			length = mono_array_length(array);

		return length;
	}

	void ScriptEngine::UpdateAIBehaviorNodePublicFields(AIBehaviorNode& node)
	{
		UpdateAIClassPublicFields(node.Data);
		for (auto& decorator : node.AttachedDecorators)
		{
			UpdateAIClassPublicFields(decorator);
		}

		for (auto& child : node.Children)
		{
			UpdateAIBehaviorNodePublicFields(child);
		}
	}

	bool ScriptEngine::LoadCoreAssembly(const Path& assemblyPath)
	{
		char domainName[] = "Eagle Runtime";

		s_CoreAssemblyPath = assemblyPath;

		if (s_CurrentMonoDomain)
		{
			mono_domain_set(s_RootDomain, false);
			mono_domain_unload(s_CurrentMonoDomain);

			s_CurrentMonoDomain = mono_domain_create_appdomain(domainName, nullptr);
			mono_domain_set(s_CurrentMonoDomain, false);
		}
		else
		{
			s_CurrentMonoDomain = mono_domain_create_appdomain(domainName, nullptr);
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
		s_AttrTooltipClass = mono_class_from_name(s_CoreAssemblyImage, "Eagle", "TooltipAttribute");

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
		s_BuiltInEagleTypes[s_AITaskClass]                                                                  = FieldType::None;
		s_BuiltInEagleTypes[s_AIDecoratorClass]                                                             = FieldType::None;
		s_BuiltInEagleTypes[s_AICompositeClass]                                                             = FieldType::None;
		s_BuiltInEagleTypes[s_AITaskManagerClass]                                                           = FieldType::None;

		s_AITaskManagerSetRootMethod = ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.AITaskManager:SetRoot(AINode)");
		s_AICompositeAddChildMethod = ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.AICompositeNode:AddChild_Interop(Type)");
		s_AINodeAddDecoratorMethod = ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.AINode:AddDecorator_Interop(Type)");

		LoadListOfCoreAIClasses();

		PrintAssemblyTypes(s_CoreAssembly);

		return true;
	}

	MonoAssembly* ScriptEngine::LoadAssembly(const Path& assemblyPath)
	{
		MonoAssembly* assembly = LoadAssemblyFromFile(assemblyPath);

		if (assembly)
			EG_CORE_INFO("[ScriptEngine] Successfully loaded assembly at {0}!", assemblyPath);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't load assembly at {0}!", assemblyPath);

		return assembly;
	}

	MonoAssembly* ScriptEngine::LoadAssemblyFromFile(const Path& assemblyPath)
	{
		if (!std::filesystem::exists(assemblyPath))
		{
			EG_CORE_WARN("[ScriptEngine] Failed to load C# assembly. File doesn't exist: {}", assemblyPath);
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
				EG_CORE_INFO("[ScriptEngine] Loaded PDB-file for debugging: {}", pdbPath);
			}
			else
			{
				EG_CORE_WARN("[ScriptEngine] Failed to load PDB-file for debugging: {}", pdbPath);
			}
		}

		MonoAssembly* assemb = mono_assembly_load_from_full(image, assemblyPath.string().c_str(), &status, 0);
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

	UnmanagedMethod ScriptEngine::GetMethodUnmanaged(MonoImage* image, const ScriptClass& classData, const std::string& methodDesc, bool bCheckParents)
	{
		UnmanagedMethod result;
		result.Method = ScriptEngine::GetMethod(image, classData.FullName + methodDesc);
		if (result.Method)
		{
			result.Thunk = mono_method_get_unmanaged_thunk(result.Method);
		}
		else if (MonoClass* parent = mono_class_get_parent(classData.Class); bCheckParents && parent && parent != s_EntityClass)
		{
			ScriptClass parentClassData;
			parentClassData.Class = parent;
			parentClassData.FullName = ToFullName(mono_class_get_namespace(parent), mono_class_get_name(parent));
			result = GetMethodUnmanaged(image, parentClassData, methodDesc);
		}

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

	EntityInstance* ScriptEngine::CreateEntityInstance(const Entity& entity)
	{
		const GUID& entityGUID = entity.GetGUID();
		auto it = s_EntityInstanceDataMap.find(entityGUID);
		if (it != s_EntityInstanceDataMap.end())
			return &(it->second);

		if (InitEntityScript(entity))
		{
			return &s_EntityInstanceDataMap[entityGUID];
		}
		else
		{
			return nullptr;
		}
	}

	EntityInstance* ScriptEngine::GetEntityInstance(const Entity& entity)
	{
		const GUID& entityGUID = entity.GetGUID();
		auto it = s_EntityInstanceDataMap.find(entityGUID);
		if (it == s_EntityInstanceDataMap.end())
			return nullptr;

		return &it->second;
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
	
	void EntityScriptClass::InitClassMethods()
	{
		constexpr bool bCheckParentClasses = true;

		Methods.Constructor				= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:.ctor(GUID)");
		Methods.OnCreateMethod			= ScriptEngine::GetMethodUnmanaged(s_AppAssemblyImage, ClassData, ":OnCreate()", bCheckParentClasses);
		Methods.OnDestroyMethod			= ScriptEngine::GetMethodUnmanaged(s_AppAssemblyImage, ClassData, ":OnDestroy()", bCheckParentClasses);
		Methods.OnUpdateMethod			= ScriptEngine::GetMethodUnmanaged(s_AppAssemblyImage, ClassData, ":OnUpdate(single)", bCheckParentClasses);
		Methods.OnEventMethod           = ScriptEngine::GetMethodUnmanaged(s_AppAssemblyImage, ClassData, ":OnEvent(Event)", bCheckParentClasses);
		Methods.OnPhysicsUpdateMethod	= ScriptEngine::GetMethodUnmanaged(s_AppAssemblyImage, ClassData, ":OnPhysicsUpdate(single)", bCheckParentClasses);
		Methods.OnAnimationEventMethod  = ScriptEngine::GetMethodUnmanaged(s_AppAssemblyImage, ClassData, ":OnAnimationEvent(string,single)", bCheckParentClasses);

		Methods.OnCollisionBeginMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnCollisionBegin(Entity,Vector3,Vector3,Vector3,Vector3)");
		Methods.OnCollisionEndMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnCollisionEnd(Entity,Vector3,Vector3,Vector3,Vector3)");
		Methods.OnTriggerBeginMethod	= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnTriggerBegin(Entity)");
		Methods.OnTriggerEndMethod		= ScriptEngine::GetMethod(s_CoreAssemblyImage, "Eagle.Entity:OnTriggerEnd(Entity)");
	}
}
