#pragma once

#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

enum LogLevel : char
{
	Debug,
	Info,
	Warn,
	Error
};

class Logger
{
public:
	static void Init(const std::string& filename = "", const std::string& basePath = "", LogLevel level = LogLevel::Info);
	static void Shutdown();

	static void Log(LogLevel level, const char* file, int line, const char* fmt, ...);
	
	static void SetLevel(LogLevel level) { _level.store(level); }

private:
	static std::string NormalizePath(const std::string& p);
	static void WorkerThread();

private:
	static inline std::string									_basePath;
	static inline std::atomic<LogLevel>							_level{ LogLevel::Info };
	static inline std::unique_ptr<std::ofstream>				_ofs;
	static inline concurrency::concurrent_queue<std::string*>	_queue;
	static inline std::thread									_worker;
	static inline std::atomic<bool>								_exitFlag{ false };
};

