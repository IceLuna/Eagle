#include "egpch.h"
#include "ScriptUtils.h"

#include "ScriptEngine.h"

#include <mono/jit/jit.h>

namespace Eagle
{
	Scope<MonoInstance> MonoInstance::Create(MonoClass* klass, std::string_view debugName)
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

	MonoInstance::~MonoInstance()
	{
		if (m_Handle != 0)
		{
			ScriptEngine::FreeHandle(m_Handle);
			m_Handle = 0;
			m_Instance = nullptr;
		}
	}

	MonoInstance::MonoInstance(MonoInstance&& other) noexcept
	{
		m_Handle = other.m_Handle;
		m_Instance = other.m_Instance;

		other.m_Handle = 0;
		other.m_Instance = nullptr;
	}

	MonoInstance& MonoInstance::operator=(MonoInstance&& other) noexcept
	{
		if (this == &other)
			return *this;

		m_Handle = other.m_Handle;
		m_Instance = other.m_Instance;

		other.m_Handle = 0;
		other.m_Instance = nullptr;

		return *this;
	}

	MonoStringHandler::MonoStringHandler(MonoString* monoStr)
	{
		m_Str = mono_string_to_utf8(monoStr);
		bSetByMono = m_Str != nullptr;
		if (!bSetByMono)
		{
			m_Str = "";
		}
	}

	MonoStringHandler::~MonoStringHandler()
	{
		if (bSetByMono)
			mono_free((void*)m_Str);
	}
}
