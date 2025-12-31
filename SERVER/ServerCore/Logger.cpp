#include "pch.h"
#include "Logger.h"
#include "DBManager.h"

void Logger::Init(const std::string& filename, const std::string& basePath, LogLevel level)
{
	_level.store(level, std::memory_order_relaxed);
	if (not filename.empty()) {
		_ofs = std::make_unique<std::ofstream>(filename, std::ios::app);
	}
	_basePath = NormalizePath(basePath);
	_exitFlag.store(false);
	_worker = std::thread(&Logger::WorkerThread);

}

void Logger::Log(LogLevel level, const char* file, int line, const char* fmt, ...)
{
	if (level < _level.load(std::memory_order_relaxed))
		return;

	auto now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(now);

	std::tm bt;
	localtime_s(&bt, &in_time_t);

	std::ostringstream ts;
	ts << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");

	const char* levelString = "";
	switch (level) {
	case LogLevel::Debug: levelString = "DBG"; break;
	case LogLevel::Info: levelString = "INF"; break;
	case LogLevel::Warn: levelString = "WRN"; break;
	case LogLevel::Error: levelString = "ERR"; break;
	}

	std::string path = NormalizePath(file);

	if (not _basePath.empty() && path.rfind(_basePath, 0) == 0) {
		path.erase(0, _basePath.size());
	}

	ts << "[" << ts.str() << "][" << levelString << "][" << path << ":" << line << "] ";

	va_list ap;
	va_start(ap, fmt);

	char msg[512];
	std::vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	ts << msg << "\n";

	auto buf = new std::string{ ts.str() };
	_queue.push(buf);
}

void Logger::Shutdown()
{
	_exitFlag.store(true);
	if (_worker.joinable()) {
		_worker.join();
	}

	if (_ofs) {
		_ofs->flush();
	}
}

std::string Logger::NormalizePath(const std::string& p)
{
	std::string s = p;
	std::replace(s.begin(), s.end(), '\\', '/');
	if (!s.empty() && s.back() != '/') s += '/';
	return s;
}

void Logger::WorkerThread()
{
	auto out = _ofs ? static_cast<std::ostream*>(_ofs.get()) : &std::cout;
	std::vector<std::string*> batch;
	batch.reserve(256);

	while (not _exitFlag.load() or not _queue.empty()) {
		std::string* msg;
		while (_queue.try_pop(msg)) {
			batch.push_back(msg);
			if (batch.size() >= 256) {
				break;
			}
		}

		for (auto s : batch) {
			(*out) << *s;
			delete s;
		}

		if (not batch.empty()) {
			out->flush();
			batch.clear();
		}

		if (_queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}