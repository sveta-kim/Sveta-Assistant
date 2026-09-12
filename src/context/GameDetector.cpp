#include "context/GameDetector.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <thread>

#include <nlohmann/json.hpp>

#include "context/EpicLibraryScanner.h"
#include "context/SteamLibraryScanner.h"
#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::context {

namespace {

// Real games observed off by a few pixels on every edge from the
// monitor's true bounds -- this app has no DPI-awareness manifest yet
// (see README), so coordinates it reads back can be off from another
// window's real ones by DPI-virtualization rounding. Exact-match used to
// reject genuinely fullscreen games for this reason.
constexpr int kMonitorMatchTolerancePx = 8;

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

    // Compared against the full monitor rect (taskbar included), not the
    // work area -- a normal maximized window (browser, IDE) snaps to the
    // work area and stays short of this, while an exclusive/borderless
    // fullscreen game deliberately covers the whole monitor. That
    // distinction alone is what's actually reliable; a real game's
    // WS_CAPTION bit was observed still set despite being visually
    // fullscreen, so that used to be checked too and isn't anymore.
    // (Known false-positive case: a normal window maximized with the
    // taskbar set to auto-hide looks the same as this.)
    const auto within = [](int a, int b) { return std::abs(a - b) <= kMonitorMatchTolerancePx; };
    return within(windowRect.left, monitorInfo.rcMonitor.left) &&
        within(windowRect.top, monitorInfo.rcMonitor.top) &&
        within(windowRect.right, monitorInfo.rcMonitor.right) &&
        within(windowRect.bottom, monitorInfo.rcMonitor.bottom);
}

} // namespace

namespace {

// Common apps that legitimately go fullscreen but aren't games -- the
// fullscreen heuristic can't otherwise tell these apart from an actual
// borderless-fullscreen game. User-editable/extendable via
// games_config.json's "fullscreen_heuristic_exclusions".
std::vector<std::wstring> DefaultFullscreenHeuristicExclusions() {
    return {
        L"chrome.exe",   L"msedge.exe", L"firefox.exe",         L"brave.exe",     L"opera.exe",
        L"vlc.exe",      L"mpv.exe",    L"wmplayer.exe",        L"mpc-hc64.exe",  L"mpc-hc.exe",
        L"POWERPNT.EXE", L"Spotify.exe", L"Code.exe",           L"devenv.exe",    L"WindowsTerminal.exe",
        L"explorer.exe", L"AcroRd32.exe", L"Acrobat.exe",       L"claude.exe",
    };
}

} // namespace

GamesConfig GamesConfig::Load() {
    GamesConfig config;
    config.fullscreenHeuristicExclusions = DefaultFullscreenHeuristicExclusions();

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
        // Additive, not a replacement: extra entries here add to the
        // built-in defaults above rather than requiring the user to
        // re-list every default just to add one more.
        for (const auto& entry : parsed.value("fullscreen_heuristic_exclusions", nlohmann::json::array())) {
            config.fullscreenHeuristicExclusions.push_back(core::Utf8ToWide(entry.get<std::string>()));
        }
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse games_config.json: ") + e.what());
    }

    return config;
}

GameDetector::GameDetector() : config_(GamesConfig::Load()) {
    std::thread([state = libraryScanState_]() {
        const std::vector<SteamGame> steamGames = ScanInstalledSteamGames();
        const std::vector<EpicGame> epicGames = ScanInstalledEpicGames();

        std::vector<std::wstring> exeNames;
        exeNames.reserve(steamGames.size() + epicGames.size());
        for (const auto& game : steamGames) {
            exeNames.push_back(game.exeName);
        }
        for (const auto& game : epicGames) {
            exeNames.push_back(game.exeName);
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->processNames = std::move(exeNames);
        }
        core::Logger::Info(std::format(
            "GameDetector: library scan found {} Steam + {} Epic game executable(s)", steamGames.size(),
            epicGames.size()));
    }).detach();
}

bool GameDetector::IsLikelyGame(const std::wstring& processName, HWND hwnd) const {
    for (const auto& known : config_.knownProcessNames) {
        if (_wcsicmp(known.c_str(), processName.c_str()) == 0) {
            return true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(libraryScanState_->mutex);
        for (const auto& known : libraryScanState_->processNames) {
            if (_wcsicmp(known.c_str(), processName.c_str()) == 0) {
                return true;
            }
        }
    }

    for (const auto& excluded : config_.fullscreenHeuristicExclusions) {
        if (_wcsicmp(excluded.c_str(), processName.c_str()) == 0) {
            return false;
        }
    }

    return IsFullscreenExclusive(hwnd);
}

} // namespace sveta::context
