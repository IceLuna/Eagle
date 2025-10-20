#pragma once

#include "PublicField.h"

#include <map>

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
	struct UnmanagedMethod
	{
		MonoMethod* Method = nullptr;
		void* Thunk = nullptr;

		operator bool() const
		{
			return Method != nullptr;
		}
	};

	struct EntityScriptMethods
	{
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
	};

	class MonoInstance
	{
	public:
		~MonoInstance();

		MonoObject* GetInstance() const { return m_Instance; }
		bool IsValid() const { return m_Handle != 0; }

		static Scope<MonoInstance> Create(MonoClass* klass, std::string_view debugName);

	private:
		MonoInstance() = default;
		MonoInstance(const MonoInstance&) = delete;
		MonoInstance(MonoInstance&& other) noexcept;

		MonoInstance& operator=(const MonoInstance&) = delete;
		MonoInstance& operator=(MonoInstance&& other) noexcept;

		uint32_t m_Handle = 0u;
		MonoObject* m_Instance = nullptr;
	};

	struct ScriptClass
	{
		MonoClass* Class = nullptr;
		std::string FullName; // Namespace.Name
		std::string UIName;
		std::string Tooltip;
		std::map<std::string, PublicField> Fields;
		bool bUserClass = false; // Controlled by ScriptEngine. Should not be modified by other code

		bool operator== (const ScriptClass& other) const
		{
			return FullName == other.FullName;
		}

		bool operator!= (const ScriptClass& other) const
		{
			return !(*this == other);
		}

		bool operator< (const ScriptClass& other) const
		{
			return FullName < other.FullName;
		}
	};

	struct EntityScriptClass
	{
		ScriptClass ClassData;
		EntityScriptMethods Methods;

		void InitClassMethods();

		bool operator< (const EntityScriptClass& other) const
		{
			return ClassData < other.ClassData;
		}
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
}
