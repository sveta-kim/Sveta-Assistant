#include "memory/LongTermMemory.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/Paths.h"

namespace sveta::memory {

namespace {

std::filesystem::path StorePath() {
    std::filesystem::path dir = core::LocalAppDataDir() / "memory";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / "long_term.json";
}

std::string TodayDateString() {
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    return std::format("{:04}-{:02}-{:02}", localTime.wYear, localTime.wMonth, localTime.wDay);
}

// Parses a "YYYY-MM-DD" string produced by TodayDateString() into a
// FILETIME (midnight local time) so day counts can be computed by
// subtraction rather than manual calendar math (leap years, etc.).
FILETIME DateStringToFileTime(const std::string& date) {
    SYSTEMTIME systemTime{};
    if (date.size() == 10) {
        systemTime.wYear = static_cast<WORD>(std::stoi(date.substr(0, 4)));
        systemTime.wMonth = static_cast<WORD>(std::stoi(date.substr(5, 2)));
        systemTime.wDay = static_cast<WORD>(std::stoi(date.substr(8, 2)));
    }
    FILETIME fileTime{};
    SystemTimeToFileTime(&systemTime, &fileTime);
    return fileTime;
}

std::vector<std::string> TopN(const std::unordered_map<std::string, int64_t>& counts, int n) {
    std::vector<std::pair<std::string, int64_t>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<std::string> result;
    for (int i = 0; i < n && i < static_cast<int>(sorted.size()); ++i) {
        result.push_back(sorted[i].first);
    }
    return result;
}

} // namespace

LongTermMemory LongTermMemory::Load() {
    LongTermMemory memory;

    std::ifstream file(StorePath());
    if (!file.is_open()) {
        memory.firstLaunchDate_ = TodayDateString(); // first run ever
        return memory;
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        memory.firstLaunchDate_ = parsed.value("first_launch_date", TodayDateString());
        for (const auto& [key, value] : parsed.value("process_active_seconds", nlohmann::json::object()).items()) {
            memory.processActiveSeconds_[key] = value.get<int64_t>();
        }
        for (const auto& [key, value] : parsed.value("games_active_seconds", nlohmann::json::object()).items()) {
            memory.gamesActiveSeconds_[key] = value.get<int64_t>();
        }
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse long_term.json: ") + e.what());
        memory.firstLaunchDate_ = TodayDateString();
    }

    return memory;
}

void LongTermMemory::Save() const {
    nlohmann::json out;
    out["first_launch_date"] = firstLaunchDate_;
    out["process_active_seconds"] = processActiveSeconds_;
    out["games_active_seconds"] = gamesActiveSeconds_;

    std::ofstream file(StorePath());
    if (!file.is_open()) {
        core::Logger::Error("Failed to open long_term.json for writing");
        return;
    }
    file << out.dump(4);
}

void LongTermMemory::RecordActiveSecond(const std::string& processName, bool isGaming) {
    if (processName.empty()) {
        return;
    }
    ++processActiveSeconds_[processName];
    if (isGaming) {
        ++gamesActiveSeconds_[processName];
    }
}

int LongTermMemory::DaysSinceFirstLaunch() const {
    const FILETIME first = DateStringToFileTime(firstLaunchDate_);
    const FILETIME today = DateStringToFileTime(TodayDateString());

    ULARGE_INTEGER firstLarge{};
    firstLarge.LowPart = first.dwLowDateTime;
    firstLarge.HighPart = first.dwHighDateTime;
    ULARGE_INTEGER todayLarge{};
    todayLarge.LowPart = today.dwLowDateTime;
    todayLarge.HighPart = today.dwHighDateTime;

    if (todayLarge.QuadPart < firstLarge.QuadPart) {
        return 1; // clock skew or malformed date; don't report a negative
    }
    constexpr uint64_t kTicksPerDay = 24ULL * 60ULL * 60ULL * 10'000'000ULL;
    return static_cast<int>((todayLarge.QuadPart - firstLarge.QuadPart) / kTicksPerDay) + 1;
}

std::vector<std::string> LongTermMemory::TopPrograms(int n) const {
    return TopN(processActiveSeconds_, n);
}

std::vector<std::string> LongTermMemory::TopGames(int n) const {
    return TopN(gamesActiveSeconds_, n);
}

} // namespace sveta::memory
