#pragma once

#include <memory>
#include "Core.h"
#include "spdlog/spdlog.h"

namespace Eagle
{
	class EAGLE_API Log
	{
	public:

		static void Init();
		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() {return s_CoreLogger;}
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() {return s_ClientLogger;}

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

//Core Log MACROS
#define EG_CORE_TRACE(...) ::Eagle::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define EG_CORE_INFO(...)  ::Eagle::Log::GetCoreLogger()->info(__VA_ARGS__)
#define EG_CORE_WARN(...)  ::Eagle::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define EG_CORE_ERROR(...) ::Eagle::Log::GetCoreLogger()->error(__VA_ARGS__)
#define EG_CORE_FATAL(...) ::Eagle::Log::GetCoreLogger()->fatal(__VA_ARGS__)

//Client Log MACROS
#define EG_TRACE(...) ::Eagle::Log::GetClientLogger()->trace(__VA_ARGS__)
#define EG_INFO(...)  ::Eagle::Log::GetClientLogger()->info(__VA_ARGS__)
#define EG_WARN(...)  ::Eagle::Log::GetClientLogger()->warn(__VA_ARGS__)
#define EG_ERROR(...) ::Eagle::Log::GetClientLogger()->error(__VA_ARGS__)
#define EG_FATAL(...) ::Eagle::Log::GetClientLogger()->fatal(__VA_ARGS__)

