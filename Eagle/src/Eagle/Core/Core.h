#pragma once

#include <memory>

#include "PlatformDetection.h"
#include "DelayCall.h"

#define EG_VERSION "0.4"
#define EG_VERSION_MAJOR 0
#define EG_VERSION_MINOR 4
#define EG_VERSION_PATCH 0

#define EG_HOVER_THRESHOLD 0.5f

#ifdef EG_DEBUG
	#if defined(EG_PLATFORM_WINDOWS)
		#define EG_DEBUGBREAK() __debugbreak()
	#elif defined(EG_PLATFORM_LINUX)
		#include <signal.h>
		#define EG_DEBUGBREAK() raise(SIGTRAP)
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define EG_ENABLE_ASSERTS
#else
	#define EG_DEBUGBREAK()
#endif

#ifdef EG_DEBUG
	#define EG_ENABLE_ASSERTS
	#define EG_PROFILE
#endif

#ifdef EG_RELEASE
	#define EG_PROFILE
#endif

#undef EG_PROFILE

#ifdef EG_ENABLE_ASSERTS
	#define EG_ASSERT(x, ...) { if(!(x)) {EG_ERROR("Assertion Failed: {0}", __VA_ARGS__); EG_DEBUGBREAK(); } }
	#define EG_CORE_ASSERT(x, ...) { if(!(x)) {EG_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); EG_DEBUGBREAK(); } }
#else
	#define EG_ASSERT(x, ...)
	#define EG_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define EG_BIND_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#define EG_SET_TIMER_BY_FUNC(fn, ms, ...) (new ::Eagle::DelayCall(std::bind(&fn, this, __VA_ARGS__), ms))

#ifdef EG_PROFILE

	// Resolve which function signature macro will be used. Note that this only
	// is resolved when the (pre)compiler starts, so the syntax highlighting
	// could mark the wrong one in your editor!
	#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
		#define EG_FUNC_SIG __PRETTY_FUNCTION__
	#elif defined(__DMC__) && (__DMC__ >= 0x810)
		#define EG_FUNC_SIG __PRETTY_FUNCTION__
	#elif defined(__FUNCSIG__)
		#define EG_FUNC_SIG __FUNCSIG__
	#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
		#define EG_FUNC_SIG __FUNCTION__
	#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
		#define EG_FUNC_SIG __FUNC__
	#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
		#define EG_FUNC_SIG __func__
	#elif defined(__cplusplus) && (__cplusplus >= 201103)
		#define EG_FUNC_SIG __func__
	#else
		#define EG_FUNC_SIG "EG_FUNC_SIG unknown!"
	#endif

	#define EG_PROFILE_BEGIN_SESSION(name, filepath) ::Eagle::Instrumentor::Get().BeginSession(name, filepath)
	#define EG_PROFILE_END_SESSION() ::Eagle::Instrumentor::Get().EndSession()
	#define EG_PROFILE_SCOPE(name) EG_PROFILE_SCOPE_LINE(name, __LINE__)
	#define EG_PROFILE_SCOPE_LINE(name, line) ::Eagle::InstrumentationTimer timer##line (name)
	#define EG_PROFILE_FUNCTION() EG_PROFILE_SCOPE(EG_FUNC_SIG)
#else
	#define EG_PROFILE_BEGIN_SESSION(name, filepath)
	#define EG_PROFILE_END_SESSION()
	#define EG_PROFILE_SCOPE(name)
	#define EG_PROFILE_FUNCTION()
#endif

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

	template<typename ToType, typename FromType>
	constexpr Ref<ToType> Cast(const Ref<FromType>& ref)
	{
		return std::dynamic_pointer_cast<ToType>(ref);
	}
}