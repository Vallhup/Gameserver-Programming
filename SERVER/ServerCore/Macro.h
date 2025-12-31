#pragma once

#include "Logger.h"

#define LOG_DBG(fmt, ...) Logger::Log(LogLevel::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INF(fmt, ...) Logger::Log(LogLevel::Info , __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WRN(fmt, ...) Logger::Log(LogLevel::Warn , __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) Logger::Log(LogLevel::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)