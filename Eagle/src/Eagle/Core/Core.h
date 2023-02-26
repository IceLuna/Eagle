#pragma once

#include <memory>
#include <filesystem>

#include "PlatformDetection.h"
#include "DelayCall.h"
#include "EnumUtils.h"

#define EG_VERSION "0.6"
#define EG_VERSION_MAJOR 0
#define EG_VERSION_MINOR 6
#define EG_VERSION_PATCH 0

// TODO: undef it for game-builds
#define EG_WITH_EDITOR 1

#ifdef EG_WITH_EDITOR

#define EG_CPU_TIMINGS
#define EG_GPU_TIMINGS
#define EG_GPU_MARKERS 0

#endif

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
#endif

// __VA_ARGS__ expansion to get past MSVC "bug"
#define EG_EXPAND_VARGS(x) x

#ifdef EG_ENABLE_ASSERTS
	#define EG_ASSERT_NO_MESSAGE(condition) { if(!(condition)) { EG_ERROR("Assertion Failed"); EG_DEBUGBREAK(); } }
	#define EG_ASSERT_MESSAGE(condition, ...) { if(!(condition)) { EG_ERROR("Assertion Failed: {0}", __VA_ARGS__); EG_DEBUGBREAK(); } }

	#define EG_ASSERT_RESOLVE(arg1, arg2, macro, ...) macro
	#define EG_GET_ASSERT_MACRO(...) EG_EXPAND_VARGS(EG_ASSERT_RESOLVE(__VA_ARGS__, EG_ASSERT_MESSAGE, EG_ASSERT_NO_MESSAGE))

	#define EG_ASSERT(...) EG_EXPAND_VARGS( EG_GET_ASSERT_MACRO(__VA_ARGS__)(__VA_ARGS__) )
	#define EG_CORE_ASSERT(...) EG_EXPAND_VARGS( EG_GET_ASSERT_MACRO(__VA_ARGS__)(__VA_ARGS__) )
#else
#define EG_ASSERT_NO_MESSAGE(condition)
#define EG_ASSERT_MESSAGE(condition, ...)

#define EG_ASSERT_RESOLVE(arg1, arg2, macro, ...) macro
#define EG_GET_ASSERT_MACRO(...) EG_EXPAND_VARGS(EG_ASSERT_RESOLVE(__VA_ARGS__, EG_ASSERT_MESSAGE, EG_ASSERT_NO_MESSAGE))

#define EG_ASSERT(...) EG_EXPAND_VARGS( EG_GET_ASSERT_MACRO(__VA_ARGS__)(__VA_ARGS__) )
#define EG_CORE_ASSERT(...) EG_EXPAND_VARGS( EG_GET_ASSERT_MACRO(__VA_ARGS__)(__VA_ARGS__) )
#endif

#define BIT(x) (1 << x)

#define EG_BIND_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#define EG_SET_TIMER_BY_FUNC(fn, ms, ...) (new ::Eagle::DelayCall(std::bind(&fn, this, __VA_ARGS__), ms))

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

namespace Eagle
{
	using Path = std::filesystem::path;

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename Type, class... Args>
	constexpr Scope<Type> MakeScope(Args&&... args)
	{
		return std::make_unique<Type>(std::forward<Args>(args)...);
	}
	
	template<typename T>
	using Weak = std::weak_ptr<T>;

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

	template <class T>
	inline void HashCombine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	enum class TonemappingMethod
	{
		None,
		Reinhard,
		Filmic,
		ACES,
		PhotoLinear
	};

	struct PhotoLinearTonemappingParams
	{
		float Sensetivity = 1.f;
		float ExposureTime = 0.12f;
		float FStop = 1.f;
	};

	struct FilmicTonemappingParams
	{
		float WhitePoint = 1.f;
	};
}