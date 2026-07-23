#include "Logging.h"
#include <iostream>
#include "Framework/Config.h"

extern ConfigVar colorLog;
extern ConfigVar loglevel;
#include <chrono>
#include <iostream>
#include <fstream>
#include "GameEnginePublic.h"
#include "DebugConsole.h"
#include <iomanip>
#include <mutex>
#include <cstdarg>
Color32 get_color_of_print(LogType type) {

	const Color32 err = { 255, 105, 105 };
	const Color32 warn = { 252, 224, 121 };
	const Color32 info = COLOR_WHITE;
	const Color32 debug = { 136, 161, 252 };
	const Color32 consolePrint = { 136, 23, 152 };
	Color32 out = info;
	if (type == LogType::Error)
		out = err;
	else if (type == LogType::Warning)
		out = warn;
	else if (type == LogType::Debug)
		out = debug;
	else if (type == LogType::LtConsoleCommand)
		out = consolePrint;
	return out;
}

#include <iostream>

class ConsoleSink : public LogSink
{
public:
	void log(LogType type, const std::string& message) override {
		bool print_end = false;
		if (colorLog.get_bool()) {
			print_end = true;
			if (type == LogType::Error) {
				printf("\033[91m");
			}
			else if (type == LogType::Warning) {
				printf("\033[33m");
			}
			else if (type == LogType::Debug) {
				printf("\033[32m");
			}
			else if (type == LogType::LtConsoleCommand) {
				printf("\033[35m");
			}
			else
				print_end = false;
		}
		printf("%s", message.c_str());
		if (print_end)
			printf("\033[0m");
	}
};


class FileSink : public LogSink
{
public:
	FileSink(const std::string& filename) : file(filename) {}
	void log(LogType type, const std::string& message) override {
		if (file.is_open()) {
			if (type == LogType::Error) {
				file << "[Error] ";
			}
			else if (type == LogType::Warning) {
				file << "[Warning] ";
			}
			else if (type == LogType::Debug) {
				file << "[Debug] ";
			}
			else if (type == LogType::LtConsoleCommand) {

			}
			else {
				file << "[Info] ";
			}
			file << message << std::flush;
		}
	}

private:
	std::ofstream file;
};
class GameConsoleSink : public LogSink
{
	void log(LogType type, const std::string& message) override {
		if (Debug_Console::inst)
			Debug_Console::inst->print(type, "%s", message.c_str());
		if (type == LogType::LtConsoleCommand)
			eng->log_to_fullscreen_gui(LtConsoleCommand, message.c_str());
	}
};

Logger* Logger::inst = nullptr;

Logger* Logger::make_logger(std::string log_file, bool no_console_print)
{
	auto* log = new Logger;
	log->log_path = log_file;
	log->add_sink(std::make_shared<FileSink>(log_file));
	if (!no_console_print) {
		log->add_sink(std::make_shared<ConsoleSink>());
		log->add_sink(std::make_shared<GameConsoleSink>());
	}
	return log;
}

void Logger::log(LogType type, const char* fmt, va_list args) {
	if (static_cast<int>(type) > loglevel.get_integer())
		return;

	char buf[1024];

	vsnprintf(buf, sizeof(buf), fmt, args);
	buf[sizeof(buf) - 1] = 0;

	std::string message(buf);

	for (auto& sink : sinks) {
		sink->log(type, message);
	}
}

void sys_print(LogType type, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (Logger::inst)
		Logger::inst->log(type, fmt, args);
	else
		vprintf(fmt, args);
	va_end(args);
}
std::mutex printMutex; // fixme

char* string_format(const char* fmt, ...) {
	std::lock_guard<std::mutex> printLock(printMutex); // fixme

	va_list argptr;
	static int index = 0;
	static char string[4][512];
	char* buf;

	buf = string[index];
	index = (index + 1) & 3;

	va_start(argptr, fmt);
	vsprintf(buf, fmt, argptr);
	va_end(argptr);

	return buf;
}
void sys_vprint(const char* fmt, va_list args) {
	std::lock_guard<std::mutex> printLock(printMutex);

	vprintf(fmt, args);
	Debug_Console::inst->print_args(Info, fmt, args);
}

void Fatalf(const char* format, ...) {
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
	fflush(stdout);
	exit(-1);
}