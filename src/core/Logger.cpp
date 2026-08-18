#include "core/Logger.h"

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>

namespace sveta::core {

namespace {

std::filesystem::path LogFilePath() {
    wchar_t* localAppData = nullptr;
    size_t len = 0;
    _wdupenv_s(&localAppData, &len, L"LOCALAPPDATA");
    std::filesystem::path base = localAppData ? std::filesystem::path(localAppData) : std::filesystem::temp_directory_path();
    free(localAppData);

    std::filesystem::path dir = base / L"SvetaAssistant" / L"logs";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / L"sveta.log";
}

std::mutex& LogMutex() {
    static std::mutex mutex;
    return mutex;
}

} // namespace

std::string Logger::Timestamp() {
    const auto now = std::chrono::system_clock::now();
    return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::milliseconds>(now));
}

std::string_view Logger::LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::Log(LogLevel level, std::string_view message) {
    const std::string line = std::format("[{}] [{}] {}\n", Timestamp(), LevelName(level), message);

    OutputDebugStringA(line.c_str());

    std::lock_guard lock(LogMutex());
    std::ofstream file(LogFilePath(), std::ios::app);
    if (file.is_open()) {
        file << line;
    }
}

} // namespace sveta::core
