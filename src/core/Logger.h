#pragma once

#include <string>
#include <string_view>

namespace sveta::core {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

class Logger {
public:
    static void Log(LogLevel level, std::string_view message);

    static void Debug(std::string_view message) { Log(LogLevel::Debug, message); }
    static void Info(std::string_view message) { Log(LogLevel::Info, message); }
    static void Warn(std::string_view message) { Log(LogLevel::Warn, message); }
    static void Error(std::string_view message) { Log(LogLevel::Error, message); }

private:
    static std::string Timestamp();
    static std::string_view LevelName(LogLevel level);
};

} // namespace sveta::core
