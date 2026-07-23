#pragma once

#include "Framework/SysPrint.h"
#include <memory>
#include <mutex>

extern std::mutex printMutex;

class LogSink
{
public:
	virtual ~LogSink() = default;
	virtual void log(LogType type, const std::string& message) = 0;
};
extern Color32 get_color_of_print(LogType type);

class Logger
{
public:
	static Logger* inst;

	static Logger* make_logger(std::string log_file, bool no_console_print);

	// Path passed to make_logger, relative to the engine working dir. Lets tools (e.g.
	// AgentBridge's get_log command) find the log file without hardcoding it themselves.
	const std::string& get_log_path() const { return log_path; }

	void add_sink(std::shared_ptr<LogSink> sink) { sinks.push_back(sink); }
	void remove_sink(LogSink* sink) {
		sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
								   [sink](const std::shared_ptr<LogSink>& s) { return s.get() == sink; }),
					sinks.end());
	}

	void log(LogType type, const char* fmt, va_list args);

private:
	std::vector<std::shared_ptr<LogSink>> sinks;
	std::string log_path;
};