#include "egpch.h"

#include "Log.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>

namespace Eagle
{
	class my_callback_sink_mt : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		using CallbackFunc = std::function<void(std::string&&, spdlog::level::level_enum)>;

		explicit my_callback_sink_mt(CallbackFunc cb)
			: m_Callback(std::move(cb)) {
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			if (m_Callback)
			{
				spdlog::memory_buf_t formatted;
				this->formatter_->format(msg, formatted);
				m_Callback(std::string(formatted.data(), formatted.size()), msg.level);
			}
		}

		void flush_() override {}

	private:
		CallbackFunc m_Callback;
	};

	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
	std::mutex s_LogHistoryMutex;

	static std::vector<Log::LogMessage> s_LogHistory;

	static void LoggerCallback(std::string&& msg, spdlog::level::level_enum level)
	{
		std::scoped_lock lock(s_LogHistoryMutex);
		s_LogHistory.emplace_back(std::move(msg), level);
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

	static auto CreateCallbackLogger()
	{
		auto sink = MakeRef<my_callback_sink_mt>(LoggerCallback);
		sink->set_pattern("[%T.%e] %n: %v");
		return sink;
	}

	void Log::Init()
	{
		std::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(CreateFileLogger());

#ifndef EG_RELEASE
		logSinks.emplace_back(CreateConsoleLogger());
#endif
		logSinks.emplace_back(CreateCallbackLogger());

		s_CoreLogger = MakeRef<spdlog::logger>("EAGLE", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_CoreLogger);

		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::trace);

		s_ClientLogger = MakeRef<spdlog::logger>("APP", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);

		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::trace);

	}

	void Log::Destroy()
	{
		ClearLogHistory();
		s_CoreLogger.reset();
		s_ClientLogger.reset();
		spdlog::drop_all();
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
		std::scoped_lock lock(s_LogHistoryMutex);
		s_LogHistory.clear();
	}
}