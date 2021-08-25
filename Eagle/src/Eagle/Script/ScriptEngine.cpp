#include "egpch.h"
#include "ScriptEngine.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-gc.h>

#ifdef EG_PLATFORM_WINDOWS
	#include <winioctl.h>
#endif

namespace Eagle
{
	static MonoDomain* s_CurrentMonoDomain = nullptr;
	static MonoDomain* s_NewMonoDomain = nullptr;
	static std::filesystem::path s_CoreAssemblyPath;
	static bool s_PostLoadCleanup = false;

	static MonoAssembly* s_AppAssembly = nullptr;
	static MonoAssembly* s_CoreAssembly = nullptr;

	static MonoImage* s_AppAssemblyImage = nullptr;
	static MonoImage* s_CoreAssemblyImage = nullptr;

	static MonoMethod* s_ExceptionMethod = nullptr;
	static MonoClass* s_EntityClass = nullptr;

	void ScriptEngine::Init(const std::filesystem::path& assemblyPath)
	{
		//Init mono
		mono_set_assemblies_path("mono/lib");
		mono_jit_init("Eagle");

		//Load assembly
		LoadRuntimeAssembly(assemblyPath);
	}

	void ScriptEngine::Shutdown()
	{
	}

	MonoClass* ScriptEngine::GetClass(const std::string& namespaceName, const std::string& className)
	{
		static std::unordered_map<std::string, MonoClass*> classes;
		const std::string& fullName = namespaceName.empty() ? className : (namespaceName + "." + className);
		
		auto it = classes.find(fullName);
		if (it != classes.end())
			return it->second;

		MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());
		if (!monoClass)
			EG_CORE_ERROR("[ScriptEngine]::GetClass. mono_class_from_name failed!");
		else
			classes[fullName] = monoClass;

		return monoClass;
	}

	//TODO: Rewrite to Construct(namespace, class, bCallConstructor, params)
	MonoObject* ScriptEngine::Construct(const std::string& fullName, bool callConstructor, void** parameters)
	{
		std::string namespaceName;
		std::string className;
		std::string parameterList;

		if (fullName.find(".") != std::string::npos)
		{
			const size_t firstDot = fullName.find_first_of('.');
			namespaceName = fullName.substr(0, firstDot);
			className = fullName.substr(firstDot + 1, (fullName.find_first_of(':') - firstDot) - 1);
		}

		if (fullName.find(":") != std::string::npos)
		{
			parameterList = fullName.substr(fullName.find_first_of(':'));
		}

		MonoClass* monoClass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());
		MonoObject* obj = mono_object_new(mono_domain_get(), monoClass);

		if (callConstructor)
		{
			MonoMethodDesc* desc = mono_method_desc_new(parameterList.c_str(), NULL);
			MonoMethod* constructor = mono_method_desc_search_in_class(desc, monoClass);
			MonoObject* exception = nullptr;
			mono_runtime_invoke(constructor, obj, parameters, &exception);
		}

		return obj;
	}

	bool ScriptEngine::LoadRuntimeAssembly(const std::filesystem::path& assemblyPath)
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

	MonoAssembly* ScriptEngine::LoadAssembly(const std::filesystem::path& assemblyPath)
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

	#ifdef EG_PLATFORM_WINDOWS

		HANDLE file = CreateFileA(assemblyPath, FILE_READ_ACCESS, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
			return nullptr;

		DWORD file_size = GetFileSize(file, NULL);
		if (file_size == INVALID_FILE_SIZE)
		{
			CloseHandle(file);
			return nullptr;
		}

		void* file_data = malloc(file_size);
		if (file_data == NULL)
		{
			CloseHandle(file);
			return nullptr;
		}

		DWORD read = 0;
		ReadFile(file, file_data, file_size, &read, NULL);
		if (file_size != read)
		{
			free(file_data);
			CloseHandle(file);
			return NULL;
		}

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char*>(file_data), file_size, 1, &status, 0);
		if (status != MONO_IMAGE_OK)
		{
			return NULL;
		}
		auto assemb = mono_assembly_load_from_full(image, assemblyPath, &status, 0);
		free(file_data);
		CloseHandle(file);
		mono_image_close(image);
		return assemb;
	#else
		EG_CORE_ASSERT("Rewrite to open on other platform");
		return nullptr;
	#endif
	}

	MonoMethod* ScriptEngine::GetMethod(MonoImage* image, const std::string& methodDesc)
	{
		MonoMethodDesc* desc = mono_method_desc_new(methodDesc.c_str(), false);
		if (!desc)
			EG_CORE_ERROR("[ScriptEngine] mono_method_desc_new failed ({0})", methodDesc);

		MonoMethod* method = mono_method_desc_search_in_image(desc, image);
		if (!method)
			EG_CORE_ERROR("[ScriptEngine] mono_method_desc_search_in_image failed ({0})", methodDesc);

		return method;
	}

	MonoImage* ScriptEngine::GetAssemblyImage(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		if (!image)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get assembly image!");
		return image;
	}
}
