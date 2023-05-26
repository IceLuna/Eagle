#include "egpch.h"

#include "Log.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>

namespace Eagle
{
	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
	std::mutex s_LogHistoryMutex;

	static std::vector<Log::LogMessage> s_LogHistory;

	void LoggerCallback(const spdlog::details::log_msg& msg)
	{
		std::scoped_lock lock(s_LogHistoryMutex);
		s_LogHistory.push_back({ std::string(msg.payload.begin(), msg.payload.end()), msg.level });
	}

	static auto CreateConsoleLogger()
	{
		auto sink = MakeRef<spdlog::sinks::stdout_color_sink_mt>();
		sink->set_pattern("%^[%T.%e] %n: %v%$");
		return sink;
	}

	static auto CreateFileLogger()
	{
		auto sink = MakeRef<spdlog::sinks::basic_file_sink_mt>("Eagle.log", true);
		sink->set_pattern("[%T.%e] [%l] %n: %v");
		return sink;
	}

	static auto CreateEditorLogger()
	{
		auto sink = MakeRef<spdlog::sinks::callback_sink_mt>(LoggerCallback);
		sink->set_pattern("[%T.%e] %n: %v");
		return sink;
	}

	void Log::Init()
	{
		std::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(CreateFileLogger());

#ifndef EG_DIST
		logSinks.emplace_back(CreateConsoleLogger());
#endif
#ifdef EG_WITH_EDITOR
		logSinks.emplace_back(CreateEditorLogger());
#endif

		s_CoreLogger = MakeRef<spdlog::logger>("EAGLE", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_CoreLogger);

		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::trace);

		s_ClientLogger = MakeRef<spdlog::logger>("APP", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);

		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::trace);

	}

	std::vector<Log::LogMessage> Log::GetLogHistory()
	{
		std::vector<Log::LogMessage> result;
		{
			std::scoped_lock lock(s_LogHistoryMutex);
			result = s_LogHistory;
		}
		return result;
	}
	
	void Log::ClearLogHistory()
	{
		s_LogHistory.clear();
	}
}