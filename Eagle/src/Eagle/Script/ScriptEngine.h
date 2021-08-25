#pragma once

extern "C" {
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoAssembly MonoAssembly;
}

namespace Eagle
{
	class ScriptEngine
	{
	public:
		static void Init(const std::filesystem::path& assemblyPath);
		static void Shutdown();

		static MonoClass* GetClass(const std::string& namespaceName, const std::string& className);
		static MonoObject* Construct(const std::string& fullName, bool callConstructor, void** parameters);

	private:
		static bool LoadRuntimeAssembly(const std::filesystem::path& assemblyPath);
		static MonoAssembly* LoadAssembly(const std::filesystem::path& assemblyPath);
		static MonoAssembly* LoadAssemblyFromFile(const char* assemblyPath);
		static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);
		static MonoImage* GetAssemblyImage(MonoAssembly* assembly);
	};
}