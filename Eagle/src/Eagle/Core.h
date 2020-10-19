#pragma once

#ifdef EG_PLATFORM_WINDOWS
	#ifdef EG_BUILD_DLL
		#define EAGLE_API __declspec(dllexport)
	#else
		#define EAGLE_API __declspec(dllimport)
	#endif

#else
	#error Eagle only supports windows for now!
 
#endif