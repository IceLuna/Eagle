#pragma once

#ifdef EG_PLATFORM_WINDOWS
	#if EG_DYNAMIC_LINK
		#ifdef EG_BUILD_DLL
			#define EAGLE_API __declspec(dllexport)
		#else
			#define EAGLE_API __declspec(dllimport)
		#endif
	#else
		#define EAGLE_API
	#endif

#else
	#error Eagle only supports windows for now!
 
#endif

#ifdef EG_ENABLE_ASSERTS
	#define EG_ASSERT(x, ...) { if(!(x)) {EG_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define EG_CORE_ASSERT(x, ...) { if(!(x)) {EG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define EG_ASSERT(x, ...)
	#define EG_CORE_ASSERT(x, ...)
#endif


#define BIT(x) (1 << x)

#define EG_BIND_FN(fn) std::bind(&fn, this, std::placeholders::_1)