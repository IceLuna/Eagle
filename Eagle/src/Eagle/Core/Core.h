#pragma once

#include <memory>
#include <utility>
#include "Eagle/Core/Timestep.h"

namespace Eagle
{
	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename Type, class... Args>
	constexpr Scope<Type> MakeScope(Args&&... args)
	{
		return std::make_unique<Type>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename Type, class... Args>
	constexpr Ref<Type> MakeRef(Args&&... args)
	{
		return std::make_shared<Type>(std::forward<Args>(args)...);
	}
}

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

#ifdef EG_DIST
	#define EG_PROFILE 0
#else
	#define EG_PROFILE 1
#endif

#if EG_PROFILE
	#define EG_PROFILE_BEGIN_SESSION(name, filepath) ::Eagle::Instrumentor::Get().BeginSession(name, filepath)
	#define EG_PROFILE_END_SESSION() ::Eagle::Instrumentor::Get().EndSession()
	#define EG_PROFILE_SCOPE(name) ::Eagle::InstrumentationTimer timer##__LINE__(name);
	#define EG_PROFILE_FUNCTION() EG_PROFILE_SCOPE(__FUNCSIG__)
#else
	#define EG_PROFILE_BEGIN_SESSION(name, filepath)
	#define EG_PROFILE_END_SESSION()
	#define EG_PROFILE_SCOPE(name)
	#define EG_PROFILE_FUNCTION()
#endif

#define EG_SET_TIMER_BY_FUNC(fn, ms, ...) (new ::Eagle::DelayCall(std::bind(&fn, this, __VA_ARGS__), ms))
