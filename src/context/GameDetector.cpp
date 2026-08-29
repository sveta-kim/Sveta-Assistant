#include "context/GameDetector.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::context {

namespace {

bool IsFullscreenExclusive(HWND hwnd) {
    if (!hwnd) {
        return false;
    }

    RECT windowRect{};
    if (!GetWindowRect(hwnd, &windowRect)) {
        return false;
    }

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        return false;
    }

    const bool coversMonitor = windowRect.left == monitorInfo.rcMonitor.left &&
        windowRect.top == monitorInfo.rcMonitor.top && windowRect.right == monitorInfo.rcMonitor.right &&
        windowRect.bottom == monitorInfo.rcMonitor.bottom;
    if (!coversMonitor) {
        return false;
    }

    // A maximized normal window (browser, IDE) still covers the monitor
    // but keeps WS_CAPTION; borderless-fullscreen games drop it.
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    return (style & WS_CAPTION) == 0;
}

} // namespace

GamesConfig GamesConfig::Load() {
    GamesConfig config;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "games_config.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        core::Logger::Warn(
            "Missing config/games_config.json; game detection will rely on the fullscreen heuristic only");
        return config;
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        for (const auto& entry : parsed.value("known_process_names", nlohmann::json::array())) {
            config.knownProcessNames.push_back(core::Utf8ToWide(entry.get<std::string>()));
        }
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse games_config.json: ") + e.what());
    }

    return config;
}

GameDetector::GameDetector() : config_(GamesConfig::Load()) {}

bool GameDetector::IsLikelyGame(const std::wstring& processName, HWND hwnd) const {
    for (const auto& known : config_.knownProcessNames) {
        if (_wcsicmp(known.c_str(), processName.c_str()) == 0) {
            return true;
        }
    }
    return IsFullscreenExclusive(hwnd);
}

} // namespace sveta::context
