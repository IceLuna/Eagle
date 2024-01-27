#include "egpch.h"
#include "Window.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#ifdef EG_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Eagle
{
	Ref<Window> Window::Create(const WindowProperties& props)
	{
		#ifdef EG_PLATFORM_WINDOWS
			return MakeRef<WindowsWindow>(props);
		#else
			EG_CORE_ASSERT(false, "Unknown platform!");
			return nullptr;
		#endif
	}
}