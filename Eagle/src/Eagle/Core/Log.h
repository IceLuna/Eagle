#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace Eagle
{
	class Log
	{
	public:

		static void Init();
		inline static Ref<spdlog::logger>& GetCoreLogger() {return s_CoreLogger;}
		inline static Ref<spdlog::logger>& GetClientLogger() {return s_ClientLogger;}

	private:
		static Ref<spdlog::logger> s_CoreLogger;
		static Ref<spdlog::logger> s_ClientLogger;
	};
}

//Core Log MACROS
#define EG_CORE_TRACE(...) ::Eagle::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define EG_CORE_INFO(...)  ::Eagle::Log::GetCoreLogger()->info(__VA_ARGS__)
#define EG_CORE_WARN(...)  ::Eagle::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define EG_CORE_ERROR(...) ::Eagle::Log::GetCoreLogger()->error(__VA_ARGS__)
#define EG_CORE_CRITICAL(...) ::Eagle::Log::GetCoreLogger()->critical(__VA_ARGS__)

//Client Log MACROS
#define EG_TRACE(...) ::Eagle::Log::GetClientLogger()->trace(__VA_ARGS__)
#define EG_INFO(...)  ::Eagle::Log::GetClientLogger()->info(__VA_ARGS__)
#define EG_WARN(...)  ::Eagle::Log::GetClientLogger()->warn(__VA_ARGS__)
#define EG_ERROR(...) ::Eagle::Log::GetClientLogger()->error(__VA_ARGS__)
#define EG_CRITICAL(...) ::Eagle::Log::GetClientLogger()->critical(__VA_ARGS__)

