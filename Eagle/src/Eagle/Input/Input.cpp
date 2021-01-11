#include "egpch.h"
#include "Input.h"

#ifdef EG_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsInput.h"
#endif

namespace Eagle
{
	Scope<Input> Input::s_Instance = Input::Create();
	
	Scope<Input> Input::Create()
	{
		#ifdef EG_PLATFORM_WINDOWS
			return MakeScope<WindowsInput>();
		#else
			EG_CORE_ASSERT(false, "Unknown platform!");
			return nullptr;
		#endif
	}
}